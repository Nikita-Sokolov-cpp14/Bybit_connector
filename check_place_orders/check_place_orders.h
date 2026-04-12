#pragma once

#include "data_loader/order_sender.h"

void addNewOrders(std::shared_ptr<OrderSender> orderSender);
void cancelOrders(std::shared_ptr<OrderSender> orderSender);
void replaceOrders(std::shared_ptr<OrderSender> orderSender);
