#include "status_json_parser.h"

namespace {

const std::string_view statusFieldName = "success";
const std::string_view retMsgFieldName = "ret_msg";
const std::string_view connIdFieldName = "conn_id";
const std::string_view reqIdFieldName = "req_id";
const std::string_view opFieldName = "op";

} // namespace

StatusJsonParser::StatusJsonParser(StatusMessage *const statusMessage) :
statusMessage_(statusMessage) {
}

void StatusJsonParser::parse() {
    if (statusMessage_ == nullptr) {
        std::cout << "StatusJsonParser::parse: ERROR: statusMessage = nulptr" << std::endl;
        return;
    }

    std::string_view sucsessStr = getFieldValue(statusFieldName, string_);
    if (sucsessStr == "true") {
        statusMessage_->sucsess = true;
    } else {
        statusMessage_->sucsess = false;
    }
    statusMessage_->retMessage = getFieldValue(retMsgFieldName, string_);
    statusMessage_->conId = getFieldValue(connIdFieldName, string_);
    statusMessage_->reqId = getFieldValue(reqIdFieldName, string_);
    statusMessage_->operation = getFieldValue(opFieldName, string_);
}

void StatusJsonParser::printData() {
    std::cout << "sucsess: " << statusMessage_->sucsess << std::endl;
    std::cout << "retMessage: " << statusMessage_->retMessage << std::endl;
    std::cout << "conId: " << statusMessage_->conId << std::endl;
    std::cout << "reqId: " << statusMessage_->reqId << std::endl;
    std::cout << "operation: " << statusMessage_->operation << std::endl;
}
