#include "stages/unpacking/midas_event_to_byte_stream_stage.h"
#include <spdlog/spdlog.h>
#include <memory>

ClassImp(MidasEventToByteStreamStage)

MidasEventToByteStreamStage::MidasEventToByteStreamStage() {
    spdlog::debug("[{}] Constructor called", Name());
}

MidasEventToByteStreamStage::~MidasEventToByteStreamStage() {
    spdlog::debug("[{}] Destructor called", Name());
}

void MidasEventToByteStreamStage::ProcessMidasEvent(std::shared_ptr<TMEvent> event) {
    if (!event) {
        spdlog::error("[{}] ProcessMidasEvent called with null event", Name());
        return;
    }

    event->FindAllBanks();
    if (event->banks.empty()) {
        spdlog::warn("[{}] No banks found in event", Name());
        return;
    }

    // --- Add event metadata product ---
    {
        nlohmann::json jmeta;
        jmeta["event_id"] = event->event_id;
        jmeta["serial_number"] = event->serial_number;
        jmeta["trigger_mask"] = event->trigger_mask;
        jmeta["timestamp"] = event->time_stamp;
        jmeta["data_size"] = event->data_size;
        jmeta["event_header_size"] = event->event_header_size;
        jmeta["bank_header_flags"] = event->bank_header_flags;
        jmeta["num_banks"] = event->banks.size();

        auto metaProduct = std::make_unique<dataProducts::JsonProduct>();
        metaProduct->jsonString = jmeta.dump();

        auto metaPipelineProduct = std::make_unique<PipelineDataProduct>();
        metaPipelineProduct->setName("event_metadata");
        metaPipelineProduct->setObject(std::move(metaProduct));
        metaPipelineProduct->addTag("event_metadata");
        metaPipelineProduct->addTag("built_by_midas_event_to_bytestream_stage");

        getDataProductManager()->addOrUpdate("event_metadata", std::move(metaPipelineProduct));

        spdlog::debug("[{}] Created event metadata product", Name());
    }

    // --- Existing bank bytestream products below ---
    std::vector<std::pair<std::string, std::unique_ptr<PipelineDataProduct>>> products;

    for (const auto& bank : event->banks) {
        const char* rawBankData = event->GetBankData(&bank);
        if (!rawBankData || bank.data_size == 0) {
            spdlog::warn("[{}] Bank '{}' has null or zero-size data, skipping", Name(), bank.name);
            continue;
        }

        auto byteStream = std::make_shared<dataProducts::ByteStream>();
        byteStream->data = reinterpret_cast<const uint8_t*>(rawBankData);
        byteStream->size = bank.data_size;
        byteStream->owner = std::static_pointer_cast<void>(event);

        std::string productName = "bytestream_bank_" + std::string(bank.name) + "_type_" + std::to_string(bank.type);

        auto product = std::make_unique<PipelineDataProduct>();
        product->setName(productName);
        product->setSharedObject(byteStream);
        product->addTag("unpacked_data");
        product->addTag("built_by_midas_event_to_bytestream_stage");
        product->addTag("bank");
        product->addTag(bank.name);
        product->addTag("type_" + std::to_string(bank.type));

        products.emplace_back(productName, std::move(product));

        spdlog::debug("[{}] Created ByteStream product for bank '{}', size={}, type={}",
                      Name(), bank.name, bank.data_size, bank.type);
    }

    if (!products.empty()) {
        getDataProductManager()->addOrUpdateMultiple(std::move(products));
    } else {
        spdlog::warn("[{}] No valid bank bytestream products created", Name());
    }
}


std::string MidasEventToByteStreamStage::Name() const {
    return "MidasEventToByteStreamStage";
}
