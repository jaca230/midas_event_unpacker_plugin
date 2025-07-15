#ifndef MINIMAL_MIDAS_EVENT_UNPACKER_STAGE_H
#define MINIMAL_MIDAS_EVENT_UNPACKER_STAGE_H

#include "stages/unpacking/base_midas_event_unpacker_stage.h"
#include <nlohmann/json.hpp>

class MinimalMidasEventUnpackerStage : public BaseMidasEventUnpackerStage {
public:
    MinimalMidasEventUnpackerStage();
    ~MinimalMidasEventUnpackerStage() override;

    void ProcessMidasEvent(TMEvent& event) override;

    std::string Name() const override;

private:
    nlohmann::json decodeBankData(const TMBank& bank, TMEvent& event) const;
    std::string toHexString(const char* data, size_t size) const;
    std::string event_json_str_;

    ClassDefOverride(MinimalMidasEventUnpackerStage, 1);
};

#endif // MINIMAL_MIDAS_EVENT_UNPACKER_STAGE_H
