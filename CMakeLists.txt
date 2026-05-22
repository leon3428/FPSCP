cmake_minimum_required(VERSION 3.20)
project(evrp_optimal_decoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bhga_case STATIC extern/bhga/Program/case.cpp)
target_include_directories(bhga_case PUBLIC extern/bhga/Program)
set_property(TARGET bhga_case PROPERTY CXX_STANDARD 17)
set_target_properties(bhga_case PROPERTIES POSITION_INDEPENDENT_CODE ON)

# cevrp algorithms compiled into a shared library with all symbols hidden except
# the two exported adapter functions.  This avoids an ODR clash with bhga_case:
# both submodules define a global-scope `Case` class with incompatible layouts.
add_library(cevrp_lib SHARED
    src/evrp/cevrp_solve.cpp
    extern/cevrp/case.cpp
    extern/cevrp/utilities.cpp
    extern/cevrp/BACO2.cpp
    extern/cevrp/CACO.cpp
)
target_include_directories(cevrp_lib PRIVATE extern/cevrp src/evrp)
target_compile_options(cevrp_lib PRIVATE -fvisibility=hidden)
set_property(TARGET cevrp_lib PROPERTY CXX_STANDARD 17)
set_target_properties(cevrp_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(TARGETS cevrp_lib DESTINATION evrp_decoder)

add_subdirectory(src)
