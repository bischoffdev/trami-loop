# Core library: model + real-time engine logic. Pure C++, no JUCE, unit-testable.
add_library(trami_core STATIC
    model/Timing.cpp
    model/Sample.cpp
    model/Pattern.cpp
    model/Song.cpp
    engine/Sequencer.cpp
    engine/SnapshotBuilder.cpp
    engine/Synth.cpp
    engine/TimeStretcher.cpp
    engine/StretchCache.cpp
    engine/AudioRecorder.cpp
)
target_include_directories(trami_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(trami_core PUBLIC rubberband)
target_compile_options(trami_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Desktop application.
juce_add_gui_app(TramiLoop
    PRODUCT_NAME "trami-loop"
    COMPANY_NAME "trami"
    BUNDLE_ID    "dev.bischoff.trami-loop"
)

target_sources(TramiLoop PRIVATE
    app/Main.cpp
    app/MainWindow.cpp
    engine/AudioEngine.cpp
    io/SampleImporter.cpp
    io/ProjectSerializer.cpp
    io/WavExporter.cpp
    ui/TramiLookAndFeel.cpp
    ui/IconButton.cpp
    ui/PatternGrid.cpp
    ui/TrackHeader.cpp
    ui/SampleBrowserPanel.cpp
    ui/PatternView.cpp
    ui/ArrangerGrid.cpp
    ui/ArrangerTrackHeader.cpp
    ui/ArrangerView.cpp
    ui/MainContainer.cpp
)

target_compile_definitions(TramiLoop PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:TramiLoop,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:TramiLoop,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(TramiLoop
    PRIVATE
        trami_core
        rubberband
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
