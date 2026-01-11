# ==============================================================================
# VTS Terminal 示例 - 子项目CMakeLists.txt
# ==============================================================================
# 这是 User/vts_terminal 示例的子项目配置
# 父项目（根目录的CMakeLists.txt）已经定义了共享的源文件和头文件路径
# 这里只需要定义当前示例特有的部分
# ==============================================================================

# 当前示例的源文件
file(GLOB VTS_EXAMPLE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# 创建可执行目标
add_executable(MyRTOS_VTS_Terminal_Example
    # 共享的源文件（从父项目继承）
    ${MYRTOS_SOURCES}
    ${FIRMWARE_SOURCES}
    ${LIBRARY_SOURCES}
    ${USER_COMMON_SOURCES}

    # 当前示例的源文件
    ${VTS_EXAMPLE_SOURCES}
)

# 头文件包含路径
target_include_directories(MyRTOS_VTS_Terminal_Example PRIVATE
    # 当前示例的配置文件目录（优先级最高）
    # 这样 MyRTOS_Config.h 等配置文件会使用当前示例目录下的版本
    ${CMAKE_CURRENT_SOURCE_DIR}

    # 共享的头文件路径（从父项目继承）
    ${COMMON_INCLUDE_DIRS}
)

# 编译选项
target_compile_definitions(MyRTOS_VTS_Terminal_Example PRIVATE
    GD32F407
    USE_STDPERIPH_DRIVER
)

# 可选：ARM GCC编译选项
# target_compile_options(MyRTOS_VTS_Terminal_Example PRIVATE
#     -mcpu=cortex-m4
#     -mthumb
#     -mfloat-abi=hard
#     -mfpu=fpv4-sp-d16
# )
