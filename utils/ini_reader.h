#pragma once

#include "config.h"

/**
* @brief Загрузить данные из файла конфигурации для авторизации.
* @return Структура с данными авторизации.
*/
AuthConfig readAuthConfig();

/**
* @brief Загрузить данные из файла конфигурации для подключения к бирже.
* @return Структура с данными подключения к бирже
*/
ConnectionConfig readConnectionConfig();
