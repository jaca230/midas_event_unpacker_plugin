#include "stages/unpacking/midas_event_unpacker_stage.h"
#include <spdlog/spdlog.h>

ClassImp(MidasEventUnpackerStage)

MidasEventUnpackerStage::MidasEventUnpackerStage() = default;
MidasEventUnpackerStage::~MidasEventUnpackerStage() = default;

void MidasEventUnpackerStage::OnInit() {
    spdlog::debug("[{}] Initializing internal AnalysisPipeline...", Name());

    local_config_ = std::make_shared<ConfigManager>();

    // Correct usage of nlohmann::json::contains instead of isMember
    if (parameters_.contains("pipeline_config")) {
        nlohmann::json pipelineJson = parameters_["pipeline_config"];
        if (!local_config_->addJsonObject(pipelineJson)) {
            throw std::runtime_error("Failed to load inline pipeline_config");
        }
    } 
    else if (parameters_.contains("pipeline_config_file")) {
        std::string path = parameters_["pipeline_config_file"].get<std::string>();
        if (!local_config_->loadFiles({path})) {
            throw std::runtime_error("Failed to load pipeline_config_file: " + path);
        }
    } 
    else {
        throw std::runtime_error("midas_event_unpacker_stage: no pipeline config provided in parameters");
    }

    if (!local_config_->validate()) {
        throw std::runtime_error("Internal pipeline configuration failed validation");
    }

    local_pipeline_ = std::make_unique<Pipeline>(local_config_);
    if (!local_pipeline_->buildFromConfig()) {
        throw std::runtime_error("Failed to build internal pipeline");
    }

    spdlog::debug("[{}] Internal pipeline successfully built", Name());
}

void MidasEventUnpackerStage::ProcessMidasEvent(TMEvent& event) {
    spdlog::debug("[{}] ProcessMidasEvent called", Name());
    //1. Turn TMEvent into bytestream

    //2. Pass bytestream as input to the internal pipeline

    //3. Extract output and set data products for external pipeline
}

