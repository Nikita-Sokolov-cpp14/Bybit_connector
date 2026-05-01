#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include "json_parser/orderbook_json_parser.h"
#include "json_parser/status_json_parser.h"
#include "json_parser/public_trade_json_parser.h"
#include "data_structures/orderbook.h"
#include "data_structures/status_message.h"
#include "data_structures/public_trade.h"

// Для установки nlohmann/json в Ubuntu: sudo apt install nlohmann-json-dev
// В других системах можно скачать с https://github.com/nlohmann/json

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

/**
 * @brief Базовый класс для создания websocket соединения.
 * @details Наследуется от enable_shared_from_this, чтобы работали асинхронные операции
 * (колбэки).
 */
class BaseWebSocketClient : public std::enable_shared_from_this<BaseWebSocketClient> {
public:
    /**
     * @brief Конструктор
     * @param ioc Основной обработчик (планировщик) задач io_context.
     * @param sslCtx Защищенный поток SSL.
     * @param userAgent Заголовок всех пакетов.
     */
    BaseWebSocketClient(net::io_context &ioc, ssl::context &sslCtx,
            const std::string_view userAgent);

    ~BaseWebSocketClient();

    /**
     * @brief Подключиться с парметрами.
     * @details Основной метод подключения.
     * @param host Хост
     * @param port Порт.
     * @param target Таргет.
     */
    void connect(const std::string &host, const std::string &port, const std::string &target);

    // Устанавливаем колбэк на переподключение
    void setReconnectCallback(std::function<void()> callback);

protected:
    /**
     * @brief Обработчик результатов DNS.
     * @details Передается в async_resolve как колбэк.
     * @param ec Код ошибки.
     * @param results Результаты.
     */
    void onResolve(beast::error_code ec, tcp::resolver::results_type results);

    /**
     * @brief Обработчик успешного TCP подключения.
     * @details Передается в async_resolve как колбэк.
     * @param ec Код ошибки.
     * @param tcp::resolver::results_type::endpoint_type ?
     */
    void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type);

    /**
     * @brief Обработчик завершения SSL handshake.
     * @details Передается в async_resolve как колбэк.
     * @param ec Код ошибки.
     */
    void onSslHandshake(beast::error_code ec);

    /**
     * @brief Обработчик завершения WebSocket handshake.
     * @details Передается в async_resolve как колбэк.
     * @param ec Код ошибки.
     */
    virtual void onHandshake(beast::error_code ec) = 0;

    /**
     * @brief Отправить подписку на потоки данных.
     */
    virtual void subscribeToStreams() = 0;

    /**
     * @brief Обработчик отправки подписки.
     * @param ec Код ошибки.
     * @param bytesTransferred Число полученных байт.
     */
    virtual void onSubscribeSent(beast::error_code ec, std::size_t bytesTransferred) = 0;

    /**
     * @brief Асинхронно прочитать сообщение.
     */
    virtual void doRead() = 0;

    /**
     * @brief Обработчик полученных сообщений.
     * @param ec Код ошибки.
     * @param bytesTransferred Число полученных байт.
     */
    virtual void onRead(beast::error_code ec, std::size_t bytesTransferred) = 0;

    /**
     * @brief Запуск периодического PING для поддержания соединения.
     */
    void startPing();

    /**
     * @brief бработчик таймера PING.
     * @param ec Код ошибки.
     */
    void onPingTimer(beast::error_code ec);

    /**
     * @brief Обработчик отправки PING.
     * @param ec Код ошибки.
     */
    void onPingSent(beast::error_code ec);

    /**
     * @brief Планирование переподключения при ошибке.
     */
    void scheduleReconnect();

protected:
    tcp::resolver resolver_; // DNS резолвер
    websocket::stream<ssl::stream<beast::tcp_stream> > ws_; // WebSocket поток с SSL
    net::steady_timer reconnectTimer_; // Таймер для переподключения
    net::steady_timer pingTimer_; // Таймер для ping
    net::io_context &ioc_; // Ссылка на io_context
    beast::flat_buffer buffer_; // Буфер для чтения данных
    TypeMessage typeMessage_; //!< Тип сообщения.
    std::string host_; // Хост для переподключения
    std::string port_; // Порт для переподключения
    std::string target_; // WebSocket путь
    std::string lastPrice_; // Последняя полученная цена
    std::string message_; //!< Полученное сообщение.
    std::string_view messageView_; //!< Полученно
    std::chrono::steady_clock::time_point pingSentTime_;
    std::atomic<bool> isWaitPing_; //!< Ожидается ли ответ на пинг.
    std::atomic<bool> isReconnecting_ {false};

    std::function<void()> reconnectCallback_; // Колбэк для переподключения

    /**
     * @brief Вычислить пинг.
     * @param sentTime Время отправки пинг запроса на сервер.
     */
    void measureLatency(std::chrono::steady_clock::time_point sentTime);

    /**
     * @brief Обработчик управляющего фрейма.
     * @details Вызывается каждый раз, когда приходит управляющий фрейм (PING/PONG/CLOSE).
     * @param kind Тип.
     * @param payload Строка с ответом.
     */
    void onControlFrame(websocket::frame_type kind, beast::string_view payload);

    void onClose(beast::error_code ec);
};
