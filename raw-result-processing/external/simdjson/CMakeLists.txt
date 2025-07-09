add_library(simdjson
            "simdjson.cpp"
            )

target_include_directories(simdjson INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
