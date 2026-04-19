#pragma once

#include "base_websocket_client.h"

// Класс WebSocket клиента для Bybit
class PrivateConnector : public BaseWebSocketClient {
public:
    // Конструктор: инициализируем все необходимые компоненты
    PrivateConnector(net::io_context &ioc, ssl::context &ssl_ctx, const std::string &api_key,
            const std::string &api_secret, const std::string_view user_agent);

protected:
    StatusMessage statusMessage_;
    StatusJsonParser statusParser_;
    bool authenticated_;

    // Таймер для ожидания аутентификации
    net::steady_timer auth_timer_;

    virtual void checkStatus() = 0;

    // Аутентификация
    void authenticate();

private:
    std::string api_key_;
    std::string api_secret_;

    // Обработчик ответа аутентификации
    void on_auth_response(beast::error_code ec, std::size_t bytes_transferred);

    // Генерация подписи для аутентификации
    std::string generate_signature(long long expires, const std::string &api_secret);
};
