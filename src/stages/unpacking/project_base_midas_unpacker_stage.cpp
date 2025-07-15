#include "stages/unpacking/project_base_midas_unpacker_stage.h"
#include <stdexcept>

ClassImp(ProjectBaseMidasUnpackerStage)

ProjectBaseMidasUnpackerStage::ProjectBaseMidasUnpackerStage() = default;
ProjectBaseMidasUnpackerStage::~ProjectBaseMidasUnpackerStage() = default;

void ProjectBaseMidasUnpackerStage::SetInput(const InputBundle& input) {
    if (!input.has<TMEvent>("TMEvent")) {
        throw std::runtime_error("ProjectBaseMidasUnpackerStage::SetInput - InputBundle missing TMEvent");
    }
    TMEvent& event = input.getRef<TMEvent>("TMEvent");
    SetCurrentEvent(event);
}

void ProjectBaseMidasUnpackerStage::SetCurrentEvent(TMEvent& event) {
    current_event_ = &event;
}

void ProjectBaseMidasUnpackerStage::Process() {
    if (!current_event_) {
        throw std::runtime_error("ProjectBaseMidasUnpackerStage: current_event_ not set");
    }
    ProcessMidasEvent(*const_cast<TMEvent*>(current_event_));
}
