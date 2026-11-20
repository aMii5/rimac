cmake_minimum_required(VERSION 3.22)

project(Rimac VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)

# ------------------------------------------------------------------------------
# JUCE
# ------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Rimac
    COMPANY_NAME              "Rimac Audio"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    PLUGIN_MANUFACTURER_CODE  Rimc
    PLUGIN_CODE               Rmac
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "Rimac"
    DESCRIPTION               "Dynamic resonance suppressor with M/S processing"
    BUNDLE_ID                 "com.rimacaudio.rimac"
    VST3_CATEGORIES           Fx Dynamics
    AU_MAIN_TYPE              kAudioUnitType_Effect
)

target_sources(Rimac PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/SpectralSuppressor.cpp
    Source/ui/Theme.cpp
    Source/ui/Knob.cpp
    Source/ui/EQDisplay.cpp
    Source/ui/PortraitPanel.cpp
)

target_compile_definitions(Rimac PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    RIMAC_VERSION="${PROJECT_VERSION}"
)

target_include_directories(Rimac PRIVATE Source)

target_link_libraries(Rimac PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(Rimac)
