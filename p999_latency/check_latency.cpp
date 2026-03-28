#include "check_latency.h"

namespace {

const std::string_view snapshotStr = R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","ts":1773386409389,"data":{"s":"BTCUSDT","b":[["71423.80","0.557"],["71423.10","0.102"],["71423.00","0.324"],["71422.80","0.003"],["71422.60","0.001"],["71422.50","0.001"],["71422.10","0.002"],["71422.00","0.006"],["71421.90","0.001"],["71421.50","0.001"],["71421.40","0.003"],["71421.30","0.001"],["71421.00","0.001"],["71420.30","0.001"],["71419.80","0.002"],["71419.60","0.001"],["71419.40","0.001"],["71419.30","0.001"],["71418.90","0.015"],["71418.80","0.003"],["71418.20","0.161"],["71418.10","0.013"],["71417.60","0.001"],["71417.50","0.001"],["71417.40","0.050"],["71417.30","0.342"],["71417.00","0.002"],["71416.80","0.008"],["71416.70","0.884"],["71416.60","1.235"],["71416.50","0.002"],["71416.40","0.002"],["71416.10","0.234"],["71415.80","0.004"],["71415.50","0.007"],["71415.40","0.001"],["71415.20","0.204"],["71415.10","0.001"],["71414.90","0.002"],["71414.70","0.141"],["71414.60","0.004"],["71414.50","0.840"],["71414.40","0.126"],["71414.20","0.982"],["71414.10","0.099"],["71414.00","0.002"],["71413.90","0.049"],["71413.80","0.073"],["71413.60","0.001"],["71413.50","0.070"]],"a":[["71423.90","4.669"],["71424.00","0.001"],["71424.70","1.002"],["71424.80","0.218"],["71425.20","0.001"],["71425.60","0.001"],["71426.00","0.017"],["71426.40","0.002"],["71426.70","0.001"],["71427.10","0.001"],["71428.00","0.002"],["71428.10","0.001"],["71428.20","0.001"],["71428.30","0.006"],["71428.40","0.001"],["71428.80","0.098"],["71428.90","0.004"],["71429.30","0.001"],["71429.50","0.129"],["71429.80","0.001"],["71429.90","0.003"],["71430.00","0.020"],["71430.70","0.002"],["71430.80","0.001"],["71430.90","0.347"],["71431.00","0.061"],["71431.20","0.002"],["71431.30","1.417"],["71431.40","0.982"],["71431.50","0.003"],["71431.60","0.048"],["71431.70","0.002"],["71431.80","0.602"],["71431.90","0.001"],["71432.00","0.017"],["71432.40","0.001"],["71432.60","0.282"],["71432.80","0.003"],["71433.00","0.017"],["71433.20","0.072"],["71433.30","0.002"],["71433.40","0.003"],["71433.60","0.001"],["71433.80","0.840"],["71433.90","0.003"],["71434.00","0.020"],["71434.40","0.613"],["71434.60","0.081"],["71435.00","0.002"],["71435.30","0.060"]],"u":10132540,"seq":546137962055},"cts":1773386409385})";
const std::string_view deltaStr = R"({"topic":"orderbook.50.BTCUSDT","type":"delta","ts":1773386409407,"data":{"s":"BTCUSDT","b":[["71419.80","0"],["71418.20","0.159"],["71417.00","0"],["71416.60","1.163"],["71416.50","0"],["71415.50","0"],["71414.80","0.007"],["71413.80","0.075"],["71413.40","0.013"],["71413.30","0.270"],["71413.20","0.603"]],"a":[["71427.90","0.006"],["71428.30","0"],["71433.30","0"],["71435.40","0.001"]],"u":10132541,"seq":546137962186},"cts":1773386409404})";
const std::string_view statusStr = R"({"success":true,"ret_msg":"","conn_id":"d6ki1eae0cl5msgggj7g-1uoz2","req_id":"","op":"subscribe"})";
const std::string_view publicTradeStr = R"({"topic":"publicTrade.BTCUSDT","type":"snapshot","ts":1774021164273,"data":[{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.001","p":"69916.80","L":"MinusTick","i":"8c931d9a-b703-5c34-9aeb-860867cf1f66","BT":false,"RPI":false,"seq":549792210006},{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.001","p":"69916.80","L":"ZeroMinusTick","i":"307fb58f-09dc-5121-b24f-eb2f81873445","BT":false,"RPI":false,"seq":549792210006},{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.001","p":"69916.80","L":"ZeroMinusTick","i":"e4f62be6-ee74-5a67-a9da-b3829b911246","BT":false,"RPI":false,"seq":549792210006},{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.001","p":"69916.80","L":"ZeroMinusTick","i":"e638a9e6-ade1-541e-8e22-e6e4c04fbfc1","BT":false,"RPI":false,"seq":549792210006},{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.032","p":"69916.80","L":"ZeroMinusTick","i":"ddea3520-0444-515c-af09-07471b39e1e5","BT":false,"RPI":false,"seq":549792210006},{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.002","p":"69916.10","L":"MinusTick","i":"58766b19-9dba-5ad3-8931-f71eff90e8d8","BT":false,"RPI":false,"seq":549792210006},{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.001","p":"69915.40","L":"MinusTick","i":"f3a898c6-d91f-5860-9c77-dc31411e6d3a","BT":false,"RPI":false,"seq":549792210006},{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.002","p":"69915.40","L":"ZeroMinusTick","i":"597fb5be-4ef1-5cf7-846e-3f8bf88868e2","BT":false,"RPI":false,"seq":549792210006},{"T":1774021164272,"s":"BTCUSDT","S":"Sell","v":"0.008","p":"69914.20","L":"MinusTick","i":"86245400-ea12-510f-a809-78e07e373378","BT":false,"RPI":false,"seq":549792210006}]})";

}

