#pragma once

#include "base_data_loader.h"

// Класс WebSocket клиента для Bybit
class PrivateWebSocketClient : public BaseWebSocketClient {
public:
    // Конструктор: инициализируем все необходимые компоненты
    PrivateWebSocketClient(net::io_context &ioc, ssl::context &ssl_ctx, const std::string &api_key,
            const std::string &api_secret);

private:
    StatusMessage statusMessage_;
    StatusJsonParser statusParser_;
    std::string api_key_;
    std::string api_secret_;
    bool authenticated_;

    // Таймер для ожидания аутентификации
    net::steady_timer auth_timer_;
    // Обработчик завершения WebSocket handshake
    void on_handshake(beast::error_code ec) override;

    // Аутентификация
    void authenticate();

    // Обработчик ответа аутентификации
    void on_auth_response(beast::error_code ec, std::size_t bytes_transferred);

    // Отправка подписки на потоки данных
    void subscribe_to_streams() override;

    // Обработчик отправки подписки
    void on_subscribe_sent(beast::error_code ec, std::size_t bytes_transferred) override;

    // Асинхронное чтение сообщений
    void do_read() override;

    // Обработчик полученных сообщений
    void on_read(beast::error_code ec, std::size_t bytes_transferred) override;

    // Генерация подписи для аутентификации
    std::string generate_signature(long long expires, const std::string &api_secret);

    void checkStatus();
};
