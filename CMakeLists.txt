# MyRTOS 项目 - 根 CMakeLists.txt
# 此 CMake 配置主要用于 CLion IDE 代码提示和索引，不用于实际编译
# 实际编译请使用 Keil 工程 (demos/gd32f407_demo/Keil/GD32F407.uvprojx)

cmake_minimum_required(VERSION 3.16)
project(MyRTOS_Project C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_SYSTEM_NAME Generic)

# 添加子目录
add_subdirectory(MyRTOS)
add_subdirectory(demos)
