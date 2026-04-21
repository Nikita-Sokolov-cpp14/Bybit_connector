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
    PublicDataHandler(net::io_context &ioc, ssl::context &sslCtx, OrderBook *const orderBook,
            StatusMessage *const statusMessage, PublicTrade *const publicTrade,
            const std::string_view userAgent);

private:
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

private:
    OrderBookJsonParser orderbookParser_;
    StatusJsonParser statusParser_;
    PublicTradeJsonParser publicTradeJsonParser_;
};
