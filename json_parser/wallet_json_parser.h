#pragma once
#include <string>
#include <iostream>

#include "base_json_parser.h"
#include "data_structures/wallet.h"

class WalletJsonParser : public BaseJsonParser {
public:
    WalletJsonParser(WalletHFT *const walletHFT);

    void parse() override;

    void parseData(std::string_view dataStr);

    void parseCoin(std::string_view coinStr);

    void printData() override;

private:
    WalletHFT *const walletHFT_;
};
