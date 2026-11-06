#include "loger.h"
#include <chrono>

Logger::Logger(const std::string &pathToTargetFile, bool flushAfterRow) :
file_(pathToTargetFile),
flushAfterRow_(flushAfterRow) {
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
    if (flushAfterRow_) {
        file_.flush();
    }
}

void Logger::flush() {
    file_.flush();
}
