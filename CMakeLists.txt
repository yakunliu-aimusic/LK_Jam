cmake_minimum_required(VERSION 3.15)

project(LK_Jam VERSION 0.03)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
set(CMAKE_OSX_ARCHITECTURES arm64;x86_64)

# 生成编译命令数据库，提升CLion代码提示和跳转体验
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(./JUCE ./JUCE)

juce_add_plugin(LK_Jam
        COMPANY_NAME "LK_ai_music"
        PLUGIN_NAME "LK_Jam_1.00"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Lkai
        PLUGIN_CODE LkJ2
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "LK_Jam_1.00"
)

juce_generate_juce_header(LK_Jam)

file(GLOB_RECURSE SourceFiles
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

target_sources(LK_Jam PRIVATE ${SourceFiles})

target_compile_definitions(LK_Jam
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISABLE_JUCE_VERSION_PRINTING=1
)

target_compile_options(LK_Jam
        PRIVATE
        -Wno-float-conversion
        -Wno-sign-conversion
        -Wno-unused-parameter
)

target_link_libraries(LK_Jam
        PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_sources(LK_Jam_Standalone PRIVATE
        # ... 其他现有的源文件 ...

        # 🌟 新增的马尔可夫算法适配器与源码
        Source/Engine/MarkovEngine.cpp
        Source/Engine/MarkovEngine.h
        Source/Engine/Markov/MarkovChain.cpp
        Source/Engine/Markov/MarkovChain.h
        Source/Engine/Markov/MarkovManager.cpp
        Source/Engine/Markov/MarkovManager.h
)