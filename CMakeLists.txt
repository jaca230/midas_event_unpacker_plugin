cmake_minimum_required(VERSION 3.14)

project(midas_event_unpacker_plugin VERSION 1.0 LANGUAGES CXX)

# ----------------------- Compiler Settings ------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-stringop-overflow)
  endif()
endif()

# ----------------------- Build Options ----------------------------
option(USE_BUNDLED_MIDAS "Use the bundled MIDAS snapshot instead of system MIDASSYS" OFF)

# ----------------------- Includes and Utilities -------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

# ----------------------- CPM Dependencies -------------------------
set(CPM_SPDLOG_VERSION "1.13.0" CACHE STRING "spdlog version")
set(CPM_NLOHMANN_JSON_VERSION "3.11.3" CACHE STRING "nlohmann_json version")

CPMAddPackage(
  NAME spdlog
  GITHUB_REPOSITORY gabime/spdlog
  VERSION ${CPM_SPDLOG_VERSION}
  OPTIONS "SPDLOG_BUILD_EXAMPLES OFF" "SPDLOG_BUILD_TESTS OFF" "SPDLOG_FMT_EXTERNAL OFF"
  DOWNLOAD_ONLY YES
)
if(spdlog_ADDED)
  add_library(spdlog_header_only INTERFACE)
  target_include_directories(spdlog_header_only INTERFACE
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  add_library(spdlog::spdlog ALIAS spdlog_header_only)
endif()

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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json_header_only)
endif()

# ----------------------- Required Packages ------------------------
find_package(ROOT REQUIRED COMPONENTS Core RIO)
find_package(analysis_pipeline_core REQUIRED)
find_package(unpacker_data_products_core REQUIRED)
include(${ROOT_USE_FILE})

# ----------------------- MIDAS Handling ---------------------------
if(USE_BUNDLED_MIDAS)
  add_subdirectory(external/midas_snapshot)
  set(MIDAS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/external/midas_snapshot/include)
  set(MIDAS_LIBS midas_snapshot::midas_snapshot)
else()
  if(NOT DEFINED ENV{MIDASSYS})
    message(FATAL_ERROR "USE_BUNDLED_MIDAS=OFF but MIDASSYS is not set")
  endif()

  set(MIDAS_INCLUDE_DIRS
    $ENV{MIDASSYS}/include
    $ENV{MIDASSYS}/midasio
  )
  set(MIDAS_LIBS
    $ENV{MIDASSYS}/lib/libmfe.a
    $ENV{MIDASSYS}/lib/libmidas.a
  )
endif()

# ----------------------- Source Files -----------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# ----------------------- Library Target ---------------------------
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${MIDAS_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    ${MIDAS_LIBS}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ROOT::Core ROOT::RIO
    analysis_pipeline_core::analysis_pipeline_core
    unpacker_data_products_core::unpacker_data_products_core
)

# ----------------------- ROOT Dictionary Helper -------------------
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

# ----------------------- Plugin Root Detection --------------------
file(GLOB PLUGIN_ROOT_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline
  ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/*
)

set(PLUGIN_ROOTS "")
foreach(dir IN LISTS PLUGIN_ROOT_DIRS)
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/${dir})
    list(APPEND PLUGIN_ROOTS ${dir})
  endif()
endforeach()

list(LENGTH PLUGIN_ROOTS plugin_root_count)
if(plugin_root_count EQUAL 1)
  list(GET PLUGIN_ROOTS 0 PLUGIN_ROOT_DIR)
else()
  message(FATAL_ERROR
    "Expected one plugin directory under include/analysis_pipeline/, found ${plugin_root_count}: ${PLUGIN_ROOTS}")
endif()

set(PLUGIN_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/${PLUGIN_ROOT_DIR}")

# ----------------------- ROOT Dictionary Generation --------------
set(ROOT_LINKDEF ${PLUGIN_ROOT_PATH}/LinkDef.h)
set(ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/include")

append_target_includes_to_root_dict(analysis_pipeline_core::analysis_pipeline_core)
append_target_includes_to_root_dict(unpacker_data_products_core::unpacker_data_products_core)

foreach(midas_dir IN LISTS MIDAS_INCLUDE_DIRS)
  list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${midas_dir}")
endforeach()

file(GLOB_RECURSE STAGE_HEADERS CONFIGURE_DEPENDS
  ${PLUGIN_ROOT_PATH}/stages/*.h)
file(GLOB_RECURSE DATAPRODUCT_HEADERS CONFIGURE_DEPENDS
  ${PLUGIN_ROOT_PATH}/data_products/*.h)

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

# ----------------------- Install Rules ----------------------------
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.rootmap
    ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}_rdict.pcm
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    OPTIONAL
  )

  install(TARGETS spdlog_header_only nlohmann_json_header_only
    EXPORT ${PROJECT_NAME}Targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  if(USE_BUNDLED_MIDAS)
    install(TARGETS midas_snapshot
      EXPORT ${PROJECT_NAME}Targets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY external/midas_snapshot/include/
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/midas_snapshot
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
