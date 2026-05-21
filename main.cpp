#include <iostream>

#include "connection_manager/connection_manager.h"
#include "check_place_orders/check_place_orders.h"
#include "p999_latency/check_latency.h"
#include "check_parsing/check_parsing.h"
#include "check_place_orders/check_place_orders.h"
#include "trading_strategy/imbalance_and_large/imbalance_and_large.h"

int main() {
    std::cout << "hello" << std::endl;
    try {
        std::cout << "start" << std::endl;

        ImbalanceAndLarge strategy;
        ConnectionManager connectionManager;
        connectionManager.subscribeStrategy(&strategy);
        strategy.start(); // Запускает торговый поток

        connectionManager.connect();

        // std::jthread tr(addNewOrders, std::ref(connectionManager));
        // std::jthread tr(cancelOrders, std::ref(connectionManager));
        // std::jthread tr(replaceOrders, std::ref(connectionManager));

        std::cout << "Press Enter to stop..." << std::endl;
        std::cin.get();

        strategy.stop();
        std::cout << "end section" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Фатальная ошибка: " << e.what() << std::endl;
        return 1;
    }
    // checkLatency();
    // checkParsing();
    return 0;
}
