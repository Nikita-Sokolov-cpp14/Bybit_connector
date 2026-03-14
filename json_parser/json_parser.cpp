#include "json_parser.h"

namespace {

const std::string_view statusFieldName = "success";
const std::string_view retMsgFieldName = "ret_msg";
const std::string_view connIdFieldName = "conn_id";
const std::string_view reqIdFieldName = "req_id";
const std::string_view opFieldName = "op";

const std::string_view typeFieldName = "type";
const std::string_view snapshot = "snapshot";
const std::string_view delta = "delta";
const std::string_view data = "data";
const std::string_view bids = R"("b":[)";
const std::string_view asks = R"("a":[)";
const std::string_view uName = "u";
const std::string_view seqName = "seq";
const std::string_view tsName = "ts";
const std::string_view ctsName = "cts";
const std::string_view sName = "s";
const std::string_view topicName = "topic";

} // namespace

StatusMessage::StatusMessage() :
sucsess(false),
retMessage(""),
conId(""),
reqId(""),
operation("") {
}

void JsonParser::setString(std::string_view str) {
    string_ = str;
}

void JsonParser::parseTypeMessage() {
    typeMessage_ = TypeMessage_Unknown;
    std::string_view type = getFieldValue(typeFieldName, string_);
    if (type == "") {
        type = getFieldValue(statusFieldName, string_);
        if (type == "true" || type == "false") { // TODO
            typeMessage_ = TypeMessage_Status;
        }
    }

    if (type == snapshot) {
        typeMessage_ = TypeMessage_Snapshot;
    } else if (type == delta) {
        typeMessage_ = TypeMessage_Delta;
    }
}

std::string_view JsonParser::getFieldValue(std::string_view fieldName, std::string_view source) {
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

void JsonParser::parse() {
    parseTypeMessage();

    switch (typeMessage_) {
        case TypeMessage_Delta:
            parseDataMessage();
            break;
        case TypeMessage_Snapshot:
            parseDataMessage();
            break;
        case TypeMessage_Status:
            parseStatusMessage();
            break;
        default:
            std::cout << "unknown message" << std::endl;
            break;
    }
}

void JsonParser::parseStatusMessage() {
    std::string_view sucsessStr = getFieldValue(statusFieldName, string_);
    if (sucsessStr == "true") {
        statusMessage_.sucsess = true;
    } else {
        statusMessage_.sucsess = false;
    }
    statusMessage_.retMessage = getFieldValue(retMsgFieldName, string_);
    statusMessage_.conId = getFieldValue(connIdFieldName, string_);
    statusMessage_.reqId = getFieldValue(reqIdFieldName, string_);
    statusMessage_.operation = getFieldValue(opFieldName, string_);
}

void JsonParser::parseDataMessage() {
    topic_ = getFieldValue(topicName, string_);
    convertTo(getFieldValue(tsName, string_), ts_);
    convertTo(getFieldValue(ctsName, string_), cts_);
    size_t dataStartPos = string_.find(data);
    size_t dataEndPos = string_.find('}', dataStartPos);
    parseDataSection(string_.substr(dataStartPos, dataEndPos - dataStartPos));
}

void JsonParser::printData() {
    std::cout << "topic: " << topic_ << std::endl;
    std::cout << "typeMessage: " << typeMessage_ << std::endl;
    std::cout << "ts: " << ts_ << std::endl;
    std::cout << " === data == " << std::endl;
    std::cout << "s: " << s_ << std::endl;
    std::cout << " bids " << std::endl;
    for (const auto &val : levelsBids_) {
        std::cout << val.first << " " << val.second << std::endl;
    }
    std::cout << " asks " << std::endl;
    for (const auto &val : levelsBids_) {
        std::cout << val.first << " " << val.second << std::endl;
    }
    std::cout << "u: " << u_ << std::endl;
    std::cout << "seq: " << seq_ << std::endl;
    std::cout << " === end data == " << std::endl;
    std::cout << "cts: " << cts_ << std::endl;
}

void JsonParser::printStatus() {
    std::cout << "sucsess: " << statusMessage_.sucsess << std::endl;
    std::cout << "retMessage: " << statusMessage_.retMessage << std::endl;
    std::cout << "conId: " << statusMessage_.conId << std::endl;
    std::cout << "reqId: " << statusMessage_.reqId << std::endl;
    std::cout << "operation: " << statusMessage_.operation << std::endl;
}

void JsonParser::parseDataSection(std::string_view dataStr) {
    s_ = getFieldValue(sName, dataStr);
    size_t startBidsArr = dataStr.find(bids) + bids.size();
    size_t endBidsArr = dataStr.find("]]", startBidsArr) + 1;
    parseArray(dataStr.substr(startBidsArr, endBidsArr - startBidsArr), TypeArray_Bids);

    size_t startAsksArr = dataStr.find(asks) + asks.size();
    size_t endAsksArr = dataStr.find("]]", startAsksArr) + 1;
    parseArray(dataStr.substr(startAsksArr, endAsksArr - startAsksArr), TypeArray_Asks);

    convertTo(getFieldValue(uName, dataStr), u_);
    convertTo(getFieldValue(seqName, dataStr), seq_);
}

void JsonParser::parseArray(std::string_view arrayStr, const TypeArray &typeArray) {
    std::pair<double, double> priceVolume;
    for (size_t pos = 1; pos < arrayStr.length();) {
        size_t commaPos = arrayStr.find("],[", pos);
        if (commaPos >= arrayStr.length()) {
            commaPos = arrayStr.length() - 1;
        }
        priceVolume = parsePair(arrayStr.substr(pos, commaPos - pos));
        switch (typeArray) {
            case TypeArray_Bids:
                levelsBids_[priceVolume.first] = priceVolume.second;
                break;
            default:
            case TypeArray_Asks:
                levelsAsks_[priceVolume.first] = priceVolume.second;
                break;
        }
        pos = commaPos + 3;
    }
}

std::pair<double, double> JsonParser::parsePair(std::string_view pairStr) {
    size_t posComma = pairStr.find(',');
    std::string_view priceStr = pairStr.substr(1, posComma - 2);
    std::string_view volumeStr = pairStr.substr(posComma + 2, pairStr.length() - posComma - 3);
    std::pair<double, double> priceVolume;
    convertTo(priceStr, priceVolume.first);
    convertTo(volumeStr, priceVolume.second);
    return priceVolume;
}
