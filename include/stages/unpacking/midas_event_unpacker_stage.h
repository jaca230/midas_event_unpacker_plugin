#ifndef MIDAS_EVENT_UNPACKER_STAGE_H
#define MIDAS_EVENT_UNPACKER_STAGE_H

#include "stages/unpacking/project_base_midas_unpacker_stage.h"
#include <memory>
#include "config/config_manager.h"
#include "pipeline/pipeline.h"

class MidasEventUnpackerStage : public ProjectBaseMidasUnpackerStage {
public:
    MidasEventUnpackerStage();
    ~MidasEventUnpackerStage() override;

    void ProcessMidasEvent(TMEvent& event) override;

    std::string Name() const override { return "MidasEventUnpackerStage"; }

protected:
    void OnInit() override;

private:
    std::shared_ptr<ConfigManager> local_config_;
    std::unique_ptr<Pipeline> local_pipeline_;

    ClassDefOverride(MidasEventUnpackerStage, 1);
};

#endif // MIDAS_EVENT_UNPACKER_STAGE_H
