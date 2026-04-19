#pragma once

#include "private_connector.h"
#include "data_structures/order_request.h"

#include <boost/lockfree/queue.hpp>

// Класс WebSocket клиента для Bybit
class OrderSender : public PrivateConnector {
public:
    // Конструктор: инициализируем все необходимые компоненты
    OrderSender(net::io_context &ioc, ssl::context &ssl_ctx, const std::string &api_key,
            const std::string &api_secret, const std::string_view user_agent);

    bool placeOrder(const OrderRequest &orderRequest);

private:
    static const size_t maxQueueSize = 1000;

    boost::lockfree::queue<OrderRequest, boost::lockfree::fixed_sized<true> > order_queue_cancel_;
    boost::lockfree::queue<OrderRequest, boost::lockfree::fixed_sized<true> > order_queue_replace_;
    boost::lockfree::queue<OrderRequest, boost::lockfree::fixed_sized<true> > order_queue_new_;

    std::string write_buffer_;
    std::atomic<bool> startSending_{false};

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

    void sendNext();

    void sendOrder(const OrderRequest &order);

    void on_write(beast::error_code ec);

    void serialize_order(std::string& buffer, const OrderRequest& order);

    void serialize_replace_order(std::string& buffer, const OrderRequest& order);

    void serialize_cancel_order(std::string& buffer, const OrderRequest& order);
};
