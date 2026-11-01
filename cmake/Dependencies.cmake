# All third-party dependencies are fetched automatically at configure time.
include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# --- JUCE (AGPLv3) -----------------------------------------------------------
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.10
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# --- Rubber Band (GPLv2+) ----------------------------------------------------
# Built from its single-file amalgamation to avoid a separate Meson build.
FetchContent_Declare(
    rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG        v4.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(rubberband)

add_library(rubberband STATIC ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp)
target_include_directories(rubberband PUBLIC ${rubberband_SOURCE_DIR})
target_compile_definitions(rubberband PRIVATE USE_BQRESAMPLER USE_BUILTIN_FFT)
set_target_properties(rubberband PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    target_link_libraries(rubberband PUBLIC "-framework Accelerate")
endif()

# --- Catch2 (tests only) -----------------------------------------------------
if(TRAMI_BUILD_TESTS)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.8.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(catch2)
endif()
