#include "check_latency.h"

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

void checkLatency() {
    OrderBook orderBook;
    JsonParser parser(&orderBook);

    constexpr size_t WARMUP = 10000; // прогрев
    constexpr size_t SAMPLES = 1000000; // основное измерение

    std::cout << "Warming up...\n";
    for (int i = 0; i < WARMUP; ++i) {
        parser.setString(snapshotStr);
        parser.parse();
    }

    std::cout << "Measuring...\n";
    std::vector<double> latencies;
    latencies.reserve(SAMPLES);

    for (int i = 0; i < SAMPLES; ++i) {
        auto start = std::chrono::steady_clock::now();
        parser.setString(snapshotStr);
        parser.parse();
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
