#pragma once
#include <string>

struct AuthConfig {
    std::string apiKey;
    std::string apiSecret;
};

struct ConnectionConfig {
    std::string host;
    std::string port;
    std::string targetPublic;
    std::string targetPrivate;
    std::string targetTrade;
};
