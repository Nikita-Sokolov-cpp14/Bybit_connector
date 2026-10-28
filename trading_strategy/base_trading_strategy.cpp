#include "base_trading_strategy.h"

BaseTradingStrategy::~BaseTradingStrategy() {
    stop();
}

void BaseTradingStrategy::start() {
    if (running_) {
        return;
    }
    running_ = true;
    strategyThread_ = std::make_unique<std::jthread>([this]() { work(); });
}

void BaseTradingStrategy::stop() {
    running_ = false;
    {
        std::lock_guard lock(dataMutex_);
    }
    dataCV_.notify_all();
    if (strategyThread_ && strategyThread_->joinable()) {
        strategyThread_->join();
    }
}

void BaseTradingStrategy::notifyNewData() {
    {
        std::lock_guard lock(dataMutex_);
        newData_.store(true);
    }
    dataCV_.notify_all();
}

void BaseTradingStrategy::setOrderSender(Trade::OrderSender orderSender) {
    tradeManager_.setOrderSender(orderSender);
}

void BaseTradingStrategy::work() {
    while (running_) {
        // Ждем новые данные
        std::unique_lock lock(dataMutex_);
        dataCV_.wait(lock, [this] { return newData_.load() || !running_.load(); });
        if (!running_) {
            break;
        }
        newData_.store(false);
        lock.unlock();

        // Вызываем торговую логику
        try {
            onMarketUpdate();
        } catch (const std::exception &e) {
            std::cerr << "Strategy error: " << e.what() << std::endl;
        }
    }
}