void calculate_percentiles(std::vector<double> &latencies) {
    // Сортируем для расчета перцентилей
    std::sort(latencies.begin(), latencies.end());

    size_t n = latencies.size();

    // Среднее
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / n;

    // Минимум и максимум
    double min = latencies.front();
    double max = latencies.back();

    // Перцентили
    double p50 = latencies[static_cast<size_t>(n * 0.50)];
    double p90 = latencies[static_cast<size_t>(n * 0.90)];
    double p95 = latencies[static_cast<size_t>(n * 0.95)];
    double p99 = latencies[static_cast<size_t>(n * 0.99)];
    double p999 = latencies[static_cast<size_t>(n * 0.999)];
    double p9999 = latencies[static_cast<size_t>(n * 0.9999)];

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "=== Latency Analysis (" << n << " samples) ===\n";
    std::cout << "Mean:   " << mean << " µs\n";
    std::cout << "Min:    " << min << " µs\n";
    std::cout << "Max:    " << max << " µs\n";
    std::cout << "P50:    " << p50 << " µs\n";
    std::cout << "P90:    " << p90 << " µs\n";
    std::cout << "P95:    " << p95 << " µs\n";
    std::cout << "P99:    " << p99 << " µs\n";
    std::cout << "P99.9:  " << p999 << " µs\n";
    std::cout << "P99.99: " << p9999 << " µs\n";
}

void checkLatency(BaseJsonParser &parser, std::string_view message) {
    constexpr size_t WARMUP = 10000; // прогрев
    constexpr size_t SAMPLES = 1000000; // основное измерение

    std::cout << "Warming up...\n";
    for (int i = 0; i < WARMUP; ++i) {
        parser.setString(message);
        parser.parse();
    }

    std::cout << "Measuring...\n";
    std::vector<double> latencies;
    latencies.reserve(SAMPLES);

    TypeMessage typeMessage_;
    for (int i = 0; i < SAMPLES; ++i) {
        auto start = std::chrono::steady_clock::now();
        typeMessage_ = parseTypeMessage(snapshotStr.substr(0, maxTypeStrLen));
        switch (typeMessage_) {
            case TypeMessage_Orderbook:
                parser.setString(message);
                parser.parse();
                break;
            case TypeMessage_PublicTrade:
                // publicTradeJsonParser_.setString(snapshotStr);
                // publicTradeJsonParser_.parse();
                break;
            case TypeMessage_Status:
                // statusParser_.setString(snapshotStr);
                // statusParser_.parse();
                break;
            default:
                std::cout << "BybitWebSocketClient::on_read: Unknown message type" << std::endl;
                std::cout << snapshotStr << std::endl;
                break;
        }
        auto end = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(us);
    }

    calculate_percentiles(latencies);

    // Дополнительно: анализ распределения
    std::cout << "\n=== Distribution ===\n";
    auto outliers =
            std::count_if(latencies.begin(), latencies.end(), [](double x) { return x > 20.0; });
    std::cout << "Outliers (>20 µs): " << outliers << " (" << (100.0 * outliers / SAMPLES)
              << "%)\n";
}

void checkLatency() {
    OrderBook orderBook;
    OrderBookJsonParser parserOrderbook(&orderBook);
    PublicTrade publicTradeStruct;
    PublicTradeJsonParser parserPublicTrade(&publicTradeStruct);
    std::cout << "snapshot" << std::endl;
    checkLatency(parserOrderbook, snapshotStr);
    std::cout << "public trade" << std::endl;
    checkLatency(parserPublicTrade, publicTradeStr);
}
