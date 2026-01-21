# QEMU MPS2-AN385 Demo CMakeLists.txt
# 此配置用于 CLion IDE 代码提示，实际编译请使用 Makefile

# Demo 应用源文件
file(GLOB DEMO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# CMSIS 源文件
file(GLOB CMSIS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/CMSIS/*.c")

# Platform 源文件
file(GLOB PLATFORM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.c")

# 创建可执行目标（仅用于 IDE 索引）
add_executable(qemu_mps2_an385_demo
    ${DEMO_SRC}
    ${CMSIS_SRC}
    ${PLATFORM_SRC}
)

# 链接 MyRTOS 库
target_link_libraries(qemu_mps2_an385_demo myrtos)

# 为 MyRTOS 库提供配置和平台路径（供 IDE 索引 MyRTOS 源文件）
target_include_directories(myrtos PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_SOURCE_DIR}/MyRTOS/kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS
)

# Demo 自身的包含目录
target_include_directories(qemu_mps2_an385_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS
)

# 定义目标芯片宏（用于代码提示）
target_compile_definitions(qemu_mps2_an385_demo PRIVATE
    ARM_MATH_CM3
)
