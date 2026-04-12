#pragma once

#include <cstdint>
#include <string_view>
#include "common_data.h"


enum OrderStatus {
    OrderStatus_Unknown = 0,
    OrderStatus_Created,
    OrderStatus_New,
    OrderStatus_PartiallyFilled,
    OrderStatus_Filled,
    OrderStatus_Cancelled,
    OrderStatus_Rejected,
    OrderStatus_Expired,
    OrderStatus_Deactivated,
    OrderStatus_Triggered
};

enum class TimeInForce : uint8_t {
    Unknown = 0,
    GTC = 1,      // Good Till Cancel
    IOC = 2,      // Immediate or Cancel
    FOK = 3,      // Fill or Kill
    PostOnly = 4  // Post Only
};

enum class CreateType : uint8_t {
    Unknown = 0,
    CreateByUser = 1,
    CreateByStrategy = 2,
    CreateByApi = 3
};

class Order {
public:
   // Корневые поля сообщения
    std::string_view topic;        // "order" - тип сообщения
    std::string_view id;           // "140894896_BTCPERP_37449201146" - ID сообщения
    uint64_t creationTime;         // 1774779351203 - время создания сообщения

    // Поля из data[0] в порядке следования
    std::string_view category;     // "linear" - категория инструмента
    std::string_view symbol;       // "BTCPERP" - символ

    std::string_view orderId;      // ID ордера
    std::string_view orderLinkId;  // Клиентский ID
    std::string_view blockTradeId; // ID блочной сделки

    Side side;                     // "Buy"/"Sell" - сторона
    uint64_t positionIdx;          // 0 - индекс позиции

    std::string_view orderStatus;  // "Filled" - статус ордера (критично!)
    std::string_view cancelType;   // "UNKNOWN" - тип отмены
    std::string_view rejectReason; // "EC_NoError" - причина отказа

    std::string_view timeInForce;  // "IOC" - время жизни ордера
    std::string_view isLeverage;   // "" - флаг кредитного плеча

    double price;                  // "69795.6" - цена ордера
    double qty;                    // "0.001" - количество

    double avgPrice;               // "66473.5" - средняя цена исполнения
    double leavesQty;              // "0" - остаток к исполнению
    double leavesValue;            // "0" - остаток стоимости
    double cumExecQty;             // "0.001" - исполненное количество
    double cumExecValue;           // "66.4735" - исполненная стоимость
    double cumExecFee;             // "0.0664735" - комиссия

    std::string_view orderType;    // "Market" - тип ордера
    std::string_view stopOrderType;// "" - тип стоп-ордера
    std::string_view orderIv;      // "" - IV для опционов
    std::string_view triggerPrice; // "" - цена триггера
    std::string_view takeProfit;   // "" - тейк-профит
    std::string_view stopLoss;     // "" - стоп-лосс
    std::string_view triggerBy;    // "" - условие триггера
    std::string_view tpTriggerBy;  // "" - условие TP
    std::string_view slTriggerBy;  // "" - условие SL
    int64_t triggerDirection;      // 0 - направление триггера
    std::string_view placeType;    // "" - тип размещения

    double lastPriceOnCreated;     // "66467.5" - цена на момент создания
    bool closeOnTrigger;           // false - закрыть при триггере
    bool reduceOnly;               // false - только уменьшение позиции

    int64_t smpGroup;              // 0 - группа SMP
    std::string_view smpType;      // "None" - тип SMP
    std::string_view smpOrderId;   // "" - SMP ордер ID

    double slLimitPrice;           // "0" - лимит цены SL
    double tpLimitPrice;           // "0" - лимит цены TP
    std::string_view tpslMode;     // "UNKNOWN" - режим TP/SL

    std::string_view createType;   // "CreateByUser" - тип создания
    std::string_view marketUnit;   // "" - единица рынка

    uint64_t createdTime;          // 1774779351198 - время создания ордера
    uint64_t updatedTime;          // 1774779351201 - время обновления

    std::string_view feeCurrency;  // "" - валюта комиссии
    double closedPnl;              // "0" - закрытый PnL
    std::string_view parentOrderLinkId; // "" - родительский ордер

    double slippageTolerance;      // "0" - допуск проскальзывания
    std::string_view slippageToleranceType; // "UNKNOWN" - тип допуска

    // cumFeeDetail - вложенный объект (можно опустить или добавить отдельно)
    double cumFeeUSDC;             // "0.0664735" - комиссия в USDC

    void print();
};


class OrderHFT {
public:
    // Корневые поля
    std::string_view topic;        // 1. "order"
    std::string_view id;           // 2. ID сообщения
    uint64_t creationTime;         // 3. Время сообщения

    Category category;

    // Поля из data[0] - только критические
    std::string_view symbol;       // 4. Инструмент
    std::string_view orderId;      // 5. ID ордера
    std::string_view orderLinkId;  // 6. Клиентский ID
    Side side;                     // 7. Сторона

    OrderStatus orderStatus;  // 8. Статус (Filled/PartiallyFilled/Cancelled)
    std::string_view timeInForce;  // 9. IOC/GTC/FOK

    double price;                  // 11. Цена ордера
    double qty;                    // 12. Количество
    double avgPrice;               // 13. Средняя цена исполнения
    double leavesQty;              // 15. Остаток к исполнению
    double cumExecQty;             // 14. Исполненное количество

    std::string_view orderType;    // 10. Market/Limit

    bool reduceOnly;               // 16. Только уменьшение

    uint64_t createdTime;          // 17. Время создания
    uint64_t updatedTime;          // 18. Время обновления

    void print();
};
