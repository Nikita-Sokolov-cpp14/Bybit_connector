#include "order_sender.h"
#include "p999_latency/check_latency.h"
#include <optional>

DECLARE_LATENCY_MEMBERS(1000)

namespace {

const std::string_view retCodeFieldName = "retCode";

}

OrderSender::OrderSender(net::io_context &ioc, ssl::context &sslCtx, const std::string &api_key,
        const std::string &api_secret, const std::string_view userAgent) :
PrivateConnector(ioc, sslCtx, api_key, api_secret, userAgent),
order_queue_cancel_(maxQueueSize),
order_queue_replace_(maxQueueSize),
order_queue_new_(maxQueueSize) {
}

bool OrderSender::placeOrder(const OrderRequest &orderRequest) {
    bool result = false;

    switch (orderRequest.typeOrderRequest) {
        case TypeOrderRequest_Unknown:
            std::cout << "OrderSender::placeOrder: Unknown order type " << std::endl;
            return false;
        case TypeOrderRequest_Cancel:
            result = order_queue_cancel_.bounded_push(orderRequest);
            break;
        case TypeOrderRequest_Replace:
            result = order_queue_replace_.bounded_push(orderRequest);
            break;
        case TypeOrderRequest_New:
            result = order_queue_new_.bounded_push(orderRequest);
            break;
        default:
            result = false;
            break;
    }

    if (!result) {
        std::cout << "OrderSender::placeOrder: error add" << std::endl;
        return false;
    }

    // Атомарно проверяем и устанавливаем флаг
    bool expected = false;
    if (startSending_.compare_exchange_strong(expected, true)) {
        // Отправка не активна - запускаем
        sendNext();
    }

    // TODO
    // sendNext() может быть вызван из двух мест:

    // Из placeOrder() (поток стратегии)
    // Из on_write() (поток io_context)
    // А startSending_ сбрасывается только в sendNext() когда очередь пуста. Но что если:
    // Поток A вызывает placeOrder() → видит startSending_=false → запускает sendNext()
    // sendNext() забирает ордер, вызывает sendOrder() → async_write
    // В этот момент очередь пустеет, но async_write еще не завершился
    // Поток B вызывает placeOrder() с новым ордером → видит startSending_=true (еще не сброшен) → НЕ запускает отправку
    // on_write() вызывает sendNext(), который забирает новый ордер → все работает

    // Повторная проверка после сброса флага
    // startSending_.store(false, std::memory_order_release);

    // Если кто-то добавил ордер прямо перед сбросом флага,
    // он увидит startSending_=false и запустит отправку заново
    // Но для надежности - еще одна проверка:
    // if (!order_queue_cancel_.empty() ||
    //     !order_queue_replace_.empty() ||
    //     !order_queue_new_.empty()) {
    //     // Есть ордера, но флаг уже false - нужно перезапустить
    //     bool expected = false;
    //     if (startSending_.compare_exchange_strong(expected, true)) {
    //         sendNext();
    //     }
    // }

    return true;
}

// Обработчик завершения WebSocket handshake
void OrderSender::onHandshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка WebSocket handshake: " << ec.message() << std::endl;
        scheduleReconnect();
        return;
    }

    std::cout << "WebSocket соединение установлено!" << std::endl;

    // // Запускаем пинг-понг для поддержания соединения
    startPing();

    // // Начинаем читать сообщения
    doRead();

    // // Подписываемся на потоки данных Bybit
    authenticate();
}

// Обработчик отправки подписки
void OrderSender::onSubscribeSent(beast::error_code ec, std::size_t bytesTransferred) {
    if (ec) {
        std::cerr << "Ошибка отправки подписки: " << ec.message() << std::endl;
        scheduleReconnect();
        return;
    }

    std::cout << "Подписка отправлена, ожидаем данные..." << std::endl;
}

// Отправка подписки на потоки данных
void OrderSender::subscribeToStreams() {
    // Подписываемся на приватные каналы
    std::string sub_msg = R"({"op":"subscribe","args":["order","execution.fast
                          ","position","wallet"]})";
    // можно просто execution - больше данных, но медленнее.

    std::cout << "Отправляем подписку на приватные каналы: " << sub_msg << std::endl;

    auto self = static_cast<OrderSender *>(shared_from_this().get());
    ws_.async_write(net::buffer(sub_msg),
            [self](beast::error_code ec, std::size_t bytesTransferred) {
                self->onSubscribeSent(ec, bytesTransferred);
            });
}

