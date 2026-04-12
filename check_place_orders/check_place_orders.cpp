#include "check_place_orders.h"

void addNewOrders(std::shared_ptr<OrderSender> orderSender) {
    std::this_thread::sleep_for(std::chrono::seconds(4));
    for (int i = 50000; i < 55000; i += 1000) {
        std::this_thread::sleep_for(std::chrono::seconds(4));
        OrderRequest order;
        order.req_id = i + 2;
        order.typeOrderRequest = TypeOrderRequest_New;
        order.side = Side_Buy;
        order.order_type = OrderType_Limit;
        order.qty = 0.001; // дробное количество!
        order.price = i;
        order.leverage = 10;
        order.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                                     .count();
        strcpy(order.symbol, "BTCUSDT");
        // strcpy(order.category, "linear");

        orderSender->placeOrder(order);
    }
}

void cancelOrders(std::shared_ptr<OrderSender> orderSender) {
    std::this_thread::sleep_for(std::chrono::seconds(4));
    OrderRequest order;
    order.typeOrderRequest = TypeOrderRequest_Cancel;
    order.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                 .count();
    strcpy(order.symbol, "BTCUSDT");
    // strcpy(order.category, "linear");
    strcpy(order.order_id, "2b52f835-7141-4036-9831-42e6b4e1cf15");
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    strcpy(order.order_id, "b43aa33b-ad7f-48ce-9e5e-cedf5288f604");
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    strcpy(order.order_id, "e1bfa7fe-f910-4d62-9880-889a1cc47e92");
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    strcpy(order.order_id, "7feba1d4-cf3d-440e-9a3d-4a6403e2d73e");
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    strcpy(order.order_id, "c88ff7d9-7cf3-455f-8815-2fb2f0abd098");
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
}

void replaceOrders(std::shared_ptr<OrderSender> orderSender) {
    std::this_thread::sleep_for(std::chrono::seconds(4));
    OrderRequest order;
    order.typeOrderRequest = TypeOrderRequest_Replace;
    order.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                 .count();
    strcpy(order.symbol, "BTCUSDT");
    // strcpy(order.category, "linear");
    strcpy(order.order_id, "dcb959e5-c8dc-41a8-ad66-2f6e901db8b5");
    order.qty = 0.002;
    order.price = 45000;
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    strcpy(order.order_id, "461c2de7-0823-4725-998e-33df57e66b64");
    order.qty = 0.002;
    order.price = 45100;
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    strcpy(order.order_id, "483a0409-e380-4567-b627-8eb2e4d0d0a7");
    order.qty = 0.002;
    order.price = 45200;
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    strcpy(order.order_id, "516bd2fe-ae81-4200-8c78-3800e9451560");
    order.qty = 0.002;
    order.price = 45300;
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    strcpy(order.order_id, "e9f5f344-a05a-4805-93f9-8671e5c473e0");
    order.qty = 0.002;
    order.price = 45400;
    orderSender->placeOrder(order);
    std::this_thread::sleep_for(std::chrono::seconds(4));
}
