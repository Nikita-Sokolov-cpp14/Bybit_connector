#include "check_place_orders.h"

#include "connection_manager/connection_manager.h"
#include "trading_strategy/base_trading_strategy.h"
#include "trading_strategy/settings.h"
#include "json_parser/base_json_parser.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace {

using namespace std::chrono_literals;

constexpr auto waitTime = 20s;
constexpr double safeBuyPrice = 60000.0;
constexpr double safeSellPrice = 150000.0;

const char *sideToString(Side side) {
    return side == Side_Buy ? "Buy" : (side == Side_Sell ? "Sell" : "Unknown");
}

const char *statusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus_Created:
            return "Created";
        case OrderStatus_New:
            return "New";
        case OrderStatus_PartiallyFilled:
            return "PartiallyFilled";
        case OrderStatus_Filled:
            return "Filled";
        case OrderStatus_Cancelled:
            return "Cancelled";
        case OrderStatus_Rejected:
            return "Rejected";
        case OrderStatus_Expired:
            return "Expired";
        case OrderStatus_Deactivated:
            return "Deactivated";
        case OrderStatus_Triggered:
            return "Triggered";
        default:
            return "Unknown";
    }
}

bool isTerminal(OrderStatus status) {
    return status == OrderStatus_Filled || status == OrderStatus_Cancelled ||
            status == OrderStatus_Rejected || status == OrderStatus_Expired ||
            status == OrderStatus_Deactivated;
}

struct Observation {
    std::mutex mutex;
    std::condition_variable cv;

    std::unordered_set<uint64_t> requestOk;
    std::unordered_set<uint64_t> requestFailed;
    std::unordered_map<uint64_t, std::string> requestError;
    std::unordered_map<uint64_t, OrderStatus> orderStatus;

    void onRequestResult(const OrderRequestResult &result) {
        {
            std::lock_guard lock(mutex);
            if (result.success) {
                requestOk.insert(result.request.req_id);
            } else {
                requestFailed.insert(result.request.req_id);
                requestError[result.request.req_id] = result.retMsg;
            }
        }
        cv.notify_all();
    }

    void onOrder(const OrderHFT &order) {
        const uint64_t orderLinkId = convertTo<uint64_t>(order.orderLinkId);
        {
            std::lock_guard lock(mutex);
            orderStatus[orderLinkId] = order.orderStatus;
        }
        cv.notify_all();
    }

    bool waitRequestResult(uint64_t reqId, bool &success, std::string &error) {
        std::unique_lock lock(mutex);
        const bool done = cv.wait_for(lock, waitTime, [&]() {
            return requestOk.contains(reqId) || requestFailed.contains(reqId);
        });

        if (!done) {
            return false;
        }

        success = requestOk.contains(reqId);
        if (!success) {
            error = requestError[reqId];
        }
        return true;
    }

    bool waitOrderStatus(uint64_t orderLinkId, OrderStatus expected, OrderStatus &actual) {
        std::unique_lock lock(mutex);
        const bool done = cv.wait_for(lock, waitTime, [&]() {
            auto it = orderStatus.find(orderLinkId);
            if (it == orderStatus.end()) {
                return false;
            }
            return it->second == expected || isTerminal(it->second);
        });

        if (!done) {
            return false;
        }

        actual = orderStatus[orderLinkId];
        return actual == expected;
    }
};

class CheckStrategy : public BaseTradingStrategy {
public:
    explicit CheckStrategy(Observation &observation) :
    BaseTradingStrategy(false),
    observation_(observation) {
    }

    void setOrderbook(const OrderBook &orderbook) override {
        if (orderbook.asks.empty() || orderbook.bids.empty()) {
            return;
        }
        bestBid_.store(orderbook.bids.begin()->first);
        bestAsk_.store(orderbook.asks.begin()->first);
    }

    void setPublicTradeData(const PublicTrade &) override {
    }

    void setPosition(const PositionHFT &) override {
    }

    void setOrder(const OrderHFT &order) override {
        observation_.onOrder(order);
    }

    void setOrderRequestResult(const OrderRequestResult &result) override {
        observation_.onRequestResult(result);
    }

    double bestBid() const {
        return bestBid_.load();
    }

    double bestAsk() const {
        return bestAsk_.load();
    }

private:
    Observation &observation_;
    std::atomic<double> bestBid_ {0.0};
    std::atomic<double> bestAsk_ {0.0};
};

struct TestOrder {
    const char *name;
    Side side;
    double price;
    uint64_t orderLinkId;
    uint64_t cancelRequestId;
    bool requestAccepted = false;
    bool placed = false;
    bool cancelled = false;
};

bool waitForOrderbook(const CheckStrategy &strategy) {
    const auto deadline = std::chrono::steady_clock::now() + waitTime;
    while (std::chrono::steady_clock::now() < deadline) {
        if (strategy.bestBid() > 0.0 && strategy.bestAsk() > 0.0) {
            return true;
        }
        std::this_thread::sleep_for(100ms);
    }
    return false;
}

