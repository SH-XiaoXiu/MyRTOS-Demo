# ==============================================================================
# MyRTOS 多示例项目 - 根CMakeLists.txt
# ==============================================================================
# 项目结构：
#   - 根目录定义共享的源文件（MyRTOS内核、Firmware库等）
#   - 每个示例是独立的子项目，有自己的CMakeLists.txt和配置文件
#   - CLion中可以通过切换编译目标（target）来选择不同示例
# ==============================================================================

cmake_minimum_required(VERSION 3.20)
project(MyRTOS_Examples C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_SYSTEM_NAME Generic)

# ==============================================================================
# 共享源文件定义（所有示例都需要的代码）
# ==============================================================================

# MyRTOS 内核和服务
file(GLOB_RECURSE MYRTOS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/**.c"
)

# GD32 固件库
file(GLOB_RECURSE FIRMWARE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Firmware/CMSIS/GD/GD32F4xx/Source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Firmware/GD32F4xx_standard_peripheral/Source/*.c"
)

# 应用库
file(GLOB_RECURSE LIBRARY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/**.c"
)

# 共享的User/common代码（中断处理等）
file(GLOB USER_COMMON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/User/common/*.c"
)

# ==============================================================================
# 共享的头文件包含路径（所有示例都需要的）
# ==============================================================================
set(COMMON_INCLUDE_DIRS
    # GD32 固件库
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/CMSIS
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/CMSIS/GD/GD32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Firmware/GD32F4xx_standard_peripheral/Include

    # MyRTOS 内核和服务
    ${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/services/include
    ${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/programs/shell/include
    ${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/programs/init/include
    ${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/platform/GD32F4xx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/include

    # 应用库
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries

    # User/common 共享头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/User/common
)

# ==============================================================================
# 添加各个示例子项目
# ==============================================================================
# 每个示例都是独立的子项目，有自己的：
#   - CMakeLists.txt（定义编译目标）
#   - MyRTOS_Config.h（内核配置）
#   - MyRTOS_Service_Config.h（服务配置）
#   - platform_config.h（平台配置）
# ==============================================================================

# Shell示例 - Process机制测试
add_subdirectory(User/shell)

# VTS Terminal示例 - 虚拟终端测试
add_subdirectory(User/vts_terminal)

# Kernel Basic示例 - 基础内核功能演示
add_subdirectory(User/kernel_basic)

# Kernel Advanced示例 - 高级内核功能演示
add_subdirectory(User/kernel_advanced)

# IO & Logging示例 - IO流与日志框架演示
add_subdirectory(User/io_logging)

# ==============================================================================
# 使用说明
# ==============================================================================
#
# 在 CLion 中使用
# 1. 打开 CLion，选择整个项目根目录
# 2. CLion 会识别到多个编译目标（每个示例一个）
# 3. 在 CLion 右上角的 target 下拉菜单中选择要编译的示例
# 4. 每个示例使用各自目录下的配置文件，互不干扰
#
# 配置文件隔离
# - User/shell/MyRTOS_Config.h         ← shell示例的配置
# - User/vts_terminal/MyRTOS_Config.h  ← vts_terminal示例的配置
# - 两者互不影响，CLion会根据选择的target使用对应的配置
#
# 编译目标命名
# - MyRTOS_Shell_Example              (Shell与Process管理)
# - MyRTOS_VTS_Terminal_Example       (虚拟终端服务)
# - MyRTOS_Kernel_Basic_Example       (基础内核功能)
# - MyRTOS_Kernel_Advanced_Example    (高级内核功能)
# - MyRTOS_IO_Logging_Example         (IO流与日志框架)
#
# 与 Keil 工程配合
# - 每个示例目录下可以放置对应的.uvprojx工程文件
# - CMake 用于 CLion 的代码导航和智能提示
# - Keil 用于实际的编译、调试和下载
#
