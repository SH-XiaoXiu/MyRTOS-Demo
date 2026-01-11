cmake_minimum_required(VERSION 3.20)
project(MyRTOS C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_SYSTEM_NAME Generic)

# 收集所有源文件
file(GLOB_RECURSE SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/User/**.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Firmware/**.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/**.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/**.c"
)

# 创建可执行文件目标
add_executable(MyRTOS ${SOURCE_FILES})

# 设置头文件包含路径
target_include_directories(MyRTOS PRIVATE
        ./Firmware/CMSIS
        ./Firmware/CMSIS/GD/GD32F4xx/Include
        ./Firmware/GD32F4xx_standard_peripheral/Include
        ./Libraries
        ./MyRTOS/kernel/include
        ./MyRTOS/services/include
        ./MyRTOS/programs/include
        ./MyRTOS/platform/GD32F4xx/include
        ./MyRTOS/include
        ./User
)
