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
        PLUGIN_NAME "LK_Jam_1.01"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Lkai
        PLUGIN_CODE LkJ2
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "LK_Jam_1.01"
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

# 1. 注册你的图片资源，注意这里改成了 Assets 并使用了绝对路径策保不出错
juce_add_binary_data(LK_Jam_BinaryData
        SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/logo.jpg"
)

# 2. 将所有库（包含 BinaryData）一次性链接给 LK_Jam
target_link_libraries(LK_Jam
        PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
        LK_Jam_BinaryData       # <--- 你的图片数据绑定在这里
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# 注意：我去掉了冗余的 target_sources(LK_Jam_Standalone ...)，
# 因为上面的 GLOB_RECURSE 已经把你 Source 目录下的马尔可夫等所有文件自动抓取给 LK_Jam 了。