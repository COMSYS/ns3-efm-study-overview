add_library(simdata
            "sim-data-manager.cc"
            "sim-result-set.cc"
            "sim-vantage-point.cc"
            "sim-events.cc"
            "sim-flow.cc"
            "sim-filter.cc"
            "sim-path.cc"
            "sim-ping-pair.cc"
            )

target_include_directories(simdata INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(simdata PUBLIC project_compiler_flags nlohmann_json::nlohmann_json simdjson)
