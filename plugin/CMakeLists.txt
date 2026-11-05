# MASALA by epices audio — plugin VST3 / AU / Standalone (JUCE 8)
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
# JUCE est téléchargé automatiquement (ou passe -DJUCE_DIR=/chemin/vers/JUCE).
cmake_minimum_required(VERSION 3.22)
project(MASALA VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce)
else()
  include(FetchContent)
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
  FetchContent_MakeAvailable(juce)
endif()

set(MASALA_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND MASALA_FORMATS AU)
endif()

juce_add_plugin(Masala
  COMPANY_NAME "epices audio"
  COMPANY_WEBSITE "https://epices.audio"
  BUNDLE_ID "audio.epices.masala"
  PLUGIN_MANUFACTURER_CODE Epcs
  PLUGIN_CODE Msla
  PRODUCT_NAME "Masala"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  FORMATS ${MASALA_FORMATS})

juce_generate_juce_header(Masala)

juce_add_binary_data(MasalaData SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/presets/factory.json)

target_sources(Masala PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/PresetManager.cpp
  Source/CloudClient.cpp
  Source/dsp/Engine.cpp)

target_compile_definitions(Masala PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=1
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Masala
  PRIVATE MasalaData juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
