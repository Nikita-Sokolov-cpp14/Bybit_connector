#include "orderbook_json_parser.h"

namespace {

const std::string_view typeFieldName = "type";
const std::string_view snapshot = "snapshot";
const std::string_view delta = "delta";
const std::string_view data = "data";
const std::string_view bids = R"("b":[)";
const std::string_view asks = R"("a":[)";
const std::string_view uName = "u";
const std::string_view seqName = "seq";
const std::string_view tsName = "ts";
const std::string_view ctsName = "cts";
const std::string_view sName = "s";
const std::string_view topicName = "topic";

} // namespace

OrderBookJsonParser::OrderBookJsonParser(OrderBook *const orderBook) :
orderBook_(orderBook),
typeOrderbookMessage_(TypeOrderbookMessage_Unkown) {
}

void OrderBookJsonParser::parse() {
    if (orderBook_ == nullptr) {
        std::cout << " OrderBookJsonParser::parse: ERROR: orderBook = nulptr" << std::endl;
        return;
    }

    parseTypeOrderbookMessage();
    if (typeOrderbookMessage_ == TypeOrderbookMessage_Unkown) {
        std::cout << "OrderBookJsonParser::parse: Unknown orderbook type" << std::endl;
        return;
    }
    if (typeOrderbookMessage_ == TypeOrderbookMessage_Snapshot) {
        orderBook_->clearLevels();
    }

    parseDataMessage();
}

void OrderBookJsonParser::parseTypeOrderbookMessage() {
    std::string_view type = getFieldValue(typeFieldName, string_);

    typeOrderbookMessage_ = TypeOrderbookMessage_Unkown;
    if (type == snapshot) {
        typeOrderbookMessage_ = TypeOrderbookMessage_Snapshot;
    } else if (type == delta) {
        typeOrderbookMessage_ = TypeOrderbookMessage_Delta;
    }
}

void OrderBookJsonParser::parseDataMessage() {
    orderBook_->topic = getFieldValue(topicName, string_);
    orderBook_->ts = convertTo<uint64_t>(getFieldValue(tsName, string_));
    size_t dataStartPos = string_.find(data);
    size_t dataEndPos = string_.find('}', dataStartPos);
    orderBook_->cts = convertTo<uint64_t>(getFieldValue(ctsName, string_));
    parseDataSection(string_.substr(dataStartPos, dataEndPos - dataStartPos));
}

void OrderBookJsonParser::printData() {
    orderBook_->print();
}

void OrderBookJsonParser::parseDataSection(std::string_view dataStr) {
    orderBook_->s = getFieldValue(sName, dataStr);
    size_t startBidsArr = dataStr.find(bids) + bids.size();
    size_t endBidsArr = dataStr.find("]]", startBidsArr) + 1;
    parseArray(dataStr.substr(startBidsArr, endBidsArr - startBidsArr), TypeArray_Bids);

    size_t startAsksArr = dataStr.find(asks) + asks.size();
    size_t endAsksArr = dataStr.find("]]", startAsksArr) + 1;
    parseArray(dataStr.substr(startAsksArr, endAsksArr - startAsksArr), TypeArray_Asks);

    orderBook_->u = convertTo<uint64_t>(getFieldValue(uName, dataStr));
    orderBook_->seq = convertTo<uint64_t>(getFieldValue(seqName, dataStr));
}

void OrderBookJsonParser::parseArray(std::string_view arrayStr, const TypeArray &typeArray) {
    std::pair<double, double> priceVolume;
    const size_t strLen = arrayStr.length();
    for (size_t pos = 1; pos < strLen;) {
        size_t commaPos = arrayStr.find("],[", pos);
        if (commaPos >= strLen) {
            commaPos = strLen - 1;
        }
        priceVolume = parsePair(arrayStr.substr(pos, commaPos - pos));
        if (typeArray == TypeArray_Bids) {
            updateMap(orderBook_->bids, priceVolume.first, priceVolume.second);
        } else if (typeArray == TypeArray_Asks) {
            updateMap(orderBook_->asks, priceVolume.first, priceVolume.second);
        }
        pos = commaPos + 3;
    }
}
