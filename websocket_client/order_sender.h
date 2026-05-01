#pragma once

#include "private_connector.h"
#include "data_structures/order_request.h"
#include "data_structures/order_operation.h"
#include "json_parser/order_operation_json_parser.h"

#include <boost/lockfree/queue.hpp>

// Класс WebSocket клиента для Bybit
class OrderSender : public PrivateConnector {
public:
    // Конструктор: инициализируем все необходимые компоненты
    OrderSender(net::io_context &ioc, ssl::context &sslCtx, const std::string &api_key,
            const std::string &api_secret, const std::string_view userAgent);

    bool placeOrder(const OrderRequest &orderRequest);

private:
    static const size_t maxQueueSize = 1000;

    boost::lockfree::queue<OrderRequest, boost::lockfree::fixed_sized<true> > order_queue_cancel_;
    boost::lockfree::queue<OrderRequest, boost::lockfree::fixed_sized<true> > order_queue_replace_;
    boost::lockfree::queue<OrderRequest, boost::lockfree::fixed_sized<true> > order_queue_new_;

    std::string write_buffer_;
    std::atomic<bool> startSending_ {false};

    OrderOperation orderOperation_;
    OrderOperationParser orderOperationParser_;

    // Обработчик завершения WebSocket handshake
    void onHandshake(beast::error_code ec) override;

    // Отправка подписки на потоки данных
    void subscribeToStreams() override;

    // Обработчик отправки подписки
    void onSubscribeSent(beast::error_code ec, std::size_t bytesTransferred) override;

    // Асинхронное чтение сообщений
    void doRead() override;

    // Обработчик полученных сообщений
    void onRead(beast::error_code ec, std::size_t bytesTransferred) override;

    void checkStatus();

    void sendNext();

    void sendOrder(const OrderRequest &order);

    void on_write(beast::error_code ec);

    void serialize_order(std::string &buffer, const OrderRequest &order);

    void serialize_replace_order(std::string &buffer, const OrderRequest &order);

    void serialize_cancel_order(std::string &buffer, const OrderRequest &order);
};
