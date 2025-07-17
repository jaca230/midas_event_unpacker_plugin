#ifndef MIDAS_EVENT_UNPACKER_STAGE_H
#define MIDAS_EVENT_UNPACKER_STAGE_H

#include "analysis_pipeline/core/stages/input/base_input_stage.h"
#include "midasio.h"
#include <memory>
#include <stdexcept>

/**
 * MidasEventUnpackerStage provides a base class for MIDAS unpacking stages
 * that consume TMEvent via InputBundle and emit custom data products.
 */
class MidasEventUnpackerStage : public BaseInputStage {
public:
    MidasEventUnpackerStage();
    ~MidasEventUnpackerStage() override;

    // Receives externally injected input as InputBundle
    void SetInput(const InputBundle& input) override;

    // Run unpacking on the most recent input
    void Process() final override;

protected:
    void SetCurrentEvent(std::shared_ptr<TMEvent> event);

    std::shared_ptr<TMEvent> current_event_;

    // Subclasses implement MIDAS unpacking logic here
    virtual void ProcessMidasEvent(std::shared_ptr<TMEvent> event) = 0;

    ClassDefOverride(MidasEventUnpackerStage, 1);
};

#endif // MIDAS_EVENT_UNPACKER_STAGE_H
