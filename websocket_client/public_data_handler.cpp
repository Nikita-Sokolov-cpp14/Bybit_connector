#include "public_data_handler.h"
#include "p999_latency/check_latency.h"

DECLARE_LATENCY_MEMBERS(1000)

PublicDataHandler::PublicDataHandler(net::io_context &ioc, ssl::context &ssl_ctx,
        OrderBook *const orderBook, StatusMessage *const statusMessage,
        PublicTrade *const publicTrade, const std::string_view user_agent) :
BaseWebSocketClient(ioc, ssl_ctx, user_agent),
orderbookParser_(orderBook),
statusParser_(statusMessage),
publicTradeJsonParser_(publicTrade) {
}

// Обработчик завершения WebSocket handshake
void PublicDataHandler::on_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка WebSocket handshake: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "WebSocket соединение установлено!" << std::endl;
    // Подписываемся на потоки данных Bybit
    subscribe_to_streams();

    // Запускаем пинг-понг для поддержания соединения
    start_ping();

    // Начинаем читать сообщения
    do_read();
}

// Отправка подписки на потоки данных
void PublicDataHandler::subscribe_to_streams() {
    // constexpr - строка известна на этапе компиляции
    static constexpr char subscription_msg[] = "{\"op\":\"subscribe\",\"args\":["
                                               "\"orderbook.50.BTCUSDT\","
                                               "\"publicTrade.BTCUSDT\""
                                               "]}";
    // .глубина. 1 - каждые 10 мс, 50 - каждые 20 мс, 200 - каждые 100мс, 500 - каждые 200

    std::cout << "Отправляем подписку: " << subscription_msg << std::endl;

    auto self = static_cast<PublicDataHandler*>(shared_from_this().get());
    ws_.async_write(net::buffer(subscription_msg), [self](beast::error_code ec, std::size_t bytes) {
        self->on_subscribe_sent(ec, bytes);
    });
}

// Обработчик отправки подписки
void PublicDataHandler::on_subscribe_sent(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Ошибка отправки подписки: " << ec.message() << std::endl;
        schedule_reconnect();
        return;
    }

    std::cout << "Подписка отправлена, ожидаем данные..." << std::endl;
}

// Асинхронное чтение сообщений
void PublicDataHandler::do_read() {
    // LATENCY_MEASURE_START()
    // Буфер для хранения полученных данных
    buffer_.clear();

    // Асинхронно читаем сообщение
    auto self = static_cast<PublicDataHandler*>(shared_from_this().get());
    ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t bytes_transferred) {
        self->on_read(ec, bytes_transferred);
    });
}

// Обработчик полученных сообщений
void PublicDataHandler::on_read(beast::error_code ec, std::size_t bytes_transferred) {
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
            case TypeMessage_Orderbook:
                orderbookParser_.setString(message_view_);
                orderbookParser_.parse();
                break;
            case TypeMessage_PublicTrade:
                publicTradeJsonParser_.setString(message_view_);
                publicTradeJsonParser_.parse();
                break;
            case TypeMessage_Status:
                statusParser_.setString(message_view_);
                statusParser_.parse();
                break;
            default:
                std::cout << "PublicDataHandler::on_read: Unknown message type " << message_view_ << std::endl;
                // std::cout << message_view_ << std::endl;
                break;
        }
    } catch (const std::exception &e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        std::cout << "Сырые данные: " << message_view_ << std::endl;
    }

    // LATENCY_MEASURE_END()
    // READ_TIMER()
    // Продолжаем чтение следующих сообщений
    do_read();
}
