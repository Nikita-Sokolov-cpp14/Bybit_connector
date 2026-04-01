#include "position_json_parser.h"

namespace {

PositionStatus ParsePositionStatus(std::string_view statusStr) {
    if (statusStr == "Normal") {
        return PositionStatus_Normal;
    } else if (statusStr == "Liquidated") {
        return PositionStatus_Liquidated;
    } else if (statusStr == "Adl") {
        return PositionStatus_Adl;
    } else {
        std::cout << "ParsePositionStatus: Unknown" << std::endl;
    }

    return PositionStatus_Unknown;
}

} // namespace

// TODO Используется уменьшенная структура для экономии времени
PositionJsonParser::PositionJsonParser(PositionHFT *const position) :
position_(position) {
}

void PositionJsonParser::parse() {
    if (position_ == nullptr) {
        std::cout << "PositionJsonParser::parse: ERROR: statusMessage = nulptr" << std::endl;
        return;
    }

    size_t cusrsor = 0;
    position_->id = getFieldValue("id", string_);

    cusrsor = string_.find("topic");
    position_->topic = getFieldValue("topic", string_, cusrsor);

    cusrsor = string_.find("creationTime", cusrsor);
    position_->creationTime = convertTo<uint64_t>(getFieldValue("creationTime", string_, cusrsor));

    std::string_view dataStr = string_.substr(string_.find("data"));
    parseData(dataStr);
}

void PositionJsonParser::parseData(std::string_view dataStr) {

    size_t cusrsor = 0;
    cusrsor = dataStr.find("symbol", cusrsor);
    position_->symbol = getFieldValue("symbol", dataStr, cusrsor);

    cusrsor = dataStr.find("side", cusrsor);
    position_->side = parseSide(getFieldValue("side", dataStr, cusrsor));

    cusrsor = dataStr.find("size", cusrsor);
    position_->size = convertToDouble(getFieldValue("size", dataStr, cusrsor));

    cusrsor = dataStr.find("entryPrice", cusrsor);
    position_->entryPrice = convertToDouble(getFieldValue("entryPrice", dataStr, cusrsor));

    cusrsor = dataStr.find("markPrice", cusrsor);
    position_->markPrice = convertToDouble(getFieldValue("markPrice", dataStr, cusrsor));

    cusrsor = dataStr.find("unrealisedPnl", cusrsor);
    position_->unrealisedPnl = convertToDouble(getFieldValue("unrealisedPnl", dataStr, cusrsor));

    cusrsor = dataStr.find("liqPrice", cusrsor);
    position_->liqPrice = convertToDouble(getFieldValue("liqPrice", dataStr, cusrsor));

    cusrsor = dataStr.find("positionStatus", cusrsor);
    position_->positionStatus =
            ParsePositionStatus(getFieldValue("positionStatus", dataStr, cusrsor));

    cusrsor = dataStr.find("adlRankIndicator", cusrsor);
    position_->adlRankIndicator =
            convertTo<uint64_t>(getFieldValue("adlRankIndicator", dataStr, cusrsor));

    cusrsor = dataStr.find("seq", cusrsor);
    position_->seq = convertTo<uint64_t>(getFieldValue("seq", dataStr, cusrsor));

    cusrsor = dataStr.find("isReduceOnly", cusrsor);
    position_->isReduceOnly = convertToBool(getFieldValue("isReduceOnly", dataStr, cusrsor));
}

void PositionJsonParser::printData() {
    position_->print();
}
