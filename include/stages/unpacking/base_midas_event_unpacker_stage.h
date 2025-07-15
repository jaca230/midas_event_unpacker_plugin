#ifndef BASE_MIDAS_EVENT_UNPACKER_STAGE_H
#define BASE_MIDAS_EVENT_UNPACKER_STAGE_H

#include "stages/input/base_input_stage.h"
#include "midasio.h"
#include <stdexcept>

/**
 * BaseMidasEventUnpackerStage provides a base class for MIDAS unpacking stages
 * that consume TMEvent via InputBundle and emit custom data products.
 */
class BaseMidasEventUnpackerStage : public BaseInputStage {
public:
    BaseMidasEventUnpackerStage();
    ~BaseMidasEventUnpackerStage() override;

    // Receives externally injected input as InputBundle
    void SetInput(const InputBundle& input) override;

    // Run unpacking on the most recent input
    void Process() final override;

protected:
    void SetCurrentEvent(TMEvent& event);

    const TMEvent* current_event_ = nullptr;

    // Subclasses implement MIDAS unpacking logic here
    virtual void ProcessMidasEvent(TMEvent& event) = 0;

    ClassDefOverride(BaseMidasEventUnpackerStage, 1);
};

#endif // BASE_MIDAS_EVENT_UNPACKER_STAGE_H
