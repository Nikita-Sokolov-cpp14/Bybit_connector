#pragma once

#include <string>
#include <iostream>

#include "base_json_parser.h"
#include "data_structures/order_operation.h"

class OrderOperationParser : public BaseJsonParser {
public:
    OrderOperationParser(OrderOperation *const orderOperation);

    TypeOrderOperation geOperation() const;

    void parse() override;

    void printData() override;

private:
    OrderOperation *const orderOperation_;
};
