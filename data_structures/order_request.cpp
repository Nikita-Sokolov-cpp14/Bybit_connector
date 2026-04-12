#include "order_request.h"
#include <iostream>

int OrderRequest::priority() const {
    switch (typeOrderRequest) {
        case TypeOrderRequest_Cancel:
            return 1;
        case TypeOrderRequest_Replace:
            return 2;
        case TypeOrderRequest_New:
            return 3;
        default:
            std::cout << "OrderRequest::priority: unknown priority" << std::endl;
            return 0;
    }
    std::cout << "OrderRequest::priority: unknown priority" << std::endl;
    return 0;
}
