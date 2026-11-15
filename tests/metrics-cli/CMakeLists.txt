add_executable(rialto-gstreamer-metrics-test
        main.cpp
        )

target_link_libraries(rialto-gstreamer-metrics-test
        PRIVATE
        PkgConfig::GSTREAMER_APP
        )

set_target_properties(rialto-gstreamer-metrics-test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/metrics-cli"
        )
