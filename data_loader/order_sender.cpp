#include "order_sender.h"
#include "p999_latency/check_latency.h"

DECLARE_LATENCY_MEMBERS(1000)

namespace {

const std::string_view retCodeFieldName = "retCode";

}

OrderSender::OrderSender(net::io_context &ioc, ssl::context &ssl_ctx,
        const std::string &api_key, const std::string &api_secret) :
PrivateConnector(ioc, ssl_ctx, api_key, api_secret,  "Bybit-HFT-OrderSender/1.0") {
}

// Обработчик завершения WebSocket handshake
void OrderSender::on_handshake(beast::error_code ec) {
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
void OrderSender::on_subscribe_sent(beast::error_code ec,
        std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Ошибка отправки подписки: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "Подписка отправлена, ожидаем данные..." << std::endl;
}

// Отправка подписки на потоки данных
void OrderSender::subscribe_to_streams() {
    // Подписываемся на приватные каналы
    std::string sub_msg = R"({"op":"subscribe","args":["order","execution.fast","position","wallet"]})";
    // можно просто execution - больше данных, но медленнее.

    std::cout << "Отправляем подписку на приватные каналы: " << sub_msg << std::endl;

    auto self = shared_from_this();
    ws_.async_write(net::buffer(sub_msg),
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                auto ptr = static_cast<OrderSender *>(self.get());
                ptr->on_subscribe_sent(ec, bytes_transferred);
            });
}

// Асинхронное чтение сообщений
void OrderSender::do_read() {
    // LATENCY_MEASURE_START()
    // Буфер для хранения полученных данных
    buffer_.clear();

    // Асинхронно читаем сообщение
    auto self = shared_from_this();
    ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t bytes_transferred) {
        auto ptr = static_cast<OrderSender *>(self.get());
        ptr->on_read(ec, bytes_transferred);
    });
}

// Обработчик полученных сообщений
void OrderSender::on_read(beast::error_code ec, std::size_t bytes_transferred) {
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
        std::string_view retCode = getFieldValue("retCode", message_view_);
        std::string_view retMsg = getFieldValue("retMsg", message_view_);
        std::string_view connId = getFieldValue("connId", message_view_);

        if (retCode == "0" && retMsg == "OK") {
            statusMessage_.sucsess = true;
            statusMessage_.conId = connId;
            statusMessage_.operation = "auth";
            checkStatus();
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

void OrderSender::checkStatus() {
    if (statusMessage_.operation == "auth") {
        if ((statusMessage_.sucsess == true)) {
            std::cout << "Аутентификация успешна!" << std::endl;
            authenticated_ = true;
            auth_timer_.cancel(); // Отменяем таймаут

            // // ВОТ ЗДЕСЬ - подписываемся после успешной аутентификации!
            // subscribe_to_streams();
        } else {
            std::cerr << "Ошибка аутентификации!" << std::endl;
            schedule_reconnect();
            return;
        }
    } else {
        std::cout << "other operation: " << statusMessage_.operation << std::endl;
    }
}
