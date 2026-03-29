#include "base_data_loader.h"
#include "p999_latency/check_latency.h"

DECLARE_LATENCY_MEMBERS(1000)

BaseWebSocketClient::BaseWebSocketClient(net::io_context &ioc, ssl::context &ssl_ctx) :
resolver_(ioc), // для DNS запросов
ws_(ioc, ssl_ctx), // WebSocket поток с SSL
reconnect_timer_(ioc), // таймер для переподключения
ioc_(ioc), // сохраняем ссылку на io_context
ping_timer_(ioc),
typeMessage_(TypeMessage_Unknown) { // таймер для ping сообщений
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

    // *** УСТАНАВЛИВАЕМ CONTROL_CALLBACK ***
    // Этот callback будет вызываться каждый раз, когда приходит управляющий фрейм (PING/PONG/CLOSE)
    ws_.control_callback([this](websocket::frame_type kind, beast::string_view payload) {
        on_control_frame(kind, payload);
    });
}

// Основной метод для запуска подключения
void BaseWebSocketClient::connect(const std::string &host, const std::string &port,
        const std::string &target = "/v5/public/linear") {
    host_ = host;
    port_ = port;
    target_ = target;

    std::cout << "Начинаем подключение к " << host << ":" << port << std::endl;

    // Асинхронно разрешаем DNS имя хоста
    resolver_.async_resolve(host, port,
            beast::bind_front_handler(&BaseWebSocketClient::on_resolve, shared_from_this()));
}

// Обработчик результата DNS резолвинга
void BaseWebSocketClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        std::cerr << "Ошибка резолвинга: " << ec.message() << std::endl;
        schedule_reconnect(); // Планируем переподключение
        return;
    }

    std::cout << "DNS разрешен, устанавливаем соединение..." << std::endl;

    // Асинхронно подключаемся к полученному адресу
    beast::get_lowest_layer(ws_).async_connect(results,
            beast::bind_front_handler(&BaseWebSocketClient::on_connect, shared_from_this()));
}

// Обработчик успешного TCP подключения
void BaseWebSocketClient::on_connect(beast::error_code ec,
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
            beast::bind_front_handler(&BaseWebSocketClient::on_ssl_handshake, shared_from_this()));
}

// Обработчик завершения SSL handshake
void BaseWebSocketClient::on_ssl_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка SSL handshake: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "SSL handshake выполнен, выполняем WebSocket handshake..." << std::endl;

    // Выполняем WebSocket handshake
    ws_.async_handshake(host_, target_,
            beast::bind_front_handler(&BaseWebSocketClient::on_handshake, shared_from_this()));
}

// Запуск периодического PING для поддержания соединения
void BaseWebSocketClient::start_ping() {
    ping_timer_.expires_after(std::chrono::seconds(5));
    ping_timer_.async_wait(
            beast::bind_front_handler(&BaseWebSocketClient::on_ping_timer, shared_from_this()));
}

// Обработчик таймера PING
void BaseWebSocketClient::on_ping_timer(beast::error_code ec) {
    if (ec) {
        // Таймер был отменен
        return;
    }

    if (ws_.is_open()) {
        // Отправляем PING фрейм
        ping_sent_time_ = std::chrono::steady_clock::now();
        ws_.async_ping({},
                beast::bind_front_handler(&BaseWebSocketClient::on_ping_sent, shared_from_this()));
    }
}

// Обработчик отправки PING
void BaseWebSocketClient::on_ping_sent(beast::error_code ec) {
    if (!ec) {
        // Если PING отправлен успешно, планируем следующий
        ping_timer_.expires_after(std::chrono::seconds(5));
        ping_timer_.async_wait(beast::bind_front_handler(&BaseWebSocketClient::on_ping_timer,
                shared_from_this()));
    }
}

// Планирование переподключения при ошибке
void BaseWebSocketClient::schedule_reconnect() {
    // std::cout << "Планируем переподключение через 5 секунд..." << std::endl;

    // // Закрываем текущее соединение, если оно открыто
    // if (ws_.is_open()) {
    //     beast::error_code ec;
    //     ws_.close(websocket::close_code::normal, ec);
    // }

    // // Планируем переподключение
    // reconnect_timer_.expires_after(std::chrono::seconds(5));
    // reconnect_timer_.async_wait(beast::bind_front_handler(&BaseWebSocketClient::on_reconnect_timer,
    //         shared_from_this()));
}

// Обработчик таймера переподключения
void BaseWebSocketClient::on_reconnect_timer(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка таймера переподключения: " << ec.message() << std::endl;
        return;
    }

    std::cout << "Пытаемся переподключиться..." << std::endl;
    connect(host_, port_, target_);
}

void BaseWebSocketClient::measure_latency(std::chrono::steady_clock::time_point sent_time) {
    auto now = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(now - sent_time).count();

    std::cout << "WebSocket PING/PONG RTT: " << latency << " ms" << std::endl;

    // Здесь можно сохранять latency в вашу систему мониторинга
    // например, в глобальную переменную или вызывать колбэк
}

void BaseWebSocketClient::on_control_frame(websocket::frame_type kind,
        beast::string_view payload) {
    std::cout << "on_control_frame" << std::endl;
    measure_latency(ping_sent_time_);
}
