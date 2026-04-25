#include "base_websocket_client.h"
#include "p999_latency/check_latency.h"
#include <thread>

DECLARE_LATENCY_MEMBERS(1000)

BaseWebSocketClient::BaseWebSocketClient(net::io_context &ioc, ssl::context &sslCtx,
        const std::string_view userAgent) :
resolver_(ioc), // для DNS запросов
ws_(ioc, sslCtx), // WebSocket поток с SSL
reconnectTimer_(ioc), // таймер для переподключения
ioc_(ioc), // сохраняем ссылку на io_context
pingTimer_(ioc),
isWaitPing_(false),
typeMessage_(TypeMessage_Unknown) { // таймер для ping сообщений
    // Устанавливаем заголовок User-Agent (необязательно, но рекомендуется)
    ws_.set_option(websocket::stream_base::decorator([userAgent](websocket::request_type &req) {
        req.set(beast::http::field::user_agent, userAgent);
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

    ws_.control_callback([this](websocket::frame_type kind, beast::string_view payload) {
        onControlFrame(kind, payload);
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
            beast::bind_front_handler(&BaseWebSocketClient::onResolve, shared_from_this()));
}

void BaseWebSocketClient::setReconnectCallback(std::function<void()> callback) {
    reconnectCallback_ = callback;
}

// Обработчик результата DNS резолвинга
void BaseWebSocketClient::onResolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        std::cerr << "Ошибка резолвинга: " << ec.message() << std::endl;
        scheduleReconnect(); // Планируем переподключение
        return;
    }

    std::cout << "DNS разрешен, устанавливаем соединение..." << std::endl;

    // Асинхронно подключаемся к полученному адресу
    beast::get_lowest_layer(ws_).async_connect(results,
            beast::bind_front_handler(&BaseWebSocketClient::onConnect, shared_from_this()));
}

// Обработчик успешного TCP подключения
void BaseWebSocketClient::onConnect(beast::error_code ec,
        tcp::resolver::results_type::endpoint_type) {
    if (ec) {
        std::cerr << "Ошибка подключения: " << ec.message() << std::endl;
        scheduleReconnect();
        return;
    }

    std::cout << "TCP соединение установлено, выполняем SSL handshake..." << std::endl;

    // Устанавливаем SNI (Server Name Indication)
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
        beast::error_code ec {static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::cerr << "Ошибка установки SNI: " << ec.message() << std::endl;
        scheduleReconnect();
        return;
    }

    // Выполняем SSL handshake
    ws_.next_layer().async_handshake(ssl::stream_base::client,
            beast::bind_front_handler(&BaseWebSocketClient::onSslHandshake, shared_from_this()));
}

// Обработчик завершения SSL handshake
void BaseWebSocketClient::onSslHandshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка SSL handshake: " << ec.message() << std::endl;
        scheduleReconnect();
        return;
    }

    std::cout << "SSL handshake выполнен, выполняем WebSocket handshake..." << std::endl;

    // Выполняем WebSocket handshake
    ws_.async_handshake(host_, target_,
            beast::bind_front_handler(&BaseWebSocketClient::onHandshake, shared_from_this()));
}

// Запуск периодического PING для поддержания соединения
void BaseWebSocketClient::startPing() {
    pingTimer_.expires_after(std::chrono::seconds(5));
    pingTimer_.async_wait(
            beast::bind_front_handler(&BaseWebSocketClient::onPingTimer, shared_from_this()));
}

// Обработчик таймера PING
void BaseWebSocketClient::onPingTimer(beast::error_code ec) {
    if (ec) {
        // Таймер был отменен
        return;
    }

    if (isWaitPing_.load()) {
        scheduleReconnect();
    }

    if (ws_.is_open()) {
        isWaitPing_.store(true);
        // Отправляем PING фрейм
        pingSentTime_ = std::chrono::steady_clock::now();
        ws_.async_ping({},
                beast::bind_front_handler(&BaseWebSocketClient::onPingSent, shared_from_this()));
    }
}

// Обработчик отправки PING
void BaseWebSocketClient::onPingSent(beast::error_code ec) {
    if (!ec) {
        // Если PING отправлен успешно, планируем следующий
        pingTimer_.expires_after(std::chrono::seconds(5));
        pingTimer_.async_wait(
                beast::bind_front_handler(&BaseWebSocketClient::onPingTimer, shared_from_this()));
    }
}

// Планирование переподключения при ошибке
void BaseWebSocketClient::scheduleReconnect() {
    bool expected = false;
    if (!isReconnecting_.compare_exchange_strong(expected, true)) {
        std::cout << "Already reconnecting, ignoring duplicate request" << std::endl;
        return; // Уже переподключаемся - выходим
    }

    // Закрываем текущее соединение, если оно открыто
    if (!ws_.is_open()) {
        std::cout << "соединение уже закрыто" << std::endl;
    }
    std::cout << "Планируем переподключение через 5 секунд..." << std::endl;
    // Планируем переподключение
    reconnectTimer_.expires_after(std::chrono::seconds(5));
    // Это может вызвать ошибки в других операциях, но соединение будет закрыто
    reconnectTimer_.async_wait(
            beast::bind_front_handler(&BaseWebSocketClient::onClose, shared_from_this()));
}

void BaseWebSocketClient::onClose(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка таймера переподключения: " << ec.message() << std::endl;
    }

    pingTimer_.cancel();
    reconnectTimer_.cancel();

    // Получаем нижний слой (tcp socket)
    auto &lowest_layer = beast::get_lowest_layer(ws_);

    // Закрываем TCP сокет напрямую
    lowest_layer.close();
    std::cout << "Соединение закрыто" << std::endl;

    isReconnecting_.store(false);
    if (reconnectCallback_) {
        std::cout << "Вызываем колбэк переподключения" << std::endl;
        reconnectCallback_();
    }
}

// Обработчик таймера переподключения
void BaseWebSocketClient::onReconnectTimer(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка таймера переподключения: " << ec.message() << std::endl;
        return;
    }

    std::cout << "Пытаемся переподключиться..." << std::endl;
    // connect(host_, port_, target_);
}

void BaseWebSocketClient::measureLatency(std::chrono::steady_clock::time_point sentTime) {
    auto now = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(now - sentTime).count();

    std::cout << "WebSocket PING/PONG RTT: " << latency << " ms" << std::endl;
}

void BaseWebSocketClient::onControlFrame(websocket::frame_type kind, beast::string_view payload) {
    if (kind == boost::beast::websocket::frame_type::pong) {
        measureLatency(pingSentTime_);
        isWaitPing_.store(false);
    }
}