// Асинхронное чтение сообщений
void OrderSender::doRead() {
    // LATENCY_MEASURE_START()
    // Буфер для хранения полученных данных
    buffer_.clear();

    // Асинхронно читаем сообщение
    auto self = static_cast<OrderSender *>(shared_from_this().get());
    ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t bytesTransferred) {
        self->onRead(ec, bytesTransferred);
    });
}

// Обработчик полученных сообщений
void OrderSender::onRead(beast::error_code ec, std::size_t bytesTransferred) {
    if (ec) {
        if (ec == websocket::error::closed) {
            std::cout << "WebSocket соединение закрыто нормально" << std::endl;
        } else if (ec == net::error::operation_aborted) {
            // Операция была отменена - вероятно, из-за закрытия соединения
            std::cout << "Операция чтения отменена" << std::endl;
            return;  // Не планируем переподключение, оно уже запланировано
        } else {
            std::cerr << "Ошибка чтения: " << ec.message() << std::endl;
        }

        scheduleReconnect();
        return;
    }

    // Преобразуем полученные данные в строку
    message_ = beast::buffers_to_string(buffer_.data());
    messageView_ = message_;

    try {

        // TODO есть еще ответ об успешном размещении ордера. Добавить обработку
        // {"retCode":0,"retMsg":"OK","op":"order.create","data":{"orderId":"xxx"}}
        std::cout << " получено сообщение " << messageView_ << std::endl;
        std::string_view retCode = getFieldValue("retCode", messageView_);
        std::string_view retMsg = getFieldValue("retMsg", messageView_);
        std::string_view connId = getFieldValue("connId", messageView_);

        if (retCode == "0" && retMsg == "OK") {
            statusMessage_.sucsess = true;
            statusMessage_.conId = connId;
            statusMessage_.operation = "auth";
            checkStatus();
        }
    } catch (const std::exception &e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        std::cout << "Сырые данные: " << messageView_ << std::endl;
    }

    // // LATENCY_MEASURE_END()
    // // READ_TIMER()
    // // Продолжаем чтение следующих сообщений
    doRead();
}

void OrderSender::checkStatus() {
    if (statusMessage_.operation == "auth") {
        if ((statusMessage_.sucsess == true)) {
            std::cout << "Аутентификация успешна!" << std::endl;
            authenticated_ = true;
            auth_timer_.cancel(); // Отменяем таймаут

            sendNext();
        } else {
            std::cerr << "Ошибка аутентификации!" << std::endl;
            scheduleReconnect();
            return;
        }
    } else {
        std::cout << "other operation: " << statusMessage_.operation << std::endl;
    }
}

void OrderSender::sendNext() {
    OrderRequest order;

    // Пытаемся забрать ордер - pop сам скажет, получилось или нет
    if (order_queue_cancel_.pop(order)) {
        sendOrder(order);
        return;
    }

    if (order_queue_replace_.pop(order)) {
        sendOrder(order);
        return;
    }

    if (order_queue_new_.pop(order)) {
        sendOrder(order);
        return;
    }

    // Очереди пусты - сбрасываем флаг
    startSending_.store(false, std::memory_order_release);
}

void OrderSender::sendOrder(const OrderRequest &order) {
    switch (order.typeOrderRequest) {
        case TypeOrderRequest_New:
            serialize_order(write_buffer_, order);
            break;
        case TypeOrderRequest_Cancel:
            serialize_cancel_order(write_buffer_, order);
            break;
        case TypeOrderRequest_Replace:
            serialize_replace_order(write_buffer_, order);
            break;
        default:
            std::cout << "OrderSender::sendOrder: Unknown order type" << std::endl;
            return;
    }

    std::cout << "отправка ордера " << write_buffer_ << std::endl;
    // у cancel и replace свои реализации.

    auto self = static_cast<OrderSender *>(shared_from_this().get());
    ws_.async_write(net::buffer(write_buffer_), [self](beast::error_code ec, std::size_t) {
        self->on_write(ec);
    });
}

