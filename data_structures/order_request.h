#pragma once

#include <cstdint>
#include <string>

#include "common_data.h"

enum TypeOrderRequest {
    TypeOrderRequest_Unknown = 0,
    TypeOrderRequest_Cancel,
    TypeOrderRequest_Replace,
    TypeOrderRequest_New
};

// enum TimeInForce {
//     TimeInForce_Unknown = 0,
//     TimeInForce_GTC, // Висит до исполнения или отмены	Основной режим для HFT
//     TimeInForce_IOC, // Исполняется немедленно, неисполненное отменяется	Агрессивное взятие ликвидности
//     TimeInForce_FOK, // Должен исполниться полностью или отменяется	Крупные ордера
//     TimeInForce_POST_ONLY // Гарантирует роль мейкера (не снимает ликвидность)	Получение rebate, снижение slippage
// };

// TimeInForce time_in_force = TimeInForce_GTC;  // значение по умолчанию

struct OrderRequest {
    uint64_t req_id; // свой ID для трекинга
    char order_id[64];         // ID ордера ОТ БИРЖИ (для cancel/replace, пусто для new)
    // char category[16]; // TODO добавить
    char symbol[16];
    Side side;
    OrderType order_type; // "Limit", "Market"
    double qty;
    double price; // пусто для market ?
    uint16_t leverage = 1;

    // служебная инфа. Не сериализируется
    TypeOrderRequest typeOrderRequest;
    uint64_t enqueue_time; // нужно для измерения задержки
    // order.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
    //         std::chrono::steady_clock::now().time_since_epoch())
    //                              .count();
    // При получении ответа от биржи:
    // auto now = std::chrono::duration_cast<std::chrono::microseconds>(
    //         std::chrono::steady_clock::now().time_since_epoch())
    //                    .count();
    // auto total_latency = now - order.enqueue_time;

    OrderRequest() = default;

    // Приоритет: CANCEL > REPLACE > NEW
    int priority() const;
};


// структура нового отправляемого ордера
// {
//     "reqId": "10001",
//     "op": "order.create",
    // "header": {
    //     "X-BAPI-TIMESTAMP": "1711001595207",
    //     "X-BAPI-RECV-WINDOW": "8000"
    // },
//     "args": [{
//         "category": "linear",
//         "symbol": "BTCUSDT",
//         "side": "Buy",
//         "orderType": "Limit",
//         "qty": "1",
//         "price": "50000",
//         "timeInForce": "GTC",
//         "positionIdx": 0,
//         "orderLinkId": "order_123456",
//         "leverage": "10"
//     }]
// }

// отмена ордера через orderId
// [{
//     "req_id": "10001",
//     "op": "order.cancel",
//     "args": {
//         "category": "linear",
//         "symbol": "BTCUSDT",
//         "orderId": "5cf98598-39a7-459e-97bf-76ca765ee020" // Берется из ответа биржи о размещении ордера
//     }]
// }

// отмена через orderLinkId (должен быть уникальным для каждого ордера)
// [{
//     "req_id": "10001",
//     "op": "order.cancel",
//     "args": {
//         "category": "linear",
//         "symbol": "BTCUSDT",
//         "orderLinkId": "123456"
//     }]
// }

// Replace/Amend Order (Замена ордера)
// через orderId
// [{
//     "req_id": "10002",
//     "op": "order.amend",
//     "args": {
//         "category": "linear",
//         "symbol": "BTCUSDT",
//         "orderId": "5cf98598-39a7-459e-97bf-76ca765ee020",
//         "price": "51000",
//         "qty": "2"
//     }]
// }

// Replace/Amend Order (Замена ордера)
// через orderLinkId
// [{
//     "req_id": "10002",
//     "op": "order.amend",
//     "args": {
//         "category": "linear",
//         "symbol": "BTCUSDT",
//         "orderLinkId": "123456",
//         "price": "51000",
//         "qty": "2"
//     }]
// }


// void OrderSender::serialize_cancel_order(std::string& buffer, const OrderRequest& order) {
//     buffer.clear();
//     buffer.reserve(256);

//     buffer += "{\"req_id\":";
//     buffer += std::to_string(order.req_id);
//     buffer += ",\"op\":\"order.cancel\",\"args\":[{";
//     buffer += "\"category\":\"linear\",";
//     buffer += "\"symbol\":\"";
//     buffer += order.symbol;
//     buffer += "\",";

//     // Используем orderId если есть, иначе orderLinkId
//     if (strlen(order.order_id) > 0) {
//         buffer += "\"orderId\":\"";
//         buffer += order.order_id;
//         buffer += "\"";
//     } else if (order.req_id_for_cancel > 0) {  // если добавите это поле
//         buffer += "\"orderLinkId\":\"";
//         buffer += std::to_string(order.req_id_for_cancel);
//         buffer += "\"";
//     }

//     buffer += "}]}";
// }

// void OrderSender::serialize_replace_order(std::string& buffer, const OrderRequest& order) {
//     buffer.clear();
//     buffer.reserve(512);

//     buffer += "{\"req_id\":";
//     buffer += std::to_string(order.req_id);
//     buffer += ",\"op\":\"order.amend\",\"args\":[{";
//     buffer += "\"category\":\"linear\",";
//     buffer += "\"symbol\":\"";
//     buffer += order.symbol;
//     buffer += "\",";

//     // Идентификатор ордера
//     if (strlen(order.order_id) > 0) {
//         buffer += "\"orderId\":\"";
//         buffer += order.order_id;
//         buffer += "\"";
//     } else if (order.req_id_for_cancel > 0) {
//         buffer += "\"orderLinkId\":\"";
//         buffer += std::to_string(order.req_id_for_cancel);
//         buffer += "\"";
//     }

//     // Новые параметры (хотя бы один из них)
//     if (order.price > 0) {
//         buffer += ",\"price\":\"";
//         buffer += std::to_string(order.price);
//         buffer += "\"";
//     }

//     if (order.qty > 0) {
//         buffer += ",\"qty\":\"";
//         buffer += std::to_string(static_cast<long long>(order.qty));
//         buffer += "\"";
//     }

//     buffer += "}]}";
// }
