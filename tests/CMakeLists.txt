add_executable(trami_tests
    TimingTests.cpp
    SongTests.cpp
    EngineTests.cpp
    StretchTests.cpp
    ImportTests.cpp
    ProjectSerializerTests.cpp
    ExportTests.cpp
    AcidTests.cpp
    ../src/io/SampleImporter.cpp
    ../src/io/ProjectSerializer.cpp
    ../src/io/WavExporter.cpp
    ../src/io/AcidChunkReader.cpp
    ../src/io/AcidProjectReader.cpp
)
target_link_libraries(trami_tests PRIVATE trami_core juce::juce_audio_formats Catch2::Catch2WithMain)

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(trami_tests)
