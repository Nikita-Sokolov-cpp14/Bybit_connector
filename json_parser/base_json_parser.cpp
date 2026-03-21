#include "base_json_parser.h"

namespace {

const std::string_view topicName = "topic";
const std::string_view orderbook = "orderbook";
const std::string_view publicTrade = "publicTrade";
const std::string_view statusFieldName = "success";

bool checkIsStatusStr(std::string_view str) {
    std::string_view sucsess = getFieldValue(statusFieldName, str);
    if (sucsess == "true" || sucsess == "false") {
        return true;
    }
    return false;
}

} // namespace

TypeMessage parseTypeMessage(std::string_view str) {
    if (checkIsStatusStr(str)) {
        return TypeMessage_Status;
    }

    std::string_view type = getFieldValue(topicName, str);
    type = type.substr(0, type.find('.'));
    if (type == orderbook) {
        return TypeMessage_Orderbook;
    } else if (type == publicTrade) {
        return TypeMessage_PublicTrade;
    }
    std::cout << "BaseJsonParser::parseTypeMessage: Undefined type message" << std::endl;
    return TypeMessage_Unknown;
}

std::string_view getFieldValue(std::string_view fieldName, std::string_view source) {
    size_t fieldPos = source.find(fieldName);
    size_t strLen = source.length();
    if (fieldPos >= strLen) {
        return "";
    }
    size_t fieldEnd = source.find_first_of(",}", fieldPos);
    if (fieldEnd > strLen) {
        fieldEnd = strLen;
    }
    size_t valuePos = fieldPos + fieldName.length() + 2;
    std::string_view value = source.substr(valuePos, fieldEnd - valuePos);
    if (value[0] == '"') {
        return value.substr(1, value.length() - 2);
    }

    return value;
}

void BaseJsonParser::setString(std::string_view str) {
    string_ = str;
}

std::pair<double, double> BaseJsonParser::parsePair(std::string_view pairStr) {
    size_t posComma = pairStr.find(',');
    std::string_view priceStr = pairStr.substr(1, posComma - 2);
    std::string_view volumeStr = pairStr.substr(posComma + 2, pairStr.length() - posComma - 3);
    return std::pair<double, double> {convertToDouble(priceStr), convertToDouble(volumeStr)};
}

double BaseJsonParser::convertToDouble(std::string_view valueStr) {
    double result = 0.0;
    bool beforeDot = true;
    double exponent = 0.1;

    for (char c : valueStr) {
        if (c == '.') {
            beforeDot = false;
            continue;
        }
        if (c < '0' && c > '9') {
            std::cout << "JsonParser::convertToDouble: Error convertation" << std::endl;
            return 0.0;
        }

        if (beforeDot) {
            result = result * 10 + (c - '0');
        } else {
            result += (c - '0') * exponent;
            exponent *= 0.1;
        }
    }

    return result;
}
