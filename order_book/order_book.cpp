#include "order_book.h"

// Применяет snapshot - полная замена стакана
void OrderBook::apply_snapshot(std::string_view data, uint64_t timestamp_ns) {
}

// Применяет delta - инкрементальное обновление
bool OrderBook::apply_delta(std::string_view data, uint64_t timestamp_ns) {

    return false;
}
