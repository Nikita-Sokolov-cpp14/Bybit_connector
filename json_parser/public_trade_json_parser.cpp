#include "public_trade_json_parser.h"

namespace {

// const std::string_view typeFieldName = "type";
// const std::string_view snapshot = "snapshot";
// const std::string_view delta = "delta";
const std::string_view data = "data";
// const std::string_view bids = R"("b":[)";
// const std::string_view asks = R"("a":[)";
// const std::string_view uName = "u";
// const std::string_view seqName = "seq";
const std::string_view tsName = "ts";
// const std::string_view ctsName = "cts";
const std::string_view topicName = "topic";

} // namespace

PublicTradeJsonParser::PublicTradeJsonParser(PublicTrade *const publicTrade) :
publicTrade_(publicTrade) {
}

void PublicTradeJsonParser::parse() {
    if (publicTrade_ == nullptr) {
        std::cout << "PublicTradeJsonParser::parse: ERROR: publicTrade_ = nulptr" << std::endl;
        return;
    }

    std::string_view topic = getFieldValue(topicName, string_);
    size_t posDot = topic.find('.');
    publicTrade_->pairStr = topic.substr(posDot + 1, topic.length() - posDot);
    publicTrade_->ts = convertTo<uint64_t>(getFieldValue(tsName, string_));
    size_t dataStartPos = string_.find(data);
    dataStartPos += data.length() + 4;
    size_t dataEndPos = string_.find(']', dataStartPos);

    // std::cout << "topic: " << publicTrade_->pairStr << std::endl;
    // std::cout << "ts " << publicTrade_->ts << std::endl;
    publicTrade_->clearData();
    parseArray(string_.substr(dataStartPos, dataEndPos - dataStartPos));
}

void PublicTradeJsonParser::printData() {
    publicTrade_->print();
}

void PublicTradeJsonParser::parseArray(std::string_view arrayStr) {
    const size_t strLen = arrayStr.length();
    for (size_t pos = 0; pos < strLen;) {
        size_t commaPos = arrayStr.find("},{", pos);
        if (commaPos >= strLen) {
            commaPos = strLen - 1;
        }
        parseData(arrayStr.substr(pos, commaPos - pos));
        pos = commaPos + 3;
    }
}

void PublicTradeJsonParser::parseData(std::string_view dataStr) {
    // std::cout << "data: " << dataStr << std::endl;

    PublicTrade::Data data;
    size_t pos = dataStr.find(R"("T")");
    data.T = convertTo<uint64_t>(getFieldValue("T", dataStr, pos));
    pos = dataStr.find(R"("s")", pos);
    data.s = getFieldValue("s", dataStr, pos);
    pos = dataStr.find(R"("S")", pos);
    data.S = getFieldValue("S", dataStr, pos);
    pos = dataStr.find(R"("v")", pos);
    data.v = convertToDouble(getFieldValue("v", dataStr, pos));
    pos = dataStr.find(R"("p")", pos);
    data.p = convertToDouble(getFieldValue("p", dataStr, pos));
    pos = dataStr.find(R"("L")",pos);
    data.L = getTickDirection(getFieldValue("L", dataStr, pos));
    pos = dataStr.find(R"("i")",pos);
    data.i = getFieldValue("i", dataStr, pos);
    pos = dataStr.find(R"("BT")",pos);
    data.BT = convertToBool(getFieldValue("BT", dataStr, pos));
    pos = dataStr.find(R"("RPI")",pos);
    data.RPI = convertToBool(getFieldValue("RPI", dataStr, pos));
    pos = dataStr.find(R"("seq")",pos);
    data.seq = convertTo<uint64_t>(getFieldValue("seq", dataStr, pos));

    publicTrade_->data.push_back(data);
}

PublicTrade::TickDirection PublicTradeJsonParser::getTickDirection(std::string_view str) {
    if (str == "PlusTick") {
        return PublicTrade::TickDirection::TickDirection_PlusTick;
    }
    if (str == "MinusTick") {
        return PublicTrade::TickDirection::TickDirection_MinusTick;
    }
    if (str == "ZeroPlusTick") {
        return PublicTrade::TickDirection::TickDirection_ZeroPlusTick;
    }
    if (str == "ZeroMinusTick") {
        return PublicTrade::TickDirection::TickDirection_ZeroMinusTick;
    }
    std::cout << "PublicTradeJsonParser::getTickDirection(: error: can't convert to tick direction " << str << std::endl;;
    return PublicTrade::TickDirection::TickDirection_Unknown;
}
