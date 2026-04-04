#include "private_data_loader.h"
#include "p999_latency/check_latency.h"

DECLARE_LATENCY_MEMBERS(1000)

PrivateWebSocketClient::Messages::Messages(PositionHFT *const positionHFT_,
        ExecutionFast *const executionFast_, OrderHFT *const orderHFT_,
        WalletHFT *const walletHFT_) :
positionHFT(positionHFT_),
executionFast(executionFast_),
orderHFT(orderHFT_),
walletHFT(walletHFT_) {
}

PrivateWebSocketClient::PrivateWebSocketClient(net::io_context &ioc, ssl::context &ssl_ctx,
        const std::string &api_key, const std::string &api_secret, const Messages &messages) :
BaseWebSocketClient(ioc, ssl_ctx),
statusMessage_(),
statusParser_(&statusMessage_),
positionJsonParser_(messages.positionHFT),
orderJsonParser_(messages.orderHFT),
walletJsonParser_(messages.walletHFT),
executionFastJsonParser_(messages.executionFast),
api_key_(api_key),
api_secret_(api_secret),
authenticated_(false),
auth_timer_(ioc) {
}

// Обработчик завершения WebSocket handshake
void PrivateWebSocketClient::on_handshake(beast::error_code ec) {
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

std::string PrivateWebSocketClient::generate_signature(long long expires,
        const std::string &api_secret) {
    std::string message = "GET/realtime" + std::to_string(expires);

    // HMAC-SHA256 подпись
    unsigned char *digest;
    digest = HMAC(EVP_sha256(), api_secret.c_str(), api_secret.length(),
            reinterpret_cast<const unsigned char *>(message.c_str()), message.length(), nullptr,
            nullptr);

    // Конвертируем в hex строку
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }

    return ss.str();
}

void PrivateWebSocketClient::authenticate() {
    // Генерируем expires (текущее время + 30 секунд)
    auto now = std::chrono::system_clock::now();
    auto expires =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() +
            30000;

    std::string signature = generate_signature(expires, api_secret_);

    // Формируем сообщение аутентификации
    std::string auth_msg = R"({"op":"auth","args":[")" +
                       api_key_ + R"(")" + "," +
                       std::to_string(expires) + ",\"" +
                       signature + "\"]}";

    std::cout << "Отправляем аутентификацию..." << std::endl;

    // Отправляем аутентификацию
    auto self = shared_from_this();
    ws_.async_write(net::buffer(auth_msg), [self](beast::error_code ec, std::size_t bytes) {
        static_cast<PrivateWebSocketClient *>(self.get())->on_auth_response(ec, bytes);
    });
}

void PrivateWebSocketClient::on_auth_response(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Ошибка отправки аутентификации: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "Аутентификация отправлена, ожидаем подтверждения..." << std::endl;

    // Запускаем таймер на ожидание ответа аутентификации (5 секунд)
    auth_timer_.expires_after(std::chrono::seconds(5));
    auth_timer_.async_wait([this](beast::error_code ec) {
        if (!ec && !authenticated_) {
            std::cerr << "Таймаут аутентификации" << std::endl;
            schedule_reconnect();
        }
    });
}

// Обработчик отправки подписки
void PrivateWebSocketClient::on_subscribe_sent(beast::error_code ec,
        std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Ошибка отправки подписки: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "Подписка отправлена, ожидаем данные..." << std::endl;
}

// Отправка подписки на потоки данных
void PrivateWebSocketClient::subscribe_to_streams() {
    // Подписываемся на приватные каналы
    std::string sub_msg = R"({"op":"subscribe","args":["order","execution.fast","position","wallet"]})";
    // можно просто execution - больше данных, но медленнее.

    std::cout << "Отправляем подписку на приватные каналы: " << sub_msg << std::endl;

    auto self = shared_from_this();
    ws_.async_write(net::buffer(sub_msg),
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                auto ptr = static_cast<PrivateWebSocketClient *>(self.get());
                ptr->on_subscribe_sent(ec, bytes_transferred);
            });
}

// Асинхронное чтение сообщений
void PrivateWebSocketClient::do_read() {
    // LATENCY_MEASURE_START()
    // Буфер для хранения полученных данных
    buffer_.clear();

    // Асинхронно читаем сообщение
    auto self = shared_from_this();
    ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t bytes_transferred) {
        auto ptr = static_cast<PrivateWebSocketClient *>(self.get());
        ptr->on_read(ec, bytes_transferred);
    });
}

// Обработчик полученных сообщений
void PrivateWebSocketClient::on_read(beast::error_code ec, std::size_t bytes_transferred) {
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

void PrivateWebSocketClient::checkStatus() {
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
