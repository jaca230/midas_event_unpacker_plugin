#include "stages/unpacking/base_midas_event_unpacker_stage.h"
#include <stdexcept>

ClassImp(BaseMidasEventUnpackerStage)

BaseMidasEventUnpackerStage::BaseMidasEventUnpackerStage() = default;
BaseMidasEventUnpackerStage::~BaseMidasEventUnpackerStage() = default;

void BaseMidasEventUnpackerStage::SetInput(const InputBundle& input) {
    if (!input.has<TMEvent>("TMEvent")) {
        throw std::runtime_error("BaseMidasEventUnpackerStage::SetInput - InputBundle missing TMEvent");
    }
    TMEvent& event = input.getRef<TMEvent>("TMEvent");
    SetCurrentEvent(event);
}

void BaseMidasEventUnpackerStage::SetCurrentEvent(TMEvent& event) {
    current_event_ = &event;
}

void BaseMidasEventUnpackerStage::Process() {
    if (!current_event_) {
        throw std::runtime_error("BaseMidasEventUnpackerStage: current_event_ not set");
    }
    ProcessMidasEvent(*const_cast<TMEvent*>(current_event_));
}