bool placeLimitOrder(ConnectionManager &connectionManager, Observation &observation,
        TestOrder &testOrder) {
    OrderRequest order;
    order.req_id = testOrder.orderLinkId;
    order.typeOrderRequest = TypeOrderRequest_New;
    order.typeOrderId_ = TypeOrderId_OrderLinkId;
    order.side = testOrder.side;
    order.order_type = OrderType_Limit;
    order.qty = settings::defaultQty;
    order.price = testOrder.price;
    order.leverage = settings::leverage;
    order.reduceOnly = false;
    std::strcpy(order.symbol, "BTCUSDT");
    order.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                 .count();

    std::cout << "[PLACE] " << testOrder.name << " " << sideToString(order.side)
              << " limit price=" << order.price << " qty=" << order.qty
              << " orderLinkId=" << order.req_id << std::endl;

    if (!connectionManager.placeOrder(order)) {
        std::cout << "[FAIL] order was not enqueued (sender not ready?)" << std::endl;
        return false;
    }

    bool success = false;
    std::string error;
    if (!observation.waitRequestResult(order.req_id, success, error)) {
        std::cout << "[FAIL] no order.create response" << std::endl;
        return false;
    }
    if (!success) {
        std::cout << "[FAIL] order.create rejected: " << error << std::endl;
        return false;
    }

    testOrder.requestAccepted = true;
    std::cout << "[OK] order.create accepted" << std::endl;

    OrderStatus status = OrderStatus_Unknown;
    if (!observation.waitOrderStatus(order.req_id, OrderStatus_New, status)) {
        std::cout << "[FAIL] expected New, got " << statusToString(status) << std::endl;
        return false;
    }

    testOrder.placed = true;
    std::cout << "[OK] order status New" << std::endl;
    return true;
}

bool cancelLimitOrder(ConnectionManager &connectionManager, Observation &observation,
        TestOrder &testOrder) {
    OrderRequest cancel;
    cancel.req_id = testOrder.cancelRequestId;
    cancel.typeOrderRequest = TypeOrderRequest_Cancel;
    cancel.typeOrderId_ = TypeOrderId_OrderLinkId;
    std::snprintf(cancel.order_link_id, sizeof(cancel.order_link_id), "%llu",
            static_cast<unsigned long long>(testOrder.orderLinkId));
    cancel.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                  .count();
    std::strcpy(cancel.symbol, "BTCUSDT");

    std::cout << "[CANCEL] " << testOrder.name << " orderLinkId=" << testOrder.orderLinkId
              << " cancelReqId=" << cancel.req_id << std::endl;

    if (!connectionManager.placeOrder(cancel)) {
        std::cout << "[FAIL] cancel was not enqueued" << std::endl;
        return false;
    }

    bool success = false;
    std::string error;
    if (!observation.waitRequestResult(cancel.req_id, success, error)) {
        std::cout << "[FAIL] no order.cancel response" << std::endl;
    } else if (!success) {
        std::cout << "[FAIL] order.cancel rejected: " << error << std::endl;
    } else {
        std::cout << "[OK] order.cancel accepted" << std::endl;
    }

    OrderStatus status = OrderStatus_Unknown;
    if (!observation.waitOrderStatus(testOrder.orderLinkId, OrderStatus_Cancelled, status)) {
        std::cout << "[FAIL] expected Cancelled, got " << statusToString(status) << std::endl;
        return false;
    }

    testOrder.cancelled = true;
    std::cout << "[OK] order status Cancelled" << std::endl;
    return true;
}

} // namespace

int runCheckPlaceOrders() {
    std::cout << "=== check_place_orders: limit order lifecycle ===" << std::endl;

    Observation observation;
    CheckStrategy strategy(observation);

    ConnectionManager connectionManager;
    connectionManager.subscribeStrategy(&strategy);
    connectionManager.connect();

    std::cout << "Waiting for order sender authentication..." << std::endl;
    const auto authDeadline = std::chrono::steady_clock::now() + waitTime;
    while (!connectionManager.isOrderSenderReady() &&
            std::chrono::steady_clock::now() < authDeadline) {
        std::this_thread::sleep_for(100ms);
    }
    if (!connectionManager.isOrderSenderReady()) {
        std::cout << "[FAIL] order sender is not authenticated" << std::endl;
        return 1;
    }
    std::cout << "[OK] order sender authenticated" << std::endl;

    if (!waitForOrderbook(strategy)) {
        std::cout << "[FAIL] no orderbook data" << std::endl;
        return 1;
    }

    const double bestBid = strategy.bestBid();
    const double bestAsk = strategy.bestAsk();
    std::cout << "[INFO] BTCUSDT bid=" << bestBid << " ask=" << bestAsk << std::endl;

    if (bestBid <= safeBuyPrice * 1.02) {
        std::cout << "[FAIL] buy price is too close to market or market is below expected"
                  << std::endl;
        return 1;
    }
    if (bestAsk >= safeSellPrice * 0.98) {
        std::cout << "[FAIL] sell price is too close to market" << std::endl;
        return 1;
    }

    const uint64_t baseId = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                                    .count() *
            10;
    std::array<TestOrder, 2> orders {{
            {"buy", Side_Buy, safeBuyPrice, baseId + 1, baseId + 101},
            {"sell", Side_Sell, safeSellPrice, baseId + 2, baseId + 102},
    }};

    bool allOk = true;

    for (TestOrder &order : orders) {
        if (!placeLimitOrder(connectionManager, observation, order)) {
            allOk = false;
        }
    }

    for (TestOrder &order : orders) {
        //! Если order.create отвергнут, отменять нечего. Если ack получен,
        //! но статус New не дождались, всё равно пробуем отменить.
        if (!order.requestAccepted) {
            continue;
        }
        if (!cancelLimitOrder(connectionManager, observation, order)) {
            allOk = false;
        }
    }

    std::cout << "=== RESULT ===" << std::endl;
    for (const TestOrder &order : orders) {
        std::cout << order.name << ": place=" << (order.placed ? "OK" : "FAIL")
                  << " cancel=" << (order.cancelled ? "OK" : "FAIL")
                  << " orderLinkId=" << order.orderLinkId << std::endl;
    }

    if (!allOk) {
        std::cout << "[WARN] Some checks failed. If an order remains open, cancel it "
                     "manually by orderLinkId above."
                  << std::endl;
        return 1;
    }

    std::cout << "[PASS] all limit order checks passed" << std::endl;
    return 0;
}
