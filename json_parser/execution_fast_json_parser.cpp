#include "execution_fast_json_parser.h"

ExecutionFastJsonParser::ExecutionFastJsonParser(ExecutionFast *const executionFast) :
executionFast_(executionFast) {
}

void ExecutionFastJsonParser::parse() {
    if (executionFast_ == nullptr) {
        std::cout << "ExecutionFastJsonParser::parse: ERROR: statusMessage = nulptr" << std::endl;
        return;
    }

    size_t cusrsor = 0;
    cusrsor = string_.find("topic");
    executionFast_->topic = getFieldValue("topic", string_, cusrsor);

    cusrsor = string_.find("creationTime", cusrsor);
    executionFast_->creationTime =
            convertTo<uint64_t>(getFieldValue("creationTime", string_, cusrsor));

    std::string_view dataStr = string_.substr(string_.find("data"));
    parseData(dataStr);
}

void ExecutionFastJsonParser::parseData(std::string_view dataStr) {
    size_t cusrsor = 0;
    cusrsor = dataStr.find("category", cusrsor);
    executionFast_->category = ParseCategory(getFieldValue("category", dataStr, cusrsor));

    cusrsor = dataStr.find("symbol", cusrsor);
    executionFast_->symbol = getFieldValue("symbol", dataStr, cusrsor);

    cusrsor = dataStr.find("execId", cusrsor);
    executionFast_->execId = getFieldValue("execId", dataStr, cusrsor);

    cusrsor = dataStr.find("execPrice", cusrsor);
    executionFast_->execPrice = convertToDouble(getFieldValue("execPrice", dataStr, cusrsor));

    cusrsor = dataStr.find("execQty", cusrsor);
    executionFast_->execQty = convertToDouble(getFieldValue("execQty", dataStr, cusrsor));

    cusrsor = dataStr.find("orderId", cusrsor);
    executionFast_->orderId = getFieldValue("orderId", dataStr, cusrsor);

    cusrsor = dataStr.find("isMaker", cusrsor);
    executionFast_->isMaker = convertToBool(getFieldValue("isMaker", dataStr, cusrsor));

    cusrsor = dataStr.find("orderLinkId", cusrsor);
    executionFast_->orderLinkId = getFieldValue("orderLinkId", dataStr, cusrsor);

    cusrsor = dataStr.find("side", cusrsor);
    executionFast_->side = parseSide(getFieldValue("side", dataStr, cusrsor));

    cusrsor = dataStr.find("execTime", cusrsor);
    executionFast_->execTime = convertTo<uint64_t>(getFieldValue("execTime", dataStr, cusrsor));

    cusrsor = dataStr.find("seq", cusrsor);
    executionFast_->seq = convertTo<uint64_t>(getFieldValue("seq", dataStr, cusrsor));
}

void ExecutionFastJsonParser::printData() {
    executionFast_->print();
}
