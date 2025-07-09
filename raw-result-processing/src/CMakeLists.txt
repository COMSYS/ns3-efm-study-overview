add_subdirectory("simdata")
add_subdirectory("analysis")

add_executable(EfmSimProcessor "EfmSimProcessor.cpp")

target_link_libraries(EfmSimProcessor 
                        PRIVATE 
                        nlohmann_json::nlohmann_json 
                        simdjson 
                        simdata 
                        analysis)
target_link_libraries(EfmSimProcessor 
                        PUBLIC 
                        project_compiler_flags)
