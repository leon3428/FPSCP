find_package(pybind11 CONFIG REQUIRED)
pybind11_add_module(_evrp_core bindings.cpp)
target_link_libraries(_evrp_core PRIVATE evrp_core)
set_target_properties(_evrp_core PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_RPATH "$ORIGIN/../..")
install(TARGETS _evrp_core DESTINATION evrp_decoder)
