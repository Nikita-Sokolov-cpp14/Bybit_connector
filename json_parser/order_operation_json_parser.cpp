#include "order_operation_json_parser.h"

OrderOperationParser::OrderOperationParser(OrderOperation *const orderOperation) :
orderOperation_(orderOperation) {
}

TypeOrderOperation OrderOperationParser::geOperation() const {
    std::string_view operation = getFieldValue("op", string_);
    if (operation == "auth") {
        return TypeOrderOperation_Auth;
    } else if (operation == "order.create") {
        return TypeOrderOperation_Create;
    } else if (operation == "order.cancel") {
        return TypeOrderOperation_Cancel;
    } else if (operation == "order.amend") {
        return TypeOrderOperation_Amend;
    }

    return TypeOrderOperation_Unknown;
}

void OrderOperationParser::parse() {
    orderOperation_->retCode = convertTo<int>(getFieldValue("retCode", string_));
    orderOperation_->retMsg = getFieldValue("retMsg", string_);
    orderOperation_->op = geOperation();

    if (orderOperation_->op == TypeOrderOperation_Auth) {
        return;
    }

    orderOperation_->orderId = getFieldValue("orderId", string_);
    orderOperation_->orderLinkId = getFieldValue("orderLinkId", string_);
    orderOperation_->X_Bapi_Limit_Status = getFieldValue("X-Bapi-Limit-Status", string_);
    orderOperation_->X_Bapi_Limit_Reset_Timestamp = convertTo<size_t>(getFieldValue("X-Bapi-Limit-Reset-Timestamp", string_));
    orderOperation_->Traceid = getFieldValue("Traceid", string_);
    orderOperation_->Timenow = convertTo<size_t>(getFieldValue("Timenow", string_));
    orderOperation_->X_Bapi_Limit = getFieldValue("X-Bapi-Limit", string_);
    orderOperation_->connId = getFieldValue("connId", string_);
}

void OrderOperationParser::printData() {
    orderOperation_->print();
}
