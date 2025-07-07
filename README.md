# analysis_pipeline_example_plugin

[![C++17](https://img.shields.io/badge/C++-17-blue.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()


This is an example plugin demonstrating how to create a custom stage for the **[Analysis Pipeline](https://github.com/jaca230/analysis_pipeline)** framework.

## Overview

This plugin implements the `ConstantValueStage`, a simple pipeline stage that produces a constant numeric value as a data product.

It serves as a minimal example of:

* Defining a new pipeline stage  
* Integrating with the existing `analysis_pipeline_stages` library  
* Generating ROOT dictionaries for custom classes  
* Building and installing as a shared plugin library  

## Building

The project uses CMake. You can build it using the provided build script:

```bash
./scripts/build.sh
````

This will configure and build the project in the `build/` directory.

## Installation

You can install the plugin using the install script, which installs to the default prefix `/usr/local` (can be customized):

```bash
sudo ./scripts/install.sh
```

## Cleanup

To clean build artifacts before rebuilding, use the cleanup script:

```bash
./scripts/cleanup.sh
```

## Usage

* Include the plugin’s headers and link against the installed library in your application.
* Use the JSON pipeline configuration to instantiate the `ConstantValueStage`, for example:

```json
{
  "pipeline": [
    {
      "id": "const_stage",
      "type": "ConstantValueStage",
      "parameters": {
        "product_name": "const_output",
        "value": 42.0
      },
      "next": []
    }
  ]
}
```

* Your pipeline runner will load this plugin and create the stage accordingly.

## Dependencies

* [analysis\_pipeline\_stages](https://github.com/jaca230/analysis_pipeline_stages)
* [ROOT](https://root.cern/) (Core, RIO components)  
* [spdlog](https://github.com/gabime/spdlog)  
* [nlohmann_json](https://github.com/nlohmann/json)  


## License

This project is licensed under the terms specified in the [LICENSE](LICENSE) file.

```
