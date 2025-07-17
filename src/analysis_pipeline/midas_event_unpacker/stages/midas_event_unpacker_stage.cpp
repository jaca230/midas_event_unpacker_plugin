#include "analysis_pipeline/midas_event_unpacker/stages/midas_event_unpacker_stage.h"
#include <stdexcept>

ClassImp(MidasEventUnpackerStage)

MidasEventUnpackerStage::MidasEventUnpackerStage() = default;
MidasEventUnpackerStage::~MidasEventUnpackerStage() = default;

void MidasEventUnpackerStage::SetInput(const InputBundle& input) {
    if (!input.has<std::shared_ptr<TMEvent>>("TMEvent")) {
        throw std::runtime_error("MidasEventUnpackerStage::SetInput - InputBundle missing TMEvent");
    }
    auto event = input.get<std::shared_ptr<TMEvent>>("TMEvent");
    SetCurrentEvent(event);
}

void MidasEventUnpackerStage::SetCurrentEvent(std::shared_ptr<TMEvent> event) {
    current_event_ = std::move(event);
}

void MidasEventUnpackerStage::Process() {
    if (!current_event_) {
        throw std::runtime_error("MidasEventUnpackerStage: current_event_ not set");
    }
    ProcessMidasEvent(current_event_);
}