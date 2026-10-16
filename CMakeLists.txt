cmake_minimum_required(VERSION 3.22)
project(mnArpee VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# JUCE: prefer a local checkout (MNARPEE_JUCE_DIR), fall back to FetchContent 9.0.1
set(MNARPEE_JUCE_DIR "d:/dev/JUCE" CACHE PATH "Local JUCE checkout")
if(NOT MNARPEE_TESTS_ONLY)
if(EXISTS "${MNARPEE_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${MNARPEE_JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.1
    )
    FetchContent_MakeAvailable(juce)
endif()
endif()

if(NOT MNARPEE_TESTS_ONLY)
juce_add_plugin(mnArpee
    COMPANY_NAME "MNAudio"
    PLUGIN_MANUFACTURER_CODE MnAu
    PLUGIN_CODE MnAp
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    NEEDS_AUDIO_INPUT FALSE
    NEEDS_AUDIO_OUTPUT FALSE
    VST3_CATEGORIES Instrument
    PRODUCT_NAME "mnArpee"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(mnArpee)

# Montserrat (title) and Roboto Condensed (UI text), SIL Open Font License.
juce_add_binary_data(mnArpeeFonts
    SOURCES
        Resources/Fonts/Montserrat.ttf
        Resources/Fonts/RobotoCondensed.ttf
)

target_sources(mnArpee PRIVATE
    Source/PatternData.cpp
    Source/ArpEngine.cpp
    Source/PresetLibrary.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StepGrid.cpp
    Source/Fonts.cpp
)

target_compile_definitions(mnArpee
    PUBLIC
        JucePlugin_Name="mnArpee"
        MNARPEE_VERSION_STRING="${PROJECT_VERSION}"
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

if(MSVC)
    target_compile_options(mnArpee PRIVATE /utf-8 /Zc:__cplusplus)
endif()

target_link_libraries(mnArpee
    PRIVATE
        mnArpeeFonts
        juce::juce_audio_devices
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
endif()

option(MNARPEE_BUILD_TESTS "Build the engine tests (mnArpeeTests) and, with JUCE, the plugin tests (mnArpeePluginTests)" OFF)
if(MNARPEE_BUILD_TESTS)
    enable_testing()
    add_executable(mnArpeeTests
        Tests/EngineTests.cpp
        Source/PatternData.cpp
        Source/ArpEngine.cpp
    )
    target_include_directories(mnArpeeTests PRIVATE Source)
    if(MSVC)
        target_compile_options(mnArpeeTests PRIVATE /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME engine COMMAND mnArpeeTests)

    if(NOT MNARPEE_TESTS_ONLY)
        # Drives the real processor and editor without a host. It links the plugin's
        # shared code, so it gets that target's include paths and definitions.
        add_executable(mnArpeePluginTests Tests/PluginTests.cpp)
        target_include_directories(mnArpeePluginTests PRIVATE
            Source
            $<TARGET_PROPERTY:mnArpee,INCLUDE_DIRECTORIES>)
        target_compile_definitions(mnArpeePluginTests PRIVATE
            $<TARGET_PROPERTY:mnArpee,COMPILE_DEFINITIONS>)
        target_link_libraries(mnArpeePluginTests PRIVATE mnArpee)
        if(MSVC)
            target_compile_options(mnArpeePluginTests PRIVATE /utf-8 /Zc:__cplusplus)
        endif()
        add_test(NAME plugin COMMAND mnArpeePluginTests)
    endif()
endif()
