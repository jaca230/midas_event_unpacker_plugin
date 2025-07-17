// MidasEventToByteStreamStage.h
#ifndef MIDAS_EVENT_TO_BYTESTREAM_STAGE_H
#define MIDAS_EVENT_TO_BYTESTREAM_STAGE_H

#include "analysis_pipeline/midas_event_unpacker/stages/midas_event_unpacker_stage.h"
#include "analysis_pipeline/unpacker_core/data_products/ByteStream.h"
#include "analysis_pipeline/unpacker_core/data_products/JsonProduct.h"
#include <memory>

class MidasEventToByteStreamStage : public MidasEventUnpackerStage {
public:
    MidasEventToByteStreamStage();
    ~MidasEventToByteStreamStage() override;

    void ProcessMidasEvent(std::shared_ptr<TMEvent> event) override;

    std::string Name() const override;

private:
    ClassDefOverride(MidasEventToByteStreamStage, 1);
};

#endif // MIDAS_EVENT_TO_BYTESTREAM_STAGE_H
