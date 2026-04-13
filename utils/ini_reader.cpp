#include "ini_reader.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <exception>
#include <iostream>
#include <string>

#include "limits/limits.h"

AuthConfig readAuthConfig() {
    AuthConfig authConfig;
    boost::property_tree::ptree pt;

    try {
        boost::property_tree::read_ini(authConfigPath, pt);
        authConfig.apiSecret = pt.get<std::string>("Auth.api_secret");
        authConfig.apiKey = pt.get<std::string>("Auth.api_key");
    } catch (const std::exception &e) {
        std::cerr << "readAuthConfig: error: " << e.what() << std::endl;
    }

    return authConfig;
}

ConnectionConfig readConnectionConfig() {
    ConnectionConfig connectionConfig;
    boost::property_tree::ptree pt;

    try {
        boost::property_tree::read_ini(connectionConfigPath, pt);
        connectionConfig.host = pt.get<std::string>("Connection.host");
        connectionConfig.port = pt.get<std::string>("Connection.port");
        connectionConfig.targetPublic = pt.get<std::string>("Connection.targetPublic");
        connectionConfig.targetPrivate = pt.get<std::string>("Connection.targetPrivate");
        connectionConfig.targetTrade = pt.get<std::string>("Connection.targetTrade");
    } catch (const std::exception &e) {
        std::cerr << "readConnectionConfig: error: " << e.what() << std::endl;
    }

    return connectionConfig;
}
