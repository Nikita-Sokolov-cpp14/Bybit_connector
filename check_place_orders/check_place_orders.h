#pragma once

#include "websocket_client/order_sender.h"
#include "connection_manager/connection_manager.h"

void addNewOrders(ConnectionManager &connectionManager);
void cancelOrders(ConnectionManager &connectionManager);
void replaceOrders(ConnectionManager &connectionManager);
