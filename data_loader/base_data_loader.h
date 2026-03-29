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

// Класс WebSocket клиента для Bybit
class BaseWebSocketClient : public std::enable_shared_from_this<BaseWebSocketClient> {
public:
    // Конструктор: инициализируем все необходимые компоненты
    BaseWebSocketClient(net::io_context &ioc, ssl::context &ssl_ctx);

    // Основной метод для запуска подключения
    void connect(const std::string &host, const std::string &port, const std::string &target);

protected:
    // Обработчик результата DNS резолвинга
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);

    // Обработчик успешного TCP подключения
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type);

    // Обработчик завершения SSL handshake
    void on_ssl_handshake(beast::error_code ec);

    // Обработчик завершения WebSocket handshake
    virtual void on_handshake(beast::error_code ec) = 0;

    // Отправка подписки на потоки данных
    virtual void subscribe_to_streams() = 0;

    // Обработчик отправки подписки
    virtual void on_subscribe_sent(beast::error_code ec, std::size_t bytes_transferred) = 0;

    // Асинхронное чтение сообщений
    virtual void do_read() = 0;

    // Обработчик полученных сообщений
    virtual void on_read(beast::error_code ec, std::size_t bytes_transferred) =0;

    // Запуск периодического PING для поддержания соединения
    void start_ping();

    // Обработчик таймера PING
    void on_ping_timer(beast::error_code ec);

    // Обработчик отправки PING
    void on_ping_sent(beast::error_code ec);

    // Планирование переподключения при ошибке
    void schedule_reconnect();

    // Обработчик таймера переподключения
    void on_reconnect_timer(beast::error_code ec);

protected:
    tcp::resolver resolver_; // DNS резолвер
    websocket::stream<ssl::stream<beast::tcp_stream> > ws_; // WebSocket поток с SSL
    net::steady_timer reconnect_timer_; // Таймер для переподключения
    net::steady_timer ping_timer_; // Таймер для ping
    net::io_context &ioc_; // Ссылка на io_context
    beast::flat_buffer buffer_; // Буфер для чтения данных
    TypeMessage typeMessage_;
    std::string host_; // Хост для переподключения
    std::string port_; // Порт для переподключения
    std::string target_; // WebSocket путь
    std::string last_price_; // Последняя полученная цена
    std::string message_;
    std::string_view message_view_;
    std::chrono::steady_clock::time_point ping_sent_time_;

    // Метод для измерения и логирования пинга
    void measure_latency(std::chrono::steady_clock::time_point sent_time);

    void on_control_frame(websocket::frame_type kind, beast::string_view payload);
};
