#include "data_loader.h"

BybitWebSocketClient::BybitWebSocketClient(net::io_context &ioc, ssl::context &ssl_ctx,
        OrderBook *const orderBook, StatusMessage *const statusMessage,
        PublicTrade *const publicTrade) :
orderbookParser_(orderBook),
statusParser_(statusMessage),
publicTradeJsonParser_(publicTrade),
resolver_(ioc), // для DNS запросов
ws_(ioc, ssl_ctx), // WebSocket поток с SSL
reconnect_timer_(ioc), // таймер для переподключения
ioc_(ioc), // сохраняем ссылку на io_context
ping_timer_(ioc),
typeMessage_(TypeMessage_Unknown) { // таймер для ping сообщений
    std::cout << orderBook << std::endl;

    // Устанавливаем заголовок User-Agent (необязательно, но рекомендуется)
    ws_.set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
        req.set(beast::http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) + " bybit-client");
    }));

    // Включаем сжатие permessage-deflate (поддерживается Bybit)
    ws_.set_option(websocket::permessage_deflate {
            true, // server_enable - предлагать расширение на сервере
            true, // client_enable - предлагать расширение на клиенте
            15, // server_max_window_bits - макс. размер окна сервера
            15, // client_max_window_bits - макс. размер окна клиента
            false, // server_no_context_takeover - без контекста сервера
            false, // client_no_context_takeover - без контекста клиента
            8, // compLevel - уровень сжатия (0-9)
            4 // memLevel - уровень памяти (1-9)
    });
}

// Основной метод для запуска подключения
void BybitWebSocketClient::connect(const std::string &host, const std::string &port,
        const std::string &target = "/v5/public/linear") {
    host_ = host;
    port_ = port;
    target_ = target;

    std::cout << "Начинаем подключение к " << host << ":" << port << std::endl;

    // Асинхронно разрешаем DNS имя хоста
    resolver_.async_resolve(host, port,
            beast::bind_front_handler(&BybitWebSocketClient::on_resolve, shared_from_this()));
}

// Обработчик результата DNS резолвинга
void BybitWebSocketClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        std::cerr << "Ошибка резолвинга: " << ec.message() << std::endl;
        schedule_reconnect(); // Планируем переподключение
        return;
    }

    std::cout << "DNS разрешен, устанавливаем соединение..." << std::endl;

    // Асинхронно подключаемся к полученному адресу
    beast::get_lowest_layer(ws_).async_connect(results,
            beast::bind_front_handler(&BybitWebSocketClient::on_connect, shared_from_this()));
}

// Обработчик успешного TCP подключения
void BybitWebSocketClient::on_connect(beast::error_code ec,
        tcp::resolver::results_type::endpoint_type) {
    if (ec) {
        std::cerr << "Ошибка подключения: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "TCP соединение установлено, выполняем SSL handshake..." << std::endl;

    // Устанавливаем SNI (Server Name Indication)
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
        beast::error_code ec {static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::cerr << "Ошибка установки SNI: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    // Выполняем SSL handshake
    ws_.next_layer().async_handshake(ssl::stream_base::client,
            beast::bind_front_handler(&BybitWebSocketClient::on_ssl_handshake, shared_from_this()));
}

// Обработчик завершения SSL handshake
void BybitWebSocketClient::on_ssl_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка SSL handshake: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "SSL handshake выполнен, выполняем WebSocket handshake..." << std::endl;

    // Выполняем WebSocket handshake
    ws_.async_handshake(host_, target_,
            beast::bind_front_handler(&BybitWebSocketClient::on_handshake, shared_from_this()));
}

// Обработчик завершения WebSocket handshake
void BybitWebSocketClient::on_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка WebSocket handshake: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "WebSocket соединение установлено!" << std::endl;
    // Подписываемся на потоки данных Bybit
    subscribe_to_streams();

    // Запускаем пинг-понг для поддержания соединения
    // start_ping();

    // Начинаем читать сообщения
    do_read();
}

// Отправка подписки на потоки данных
void BybitWebSocketClient::subscribe_to_streams() {
    // constexpr - строка известна на этапе компиляции
    static constexpr char subscription_msg[] = "{\"op\":\"subscribe\",\"args\":["
                                               "\"orderbook.50.BTCUSDT\","
                                               "\"publicTrade.BTCUSDT\""
                                               "]}";
    // .глубина. 1 - каждые 10 мс, 50 - каждые 20 мс, 200 - каждые 100мс, 500 - каждые 200

    std::cout << "Отправляем подписку: " << subscription_msg << std::endl;

    ws_.async_write(net::buffer(subscription_msg, sizeof(subscription_msg) - 1),
            beast::bind_front_handler(&BybitWebSocketClient::on_subscribe_sent,
                    shared_from_this()));
}

// Обработчик отправки подписки
void BybitWebSocketClient::on_subscribe_sent(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Ошибка отправки подписки: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "Подписка отправлена, ожидаем данные..." << std::endl;
}

// Асинхронное чтение сообщений
void BybitWebSocketClient::do_read() {
    // Буфер для хранения полученных данных
    buffer_.clear();

    // Асинхронно читаем сообщение
    ws_.async_read(buffer_,
            beast::bind_front_handler(&BybitWebSocketClient::on_read, shared_from_this()));
}

