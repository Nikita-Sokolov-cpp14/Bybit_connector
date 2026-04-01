#pragma once

#include <cstdint>
#include <string_view>
#include "common_data.h"

enum PositionStatus {
    PositionStatus_Unknown = 0,
    PositionStatus_Normal, // "Normal"
    PositionStatus_Liquidated, // "Liquidated"
    PositionStatus_Adl // "Adl"
};

// class Position {
// public:
//     std::string_view id;
//     std::string_view topic;
//     uint64_t creationTime;
//     uint64_t positionIdx;
//     uint64_t tradeMode; // TODO уточнить
//     uint64_t riskId;
//     double riskLimitValue;
//     std::string_view symbol;
//     std::string_view side; // TODO сделать enum
//     double size;
//     double entryPrice;
//     double sessionAvgPrice;
//     uint64_t leverage;
//     double positionValue;
//     double positionBalance;
//     double markPrice;
//     double positionIM;
//     double positionMM;
//     double positionIMByMp;
//     double positionMMByMp;
//     double takeProfit;
//     double stopLoss;
//     double trailingStop;
//     double unrealisedPnl;
//     double cumRealisedPnl;
//     double curRealisedPnl;
//     uint64_t createdTime;
//     uint64_t updatedTime;
//     std::string_view tpslMode; // TODO заменить на enum "Full" или "Partial"
//     double liqPrice;
//     double bustPrice;
//     std::string_view category; // TODO заменить на enum "linear"/"inverse"/"option"
//     std::string_view positionStatus; // TODO заменить на enum "Normal"/"Liquidated"/"Adl"
//     uint64_t adlRankIndicator; // 1-5, где 1 - самый высокий риск ликвидации
//     bool autoAddMargin;
//     std::string_view leverageSysUpdatedTime;
//     std::string_view mmrSysUpdatedTime;
//     uint64_t seq;
//     double breakEvenPrice;
//     bool isReduceOnly;

//     void print();
// };

// Минимальный набор для принятия торговых решений
class PositionHFT {
public:
    std::string_view id;
    std::string_view topic;

    uint64_t creationTime;

    // Идентификация позиции
    std::string_view symbol; // Какой инструмент
    Side side; // Направление

    // Критически важные для принятия решений
    double size; // Текущий размер позиции (нужно знать)
    double entryPrice; // Цена входа (для расчета PnL)
    double markPrice; // Текущая рыночная цена
    double unrealisedPnl; // Текущий PnL
    double liqPrice; // Цена ликвидации (критично!)

    PositionStatus positionStatus; // Normal/Liquidated/Adl
    uint16_t adlRankIndicator; // Риск принудительного закрытия (1-5)

    // Временные метки для детектирования задержек
    uint64_t seq; // Порядковый номер (для детекта пропусков)

    // Флаги состояния
    bool isReduceOnly; // Можно ли увеличивать позицию?

    void print();
};
