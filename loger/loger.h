#pragma once

#include <string>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>
#include <chrono>

//! Разделитель столбцов в *.csv файле.
static const char colDelemiter = '\t';
//! Разделитель строк в *.csv файле.
static const char rowDelemiter = '\n';

/**
 * @brief Базовый класс конвертера бинарного файла с логом полета в файл *.csv.
 * @tparam SourceStructure Тип декодируемой структуры.
 */

class Logger {
public:
    /**
     * @brief Конструктор.
     * @param pathToTargetFile Путь до целевого *.csv файла.
     * @details Если файл уже существует, то он будет перезаписан.
     * Если файла нет, то он будет создан.
     * Если файл создать нельзя, то будет выдано исключение.
     */
    Logger(const std::string &pathToTargetFile);

    /**
     * @brief Деструктор.
     */
    virtual ~Logger();

    /**
     * @brief Записать значение в ячейку.
     * @tparam T Тип значения.
     * @param value Значение.
     */
    template<typename T>
    void recordValue(const T &value);

    void endStr();

protected:
    std::ofstream file_; //!< Целевой *.csv файл.
};

template<typename T>
void Logger::recordValue(const T &value) {
    file_ << value << colDelemiter;
}
