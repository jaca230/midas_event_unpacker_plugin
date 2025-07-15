#include "stages/unpacking/minimal_midas_event_unpacker_stage.h"
#include <TTree.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <spdlog/spdlog.h>
#include <TObjString.h>

ClassImp(MinimalMidasEventUnpackerStage)

using json = nlohmann::json;

MinimalMidasEventUnpackerStage::MinimalMidasEventUnpackerStage() {
    spdlog::debug("[{}] Constructor called", Name());
}

MinimalMidasEventUnpackerStage::~MinimalMidasEventUnpackerStage() {
    spdlog::debug("[{}] Destructor called", Name());
}

void MinimalMidasEventUnpackerStage::ProcessMidasEvent(TMEvent& event) {
    spdlog::debug("[{}] ProcessMidasEvent called with event_id={}, serial_number={}",
                  Name(), event.event_id, event.serial_number);

    json j;
    j["event_id"] = event.event_id;
    j["serial_number"] = event.serial_number;
    j["trigger_mask"] = event.trigger_mask;
    j["timestamp"] = event.time_stamp;
    j["data_size"] = event.data_size;
    j["event_header_size"] = event.event_header_size;
    j["bank_header_flags"] = event.bank_header_flags;

    event.FindAllBanks();
    spdlog::debug("[{}] Found {} banks", Name(), event.banks.size());

    j["banks"] = json::array();
    for (const auto& bank : event.banks) {
        spdlog::debug("[{}] Processing bank: name='{}', type={}, data_size={}",
                      Name(), bank.name, bank.type, bank.data_size);

        json jbank;
        jbank["name"] = bank.name;
        jbank["type"] = bank.type;
        jbank["data_size"] = bank.data_size;

        auto decoded_data = decodeBankData(bank, event);

        size_t data_length = 0;
        if (decoded_data.is_string()) {
            data_length = decoded_data.get<std::string>().size();
        } else {
            data_length = decoded_data.size();
        }

        spdlog::debug("[{}] Decoded bank data (type {}): size/length={}",
                      Name(), bank.type, data_length);

        jbank["data"] = std::move(decoded_data);
        j["banks"].push_back(std::move(jbank));
    }

    auto jsonString = std::make_unique<TObjString>(j.dump().c_str());

    auto pdp = std::make_unique<PipelineDataProduct>();
    pdp->setName("event_json");
    pdp->setObject(std::move(jsonString));
    pdp->addTag("unpacked_data");
    pdp->addTag("built_by_minimal_midas_unpacker");
    getDataProductManager()->addOrUpdate("event_json", std::move(pdp));

    spdlog::debug("[{}] Created PipelineDataProduct for event_json", Name());
}

json MinimalMidasEventUnpackerStage::decodeBankData(const TMBank& bank, TMEvent& event) const {
    const char* bankData = event.GetBankData(&bank);
    if (!bankData || bank.data_size == 0) {
        spdlog::warn("[{}] Bank '{}' has null data or zero size", Name(), bank.name);
        return nullptr;
    }

    size_t dataSize = bank.data_size;
    json dataArray = json::array();

    spdlog::debug("[{}] Decoding bank '{}' with type={} and dataSize={}",
                  Name(), bank.name, bank.type, dataSize);

    switch (bank.type) {
        case TID_UINT8: {
            const uint8_t* arr = reinterpret_cast<const uint8_t*>(bankData);
            for (size_t i = 0; i < dataSize; ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_INT8: {
            const int8_t* arr = reinterpret_cast<const int8_t*>(bankData);
            for (size_t i = 0; i < dataSize; ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_UINT16: {
            const uint16_t* arr = reinterpret_cast<const uint16_t*>(bankData);
            for (size_t i = 0; i < dataSize / sizeof(uint16_t); ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_INT16: {
            const int16_t* arr = reinterpret_cast<const int16_t*>(bankData);
            for (size_t i = 0; i < dataSize / sizeof(int16_t); ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_UINT32: {
            const uint32_t* arr = reinterpret_cast<const uint32_t*>(bankData);
            for (size_t i = 0; i < dataSize / sizeof(uint32_t); ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_INT32: {
            const int32_t* arr = reinterpret_cast<const int32_t*>(bankData);
            for (size_t i = 0; i < dataSize / sizeof(int32_t); ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_UINT64: {
            const uint64_t* arr = reinterpret_cast<const uint64_t*>(bankData);
            for (size_t i = 0; i < dataSize / sizeof(uint64_t); ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_INT64: {
            const int64_t* arr = reinterpret_cast<const int64_t*>(bankData);
            for (size_t i = 0; i < dataSize / sizeof(int64_t); ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_FLOAT: {
            const float* arr = reinterpret_cast<const float*>(bankData);
            for (size_t i = 0; i < dataSize / sizeof(float); ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_DOUBLE: {
            const double* arr = reinterpret_cast<const double*>(bankData);
            for (size_t i = 0; i < dataSize / sizeof(double); ++i) dataArray.push_back(arr[i]);
            break;
        }
        case TID_STRING: {
            std::string str(bankData, dataSize);
            spdlog::debug("[{}] Decoded string bank data: '{}'", Name(), str);
            return str;
        }
        default: {
            spdlog::warn("[{}] Unknown bank type {}. Returning hex string", Name(), bank.type);
            return toHexString(bankData, dataSize);
        }
    }

    spdlog::debug("[{}] Decoded array size: {}", Name(), dataArray.size());
    return dataArray;
}

std::string MinimalMidasEventUnpackerStage::toHexString(const char* data, size_t size) const {
    std::ostringstream oss;
    for (size_t i = 0; i < size; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << (static_cast<uint8_t>(data[i]) & 0xFF);
    }
    std::string hexStr = oss.str();
    spdlog::debug("[{}] Converted data to hex string: {}", Name(), hexStr);
    return hexStr;
}

std::string MinimalMidasEventUnpackerStage::Name() const {
    return "MinimalMidasEventUnpackerStage";
}
