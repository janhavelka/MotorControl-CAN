idf_component_register(SRCS "main.cpp" REQUIRES MotorControlCAN)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_11)
set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
