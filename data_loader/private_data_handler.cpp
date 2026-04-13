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

PrivateDataHandler::PrivateDataHandler(net::io_context &ioc, ssl::context &ssl_ctx,
        const std::string &api_key, const std::string &api_secret, const Messages &messages) :
PrivateConnector(ioc, ssl_ctx, api_key, api_secret, "Bybit-PrivateData/1.0"),
positionJsonParser_(messages.positionHFT),
orderJsonParser_(messages.orderHFT),
walletJsonParser_(messages.walletHFT),
executionFastJsonParser_(messages.executionFast) {
}

// Обработчик завершения WebSocket handshake
void PrivateDataHandler::on_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка WebSocket handshake: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "WebSocket соединение установлено!" << std::endl;

    // // Запускаем пинг-понг для поддержания соединения
    // start_ping();

    // // Начинаем читать сообщения
    do_read();

    // // Подписываемся на потоки данных Bybit
    authenticate();
}

// Обработчик отправки подписки
void PrivateDataHandler::on_subscribe_sent(beast::error_code ec,
        std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Ошибка отправки подписки: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "Подписка отправлена, ожидаем данные..." << std::endl;
}

// Отправка подписки на потоки данных
void PrivateDataHandler::subscribe_to_streams() {
    // Подписываемся на приватные каналы
    std::string sub_msg = R"({"op":"subscribe","args":["order","execution.fast","position","wallet"]})";
    // можно просто execution - больше данных, но медленнее.

    std::cout << "Отправляем подписку на приватные каналы: " << sub_msg << std::endl;

    auto self = static_cast<PrivateDataHandler*>(shared_from_this().get());
    ws_.async_write(net::buffer(sub_msg),
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                self->on_subscribe_sent(ec, bytes_transferred);
            });
}

// Асинхронное чтение сообщений
void PrivateDataHandler::do_read() {
    // LATENCY_MEASURE_START()
    // Буфер для хранения полученных данных
    buffer_.clear();

    // Асинхронно читаем сообщение
    auto self = static_cast<PrivateDataHandler*>(shared_from_this().get());
    ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t bytes_transferred) {
        self->on_read(ec, bytes_transferred);
    });
}

// Обработчик полученных сообщений
void PrivateDataHandler::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == websocket::error::closed) {
            std::cout << "WebSocket соединение закрыто нормально" << std::endl;
        } else {
            std::cerr << "Ошибка чтения: " << ec.message() << std::endl;
        }

        schedule_reconnect();
        return;
    }

    // Преобразуем полученные данные в строку
    message_ = beast::buffers_to_string(buffer_.data());
    message_view_ = message_;

    try {
        typeMessage_ = parseTypeMessage(message_view_.substr(0, maxTypeStrLen));
        switch (typeMessage_) {
            case TypeMessage_Status:
                statusParser_.setString(message_view_);
                statusParser_.parse();
                checkStatus();
                break;
            case TypeMessage_Position:
                positionJsonParser_.setString(message_view_);
                positionJsonParser_.parse();
                positionJsonParser_.printData();
                break;
            case TypeMessage_Order:
                orderJsonParser_.setString(message_view_);
                orderJsonParser_.parse();
                orderJsonParser_.printData();
                break;
            case TypeMessage_ExecutionFast:
                executionFastJsonParser_.setString(message_view_);
                executionFastJsonParser_.parse();
                executionFastJsonParser_.printData();
                break;
            case TypeMessage_Wallet:
                walletJsonParser_.setString(message_view_);
                walletJsonParser_.parse();
                walletJsonParser_.printData();
                break;
            default:
                std::cout << "BybitWebSocketClient::on_read: Unknown message type" << std::endl;
                std::cout << message_view_ << std::endl;
                break;
        }
    } catch (const std::exception &e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        std::cout << "Сырые данные: " << message_view_ << std::endl;
    }

    // // LATENCY_MEASURE_END()
    // // READ_TIMER()
    // // Продолжаем чтение следующих сообщений
    do_read();
}

void PrivateDataHandler::checkStatus() {
    if (statusMessage_.operation == "auth") {
        if ((statusMessage_.sucsess == true)) {
            std::cout << "Аутентификация успешна!" << std::endl;
            authenticated_ = true;
            auth_timer_.cancel(); // Отменяем таймаут

            // ВОТ ЗДЕСЬ - подписываемся после успешной аутентификации!
            subscribe_to_streams();
        } else {
            std::cerr << "Ошибка аутентификации!" << std::endl;
            schedule_reconnect();
            return;
        }
    } else if (statusMessage_.operation == "subscribe") {
        if ((statusMessage_.sucsess == true)) {
            std::cout << "Подписка успешна!" << std::endl;
        } else {
            std::cerr << "Ошибка подписи на данные!" << std::endl;
            schedule_reconnect();
            return;
        }
    } else {
        std::cout << "other operation: " << statusMessage_.operation << std::endl;
    }
}
