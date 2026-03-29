#include <iostream>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>

#include "data_loader/data_loader.h"
#include "data_loader/private_data_loader.h"
#include "json_parser/orderbook_json_parser.h"
#include "json_parser/public_trade_json_parser.h"
#include "p999_latency/check_latency.h"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

int main() {
    std::cout << "hello" << std::endl;

    try {
        OrderBook orderBook;
        StatusMessage statusMessage;
        PublicTrade publicTrade;
        // Создаем I/O контекст (нужен для всех асинхронных операций)
        net::io_context ioc;

        // Создаем SSL контекст и загружаем сертификаты
        ssl::context ssl_ctx {ssl::context::tlsv12_client};
        ssl_ctx.set_default_verify_paths(); // загружаем системные CA сертификаты

        // Верифицируем сертификат сервера (обязательно для продакшена)
        ssl_ctx.set_verify_mode(ssl::verify_peer);

        std::cout << "Запуск Bybit WebSocket клиента..." << std::endl;

        // // Создаем экземпляр клиента
        // auto client = std::make_shared<BybitWebSocketClient>(ioc, ssl_ctx, &orderBook,
        //         &statusMessage, &publicTrade);

        // // Подключаемся к Bybit
        // // Для спота используйте: stream.bybit.com/v5/public/spot
        // // Для линейных контрактов: stream.bybit.com/v5/public/linear
        // // Для инверсных: stream.bybit.com/v5/public/inverse
        // client->connect("stream.bybit.com", "443", "/v5/public/linear");
        auto client = std::make_shared<PrivateWebSocketClient>(ioc, ssl_ctx, "1SlZRsoY5x2JPBWkDa",
                "qjJBC4TwWffJQ9tz12bNSRb3yGrnf3hhf87K");
        client->connect("stream.bybit.com", "443", "/v5/private");

        std::cout << "Запускаем I/O контекст. Нажмите Ctrl+C для выхода." << std::endl;

        // Запускаем обработку асинхронных операций
        ioc.run();

        std::cout << "I/O контекст остановлен." << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Фатальная ошибка: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // OrderBook orderBook;
    // OrderBookJsonParser parser(&orderBook);
    // parser.setString(snapshot);
    // parser.parse();
    // parser.printData();
    // parser.setString(delta);
    // parser.parse();
    // parser.printData();

    // auto start = std::chrono::high_resolution_clock::now();
    // parser.setString(snapshot);
    // parser.parse(); // 180 mks или 28 мкс с флагом компилляции -02.
    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    // std::cout << "Время выполнения: " << duration.count() << " mcs" << std::endl;
    // parser.printData();

    // checkLatency();

    // auto start = std::chrono::high_resolution_clock::now();
    // parser.setString(publicTrade);
    // parser.parse();
    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    // std::cout << "Время выполнения: " << duration.count() << " mcs" << std::endl;
    // parser.printData();

    return EXIT_SUCCESS;
}
