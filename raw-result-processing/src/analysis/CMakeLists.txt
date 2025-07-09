add_library(analysis
            "classified-path-set.cc"
            "failure-localization.cc"
            "analysis-manager.cc"
            "output-generator.cc"
            "analysis-config.cc"
            "link-characteristic-set.cc"
            "combined-flow-set.cc"
)

target_include_directories(analysis INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(analysis 
                        PUBLIC 
                        project_compiler_flags
                        alglib
                        simdata 
                        nlohmann_json::nlohmann_json)

if(USE_GUROBI)
target_link_libraries(analysis
                        PUBLIC
                        ${GUROBI_CXX_LIBRARY}
                        ${GUROBI_LIBRARY})
endif()
