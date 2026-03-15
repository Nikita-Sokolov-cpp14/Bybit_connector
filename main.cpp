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
#include "json_parser/json_parser.h"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

const std::string_view snapshot = R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","ts":1773386409389,"data":{"s":"BTCUSDT","b":[["71423.80","0.557"],["71423.10","0.102"],["71423.00","0.324"],["71422.80","0.003"],["71422.60","0.001"],["71422.50","0.001"],["71422.10","0.002"],["71422.00","0.006"],["71421.90","0.001"],["71421.50","0.001"],["71421.40","0.003"],["71421.30","0.001"],["71421.00","0.001"],["71420.30","0.001"],["71419.80","0.002"],["71419.60","0.001"],["71419.40","0.001"],["71419.30","0.001"],["71418.90","0.015"],["71418.80","0.003"],["71418.20","0.161"],["71418.10","0.013"],["71417.60","0.001"],["71417.50","0.001"],["71417.40","0.050"],["71417.30","0.342"],["71417.00","0.002"],["71416.80","0.008"],["71416.70","0.884"],["71416.60","1.235"],["71416.50","0.002"],["71416.40","0.002"],["71416.10","0.234"],["71415.80","0.004"],["71415.50","0.007"],["71415.40","0.001"],["71415.20","0.204"],["71415.10","0.001"],["71414.90","0.002"],["71414.70","0.141"],["71414.60","0.004"],["71414.50","0.840"],["71414.40","0.126"],["71414.20","0.982"],["71414.10","0.099"],["71414.00","0.002"],["71413.90","0.049"],["71413.80","0.073"],["71413.60","0.001"],["71413.50","0.070"]],"a":[["71423.90","4.669"],["71424.00","0.001"],["71424.70","1.002"],["71424.80","0.218"],["71425.20","0.001"],["71425.60","0.001"],["71426.00","0.017"],["71426.40","0.002"],["71426.70","0.001"],["71427.10","0.001"],["71428.00","0.002"],["71428.10","0.001"],["71428.20","0.001"],["71428.30","0.006"],["71428.40","0.001"],["71428.80","0.098"],["71428.90","0.004"],["71429.30","0.001"],["71429.50","0.129"],["71429.80","0.001"],["71429.90","0.003"],["71430.00","0.020"],["71430.70","0.002"],["71430.80","0.001"],["71430.90","0.347"],["71431.00","0.061"],["71431.20","0.002"],["71431.30","1.417"],["71431.40","0.982"],["71431.50","0.003"],["71431.60","0.048"],["71431.70","0.002"],["71431.80","0.602"],["71431.90","0.001"],["71432.00","0.017"],["71432.40","0.001"],["71432.60","0.282"],["71432.80","0.003"],["71433.00","0.017"],["71433.20","0.072"],["71433.30","0.002"],["71433.40","0.003"],["71433.60","0.001"],["71433.80","0.840"],["71433.90","0.003"],["71434.00","0.020"],["71434.40","0.613"],["71434.60","0.081"],["71435.00","0.002"],["71435.30","0.060"]],"u":10132540,"seq":546137962055},"cts":1773386409385})";
const std::string_view delta = R"({"topic":"orderbook.50.BTCUSDT","type":"delta","ts":1773386409407,"data":{"s":"BTCUSDT","b":[["71419.80","0"],["71418.20","0.159"],["71417.00","0"],["71416.60","1.163"],["71416.50","0"],["71415.50","0"],["71414.80","0.007"],["71413.80","0.075"],["71413.40","0.013"],["71413.30","0.270"],["71413.20","0.603"]],"a":[["71427.90","0.006"],["71428.30","0"],["71433.30","0"],["71435.40","0.001"]],"u":10132541,"seq":546137962186},"cts":1773386409404})";
const std::string_view status = R"({"success":true,"ret_msg":"","conn_id":"d6ki1eae0cl5msgggj7g-1uoz2","req_id":"","op":"subscribe"})";

int main() {
    std::cout << "hello" << std::endl;

    // try {
    //     // Создаем I/O контекст (нужен для всех асинхронных операций)
    //     net::io_context ioc;

    //     // Создаем SSL контекст и загружаем сертификаты
    //     ssl::context ssl_ctx{ssl::context::tlsv12_client};
    //     ssl_ctx.set_default_verify_paths();  // загружаем системные CA сертификаты

    //     // Верифицируем сертификат сервера (обязательно для продакшена)
    //     ssl_ctx.set_verify_mode(ssl::verify_peer);

    //     std::cout << "Запуск Bybit WebSocket клиента..." << std::endl;

    //     // Создаем экземпляр клиента
    //     auto client = std::make_shared<BybitWebSocketClient>(ioc, ssl_ctx);

    //     // Подключаемся к Bybit
    //     // Для спота используйте: stream.bybit.com/v5/public/spot
    //     // Для линейных контрактов: stream.bybit.com/v5/public/linear
    //     // Для инверсных: stream.bybit.com/v5/public/inverse
    //     client->connect("stream.bybit.com", "443", "/v5/public/linear");

    //     std::cout << "Запускаем I/O контекст. Нажмите Ctrl+C для выхода." << std::endl;

    //     // Запускаем обработку асинхронных операций
    //     ioc.run();

    //     std::cout << "I/O контекст остановлен." << std::endl;
    // } catch (const std::exception &e) {
    //     std::cerr << "Фатальная ошибка: " << e.what() << std::endl;
    //     return EXIT_FAILURE;
    // }

    JsonParser parser;
    OrderBook orderBook;
    parser.setOrderBook(&orderBook);
    // parser.setString(status);
    // std::cout << parser.getTypeMessage() << std::endl;
    // parser.setString(snapshot);
    // std::cout << parser.getTypeMessage() << std::endl;
    // parser.parse();

    // auto start = std::chrono::high_resolution_clock::now();
    // parser.setString(snapshot);
    // parser.parse(); // 180 mks или 28 мкс с флагом компилляции -02.
    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    // std::cout << "Время выполнения: " << duration.count() << " mcs" << std::endl;
    // parser.printData();
    for (size_t i = 0; i < 10000; ++i) {
        parser.setString(snapshot);
        parser.parse();
    }

    // parser.setString(delta);
    // parser.parse();
    // parser.printData();

    // parser.setString("unknown");
    // parser.parse();
    // parser.printStatus();

    // parser.setString(status);
    // parser.parse();
    // parser.printStatus();

    return EXIT_SUCCESS;
}
