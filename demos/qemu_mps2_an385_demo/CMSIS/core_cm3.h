/**
 * 简化版 CMSIS core_cm3.h，提供 MyRTOS 所需的最小定义
 */

#ifndef __CORE_CM3_H_GENERIC
#define __CORE_CM3_H_GENERIC

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
//                           IO 类型定义
// ============================================================================

#ifdef __cplusplus
  #define   __I     volatile
#else
  #define   __I     volatile const
#endif
#define     __O     volatile
#define     __IO    volatile

// ============================================================================
//                           编译器特定定义
// ============================================================================

#if defined(__GNUC__)
  #define __ASM            __asm
  #define __INLINE         inline
  #define __STATIC_INLINE  static inline

  #define __PACKED         __attribute__((packed))
  #define __ALIGNED(x)     __attribute__((aligned(x)))

  #define __NO_RETURN      __attribute__((__noreturn__))
  #define __USED           __attribute__((used))
  #define __WEAK           __attribute__((weak))

  #define __enable_irq()   __ASM volatile ("cpsie i" : : : "memory")
  #define __disable_irq()  __ASM volatile ("cpsid i" : : : "memory")

  __STATIC_INLINE void __ISB(void) {
    __ASM volatile ("isb 0xF":::"memory");
  }

  __STATIC_INLINE void __DSB(void) {
    __ASM volatile ("dsb 0xF":::"memory");
  }

  __STATIC_INLINE void __DMB(void) {
    __ASM volatile ("dmb 0xF":::"memory");
  }

  __STATIC_INLINE void __NOP(void) {
    __ASM volatile ("nop");
  }

  __STATIC_INLINE void __WFI(void) {
    __ASM volatile ("wfi");
  }

  __STATIC_INLINE void __WFE(void) {
    __ASM volatile ("wfe");
  }

#else
  #error "Unsupported compiler"
#endif

// ============================================================================
//                           寄存器定义
// ============================================================================

/* SCB - System Control Block */
typedef struct {
  __I  uint32_t CPUID;          /* 0x000 CPUID */
  __IO uint32_t ICSR;           /* 0x004 中断控制和状态 */
  __IO uint32_t VTOR;           /* 0x008 向量表偏移 */
  __IO uint32_t AIRCR;          /* 0x00C 应用中断和复位控制 */
  __IO uint32_t SCR;            /* 0x010 系统控制 */
  __IO uint32_t CCR;            /* 0x014 配置和控制 */
  __IO uint8_t  SHP[12U];       /* 0x018 系统处理器优先级 */
  __IO uint32_t SHCSR;          /* 0x024 系统处理器控制和状态 */
  __IO uint32_t CFSR;           /* 0x028 可配置故障状态 */
  __IO uint32_t HFSR;           /* 0x02C 硬故障状态 */
  __IO uint32_t DFSR;           /* 0x030 调试故障状态 */
  __IO uint32_t MMFAR;          /* 0x034 内存管理故障地址 */
  __IO uint32_t BFAR;           /* 0x038 总线故障地址 */
  __IO uint32_t AFSR;           /* 0x03C 辅助故障状态 */
} SCB_Type;

/* NVIC - Nested Vectored Interrupt Controller */
typedef struct {
  __IO uint32_t ISER[8U];       /* 0x000 中断使能 */
       uint32_t RESERVED0[24U];
  __IO uint32_t ICER[8U];       /* 0x080 中断清除使能 */
       uint32_t RESERVED1[24U];
  __IO uint32_t ISPR[8U];       /* 0x100 中断挂起 */
       uint32_t RESERVED2[24U];
  __IO uint32_t ICPR[8U];       /* 0x180 中断清除挂起 */
       uint32_t RESERVED3[24U];
  __IO uint32_t IABR[8U];       /* 0x200 中断活动状态 */
       uint32_t RESERVED4[56U];
  __IO uint8_t  IP[240U];       /* 0x300 中断优先级 */
       uint32_t RESERVED5[644U];
  __O  uint32_t STIR;           /* 0xE00 软件触发中断 */
} NVIC_Type;

/* SysTick */
typedef struct {
  __IO uint32_t CTRL;           /* 0x000 控制和状态 */
  __IO uint32_t LOAD;           /* 0x004 重载值 */
  __IO uint32_t VAL;            /* 0x008 当前值 */
  __I  uint32_t CALIB;          /* 0x00C 校准值 */
} SysTick_Type;

// ============================================================================
//                           外设基地址
// ============================================================================

#define SCS_BASE            (0xE000E000UL)
#define SysTick_BASE        (SCS_BASE + 0x0010UL)
#define NVIC_BASE           (SCS_BASE + 0x0100UL)
#define SCB_BASE            (SCS_BASE + 0x0D00UL)

// ============================================================================
//                           外设实例
// ============================================================================

#define SCB                 ((SCB_Type *) SCB_BASE)
#define SysTick             ((SysTick_Type *) SysTick_BASE)
#define NVIC                ((NVIC_Type *) NVIC_BASE)

