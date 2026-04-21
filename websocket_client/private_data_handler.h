#pragma once

#include "private_connector.h"
#include "json_parser/position_json_parser.h"
#include "json_parser/order_json_parser.h"
#include "json_parser/wallet_json_parser.h"
#include "json_parser/execution_fast_json_parser.h"

// Класс WebSocket клиента для Bybit
class PrivateDataHandler : public PrivateConnector {
public:
    struct Messages {
        PositionHFT *const positionHFT;
        ExecutionFast *const executionFast;
        OrderHFT *const orderHFT;
        WalletHFT *const walletHFT;

        Messages(PositionHFT *const positionHFT_, ExecutionFast *const executionFast_,
                OrderHFT *const orderHFT_, WalletHFT *const walletHFT_);
    };

    // Конструктор: инициализируем все необходимые компоненты
    PrivateDataHandler(net::io_context &ioc, ssl::context &sslCtx, const std::string &api_key,
            const std::string &api_secret, const Messages &messages,
            const std::string_view userAgent);

private:
    PositionJsonParser positionJsonParser_;
    OrderJsonParser orderJsonParser_;
    WalletJsonParser walletJsonParser_;
    ExecutionFastJsonParser executionFastJsonParser_;

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
};
