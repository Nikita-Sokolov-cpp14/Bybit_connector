#pragma once

#include "base_websocket_client.h"
#include "json_parser/orderbook_json_parser.h"
#include "json_parser/status_json_parser.h"
#include "json_parser/public_trade_json_parser.h"
#include "data_structures/orderbook.h"
#include "data_structures/status_message.h"
#include "data_structures/public_trade.h"

// Класс WebSocket клиента для Bybit
class PublicDataHandler : public BaseWebSocketClient {
public:
    // Конструктор: инициализируем все необходимые компоненты
    PublicDataHandler(net::io_context &ioc, ssl::context &ssl_ctx, OrderBook *const orderBook,
            StatusMessage *const statusMessage, PublicTrade *const publicTrade,
            const std::string_view user_agent);

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

private:
    OrderBookJsonParser orderbookParser_;
    StatusJsonParser statusParser_;
    PublicTradeJsonParser publicTradeJsonParser_;
};
