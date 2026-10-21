# Core EVRP library (static, linked into the pybind11 module)
add_library(evrp_core STATIC
    evrp/instance.cpp
    evrp/solution.cpp
    evrp/solution.hpp
    evrp/instance.hpp
    evrp/optimal_decode.hpp
    evrp/optimal_decode.cpp
    evrp/split_decode.hpp
    evrp/split_decode.cpp
    evrp/bhga_decode.hpp
    evrp/bhga_decode.cpp
    evrp/baco_decode.hpp
    evrp/baco_decode.cpp
    evrp/caco_decode.hpp
    evrp/caco_decode.cpp
    evrp/montoya_decode.hpp
    evrp/montoya_decode.cpp
)
target_include_directories(evrp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(evrp_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(evrp_core PRIVATE bhga_case cevrp_lib)

# Python bindings (skipped when building tests standalone)
option(EVRP_BUILD_BINDINGS "Build Python bindings" ON)
if(EVRP_BUILD_BINDINGS)
    add_subdirectory(bindings)
endif()