// ============================================================================
//                           SCB 寄存器位定义
// ============================================================================

/* ICSR */
#define SCB_ICSR_PENDSVSET_Pos          28U
#define SCB_ICSR_PENDSVSET_Msk          (1UL << SCB_ICSR_PENDSVSET_Pos)
#define SCB_ICSR_PENDSVCLR_Pos          27U
#define SCB_ICSR_PENDSVCLR_Msk          (1UL << SCB_ICSR_PENDSVCLR_Pos)
#define SCB_ICSR_PENDSTSET_Pos          26U
#define SCB_ICSR_PENDSTSET_Msk          (1UL << SCB_ICSR_PENDSTSET_Pos)
#define SCB_ICSR_PENDSTCLR_Pos          25U
#define SCB_ICSR_PENDSTCLR_Msk          (1UL << SCB_ICSR_PENDSTCLR_Pos)

/* AIRCR */
#define SCB_AIRCR_VECTKEY_Pos           16U
#define SCB_AIRCR_VECTKEY_Msk           (0xFFFFUL << SCB_AIRCR_VECTKEY_Pos)
#define SCB_AIRCR_PRIGROUP_Pos          8U
#define SCB_AIRCR_PRIGROUP_Msk          (7UL << SCB_AIRCR_PRIGROUP_Pos)
#define SCB_AIRCR_SYSRESETREQ_Pos       2U
#define SCB_AIRCR_SYSRESETREQ_Msk       (1UL << SCB_AIRCR_SYSRESETREQ_Pos)

// ============================================================================
//                           SysTick 寄存器位定义
// ============================================================================

#define SysTick_CTRL_COUNTFLAG_Pos      16U
#define SysTick_CTRL_COUNTFLAG_Msk      (1UL << SysTick_CTRL_COUNTFLAG_Pos)
#define SysTick_CTRL_CLKSOURCE_Pos      2U
#define SysTick_CTRL_CLKSOURCE_Msk      (1UL << SysTick_CTRL_CLKSOURCE_Pos)
#define SysTick_CTRL_TICKINT_Pos        1U
#define SysTick_CTRL_TICKINT_Msk        (1UL << SysTick_CTRL_TICKINT_Pos)
#define SysTick_CTRL_ENABLE_Pos         0U
#define SysTick_CTRL_ENABLE_Msk         (1UL << SysTick_CTRL_ENABLE_Pos)

#define SysTick_LOAD_RELOAD_Msk         (0xFFFFFFUL)
#define SysTick_VAL_CURRENT_Msk         (0xFFFFFFUL)

// ============================================================================
//                           NVIC 函数
// ============================================================================

__STATIC_INLINE void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) {
    if ((int32_t)(IRQn) < 0) {
        SCB->SHP[(((uint32_t)(int32_t)IRQn) & 0xFUL)-4UL] =
            (uint8_t)((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL);
    } else {
        NVIC->IP[((uint32_t)(int32_t)IRQn)] =
            (uint8_t)((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL);
    }
}

__STATIC_INLINE uint32_t NVIC_GetPriority(IRQn_Type IRQn) {
    if ((int32_t)(IRQn) < 0) {
        return (((uint32_t)SCB->SHP[(((uint32_t)(int32_t)IRQn) & 0xFUL)-4UL]) >> (8U - __NVIC_PRIO_BITS));
    } else {
        return (((uint32_t)NVIC->IP[((uint32_t)(int32_t)IRQn)]) >> (8U - __NVIC_PRIO_BITS));
    }
}

__STATIC_INLINE void NVIC_EnableIRQ(IRQn_Type IRQn) {
    NVIC->ISER[(((uint32_t)(int32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)(int32_t)IRQn) & 0x1FUL));
}

__STATIC_INLINE void NVIC_DisableIRQ(IRQn_Type IRQn) {
    NVIC->ICER[(((uint32_t)(int32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)(int32_t)IRQn) & 0x1FUL));
}

__STATIC_INLINE void NVIC_ClearPendingIRQ(IRQn_Type IRQn) {
    NVIC->ICPR[(((uint32_t)(int32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)(int32_t)IRQn) & 0x1FUL));
}

// ============================================================================
//                           SysTick 函数
// ============================================================================

__STATIC_INLINE uint32_t SysTick_Config(uint32_t ticks) {
    if ((ticks - 1UL) > SysTick_LOAD_RELOAD_Msk) {
        return 1UL;
    }

    SysTick->LOAD = (uint32_t)(ticks - 1UL);
    NVIC_SetPriority(SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
    SysTick->VAL = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
    return 0UL;
}

// ============================================================================
//                           系统复位
// ============================================================================

__STATIC_INLINE void NVIC_SystemReset(void) {
    __DSB();
    SCB->AIRCR = ((0x5FAUL << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk);
    __DSB();
    for (;;) {
        __NOP();
    }
}

#ifdef __cplusplus
}
#endif

#endif /* __CORE_CM3_H_GENERIC */
