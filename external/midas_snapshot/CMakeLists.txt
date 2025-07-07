project(midas_snapshot LANGUAGES CXX)

file(GLOB MIDAS_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cxx
)

add_library(midas_snapshot STATIC ${MIDAS_SOURCES})

target_include_directories(midas_snapshot
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/midas_event_unpacker_plugin/midas_snapshot>
)

add_library(midas_snapshot::midas_snapshot ALIAS midas_snapshot)
