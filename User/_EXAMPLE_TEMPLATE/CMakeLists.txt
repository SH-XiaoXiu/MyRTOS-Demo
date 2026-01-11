# ==============================================================================
# [示例名称] - 子项目CMakeLists.txt
# ==============================================================================
# 使用说明：
# 1. 复制这个模板文件到新的示例目录
# 2. 修改下面的目标名称 "MyRTOS_XXX_Example" 为你的示例名称
# 3. 在根目录的 CMakeLists.txt 中添加: add_subdirectory(User/你的示例目录)
# ==============================================================================

# 当前示例的源文件
file(GLOB EXAMPLE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# ⭐ 修改这里的目标名称，建议使用 MyRTOS_XXX_Example 格式
add_executable(MyRTOS_XXX_Example
    # 共享的源文件（从父项目继承）
    ${MYRTOS_SOURCES}
    ${FIRMWARE_SOURCES}
    ${LIBRARY_SOURCES}
    ${USER_COMMON_SOURCES}

    # 当前示例的源文件
    ${EXAMPLE_SOURCES}
)

# 头文件包含路径
target_include_directories(MyRTOS_XXX_Example PRIVATE
    # 当前示例的配置文件目录（优先级最高）
    # 这样 MyRTOS_Config.h 等配置文件会使用当前示例目录下的版本
    ${CMAKE_CURRENT_SOURCE_DIR}

    # 共享的头文件路径（从父项目继承）
    ${COMMON_INCLUDE_DIRS}
)

# 编译选项
target_compile_definitions(MyRTOS_XXX_Example PRIVATE
    GD32F407
    USE_STDPERIPH_DRIVER
)

# 可选：ARM GCC编译选项
# target_compile_options(MyRTOS_XXX_Example PRIVATE
#     -mcpu=cortex-m4
#     -mthumb
#     -mfloat-abi=hard
#     -mfpu=fpv4-sp-d16
# )
