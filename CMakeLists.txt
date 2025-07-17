cmake_minimum_required(VERSION 3.14)

project(midas_event_unpacker_plugin VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# --------------------- Options ---------------------
option(USE_BUNDLED_MIDAS "Use the bundled MIDAS snapshot instead of system MIDASSYS" OFF)

# --------------------- GCC Warnings ---------------------
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-stringop-overflow)
  endif()
endif()

# --------------------- CPM Setup ---------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

# Use externally-defined versions if set, else fallback
set(CPM_SPDLOG_VERSION "1.13.0" CACHE STRING "spdlog version")
set(CPM_NLOHMANN_JSON_VERSION "3.11.3" CACHE STRING "nlohmann_json version")

# --------------------- spdlog ---------------------
CPMAddPackage(
  NAME spdlog
  GITHUB_REPOSITORY gabime/spdlog
  VERSION ${CPM_SPDLOG_VERSION}
  OPTIONS
    "SPDLOG_BUILD_EXAMPLES OFF"
    "SPDLOG_BUILD_TESTS OFF"
    "SPDLOG_FMT_EXTERNAL OFF"
  DOWNLOAD_ONLY YES
)

if(spdlog_ADDED)
  add_library(spdlog_header_only INTERFACE)
  target_include_directories(spdlog_header_only INTERFACE
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  add_library(spdlog::spdlog ALIAS spdlog_header_only)
endif()

# --------------------- nlohmann_json ---------------------
CPMAddPackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  VERSION ${CPM_NLOHMANN_JSON_VERSION}
  DOWNLOAD_ONLY YES
)

if(nlohmann_json_ADDED)
  add_library(nlohmann_json_header_only INTERFACE)
  target_include_directories(nlohmann_json_header_only INTERFACE
    $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json_header_only)
endif()

# --------------------- Required Packages ---------------------
find_package(ROOT REQUIRED COMPONENTS Core RIO)
find_package(AnalysisPipelineStagesCore REQUIRED)
find_package(unpacker_data_products_core REQUIRED)
include(${ROOT_USE_FILE})

# --------------------- MIDAS Source Selection ---------------------
if(USE_BUNDLED_MIDAS)
  add_subdirectory(external/midas_snapshot)
  set(MIDAS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/external/midas_snapshot/include)
  set(MIDAS_LIBS midas_snapshot::midas_snapshot)
else()
  if(NOT DEFINED ENV{MIDASSYS})
    message(FATAL_ERROR "System MIDAS selected but MIDASSYS is not defined in the environment.")
  endif()

  set(MIDASSYS_INCLUDE_DIRS
    $ENV{MIDASSYS}/include
    $ENV{MIDASSYS}/midasio
  )
  set(MIDASSYS_LIB_DIR $ENV{MIDASSYS}/lib)

  set(MIDAS_INCLUDE_DIRS ${MIDASSYS_INCLUDE_DIRS})
  set(MIDAS_LIBS
    ${MIDASSYS_LIB_DIR}/libmfe.a
    ${MIDASSYS_LIB_DIR}/libmidas.a
  )
endif()

# --------------------- Sources ---------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(midas_event_unpacker_plugin::midas_event_unpacker_plugin ALIAS ${PROJECT_NAME})

# --------------------- Include Paths ---------------------
target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
if(USE_BUNDLED_MIDAS)
  target_include_directories(${PROJECT_NAME}
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/midas_snapshot/include>
      $<INSTALL_INTERFACE:include/midas_snapshot>
  )
else()
  target_include_directories(${PROJECT_NAME}
    PUBLIC
      ${MIDAS_INCLUDE_DIRS}
  )
endif()

# --------------------- Link Libraries ---------------------
target_link_libraries(${PROJECT_NAME}
  PUBLIC
    ${MIDAS_LIBS}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ROOT::Core ROOT::RIO
    AnalysisPipelineStagesCore::analysis_pipeline_stages_core
    unpacker_data_products_core::unpacker_data_products_core
)

# --------------------- ROOT Dictionary Setup ---------------------
function(append_target_includes_to_root_dict target_name)
  get_target_property(INCLUDE_DIRS ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
  if(INCLUDE_DIRS AND NOT INCLUDE_DIRS STREQUAL "${target_name}_NOTFOUND")
    foreach(dir IN LISTS INCLUDE_DIRS)
      if(dir MATCHES "^\\$<BUILD_INTERFACE:(.+)>$")
        set(actual "${CMAKE_MATCH_1}")
        if(EXISTS "${actual}")
          list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${actual}")
        endif()
      elseif(NOT dir MATCHES "^\\$")
        if(EXISTS "${dir}")
          list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${dir}")
        endif()
      endif()
    endforeach()
  endif()
  set(ROOT_DICTIONARY_INCLUDE_OPTIONS ${ROOT_DICTIONARY_INCLUDE_OPTIONS} PARENT_SCOPE)
endfunction()

set(ROOT_LINKDEF ${CMAKE_CURRENT_SOURCE_DIR}/include/LinkDef.h)
set(ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/include")

if(USE_BUNDLED_MIDAS)
  append_target_includes_to_root_dict(midas_snapshot::midas_snapshot)
else()
  foreach(dir IN LISTS MIDASSYS_INCLUDE_DIRS)
    list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${dir}")
  endforeach()
endif()

append_target_includes_to_root_dict(AnalysisPipelineStagesCore::analysis_pipeline_stages_core)
append_target_includes_to_root_dict(unpacker_data_products_core::unpacker_data_products_core)

file(GLOB_RECURSE STAGE_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/stages/*.h)
file(GLOB_RECURSE DATAPRODUCT_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/data_products/*.h)
set(ALL_DICT_HEADERS ${STAGE_HEADERS} ${DATAPRODUCT_HEADERS})
list(FILTER ALL_DICT_HEADERS EXCLUDE REGEX "LinkDef\\.h$")

ROOT_GENERATE_DICTIONARY(G__${PROJECT_NAME}
  ${ALL_DICT_HEADERS}
  LINKDEF ${ROOT_LINKDEF}
  OPTIONS ${ROOT_DICTIONARY_INCLUDE_OPTIONS} -Wno-inconsistent-missing-override
)

target_sources(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/G__${PROJECT_NAME}.cxx
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# --------------------- Install Rules ---------------------
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  if(USE_BUNDLED_MIDAS)
    install(TARGETS midas_snapshot
      EXPORT ${PROJECT_NAME}Targets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
  endif()

  install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(TARGETS spdlog_header_only nlohmann_json_header_only
    EXPORT ${PROJECT_NAME}Targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
  )

  install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
  )

  if(USE_BUNDLED_MIDAS)
    install(DIRECTORY external/midas_snapshot/
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/midas_snapshot
    )
  endif()

  install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
endif()
