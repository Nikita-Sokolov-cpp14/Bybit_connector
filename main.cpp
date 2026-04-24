#include <iostream>

#include "connection_manager/connection_manager.h"

int main() {
    std::cout << "hello" << std::endl;
    try {
        ConnectionManager connectionManager;
        connectionManager.connect();
    } catch (const std::exception &e) {
        std::cerr << "Фатальная ошибка: " << e.what() << std::endl;
        return 1;
    }
    // checkLatency();
    // checkParsing();

    return 0;
}
