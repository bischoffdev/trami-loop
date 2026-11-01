# Core library: model + timing logic. Pure C++, no GUI, unit-testable.
add_library(trami_core STATIC
    model/Timing.cpp
)
target_include_directories(trami_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
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
    ui/TramiLookAndFeel.cpp
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
