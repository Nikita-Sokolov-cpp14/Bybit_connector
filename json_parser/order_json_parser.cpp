#include "order_json_parser.h"

namespace {

OrderStatus ParseOrderStatus(std::string_view statusStr) {
    if (statusStr == "Created") {
        return OrderStatus_Created;
    } else if (statusStr == "New") {
        return OrderStatus_New;
    } else if (statusStr == "PartiallyFilled") {
        return OrderStatus_PartiallyFilled;
    } else if (statusStr == "Filled") {
        return OrderStatus_Filled;
    } else if (statusStr == "Cancelled") {
        return OrderStatus_Cancelled;
    } else if (statusStr == "Rejected") {
        return OrderStatus_Rejected;
    } else if (statusStr == "Expired") {
        return OrderStatus_Expired;
    } else if (statusStr == "Deactivated") {
        return OrderStatus_Deactivated;
    } else if (statusStr == "Triggered") {
        return OrderStatus_Triggered;
    } else {
        std::cout << "ParseOrderStatus: Unknown" << std::endl;
    }

    return OrderStatus_Unknown;
}

} // namespace

OrderJsonParser::OrderJsonParser(OrderHFT *const orderHFT) :
orderHFT_(orderHFT) {
}

void OrderJsonParser::parse() {
    if (orderHFT_ == nullptr) {
        std::cout << "OrderJsonParser::parse: ERROR: statusMessage = nulptr" << std::endl;
        return;
    }

    size_t cusrsor = 0;
    cusrsor = string_.find("topic");
    orderHFT_->topic = getFieldValue("topic", string_, cusrsor);

    cusrsor = string_.find("id");
    orderHFT_->id = getFieldValue("id", string_, cusrsor);

    cusrsor = string_.find("creationTime", cusrsor);
    orderHFT_->creationTime = convertTo<uint64_t>(getFieldValue("creationTime", string_, cusrsor));

    std::string_view dataStr = string_.substr(string_.find("data"));
    parseData(dataStr);
}

void OrderJsonParser::parseData(std::string_view dataStr) {
    size_t cusrsor = 0;
    cusrsor = dataStr.find("category", cusrsor);
    orderHFT_->category = ParseCategory(getFieldValue("category", dataStr, cusrsor));

    cusrsor = dataStr.find("symbol", cusrsor);
    orderHFT_->symbol = getFieldValue("symbol", dataStr, cusrsor);

    cusrsor = dataStr.find("orderId", cusrsor);
    orderHFT_->orderId = getFieldValue("orderId", dataStr, cusrsor);

    cusrsor = dataStr.find("orderLinkId", cusrsor);
    orderHFT_->orderLinkId = getFieldValue("orderLinkId", dataStr, cusrsor);

    cusrsor = dataStr.find("side", cusrsor);
    orderHFT_->side = parseSide(getFieldValue("side", dataStr, cusrsor));

    cusrsor = dataStr.find("orderStatus", cusrsor);
    orderHFT_->orderStatus = ParseOrderStatus(getFieldValue("orderStatus", dataStr, cusrsor));

    cusrsor = dataStr.find("timeInForce", cusrsor);
    orderHFT_->timeInForce = getFieldValue("timeInForce", dataStr, cusrsor);

    cusrsor = dataStr.find("price", cusrsor);
    orderHFT_->price = convertToDouble(getFieldValue("price", dataStr, cusrsor));

    cusrsor = dataStr.find("qty", cusrsor);
    orderHFT_->qty = convertToDouble(getFieldValue("qty", dataStr, cusrsor));

    cusrsor = dataStr.find("avgPrice", cusrsor);
    orderHFT_->avgPrice = convertToDouble(getFieldValue("avgPrice", dataStr, cusrsor));

    cusrsor = dataStr.find("leavesQty", cusrsor);
    orderHFT_->leavesQty = convertToDouble(getFieldValue("leavesQty", dataStr, cusrsor));

    cusrsor = dataStr.find("cumExecQty", cusrsor);
    orderHFT_->cumExecQty = convertToDouble(getFieldValue("cumExecQty", dataStr, cusrsor));

    cusrsor = dataStr.find("orderType", cusrsor);
    orderHFT_->orderType = getFieldValue("orderType", dataStr, cusrsor);

    cusrsor = dataStr.find("reduceOnly", cusrsor);
    orderHFT_->reduceOnly = convertToBool(getFieldValue("reduceOnly", dataStr, cusrsor));

    cusrsor = dataStr.find("createdTime", cusrsor);
    orderHFT_->createdTime = convertTo<uint64_t>(getFieldValue("createdTime", dataStr, cusrsor));

    cusrsor = dataStr.find("updatedTime", cusrsor);
    orderHFT_->updatedTime = convertTo<uint64_t>(getFieldValue("updatedTime", dataStr, cusrsor));
}

void OrderJsonParser::printData() {
    orderHFT_->print();
}
