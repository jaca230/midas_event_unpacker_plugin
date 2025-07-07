#include "stages/unpacking/project_base_midas_unpacker_stage.h"
#include <stdexcept>
#include <typeinfo>

ClassImp(ProjectBaseMidasUnpackerStage)

ProjectBaseMidasUnpackerStage::ProjectBaseMidasUnpackerStage() = default;
ProjectBaseMidasUnpackerStage::~ProjectBaseMidasUnpackerStage() = default;

void ProjectBaseMidasUnpackerStage::SetInput(std::any input) {
    try {
        TMEvent& event = std::any_cast<std::reference_wrapper<TMEvent>>(input).get();
        SetCurrentEvent(event);
    } catch (const std::bad_any_cast& e) {
        throw std::runtime_error("ProjectBaseMidasUnpackerStage::SetInput - input is not a TMEvent reference");
    }
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
