#include "connection_manager.h"

ConnectionManager::ConnectionManager() :
authConfig_(readAuthConfig()),
connectionConfig_(readConnectionConfig()),
sslCtx_(createSSLContext()),
messages_(&positionHFT_, &executionFast_, &orderHFT_, &walletHFT_),
publicDataHandler_(std::make_shared<PublicDataHandler>(publicIoc_, sslCtx_, &orderBook_,
        &statusMessage_, &publicTrade_, "bybit-HFT-client")),
privateDataHandler_(std::make_shared<PrivateDataHandler>(privateIoc_, sslCtx_, authConfig_.apiKey,
        authConfig_.apiSecret, messages_, "Bybit-PrivateData/1.0")),
orderSender_(std::make_shared<OrderSender>(orderSenderIoc_, sslCtx_, authConfig_.apiKey,
        authConfig_.apiSecret, "Bybit-HFT-OrderSender/1.0")) {
}

void ConnectionManager::connect() {
    net::post(publicIoc_, [this]() {
        publicDataHandler_->connect(connectionConfig_.host, connectionConfig_.port,
                connectionConfig_.targetPublic);
    });
    setPublicReconnectCallback();

    net::post(privateIoc_, [this]() {
        privateDataHandler_->connect(connectionConfig_.host, connectionConfig_.port,
                connectionConfig_.targetPrivate);
    });

    net::post(orderSenderIoc_, [this]() {
        orderSender_->connect(connectionConfig_.host, connectionConfig_.port,
                connectionConfig_.targetTrade);
    });

    publicThread_ = std::make_unique<std::jthread>([this]() { publicIoc_.run(); });
    privateThread_ = std::make_unique<std::jthread>([this]() { privateIoc_.run(); });
    orderSenderThread_ = std::make_unique<std::jthread>([this]() { orderSenderIoc_.run(); });
}

void ConnectionManager::reconnectOrderSender() {
    std::shared_ptr<OrderSender> newOrderSender = std::make_shared<OrderSender>(orderSenderIoc_,
            sslCtx_, authConfig_.apiKey, authConfig_.apiSecret, "Bybit-HFT-OrderSender/1.0");
}

void ConnectionManager::reconnectPublicHandler() {
    std::cout << "1 " << publicDataHandler_.use_count() << std::endl;

    std::shared_ptr<PublicDataHandler> newPublicHandler = std::make_shared<PublicDataHandler>(
            publicIoc_, sslCtx_, &orderBook_, &statusMessage_, &publicTrade_, "bybit-HFT-client");

    publicDataHandler_ = std::move(newPublicHandler);
    setPublicReconnectCallback();

    std::cout << "2 " << publicDataHandler_.use_count() << std::endl;
}

void ConnectionManager::reconnectPrivateHandler() {
    std::shared_ptr<PrivateDataHandler> newPrivateDataHandler =
            std::make_shared<PrivateDataHandler>(privateIoc_, sslCtx_, authConfig_.apiKey,
                    authConfig_.apiSecret, messages_, "Bybit-PrivateData/1.0");

    privateDataHandler_ = std::move(newPrivateDataHandler);
}

ssl::context ConnectionManager::createSSLContext() {
    ssl::context ctx(ssl::context::tlsv12_client);
    ctx.set_default_verify_paths(); // загружаем системные CA сертификаты
    // Верифицируем сертификат сервера (обязательно для продакшена)
    ctx.set_verify_mode(ssl::verify_peer);
    // ctx.set_options(ssl::context::no_compression); // Для HFT
    return ctx;
}

void ConnectionManager::setPublicReconnectCallback() {
    publicDataHandler_->setReconnectCallback([this]() {
        std::cout << "in callback publicDataHandler_ " << std::endl;
        net::post(publicIoc_, [this]() {
            reconnectPublicHandler();
            // После создания нового хендлера - подключаем его
            publicDataHandler_->connect(connectionConfig_.host, connectionConfig_.port,
                    connectionConfig_.targetPublic);
        });
    });
}
