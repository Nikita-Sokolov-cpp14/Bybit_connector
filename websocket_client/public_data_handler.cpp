#include "public_data_handler.h"
#include "p999_latency/check_latency.h"

DECLARE_LATENCY_MEMBERS(1000)

PublicDataHandler::PublicDataHandler(net::io_context &ioc, ssl::context &sslCtx,
        OrderBook *const orderBook, StatusMessage *const statusMessage,
        PublicTrade *const publicTrade, const std::string_view userAgent) :
BaseWebSocketClient(ioc, sslCtx, userAgent),
orderbookParser_(orderBook),
statusParser_(statusMessage),
publicTradeJsonParser_(publicTrade) {
}

// Обработчик завершения WebSocket handshake
void PublicDataHandler::onHandshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ошибка WebSocket handshake: " << ec.message() << std::endl;
        scheduleReconnect();
        return;
    }

    std::cout << "WebSocket соединение установлено!" << std::endl;
    // Подписываемся на потоки данных Bybit
    subscribeToStreams();

    // Запускаем пинг-понг для поддержания соединения
    startPing();

    // Начинаем читать сообщения
    doRead();
}

// Отправка подписки на потоки данных
void PublicDataHandler::subscribeToStreams() {
    // constexpr - строка известна на этапе компиляции
    static constexpr char subscription_msg[] = "{\"op\":\"subscribe\",\"args\":["
                                               "\"orderbook.50.BTCUSDT\","
                                               "\"publicTrade.BTCUSDT\""
                                               "]}";
    // .глубина. 1 - каждые 10 мс, 50 - каждые 20 мс, 200 - каждые 100мс, 500 - каждые 200

    std::cout << "Отправляем подписку: " << subscription_msg << std::endl;

    auto self = static_cast<PublicDataHandler *>(shared_from_this().get());
    ws_.async_write(net::buffer(subscription_msg),
            [self](beast::error_code ec, std::size_t bytes) { self->onSubscribeSent(ec, bytes); });
}

// Обработчик отправки подписки
void PublicDataHandler::onSubscribeSent(beast::error_code ec, std::size_t bytesTransferred) {
    if (ec) {
        std::cerr << "Ошибка отправки подписки: " << ec.message() << std::endl;
        scheduleReconnect();
        return;
    }

    std::cout << "Подписка отправлена, ожидаем данные..." << std::endl;
}

// Асинхронное чтение сообщений
void PublicDataHandler::doRead() {
    // LATENCY_MEASURE_START()
    // Буфер для хранения полученных данных
    buffer_.clear();

    // Асинхронно читаем сообщение
    auto self = static_cast<PublicDataHandler *>(shared_from_this().get());
    ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t bytesTransferred) {
        self->onRead(ec, bytesTransferred);
    });
}

// Обработчик полученных сообщений
void PublicDataHandler::onRead(beast::error_code ec, std::size_t bytesTransferred) {
    if (ec) {
        if (ec == websocket::error::closed) {
            std::cout << "onRead: WebSocket соединение закрыто нормально" << std::endl;
        } else if (ec == net::error::operation_aborted) {
            // Операция была отменена - вероятно, из-за закрытия соединения
            std::cout << "onRead: Операция чтения отменена" << std::endl;
            return;  // Не планируем переподключение, оно уже запланировано
        } else {
            std::cerr << "onRead: Ошибка чтения: " << ec.message() << std::endl;
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
            case TypeMessage_Orderbook:
                orderbookParser_.setString(messageView_);
                orderbookParser_.parse();
                break;
            case TypeMessage_PublicTrade:
                publicTradeJsonParser_.setString(messageView_);
                publicTradeJsonParser_.parse();
                break;
            case TypeMessage_Status:
                statusParser_.setString(messageView_);
                statusParser_.parse();
                break;
            default:
                std::cout << "PublicDataHandler::onRead: Unknown message type " << messageView_
                          << std::endl;
                // std::cout << messageView_ << std::endl;
                break;
        }
    } catch (const std::exception &e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        std::cout << "Сырые данные: " << messageView_ << std::endl;
    }

    // LATENCY_MEASURE_END()
    // READ_TIMER()
    // Продолжаем чтение следующих сообщений
    doRead();
}
