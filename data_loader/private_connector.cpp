#include "private_connector.h"
#include "p999_latency/check_latency.h"

DECLARE_LATENCY_MEMBERS(1000)

PrivateConnector::PrivateConnector(net::io_context &ioc, ssl::context &ssl_ctx,
        const std::string &api_key, const std::string &api_secret) :
BaseWebSocketClient(ioc, ssl_ctx),
statusMessage_(),
statusParser_(&statusMessage_),
api_key_(api_key),
api_secret_(api_secret),
authenticated_(false),
auth_timer_(ioc) {
}

std::string PrivateConnector::generate_signature(long long expires,
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

void PrivateConnector::authenticate() {
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
        static_cast<PrivateConnector *>(self.get())->on_auth_response(ec, bytes);
    });
}

void PrivateConnector::on_auth_response(beast::error_code ec, std::size_t bytes_transferred) {
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
