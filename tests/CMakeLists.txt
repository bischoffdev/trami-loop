add_executable(trami_tests
    TimingTests.cpp
    SongTests.cpp
    EngineTests.cpp
    StretchTests.cpp
    ImportTests.cpp
    ../src/io/SampleImporter.cpp
)
target_link_libraries(trami_tests PRIVATE trami_core juce::juce_audio_formats Catch2::Catch2WithMain)

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(trami_tests)
