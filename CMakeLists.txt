cmake_minimum_required(VERSION 3.10)
project(AudioCaptureCLI)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加源文件
set(SOURCE_FILES
    audio_capture_cli.cpp
    WASAPICapture.cpp
    stdafx.cpp
)

# 添加头文件
set(HEADER_FILES
    audio_capture_cli.h
    WASAPICapture.h
    stdafx.h
    targetver.h
)

# 创建可执行文件
add_executable(audio_capture_cli ${SOURCE_FILES} ${HEADER_FILES})

# 添加Windows特定库
target_link_libraries(audio_capture_cli
    Ole32    # COM支持
    avrt     # MMCSS
)

# 添加包含路径
target_include_directories(audio_capture_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 添加预处理器定义
add_definitions(-D_CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES=1)

# 如果是MSVC编译器，设置特定选项
if(MSVC)
    # 禁用一些警告
    add_compile_options(/W3 /wd4201)
    
    # 添加预编译头文件支持
    target_compile_options(audio_capture_cli PRIVATE /Yu"stdafx.h")
    set_source_files_properties(audio_capture_cli.cpp PROPERTIES COMPILE_FLAGS /Yu"stdafx.h")
    set_source_files_properties(WASAPICapture.cpp PROPERTIES COMPILE_FLAGS /Yu"stdafx.h")
    
    # 设置stdafx.cpp作为预编译头的创建文件
    set_source_files_properties(stdafx.cpp PROPERTIES COMPILE_FLAGS /Yc"stdafx.h")
endif() 