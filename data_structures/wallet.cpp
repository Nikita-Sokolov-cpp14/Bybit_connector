#include "wallet.h"
#include <iostream>

void CoinInfoHFT::print() const {
    std::cout << "coin " << coin << std::endl;
    std::cout << "walletBalance " << walletBalance << std::endl;
    std::cout << "totalPositionIM " << totalPositionIM << std::endl;
    std::cout << "unrealisedPnl " << unrealisedPnl << std::endl;
    std::cout << "marginCollateral " << marginCollateral << std::endl;
}

void WalletHFT::print() {
    std::cout << "id " << id << std::endl;
    std::cout << "topic " << topic << std::endl;
    std::cout << "creationTime " << creationTime << std::endl;

    std::cout << "accountIMRate " << accountIMRate << std::endl;
    std::cout << "accountMMRate " << accountMMRate << std::endl;
    std::cout << "totalEquity " << totalEquity << std::endl;
    std::cout << "totalWalletBalance " << totalWalletBalance << std::endl;
    std::cout << "totalMarginBalance " << totalMarginBalance << std::endl;
    std::cout << "totalAvailableBalance " << totalAvailableBalance << std::endl;
    std::cout << "totalPerpUPL " << totalPerpUPL << std::endl;
    std::cout << "totalInitialMargin " << totalInitialMargin << std::endl;
    std::cout << "totalMaintenanceMargin " << totalMaintenanceMargin << std::endl;

    for (const auto &coin : coins) {
        std::cout << "next coin:" << std::endl;
        coin.print();
    }

    std::cout << "accountLTV " << accountLTV << std::endl;
    std::cout << "accountType " << accountType << std::endl;
}
