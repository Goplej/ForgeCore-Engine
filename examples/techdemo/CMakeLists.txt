add_executable(techdemo main.cpp)
target_link_libraries(techdemo PRIVATE forgecore)
set_target_properties(techdemo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

