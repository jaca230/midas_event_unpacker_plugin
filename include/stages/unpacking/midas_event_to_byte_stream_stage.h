// MidasEventToByteStreamStage.h
#ifndef MIDAS_EVENT_TO_BYTESTREAM_STAGE_H
#define MIDAS_EVENT_TO_BYTESTREAM_STAGE_H

#include "stages/unpacking/midas_event_unpacker_stage.h"
#include "data_products/common/ByteStream.h"
#include "data_products/common/JsonProduct.h"
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
