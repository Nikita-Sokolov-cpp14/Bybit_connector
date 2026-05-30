#include "loger.h"
#include <chrono>

Logger::Logger(const std::string &pathToTargetFile) :
file_(pathToTargetFile) {
    if (!file_.is_open()) {
        throw std::logic_error("невозможно открыть файл " + pathToTargetFile);
    }
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::endStr() {
    file_ << rowDelemiter;
}
