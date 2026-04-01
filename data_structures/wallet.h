#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include "common_data.h"

// Вложенная структура для coin
struct CoinInfo {
    std::string_view coin;              // "USDC"
    double equity;                      // "-0.0679535"
    double usdValue;                    // "-0.06794174"
    double walletBalance;               // "-0.0664735"
    std::string_view availableToWithdraw; // ""
    std::string_view availableToBorrow;   // ""
    double borrowAmount;                // "0.0679535"
    double accruedInterest;             // "0.00000022"
    double totalOrderIM;                // "0"
    double totalPositionIM;             // "6.70702815"
    double totalPositionMM;             // "0.32571423"
    double unrealisedPnl;               // "-0.00148"
    double cumRealisedPnl;              // "-0.0664735"
    double bonus;                       // "0"
    bool collateralSwitch;              // true
    bool marginCollateral;              // true
    double locked;                      // "0"
    double spotHedgingQty;              // "0"
    double spotBorrow;                  // "0"
};

class Wallet {
public:
    // Корневые поля
    std::string_view id;                // "140894896_wallet_1774779351206"
    std::string_view topic;             // "wallet"
    uint64_t creationTime;              // 1774779351206

    // Поля из data[0]
    double accountIMRate;               // "0.3521" - Initial Margin Rate
    double accountMMRate;               // "0.02" - Maintenance Margin Rate
    double accountIMRateByMp;           // "0.3521" - IM Rate by Mark Price
    double accountMMRateByMp;           // "0.02" - MM Rate by Mark Price

    double totalEquity;                 // "59.24728891" - Total Equity
    double totalWalletBalance;          // "95.49571699" - Total Wallet Balance
    double totalMarginBalance;          // "59.14951406" - Total Margin Balance
    double totalAvailableBalance;       // "38.32154203" - Available for trading
    double totalPerpUPL;                // "-36.34620293" - Unrealized PnL

    double totalInitialMargin;          // "20.82797202" - Total Initial Margin
    double totalMaintenanceMargin;      // "1.18454802" - Total Maintenance Margin
    double totalInitialMarginByMp;      // "20.82797202" - IM by Mark Price
    double totalMaintenanceMarginByMp;  // "1.18454802" - MM by Mark Price

    std::vector<CoinInfo> coins;        // Массив информации по монетам

    double accountLTV;                  // "0.0011" - Loan to Value
    std::string_view accountType;       // "UNIFIED" - тип аккаунта

    void print();
};

class WalletHFT {
public:
    // Корневые поля
    std::string_view id;                // 1. "140894896_wallet_1774779351206"
    std::string_view topic;             // 2. "wallet"
    uint64_t creationTime;              // 3. 1774779351206

    // Критические поля из data[0] в порядке следования
    double accountIMRate;               // 4. "0.3521"
    double accountMMRate;               // 5. "0.02"
    double totalEquity;                 // 6. "59.24728891"
    double totalWalletBalance;          // 7. "95.49571699"
    double totalMarginBalance;          // 8. "59.14951406"
    double totalAvailableBalance;       // 9. "38.32154203"
    double totalPerpUPL;                // 10. "-36.34620293"
    double totalInitialMargin;          // 11. "20.82797202"
    double totalMaintenanceMargin;      // 12. "1.18454802"

    // Информация по монетам (только критическое)
    struct CoinInfoHFT {
        std::string_view coin;          // "USDC"
        double walletBalance;           // "-0.0664735"
        double unrealisedPnl;           // "-0.00148"
        double totalPositionIM;         // "6.70702815"
        bool marginCollateral;          // true
    };
    std::vector<CoinInfoHFT> coins;     // 13. Массив монет

    double accountLTV;                  // 14. "0.0011"
    std::string_view accountType;       // 15. "UNIFIED"

    void print();
};
