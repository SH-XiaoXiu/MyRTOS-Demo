# GD32F407 Demo CMakeLists.txt
# 此配置用于 CLion IDE 代码提示，实际编译请使用 Keil 工程

# Demo 应用源文件
file(GLOB DEMO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Firmware 源文件
file(GLOB_RECURSE FIRMWARE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/Firmware/CMSIS/GD/GD32F4xx/Source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Firmware/GD32F4xx_standard_peripheral/Source/*.c"
)

# Libraries 源文件
file(GLOB LIBRARIES_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/*.c")

# Platform 源文件（GD32F407 硬件驱动）
file(GLOB PLATFORM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.c")

# 创建可执行目标（仅用于 IDE 索引）
add_executable(gd32f407_demo
    ${DEMO_SRC}
    ${FIRMWARE_SRC}
    ${LIBRARIES_SRC}
    ${PLATFORM_SRC}
)

# 链接 MyRTOS 库
target_link_libraries(gd32f407_demo myrtos)

# 为 MyRTOS 库提供配置和平台路径（供 IDE 索引 MyRTOS 源文件）
target_include_directories(myrtos PUBLIC
    # Demo 配置文件
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    # 平台层头文件（包含 MyRTOS_Arch_Device.h）
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    # Kernel 内部头文件
    ${CMAKE_SOURCE_DIR}/MyRTOS/kernel/include
    # Firmware 头文件（CMSIS 定义）
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/CMSIS
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/CMSIS/GD/GD32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/GD32F4xx_standard_peripheral/Include
)

# Demo 自身的包含目录
target_include_directories(gd32f407_demo PRIVATE
    # Demo 源码
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include

    # Firmware
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/CMSIS
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/CMSIS/GD/GD32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/GD32F4xx_standard_peripheral/Include

    # Libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries
)

# 定义目标芯片宏（用于代码提示）
target_compile_definitions(gd32f407_demo PRIVATE
    GD32F407
    USE_STDPERIPH_DRIVER
)
