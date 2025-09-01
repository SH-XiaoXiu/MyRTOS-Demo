#ifndef MYRTOS_CONFIG_H
#define MYRTOS_CONFIG_H

/*===========================================================================*
 *                      �����ں�����                         *
 *===========================================================================*/

// CPU����ʱ��Ƶ�� (��λ: Hz)
// ���ڼ��� SysTick ������ֵ��Ӧ�� SystemCoreClock һ��
#define MYRTOS_CPU_CLOCK_HZ                (168000000UL)

// RTOS ϵͳ���ģ�Tick����Ƶ�� (��λ: Hz)
// �Ƽ�ֵΪ 1000����ÿ 1ms ����һ��Tick�ж�
// ���ߵ�Ƶ�ʻ����ʱ��Ƭ���ȵľ��Ⱥ�������ʱ�ķֱ��ʣ���Ҳ������ϵͳ����
#define MYRTOS_TICK_RATE_HZ                (1000UL)

// ����������ȼ���
// ��������ȼ���Χ�Ǵ� 0 (���) �� (MY_RTOS_MAX_PRIORITIES - 1) (���)
// ���磬����Ϊ 32�������ȼ���ΧΪ 0-31
#define MYRTOS_MAX_PRIORITIES              (32)

// ϵͳ֧�ֵ���󲢷�������
// ���ֵ����������ID�صĴ�С
// ���磬����Ϊ 64��ϵͳ������ͬʱ����64������
#define MYRTOS_MAX_CONCURRENT_TASKS        (64)

// �������������������ȴ���Tick����ֵ
// ͨ����һ���޷������������ֵ
#define MYRTOS_MAX_DELAY                   (0xFFFFFFFFUL)
/*===========================================================================*
 *                      �ڴ�����                 *
 *===========================================================================*/

// RTOS�ں˹�����ڴ�ѵ��ܴ�С (��λ: �ֽ�)
// ����ͨ�� MyRTOS_Malloc() ������ڴ棨��TCB, ����ջ, ���д洢���������������
// ��С��Ҫ��������Ӧ����ϸ����
#define MYRTOS_MEMORY_POOL_SIZE               (64 * 1024)

// �ڴ����Ķ����ֽ���
// ����32λ��������ͨ������Ϊ 8 ��ȷ��������ܺͼ�����
// ������2����
#define MYRTOS_HEAP_BYTE_ALIGNMENT                 (8)


/*===========================================================================*
 *                         ���ô�����                  *
 *===========================================================================*/

#if MYRTOS_MAX_PRIORITIES > 32
#error "MY_RTOS_MAX_PRIORITIES must be less than or equal to 32."
#endif

#if MYRTOS_MAX_CONCURRENT_TASKS > 64
#error "MYRTOS_MAX_CONCURRENT_TASKS must be less than or equal to 64 due to the current taskIdBitmap implementation."
#endif

#if (MYRTOS_CPU_CLOCK_HZ / MYRTOS_TICK_RATE_HZ) > 0xFFFFFF
#error "SysTick reload value is too large. Either decrease MY_RTOS_CPU_CLOCK_HZ or increase MY_RTOS_TICK_RATE_HZ."
#endif


#endif /* MYRTOS_CONFIG_H */
