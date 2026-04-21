#include "private_data_handler.h"
#include "p999_latency/check_latency.h"

DECLARE_LATENCY_MEMBERS(1000)

PrivateDataHandler::Messages::Messages(PositionHFT *const positionHFT_,
        ExecutionFast *const executionFast_, OrderHFT *const orderHFT_,
        WalletHFT *const walletHFT_) :
positionHFT(positionHFT_),
executionFast(executionFast_),
orderHFT(orderHFT_),
walletHFT(walletHFT_) {
}

PrivateDataHandler::PrivateDataHandler(net::io_context &ioc, ssl::context &sslCtx,
        const std::string &api_key, const std::string &api_secret, const Messages &messages,
        const std::string_view userAgent) :
PrivateConnector(ioc, sslCtx, api_key, api_secret, userAgent),
positionJsonParser_(messages.positionHFT),
orderJsonParser_(messages.orderHFT),
walletJsonParser_(messages.walletHFT),
executionFastJsonParser_(messages.executionFast) {
}

// Обработчик завершения WebSocket handshake
void PrivateDataHandler::onHandshake(beast::error_code ec) {
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
void PrivateDataHandler::onSubscribeSent(beast::error_code ec, std::size_t bytesTransferred) {
    if (ec) {
        std::cerr << "Ошибка отправки подписки: " << ec.message() << std::endl;
        scheduleReconnect();
        return;
    }

    std::cout << "Подписка отправлена, ожидаем данные..." << std::endl;
}

// Отправка подписки на потоки данных
void PrivateDataHandler::subscribeToStreams() {
    // Подписываемся на приватные каналы
    std::string sub_msg = R"({"op":"subscribe","args":["order","execution.fast","position","wallet"]})";
    // можно просто execution - больше данных, но медленнее.

    std::cout << "Отправляем подписку на приватные каналы: " << sub_msg << std::endl;

    auto self = static_cast<PrivateDataHandler *>(shared_from_this().get());
    ws_.async_write(net::buffer(sub_msg),
            [self](beast::error_code ec, std::size_t bytesTransferred) {
                self->onSubscribeSent(ec, bytesTransferred);
            });
}

// Асинхронное чтение сообщений
void PrivateDataHandler::doRead() {
    // LATENCY_MEASURE_START()
    // Буфер для хранения полученных данных
    buffer_.clear();

    // Асинхронно читаем сообщение
    auto self = static_cast<PrivateDataHandler *>(shared_from_this().get());
    ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t bytesTransferred) {
        self->onRead(ec, bytesTransferred);
    });
}

// Обработчик полученных сообщений
void PrivateDataHandler::onRead(beast::error_code ec, std::size_t bytesTransferred) {
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
        typeMessage_ = parseTypeMessage(messageView_.substr(0, maxTypeStrLen));
        switch (typeMessage_) {
            case TypeMessage_Status:
                statusParser_.setString(messageView_);
                statusParser_.parse();
                checkStatus();
                break;
            case TypeMessage_Position:
                positionJsonParser_.setString(messageView_);
                positionJsonParser_.parse();
                positionJsonParser_.printData();
                break;
            case TypeMessage_Order:
                orderJsonParser_.setString(messageView_);
                orderJsonParser_.parse();
                orderJsonParser_.printData();
                break;
            case TypeMessage_ExecutionFast:
                executionFastJsonParser_.setString(messageView_);
                executionFastJsonParser_.parse();
                executionFastJsonParser_.printData();
                break;
            case TypeMessage_Wallet:
                walletJsonParser_.setString(messageView_);
                walletJsonParser_.parse();
                walletJsonParser_.printData();
                break;
            default:
                std::cout << "PrivateDataHandler::onRead: Unknown message type " << messageView_
                          << std::endl;
                std::cout << messageView_ << std::endl;
                break;
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

void PrivateDataHandler::checkStatus() {
    if (statusMessage_.operation == "auth") {
        if ((statusMessage_.sucsess == true)) {
            std::cout << "Аутентификация успешна!" << std::endl;
            authenticated_ = true;
            auth_timer_.cancel(); // Отменяем таймаут

            // ВОТ ЗДЕСЬ - подписываемся после успешной аутентификации!
            subscribeToStreams();
        } else {
            std::cerr << "Ошибка аутентификации!" << std::endl;
            scheduleReconnect();
            return;
        }
    } else if (statusMessage_.operation == "subscribe") {
        if ((statusMessage_.sucsess == true)) {
            std::cout << "Подписка успешна!" << std::endl;
        } else {
            std::cerr << "Ошибка подписи на данные!" << std::endl;
            scheduleReconnect();
            return;
        }
    } else {
        std::cout << "other operation: " << statusMessage_.operation << std::endl;
    }
}
