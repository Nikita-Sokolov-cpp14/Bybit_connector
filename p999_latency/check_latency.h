#include <iostream>

#include <vector>
#include <algorithm>
#include <numeric>
#include <iomanip>

#include "websocket_client/public_data_handler.h"
#include "json_parser/orderbook_json_parser.h"
#include "json_parser/base_json_parser.h"

// #define MEASURE_LATENCY

#ifdef MEASURE_LATENCY
// Для использования в классе BybitWebSocketClient
#define DECLARE_LATENCY_MEMBERS(COUNT_LATENCY_MEASUREMENTS) \
    namespace { \
    std::vector<double> latencies; \
    std::chrono::_V2::steady_clock::time_point latencyStart; \
    const size_t maxSamples = COUNT_LATENCY_MEASUREMENTS; \
    const size_t warmup = 100; \
    size_t count = 0; \
    bool isRunning = true; \
    bool isFirstCall = true; \
    double prevTime; \
    void record_latency() { \
        auto end = std::chrono::steady_clock::now(); \
        if (isFirstCall) { \
            isFirstCall = false; \
            return; \
        } \
        double us = std::chrono::duration<double, std::micro>(end - latencyStart).count(); \
        count++; \
        if (count < warmup) { \
            return; \
        } \
        if (!isRunning) { \
            return; \
        } else if (count < maxSamples) { \
            latencies.push_back(us); \
        } else { \
            calculate_percentiles(latencies); \
            isRunning = false; \
        } \
    } \
    void readTimer() { \
        auto timeer = std::chrono::steady_clock::now(); \
        double us = std::chrono::duration<double, std::micro>(timeer - latencyStart).count(); \
        std::cout << us << std::endl; \
        latencyStart = timeer; \
        if (!isRunning) { \
            return; \
        } else if (count < maxSamples) { \
            latencies.push_back(us); \
        } else { \
            calculate_percentiles(latencies); \
            isRunning = false; \
        } \
    } \
    }
#define LATENCY_MEASURE_START() latencyStart = std::chrono::steady_clock::now();
#define LATENCY_MEASURE_END() record_latency();
#define READ_TIMER() readTimer();
#else
#define LATENCY_MEASURE_START()
#define LATENCY_MEASURE_END()
#define DECLARE_LATENCY_MEMBERS(COUNT_LATENCY_MEASUREMENTS)
#endif

void calculate_percentiles(std::vector<double> &latencies);

void checkLatency(BaseJsonParser &parser, std::string_view message);

void checkLatency();
