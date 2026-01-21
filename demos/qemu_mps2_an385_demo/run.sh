#!/bin/bash
#
# MyRTOS QEMU Demo 运行脚本
# 在 WSL 中运行: ./run.sh
#
# 依赖:
#   - arm-none-eabi-gcc (ARM 工具链)
#   - qemu-system-arm (QEMU ARM 模拟器)
#
# 安装依赖 (Ubuntu/Debian):
#   sudo apt update
#   sudo apt install gcc-arm-none-eabi qemu-system-arm
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

TARGET="myrtos_qemu_demo"
BUILD_DIR="build"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  MyRTOS QEMU MPS2-AN385 Demo${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 检查依赖
check_dependency() {
    if ! command -v "$1" &> /dev/null; then
        echo -e "${RED}Error: $1 is not installed.${NC}"
        echo "Please install it first:"
        echo "  sudo apt install $2"
        exit 1
    fi
}

echo -e "${YELLOW}Checking dependencies...${NC}"
check_dependency "arm-none-eabi-gcc" "gcc-arm-none-eabi"
check_dependency "qemu-system-arm" "qemu-system-arm"
echo -e "${GREEN}All dependencies found.${NC}"
echo ""

# 解析参数
ACTION="${1:-run}"

case "$ACTION" in
    build)
        echo -e "${YELLOW}Building...${NC}"
        make clean
        make -j$(nproc)
        echo -e "${GREEN}Build complete!${NC}"
        ;;

    clean)
        echo -e "${YELLOW}Cleaning...${NC}"
        make clean
        echo -e "${GREEN}Clean complete!${NC}"
        ;;

    run)
        echo -e "${YELLOW}Building...${NC}"
        make -j$(nproc)
        echo ""
        echo -e "${GREEN}Starting QEMU...${NC}"
        echo -e "${YELLOW}Press Ctrl+A then X to exit QEMU${NC}"
        echo ""
        echo -e "${BLUE}========================================${NC}"
        qemu-system-arm \
            -M mps2-an385 \
            -cpu cortex-m3 \
            -nographic \
            -kernel "${BUILD_DIR}/${TARGET}.elf"
        ;;

    debug)
        echo -e "${YELLOW}Building...${NC}"
        make -j$(nproc)
        echo ""
        echo -e "${GREEN}Starting QEMU with GDB server...${NC}"
        echo -e "${YELLOW}Connect GDB with: arm-none-eabi-gdb -ex 'target remote :1234' ${BUILD_DIR}/${TARGET}.elf${NC}"
        echo ""
        qemu-system-arm \
            -M mps2-an385 \
            -cpu cortex-m3 \
            -nographic \
            -kernel "${BUILD_DIR}/${TARGET}.elf" \
            -s -S
        ;;

    *)
        echo "Usage: $0 [build|clean|run|debug]"
        echo ""
        echo "Commands:"
        echo "  build  - Build the project"
        echo "  clean  - Clean build files"
        echo "  run    - Build and run in QEMU (default)"
        echo "  debug  - Build and run in QEMU with GDB server"
        exit 1
        ;;
esac
