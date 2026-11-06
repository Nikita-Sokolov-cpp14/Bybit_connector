#include "loger.h"
#include <chrono>
#include <filesystem>

Logger::Logger(const std::string &pathToTargetFile, bool flushAfterRow, bool append) :
flushAfterRow_(flushAfterRow) {
    std::error_code ec;
    fileWasEmpty_ = !std::filesystem::exists(pathToTargetFile, ec);
    if (!fileWasEmpty_) {
        const auto size = std::filesystem::file_size(pathToTargetFile, ec);
        fileWasEmpty_ = !ec && size == 0;
    }

    std::ios::openmode mode = std::ios::out;
    mode |= append ? std::ios::app : std::ios::trunc;
    file_.open(pathToTargetFile, mode);

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
