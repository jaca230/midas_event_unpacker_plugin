#ifndef PROJECT_BASE_MIDAS_UNPACKER_STAGE_H
#define PROJECT_BASE_MIDAS_UNPACKER_STAGE_H

#include "stages/input/base_input_stage.h"
#include "midasio.h"
#include <any>

class ProjectBaseMidasUnpackerStage : public BaseInputStage {
public:
    ProjectBaseMidasUnpackerStage();
    ~ProjectBaseMidasUnpackerStage() override;

    // Receives externally injected input (expects TMEvent inside std::any)
    void SetInput(std::any input) override;

    // Run unpacking on the most recent input
    void Process() final override;

protected:
    void SetCurrentEvent(TMEvent& event);

    const TMEvent* current_event_ = nullptr;

    // Subclasses implement MIDAS unpacking logic here
    virtual void ProcessMidasEvent(TMEvent& event) = 0;

    ClassDefOverride(ProjectBaseMidasUnpackerStage, 1);
};

#endif // PROJECT_BASE_MIDAS_UNPACKER_STAGE_H
