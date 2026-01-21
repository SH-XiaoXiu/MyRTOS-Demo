# MyRTOS 内核库 CMakeLists.txt
# 此配置用于 CLion IDE 代码提示

# 收集内核源文件
file(GLOB KERNEL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/kernel/*.c")

# 收集服务层源文件
file(GLOB SERVICES_SRC "${CMAKE_CURRENT_SOURCE_DIR}/services/*.c")

# 收集程序层源文件
file(GLOB PROGRAMS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/programs/*.c")

# 收集架构层源文件 (ARM_CM4)
file(GLOB_RECURSE ARCH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/arch/*.c")

# 收集 Boot 模块源文件
file(GLOB BOOT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/boot/*.c")

# 创建库目标（仅用于 IDE 索引）
add_library(myrtos STATIC
    ${KERNEL_SRC}
    ${SERVICES_SRC}
    ${PROGRAMS_SRC}
    ${ARCH_SRC}
    ${BOOT_SRC}
)

# 公共头文件目录（对外暴露）
target_include_directories(myrtos PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/services/include
    ${CMAKE_CURRENT_SOURCE_DIR}/programs/include
    ${CMAKE_CURRENT_SOURCE_DIR}/boot/include
)

# 私有头文件目录（仅内部使用）
target_include_directories(myrtos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
)

# 注意：MyRTOS 需要以下配置：
# 1. MyRTOS_Config.h - 内核配置
# 2. MyRTOS_Arch_Device.h - 芯片设备头文件（由 demo 提供）
# 各个 Demo 需要将其 config 和 platform/include 目录添加到 myrtos 目标的 include 路径中
