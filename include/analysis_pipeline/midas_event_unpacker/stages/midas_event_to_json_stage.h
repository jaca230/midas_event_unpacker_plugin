// MidasEventToJsonStage.h
#ifndef MIDAS_EVENT_TO_JSON_STAGE_H
#define MIDAS_EVENT_TO_JSON_STAGE_H

#include "analysis_pipeline/midas_event_unpacker/stages/midas_event_unpacker_stage.h"
#include "analysis_pipeline/unpacker_core/data_products/JsonProduct.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

class MidasEventToJsonStage : public MidasEventUnpackerStage {
public:
    MidasEventToJsonStage();
    ~MidasEventToJsonStage() override;

    void ProcessMidasEvent(std::shared_ptr<TMEvent> event) override;

    std::string Name() const override;

private:
    nlohmann::json decodeBankData(const TMBank& bank, const TMEvent& event) const;
    std::string toHexString(const char* data, size_t size) const;

    ClassDefOverride(MidasEventToJsonStage, 1);
};

#endif // MIDAS_EVENT_TO_JSON_STAGE_H