void OrderSender::serialize_order(std::string &buffer, const OrderRequest &order) {
    buffer.clear();
    buffer.reserve(512); // Немного больше места для всех полей

    // Получаем текущий timestamp в миллисекундах
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                       .count();

    // Начало JSON: операция и req_id
    buffer += "{\"req_id\":";
    buffer += std::to_string(order.req_id);
    buffer += ",\"header\":{";
    buffer += "\"X-BAPI-TIMESTAMP\":\"";
    buffer += std::to_string(now);
    buffer += "\",\"X-BAPI-RECV-WINDOW\":\"5000\"";
    buffer += "},\"op\":\"order.create\",\"args\":[{";

    // Обязательные поля
    buffer += "\"category\":\"linear\","; // или "spot" для спота
    buffer += "\"symbol\":\"";
    buffer += order.symbol;
    buffer += "\",\"side\":\"";

    // Сторона ордера
    switch (order.side) {
        case Side_Buy:
            buffer += "Buy";
            break;
        case Side_Sell:
            buffer += "Sell";
            break;
        default:
            buffer += "Buy";
            break;
    }

    buffer += "\",\"orderType\":\"";

    // Тип ордера
    switch (order.order_type) {
        case OrderType_Limit:
            buffer += "Limit";
            break;
        case OrderType_Market:
            buffer += "Market";
            break;
        default:
            buffer += "Limit";
            break;
    }

    buffer += "\",\"qty\":\"";
    buffer += std::to_string((order.qty)); // TODO сделать более быстрый перевод double в строку
    buffer += "\"";

    // Цена (для лимитных ордеров)
    if (order.order_type != OrderType_Market && order.price > 0) {
        buffer += ",\"price\":\"";
        buffer += std::to_string(order.price);
        buffer += "\"";
    }

    // Плечо (КРИТИЧЕСКИ ВАЖНО для фьючерсов!)
    if (order.leverage > 0) {
        buffer += ",\"leverage\":\"";
        buffer += std::to_string(order.leverage);
        buffer += "\"";
    }

    // Time in Force (опционально, но рекомендуется)
    buffer += ",\"timeInForce\":\"GTC\""; // GTC, IOC, FOK, PostOnly

    // positionIdx - для хедж-режима (0 = one-way, 1 = buy side, 2 = sell side)
    buffer += ",\"positionIdx\":0";

    // Уникальный ID ордера для трекинга (очень полезно)
    if (order.req_id != 0) {
        buffer += ",\"orderLinkId\":\"";
        buffer += std::to_string(order.req_id);
        buffer += "\"";
    }

    // Reduce only (опционально)
    // buffer += ",\"reduceOnly\":false";

    // Close on trigger (опционально)
    // buffer += ",\"closeOnTrigger\":false";

    buffer += "}]}";
}

void OrderSender::serialize_cancel_order(std::string &buffer, const OrderRequest &order) {
    buffer.clear();
    buffer.reserve(256);

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                       .count();

    // Начало JSON: операция и req_id
    buffer += "{\"req_id\":";
    buffer += std::to_string(order.req_id);
    buffer += ",\"header\":{";
    buffer += "\"X-BAPI-TIMESTAMP\":\"";
    buffer += std::to_string(now);
    buffer += "\",\"X-BAPI-RECV-WINDOW\":\"5000\"";
    buffer += "},\"op\":\"order.cancel\"";
    buffer += ",\"args\":[{\"category\":\"linear\",\"symbol\":\"";
    buffer += order.symbol;
    buffer += "\",\"orderId\":\"";
    buffer += order.order_id; // Тот самый ID от биржи!
    buffer += "\"}]}";
}

void OrderSender::serialize_replace_order(std::string &buffer, const OrderRequest &order) {
    buffer.clear();
    buffer.reserve(512);

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                       .count();

    buffer += "{\"req_id\":";
    buffer += std::to_string(order.req_id);
    buffer += ",\"header\":{";
    buffer += "\"X-BAPI-TIMESTAMP\":\"";
    buffer += std::to_string(now);
    buffer += "\",\"X-BAPI-RECV-WINDOW\":\"5000\"";
    buffer += "},\"op\":\"order.amend\"";
    buffer += ",\"args\":[{\"category\":\"linear\",\"symbol\":\"";
    buffer += order.symbol;
    buffer += "\",\"orderId\":\"";
    buffer += order.order_id; // ID ордера от биржи
    buffer += "\"";

    if (order.price > 0) {
        buffer += ",\"price\":\"";
        buffer += std::to_string(order.price);
        buffer += "\"";
    }

    if (order.qty > 0) {
        buffer += ",\"qty\":\"";
        buffer += std::to_string(order.qty);
        buffer += "\"";
    }

    buffer += "}]}";
}

void OrderSender::on_write(beast::error_code ec) {
    if (ec) {
        std::cerr << "Order send error: " << ec.message() << std::endl;
        startSending_.store(false, std::memory_order_release);
        scheduleReconnect();
    }

    std::cout << "ордер отправлен" << std::endl;

    // Продолжаем с очередью
    sendNext();
}
