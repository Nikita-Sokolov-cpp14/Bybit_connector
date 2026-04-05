#pragma once

#include "private_connector.h"

// Класс WebSocket клиента для Bybit
class OrderSender : public PrivateConnector {
public:
    // Конструктор: инициализируем все необходимые компоненты
    OrderSender(net::io_context &ioc, ssl::context &ssl_ctx, const std::string &api_key,
            const std::string &api_secret);

private:
    // Обработчик завершения WebSocket handshake
    void on_handshake(beast::error_code ec) override;

    // Отправка подписки на потоки данных
    void subscribe_to_streams() override;

    // Обработчик отправки подписки
    void on_subscribe_sent(beast::error_code ec, std::size_t bytes_transferred) override;

    // Асинхронное чтение сообщений
    void do_read() override;

    // Обработчик полученных сообщений
    void on_read(beast::error_code ec, std::size_t bytes_transferred) override;

    void checkStatus();
};
