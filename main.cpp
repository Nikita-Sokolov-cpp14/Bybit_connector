#include <iostream>

#include "connection_manager/connection_manager.h"
#include "check_place_orders/check_place_orders.h"
#include "p999_latency/check_latency.h"
#include "check_parsing/check_parsing.h"
#include "check_place_orders/check_place_orders.h"

int main() {
    std::cout << "hello" << std::endl;
    try {
        ConnectionManager connectionManager;
        connectionManager.connect();
        // std::jthread tr(addNewOrders, std::ref(connectionManager));
        // std::jthread tr(cancelOrders, std::ref(connectionManager));
        // std::jthread tr(replaceOrders, std::ref(connectionManager));
    } catch (const std::exception &e) {
        std::cerr << "Фатальная ошибка: " << e.what() << std::endl;
        return 1;
    }
    // checkLatency();
    // checkParsing();
    return 0;
}