// Обработчик полученных сообщений
void BybitWebSocketClient::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == websocket::error::closed) {
            std::cout << "WebSocket соединение закрыто нормально" << std::endl;
        } else {
            std::cerr << "Ошибка чтения: " << ec.message() << std::endl;
        }

        schedule_reconnect();
        return;
    }

    // Преобразуем полученные данные в строку
    message_ = beast::buffers_to_string(buffer_.data());
    message_view_ = message_;
    try {
        typeMessage_ = parseTypeMessage(message_view_.substr(0, maxTypeStrLen));
        switch (typeMessage_) {
            case TypeMessage_Orderbook:
                orderbookParser_.setString(message_view_);
                orderbookParser_.parse();
                break;
            case TypeMessage_PublicTrade:
                publicTradeJsonParser_.setString(message_view_);
                publicTradeJsonParser_.parse();
                break;
            case TypeMessage_Status:
                statusParser_.setString(message_view_);
                statusParser_.parse();
                break;
            default:
                std::cout << "BybitWebSocketClient::on_read: Unknown message type" << std::endl;
                // std::cout << message_view_ << std::endl;
                break;
        }
    } catch (const std::exception &e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        std::cout << "Сырые данные: " << message_view_ << std::endl;
    }

    // Продолжаем чтение следующих сообщений
    do_read();
}

// Обработка snapshot стакана
void BybitWebSocketClient::process_orderbook_snapshot(const std::string &data) {
    // if (data.contains("data")) {
    //     auto &orderbook = data["data"];

    //     if (orderbook.contains("b")) {
    //         std::cout << "Bids (покупка):" << std::endl;
    //         for (const auto &bid : orderbook["b"]) {
    //             std::cout << "  Цена: " << bid[0] << ", Объем: " << bid[1] << std::endl;
    //         }
    //     }

    //     if (orderbook.contains("a")) {
    //         std::cout << "Asks (продажа):" << std::endl;
    //         for (const auto &ask : orderbook["a"]) {
    //             std::cout << "  Цена: " << ask[0] << ", Объем: " << ask[1] << std::endl;
    //         }
    //     }
    // }
}

// Обработка delta обновлений стакана
void BybitWebSocketClient::process_orderbook_delta(const std::string &data) {
    // if (data.contains("data")) {
    //     auto &delta = data["data"];

    //     if (delta.contains("b") && !delta["b"].empty()) {
    //         std::cout << "Изменения в bids (покупка): " << delta["b"].size() << " обновлений"
    //                   << std::endl;
    //     }

    //     if (delta.contains("a") && !delta["a"].empty()) {
    //         std::cout << "Изменения в asks (продажа): " << delta["a"].size() << " обновлений"
    //                   << std::endl;
    //     }
    // }
}

// Обработка данных о сделках (текущая цена)
void BybitWebSocketClient::process_trade_data(const std::string &data) {
    // if (data.contains("data")) {
    //     for (const auto &trade : data["data"]) {
    //         std::string price = trade["p"];
    //         std::string volume = trade["v"];
    //         std::string side = trade["S"]; // "Buy" или "Sell"

    //         std::cout << "Сделка: цена=" << price << ", объем=" << volume << ", сторона=" << side
    //                   << std::endl;

    //         // Здесь можно сохранять последнюю цену
    //         last_price_ = price;
    //     }
    // }
}

// Запуск периодического PING для поддержания соединения
void BybitWebSocketClient::start_ping() {
    ping_timer_.expires_after(std::chrono::seconds(20));
    ping_timer_.async_wait(
            beast::bind_front_handler(&BybitWebSocketClient::on_ping_timer, shared_from_this()));
}

// Обработчик таймера PING
void BybitWebSocketClient::on_ping_timer(beast::error_code ec) {
    if (ec) {
        // Таймер был отменен
        return;
    }

    if (ws_.is_open()) {
        // Отправляем PING фрейм
        ws_.async_ping({},
                beast::bind_front_handler(&BybitWebSocketClient::on_ping_sent, shared_from_this()));
    }
}

// Обработчик отправки PING
void BybitWebSocketClient::on_ping_sent(beast::error_code ec) {
    if (!ec) {
        // Если PING отправлен успешно, планируем следующий
        ping_timer_.expires_after(std::chrono::seconds(20));
        ping_timer_.async_wait(beast::bind_front_handler(&BybitWebSocketClient::on_ping_timer,
                shared_from_this()));
    }
}

// Планирование переподключения при ошибке
void BybitWebSocketClient::schedule_reconnect() {
    // std::cout << "Планируем переподключение через 5 секунд..." << std::endl;

    // // Закрываем текущее соединение, если оно открыто
    // if (ws_.is_open()) {
    //     beast::error_code ec;
    //     ws_.close(websocket::close_code::normal, ec);
    // }

    // // Планируем переподключение
    // reconnect_timer_.expires_after(std::chrono::seconds(5));
    // reconnect_timer_.async_wait(beast::bind_front_handler(&BybitWebSocketClient::on_reconnect_timer,
    //         shared_from_this()));
}

// Обработчик таймера переподключения
void BybitWebSocketClient::on_reconnect_timer(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка таймера переподключения: " << ec.message() << std::endl;
        return;
    }

    std::cout << "Пытаемся переподключиться..." << std::endl;
    connect(host_, port_, target_);
}
