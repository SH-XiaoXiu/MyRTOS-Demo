/**
 * Linker script for ARM Cortex-M3 (QEMU MPS2-AN385)
 *
 * Memory map:
 *   Flash: 0x00000000 - 0x003FFFFF (4MB)
 *   SRAM:  0x20000000 - 0x203FFFFF (4MB)
 */

/* 入口点 */
ENTRY(Reset_Handler)

/* 内存区域 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    SRAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 4M
}

/* 栈大小 */
_Min_Stack_Size = 0x1000;   /* 4KB */

SECTIONS
{
    /* 向量表和代码段 */
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))    /* 向量表 */
        *(.text)                /* 代码 */
        *(.text*)
        *(.rodata)              /* 只读数据 */
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* ARM 异常处理表 */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* 构造函数表 */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    /* .data 段初始化数据源 */
    _sidata = LOADADDR(.data);

    /* 已初始化数据段 */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT> FLASH

    /* 未初始化数据段 */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > SRAM

    /* 堆起始地址 (用于 _sbrk) */
    . = ALIGN(4);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    PROVIDE(__end__ = .);

    /* 栈 */
    ._stack :
    {
        . = ALIGN(8);
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > SRAM

    _estack = ORIGIN(SRAM) + LENGTH(SRAM);

    /* 丢弃调试信息 */
    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }
}
