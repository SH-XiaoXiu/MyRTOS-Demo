cmake_minimum_required(VERSION 3.31)
project(MyRTOS C)

set(CMAKE_C_STANDARD 11)
set(C_STANDARD 89)
set(CMAKE_SYSTEM_NAME Generic)

file(GLOB_RECURSE SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/User/**.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Firmware/**.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/**.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/MyRTOS/**.c"

)
add_executable(MyRTOS ${SOURCE_FILES})

target_include_directories(MyRTOS PRIVATE
        ./Firmware/CMSIS
        ./Firmware/CMSIS/GD/GD32F4xx/Include
        ./Firmware/GD32F4xx_standard_peripheral/Include
        ./Libraries
        ./MyRTOS/include
        ./User
)

