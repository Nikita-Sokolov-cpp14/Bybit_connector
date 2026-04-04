#include "wallet_json_parser.h"

namespace {


} // namespace

WalletJsonParser::WalletJsonParser(WalletHFT *const walletHFT) :
walletHFT_(walletHFT) {
}

void WalletJsonParser::parse() {
    if (walletHFT_ == nullptr) {
        std::cout << "WalletJsonParser::parse: ERROR: statusMessage = nulptr" << std::endl;
        return;
    }

    size_t cusrsor = 0;
    cusrsor = string_.find("id");
    walletHFT_->id = getFieldValue("id", string_, cusrsor);

    cusrsor = string_.find("topic");
    walletHFT_->topic = getFieldValue("topic", string_, cusrsor);

    cusrsor = string_.find("creationTime", cusrsor);
    walletHFT_->creationTime = convertTo<uint64_t>(getFieldValue("creationTime", string_, cusrsor));

    std::string_view dataStr = string_.substr(string_.find("data"));
    parseData(dataStr);
}

void WalletJsonParser::parseData(std::string_view dataStr) {
    size_t cusrsor = 0;

    cusrsor = dataStr.find("accountIMRate", cusrsor);
    walletHFT_->accountIMRate = convertToDouble(getFieldValue("accountIMRate", dataStr, cusrsor));

    cusrsor = dataStr.find("accountMMRate", cusrsor);
    walletHFT_->accountMMRate = convertToDouble(getFieldValue("accountMMRate", dataStr, cusrsor));

    cusrsor = dataStr.find("totalEquity", cusrsor);
    walletHFT_->totalEquity = convertToDouble(getFieldValue("totalEquity", dataStr, cusrsor));

    cusrsor = dataStr.find("totalWalletBalance", cusrsor);
    walletHFT_->totalWalletBalance = convertToDouble(getFieldValue("totalWalletBalance", dataStr, cusrsor));

    cusrsor = dataStr.find("totalMarginBalance", cusrsor);
    walletHFT_->totalMarginBalance = convertToDouble(getFieldValue("totalMarginBalance", dataStr, cusrsor));

    cusrsor = dataStr.find("totalAvailableBalance", cusrsor);
    walletHFT_->totalAvailableBalance = convertToDouble(getFieldValue("totalAvailableBalance", dataStr, cusrsor));

    cusrsor = dataStr.find("totalPerpUPL", cusrsor);
    walletHFT_->totalPerpUPL = convertToDouble(getFieldValue("totalPerpUPL", dataStr, cusrsor));

    cusrsor = dataStr.find("totalInitialMargin", cusrsor);
    walletHFT_->totalInitialMargin = convertToDouble(getFieldValue("totalInitialMargin", dataStr, cusrsor));

    cusrsor = dataStr.find("totalMaintenanceMargin", cusrsor);
    walletHFT_->totalMaintenanceMargin = convertToDouble(getFieldValue("totalMaintenanceMargin", dataStr, cusrsor));

    cusrsor = dataStr.find(R"("coin":)", cusrsor);
    cusrsor += 7;
    size_t endCoins = dataStr.find(R"(}])", cusrsor);
    while (cusrsor <= endCoins) {
        size_t endCoin = dataStr.find('}', cusrsor);
        parseCoin(dataStr.substr(cusrsor, endCoin - cusrsor));
        cusrsor = endCoin + 1;
    }

    cusrsor = dataStr.find("accountLTV", cusrsor);
    walletHFT_->accountLTV = convertToDouble(getFieldValue("accountLTV", dataStr, cusrsor));

    cusrsor = dataStr.find("accountType", cusrsor);
    walletHFT_->accountType = getFieldValue("accountType", dataStr, cusrsor);
}

void WalletJsonParser::parseCoin(std::string_view coinStr) {
    size_t cusrsor = 0;
    CoinInfoHFT coinInfoHFT;

    cusrsor = coinStr.find("coin", cusrsor);
    coinInfoHFT.coin = (getFieldValue("coin", coinStr, cusrsor));

    cusrsor = coinStr.find("walletBalance", cusrsor);
    coinInfoHFT.walletBalance = convertToDouble(getFieldValue("walletBalance", coinStr, cusrsor));

    cusrsor = coinStr.find("totalPositionIM", cusrsor);
    coinInfoHFT.totalPositionIM = convertToDouble(getFieldValue("totalPositionIM", coinStr, cusrsor));

    cusrsor = coinStr.find("unrealisedPnl", cusrsor);
    coinInfoHFT.unrealisedPnl = convertToDouble(getFieldValue("unrealisedPnl", coinStr, cusrsor));

    cusrsor = coinStr.find("marginCollateral", cusrsor);
    coinInfoHFT.marginCollateral = convertToBool(getFieldValue("marginCollateral", coinStr, cusrsor));

    walletHFT_->coins.push_back(coinInfoHFT);
}

void WalletJsonParser::printData() {
    walletHFT_->print();
}
