#include "MyRTOS.h"
#include "MyRTOS_Port.h"
#include <string.h>
#include "MyRTOS_Extension.h"
#include "MyRTOS_Kernel_Private.h"

/*===========================================================================*
* ˽�к�������
*===========================================================================*/

// ��ʼ���ڴ��
static void heapInit(void);

// ���ڴ����뵽����������
static void insertBlockIntoFreeList(BlockLink_t *blockToInsert);

// RTOS�ڲ�ʹ�õ��ڴ���亯��
static void *rtos_malloc(const size_t wantedSize);

// RTOS�ڲ�ʹ�õ��ڴ��ͷź���
static void rtos_free(void *pv);

// ��������ӵ���������
static void addTaskToReadyList(TaskHandle_t task);

// ��ͨ���������Ƴ�����
static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t taskToRemove);

// ������������ȼ�
static void task_set_priority(TaskHandle_t task, uint8_t newPriority);

// ��������ӵ�������ʱ��������ӳ�������
static void addTaskToSortedDelayList(TaskHandle_t task);

// ��ʼ���¼��б�
static void eventListInit(EventList_t *pEventList);

// ��������뵽�¼��б������ȼ�����
static void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert);

// �����������¼��б����Ƴ�����
static void eventListRemove(TaskHandle_t taskToRemove);

// �㲥�ں��¼���������ע�����չ
static void broadcast_event(const KernelEventData_t *pEventData);

// �ڴ�����������С�ڴ���С����������������BlockLink_t�ṹ��
#define HEAP_MINIMUM_BLOCK_SIZE ((sizeof(BlockLink_t) * 2))

// �ڴ�����ṹ�Ĵ�С���ѿ����ڴ����
static const size_t heapStructSize = (sizeof(BlockLink_t) + (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) & ~((
                                         (size_t) MYRTOS_HEAP_BYTE_ALIGNMENT - 1));

//�ڴ�ѹ���
// ��̬������ڴ�أ�����RTOS�Ķ�̬�ڴ����
static uint8_t rtos_memory_pool[MYRTOS_MEMORY_POOL_SIZE] __attribute__((aligned(MYRTOS_HEAP_BYTE_ALIGNMENT)));
// �����ڴ���������ʼ�ڱ��ڵ�
static BlockLink_t start;
// �ڴ�ѵĽ����ڱ��ڵ�
static BlockLink_t *blockLinkEnd = NULL;
// ��ǰʣ��Ŀ����ֽ���
size_t freeBytesRemaining = 0U;
// ���ڱ���ڴ���Ƿ��ѱ������λ���� (���λ)
static size_t blockAllocatedBit = 0;

//�ں�״̬
// �������Ƿ��������ı�־
volatile uint8_t g_scheduler_started = 0;
// �ٽ���Ƕ�׼���
volatile uint32_t criticalNestingCount = 0;
// ϵͳ�δ������
static volatile uint64_t systemTickCount = 0;
// �����Ѵ������������ͷ
TaskHandle_t allTaskListHead = NULL;
// ��ǰ�������е�����ľ��
TaskHandle_t currentTask = NULL;
// ��������ľ��
TaskHandle_t idleTask = NULL;
// ���ڷ�����һ������ID�ļ�����
static uint32_t nextTaskId = 0;
// ����ID��λͼ�����ڿ��ٲ��ҿ��õ�ID
static uint64_t taskIdBitmap = 0;
// �����ȼ���֯�ľ���������������
static TaskHandle_t readyTaskLists[MYRTOS_MAX_PRIORITIES];
// �ӳ����������ͷ
static TaskHandle_t delayedTaskListHead = NULL;
// һ��λͼ�����ڿ��ٲ��ҵ�ǰ���ڵ�������ȼ��ľ�������
static volatile uint32_t topReadyPriority = 0;
// �ں���չ�ص���������
static KernelExtensionCallback_t g_extensions[MAX_KERNEL_EXTENSIONS] = {NULL};
// ��ע����ں���չ����
static uint8_t g_extension_count = 0;

/**
 * @brief ��ʼ���ڴ��
 * @note  �˺������������ڴ�أ�������ʼ�Ŀ����ڴ�飬��������ʼ�ͽ����ڱ��ڵ㡣
 */
static void heapInit(void) {
    BlockLink_t *firstFreeBlock;
    uint8_t *alignedHeap;
    size_t address = (size_t) rtos_memory_pool;
    size_t totalHeapSize = MYRTOS_MEMORY_POOL_SIZE;
    // ȷ���ѵ���ʼ��ַ�Ƕ����
    if ((address & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) != 0) {
        address += (MYRTOS_HEAP_BYTE_ALIGNMENT - (address & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)));
        totalHeapSize -= address - (size_t) rtos_memory_pool;
    }
    alignedHeap = (uint8_t *) address;
    // ������ʼ�ڱ��ڵ�
    start.nextFreeBlock = (BlockLink_t *) alignedHeap;
    start.blockSize = (size_t) 0;
    // ���ý����ڱ��ڵ�
    address = ((size_t) alignedHeap) + totalHeapSize - heapStructSize;
    blockLinkEnd = (BlockLink_t *) address;
    blockLinkEnd->blockSize = 0;
    blockLinkEnd->nextFreeBlock = NULL;
    // ������һ����Ŀ����ڴ��
    firstFreeBlock = (BlockLink_t *) alignedHeap;
    firstFreeBlock->blockSize = address - (size_t) firstFreeBlock;
    firstFreeBlock->nextFreeBlock = blockLinkEnd;
    // ��ʼ��ʣ������ֽ���
    freeBytesRemaining = firstFreeBlock->blockSize;
    // �������ڱ�ǡ��ѷ��䡱��λ�����λ��
    blockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * 8) - 1);
}

/**
 * @brief ��һ���ڴ����뵽����������
 * @note  �˺����ᰴ��ַ˳������ڴ�飬�����������ڵĿ��п�ϲ���
 * @param blockToInsert Ҫ������ڴ��ָ��
 */
static void insertBlockIntoFreeList(BlockLink_t *blockToInsert) {
    BlockLink_t *iterator;
    uint8_t *puc;
    // �������������ҵ����ʵĲ���λ��
    for (iterator = &start; iterator->nextFreeBlock < blockToInsert; iterator = iterator->nextFreeBlock) {
        // ��ѭ������Ϊ�ƶ�������
    }
    // ������ǰһ�����п�ϲ�
    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        iterator->blockSize += blockToInsert->blockSize;
        blockToInsert = iterator;
    } else {
        blockToInsert->nextFreeBlock = iterator->nextFreeBlock;
    }
    // �������һ�����п�ϲ�
    puc = (uint8_t *) blockToInsert;
    if ((puc + blockToInsert->blockSize) == (uint8_t *) iterator->nextFreeBlock) {
        if (iterator->nextFreeBlock != blockLinkEnd) {
            blockToInsert->blockSize += iterator->nextFreeBlock->blockSize;
            blockToInsert->nextFreeBlock = iterator->nextFreeBlock->nextFreeBlock;
        }
    }
    // ���û�з���ǰ��ϲ����򽫵�ǰ�����ӵ�������
    if (iterator != blockToInsert) {
        iterator->nextFreeBlock = blockToInsert;
    }
}

/**
 * @brief RTOS�ڲ�ʹ�õ��ڴ���亯��
 * @note  ʵ�����״���Ӧ(First Fit)�㷨�����Һ��ʵ��ڴ�顣
 * @param wantedSize ���������ֽ���
 * @return �ɹ��򷵻ط�����ڴ�ָ�룬ʧ���򷵻�NULL
 */
static void *rtos_malloc(const size_t wantedSize) {
    BlockLink_t *block, *previousBlock, *newBlockLink;
    void *pvReturn = NULL;
    MyRTOS_Port_EnterCritical(); {
        // �������δ��ʼ��������г�ʼ��
        if (blockLinkEnd == NULL) {
            heapInit();
        }
        if ((wantedSize > 0) && ((wantedSize & blockAllocatedBit) == 0)) {
            // �����������ṹ�Ͷ������ܴ�С
            size_t totalSize = heapStructSize + wantedSize;
            if ((totalSize & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) != 0) {
                totalSize += (MYRTOS_HEAP_BYTE_ALIGNMENT - (totalSize & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)));
            }
            if (totalSize <= freeBytesRemaining) {
                // �����������������㹻����ڴ��
                previousBlock = &start;
                block = start.nextFreeBlock;
                while ((block->blockSize < totalSize) && (block->nextFreeBlock != NULL)) {
                    previousBlock = block;
                    block = block->nextFreeBlock;
                }
                // ����ҵ��˺��ʵĿ�
                if (block != blockLinkEnd) {
                    pvReturn = (void *) (((uint8_t *) block) + heapStructSize);
                    previousBlock->nextFreeBlock = block->nextFreeBlock;
                    // ���ʣ�ಿ���㹻������ѳ�һ���µĿ��п�
                    if ((block->blockSize - totalSize) > HEAP_MINIMUM_BLOCK_SIZE) {
                        newBlockLink = (BlockLink_t *) (((uint8_t *) block) + totalSize);
                        newBlockLink->blockSize = block->blockSize - totalSize;
                        block->blockSize = totalSize;
                        insertBlockIntoFreeList(newBlockLink);
                    }
                    freeBytesRemaining -= block->blockSize;
                    // ��Ǹÿ�Ϊ�ѷ���
                    block->blockSize |= blockAllocatedBit;
                    block->nextFreeBlock = NULL;
                }
            }
        }
        // �������ʧ���������С����0���������
        if (pvReturn == NULL && wantedSize > 0) {
            MyRTOS_ReportError(KERNEL_ERROR_MALLOC_FAILED, (void *) wantedSize);
        }
    }
    MyRTOS_Port_ExitCritical();
    return pvReturn;
}

/**
 * @brief RTOS�ڲ�ʹ�õ��ڴ��ͷź���
 * @note  ���ͷŵ��ڴ�����²��뵽���������У������Ժϲ���
 * @param pv Ҫ�ͷŵ��ڴ�ָ��
 */
static void rtos_free(void *pv) {
    if (pv == NULL) return;
    uint8_t *puc = (uint8_t *) pv;
    BlockLink_t *link;
    // ���û�ָ����˵��ڴ��Ĺ���ṹ
    puc -= heapStructSize;
    link = (BlockLink_t *) puc;
    // ���ÿ��Ƿ�ȷʵ���ѷ���״̬
    if (((link->blockSize & blockAllocatedBit) != 0) && (link->nextFreeBlock == NULL)) {
        // ����ѷ����־
        link->blockSize &= ~blockAllocatedBit;
        MyRTOS_Port_EnterCritical(); {
            // ����ʣ������ֽ����������ؿ�������
            freeBytesRemaining += link->blockSize;
            insertBlockIntoFreeList(link);
        }
        MyRTOS_Port_ExitCritical();
    }
}

/**
 * @brief ��һ��ͨ��˫���������Ƴ�����
 * @param ppListHead ָ������ͷָ���ָ��
 * @param taskToRemove Ҫ�Ƴ���������
 */
static void removeTaskFromList(TaskHandle_t *ppListHead, TaskHandle_t taskToRemove) {
    if (taskToRemove == NULL) return;
    // ����ǰһ���ڵ�� next ָ��
    if (taskToRemove->pPrevGeneric != NULL) {
        taskToRemove->pPrevGeneric->pNextGeneric = taskToRemove->pNextGeneric;
    } else {
        // �����ͷ�ڵ㣬���������ͷ
        *ppListHead = taskToRemove->pNextGeneric;
    }
    // ���º�һ���ڵ�� prev ָ��
    if (taskToRemove->pNextGeneric != NULL) {
        taskToRemove->pNextGeneric->pPrevGeneric = taskToRemove->pPrevGeneric;
    }
    // �����������������ָ��
    taskToRemove->pNextGeneric = NULL;
    taskToRemove->pPrevGeneric = NULL;
    // ����ǴӾ����������Ƴ�����Ҫ����Ƿ���Ҫ������ȼ�λͼ�еĶ�Ӧλ
    if (taskToRemove->state == TASK_STATE_READY) {
        if (readyTaskLists[taskToRemove->priority] == NULL) {
            topReadyPriority &= ~(1UL << taskToRemove->priority);
        }
    }
}

/**
 * @brief ��̬�ı���������ȼ�
 * @param task Ŀ��������
 * @param newPriority �µ����ȼ�
 */
static void task_set_priority(TaskHandle_t task, uint8_t newPriority) {
    if (task->priority == newPriority) return;
    // ��������Ǿ���״̬����Ҫ�Ӿɵľ��������Ƶ��µľ�������
    if (task->state == TASK_STATE_READY) {
        MyRTOS_Port_EnterCritical();
        removeTaskFromList(&readyTaskLists[task->priority], task);
        task->priority = newPriority;
        addTaskToReadyList(task);
        MyRTOS_Port_ExitCritical();
    } else {
        // ��������Ǿ���״̬��ֱ���޸����ȼ�����
        task->priority = newPriority;
    }
}

/**
 * @brief ��������ӵ�������ʱ��������ӳ�������
 * @param task Ҫ��ӵ�������
 */
static void addTaskToSortedDelayList(TaskHandle_t task) {
    const uint64_t wakeUpTime = task->delay;
    // ���뵽����ͷ
    if (delayedTaskListHead == NULL || wakeUpTime < delayedTaskListHead->delay) {
        task->pNextGeneric = delayedTaskListHead;
        task->pPrevGeneric = NULL;
        if (delayedTaskListHead != NULL) delayedTaskListHead->pPrevGeneric = task;
        delayedTaskListHead = task;
    } else {
        // ���������ҵ����ʵĲ���λ��
        Task_t *iterator = delayedTaskListHead;
        while (iterator->pNextGeneric != NULL && iterator->pNextGeneric->delay <= wakeUpTime) {
            iterator = iterator->pNextGeneric;
        }
        // ���뵽�����м��β��
        task->pNextGeneric = iterator->pNextGeneric;
        if (iterator->pNextGeneric != NULL) iterator->pNextGeneric->pPrevGeneric = task;
        iterator->pNextGeneric = task;
        task->pPrevGeneric = iterator;
    }
}

/**
 * @brief ��������ӵ���Ӧ���ȼ��ľ�������ĩβ
 * @param task Ҫ��ӵ�������
 */
static void addTaskToReadyList(TaskHandle_t task) {
    if (task == NULL || task->priority >= MYRTOS_MAX_PRIORITIES) return;
    MyRTOS_Port_EnterCritical(); {
        // ���ö�Ӧ���ȼ���λͼ��־
        topReadyPriority |= (1UL << task->priority);
        task->pNextGeneric = NULL;
        // ��������ӵ���������ĩβ
        if (readyTaskLists[task->priority] == NULL) {
            readyTaskLists[task->priority] = task;
            task->pPrevGeneric = NULL;
        } else {
            Task_t *pLast = readyTaskLists[task->priority];
            while (pLast->pNextGeneric != NULL) pLast = pLast->pNextGeneric;
            pLast->pNextGeneric = task;
            task->pPrevGeneric = pLast;
        }
        // ��������״̬
        task->state = TASK_STATE_READY;
    }
    MyRTOS_Port_ExitCritical();
}

/**
 * @brief ��ʼ��һ���¼��б�
 * @param pEventList ָ��Ҫ��ʼ�����¼��б��ָ��
 */
static void eventListInit(EventList_t *pEventList) {
    pEventList->head = NULL;
}

/**
 * @brief ���������ȼ����뵽�¼��ȴ��б���
 * @param pEventList Ŀ���¼��б�
 * @param taskToInsert Ҫ���������
 */
static void eventListInsert(EventList_t *pEventList, TaskHandle_t taskToInsert) {
    taskToInsert->pEventList = pEventList;
    // ���뵽����ͷ��������������ȼ���ߣ�
    if (pEventList->head == NULL || pEventList->head->priority <= taskToInsert->priority) {
        taskToInsert->pNextEvent = pEventList->head;
        pEventList->head = taskToInsert;
    } else {
        // �����ҵ������ȼ��������ȷλ��
        Task_t *iterator = pEventList->head;
        while (iterator->pNextEvent != NULL && iterator->pNextEvent->priority > taskToInsert->priority) {
            iterator = iterator->pNextEvent;
        }
        taskToInsert->pNextEvent = iterator->pNextEvent;
        iterator->pNextEvent = taskToInsert;
    }
}

/**
 * @brief ������ǰ�ȴ����¼��б����Ƴ�������
 * @param taskToRemove Ҫ�Ƴ�������
 */
static void eventListRemove(TaskHandle_t taskToRemove) {
    if (taskToRemove->pEventList == NULL) return;
    EventList_t *pEventList = taskToRemove->pEventList;
    // ����������¼��б��ͷ�ڵ�
    if (pEventList->head == taskToRemove) {
        pEventList->head = taskToRemove->pNextEvent;
    } else {
        // �������Ҳ��Ƴ�����
        Task_t *iterator = pEventList->head;
        while (iterator != NULL && iterator->pNextEvent != taskToRemove) {
            iterator = iterator->pNextEvent;
        }
        if (iterator != NULL) {
            iterator->pNextEvent = taskToRemove->pNextEvent;
        }
    }
    // ���������е��¼��б����ָ��
    taskToRemove->pNextEvent = NULL;
    taskToRemove->pEventList = NULL;
}

/**
 * @brief ��������ע����ں���չ�㲥һ���¼�
 * @param pEventData Ҫ�㲥���¼�����
 */
static void broadcast_event(const KernelEventData_t *pEventData) {
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i]) {
            g_extensions[i](pEventData);
        }
    }
}

/*===========================================================================*
* �����ӿ�ʵ��
*===========================================================================*/

/**
 * @brief ��ʼ��RTOS�ں�
 * @note  �˺��������ڴ����κ����������������֮ǰ���á�
 *        �����ʼ�����е��ں����ݽṹ��
 */
void MyRTOS_Init(void) {
    allTaskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    nextTaskId = 0;
    for (int i = 0; i < MYRTOS_MAX_PRIORITIES; i++) {
        readyTaskLists[i] = NULL;
    }
    delayedTaskListHead = NULL;
    topReadyPriority = 0;
}

/**
 * @brief ����RTOS������
 * @note  �˺������᷵�ء����ᴴ����������Ȼ��������һ�������ִ�С�
 *        �ڴ�֮������������ں˽ӹܡ�
 * @param idle_task_func ��������ĺ���ָ�롣��û���������������ʱ����ִ�д�����
 */
void Task_StartScheduler(void (*idle_task_func)(void *)) {
    // �����ṩһ����Ч�Ŀ���������
    if (idle_task_func == NULL) {
        while (1);
    }
    // �����������������ȼ�Ϊ���
    idleTask = Task_Create(idle_task_func, "IDLE", 128, NULL, 0);
    if (idleTask == NULL) {
        // ����������񴴽�ʧ�ܣ�ϵͳ�޷�����
        while (1);
    }
    // ��ǵ�����������
    g_scheduler_started = 1;
    // �ֶ�����һ�ε�����ѡ���һ��Ҫ���е�����
    MyRTOS_Schedule();
    // ������ֲ�������������������ͨ�������õ�һ������Ķ�ջָ�벢����SVC/PendSV��
    if (MyRTOS_Port_StartScheduler() != 0) {
        // �˴���Ӧ�ñ�ִ�е�
    }
}

/**
 * @brief ��ȡ��ǰϵͳ�δ����
 * @note  �˺������̰߳�ȫ�ġ�
 * @return �����Ե��������������ĵδ���
 */
uint64_t MyRTOS_GetTick(void) {
    MyRTOS_Port_EnterCritical();
    const uint64_t tick_value = systemTickCount;
    MyRTOS_Port_ExitCritical();
    return tick_value;
}

/**
 * @brief ϵͳ�δ��жϴ�����
 * @note  �˺���Ӧ��ϵͳ�δ�ʱ���жϣ���SysTick_Handler���е��á�
 *        ����������ϵͳ�δ������������Ƿ����ӳٵ�������Ҫ�����ѡ�
 */
int MyRTOS_Tick_Handler(void) {
    int higherPriorityTaskWoken = 0;
    // ����ϵͳ�δ����
    systemTickCount++;
    const uint64_t current_tick = systemTickCount;
    // ����ӳ�����ͷ�����Ƿ�������Ļ���ʱ���ѵ�
    while (delayedTaskListHead != NULL && delayedTaskListHead->delay <= current_tick) {
        Task_t *taskToWake = delayedTaskListHead;
        // ���ӳ��������Ƴ�
        removeTaskFromList(&delayedTaskListHead, taskToWake);
        taskToWake->delay = 0;
        // ��ӵ���������
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) {
            higherPriorityTaskWoken = 1;
        }
    }
    // �㲥�δ��¼�
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TICK};
    broadcast_event(&eventData);
    return higherPriorityTaskWoken;
}

/**
 * @brief ���������Ƿ���������
 * @return ���������������������1�����򷵻�0
 */
uint8_t MyRTOS_Schedule_IsRunning(void) {
    return g_scheduler_started;
}

/**
 * @brief ����һ���ں˼�����
 * @note  �˺���ͨ���ں���չ���ƹ㲥�����¼���������Թ��߻��û����벶��ʹ�����Щ����
 * @param error_type ��������
 * @param p_context �������ص�����������ָ��
 */
void MyRTOS_ReportError(KernelErrorType_t error_type, void *p_context) {
    KernelEventData_t eventData;
    // ���������¼����ݽṹ
    memset(&eventData, 0, sizeof(eventData));

    eventData.p_context_data = p_context;

    switch (error_type) {
        case KERNEL_ERROR_STACK_OVERFLOW:
            eventData.eventType = KERNEL_EVENT_HOOK_STACK_OVERFLOW;
            eventData.task = (TaskHandle_t) p_context;
            break;

        case KERNEL_ERROR_MALLOC_FAILED:
            eventData.eventType = KERNEL_EVENT_HOOK_MALLOC_FAILED;
            eventData.mem.size = (size_t) p_context;
            eventData.mem.ptr = NULL;
            break;

        case KERNEL_ERROR_HARD_FAULT:
            eventData.eventType = KERNEL_EVENT_ERROR_HARD_FAULT;
            break;
        default:
            return; // δ֪�Ĵ������ͣ�������
    }

    broadcast_event(&eventData);
}

/**
 * @brief ���Ȳ�ѡ����һ��Ҫ���е�����
 * @note  ����RTOS���ȵĺ��ġ������ҵ�������ȼ��ľ������񲢽�������Ϊ��ǰ����
 *        �˺�����PendSV�жϷ�����������ִ���������л���
 * @return ������һ��Ҫ��������Ķ�ջָ�� (SP)
 */
void *schedule_next_task(void) {
    TaskHandle_t prevTask = currentTask;
    TaskHandle_t nextTaskToRun = NULL;
    // �㲥�����г��¼�
    if (prevTask) {
        KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_SWITCH_OUT, .task = prevTask};
        broadcast_event(&eventData);
    }
    // ���û�о���������ѡ���������
    if (topReadyPriority == 0) {
        nextTaskToRun = idleTask;
    } else {
        // �ҵ�������ȼ��ľ�������
        // `__builtin_clz` ��һ��GCC/Clang���ú��������ڼ���ǰ��������������Ը�Ч���ҵ������λ
        uint32_t highestPriority = 31 - __builtin_clz(topReadyPriority);
        nextTaskToRun = readyTaskLists[highestPriority];
        // ʵ��ͬ���ȼ��������ת���� (Round-Robin)
        if (nextTaskToRun != NULL && nextTaskToRun->pNextGeneric != NULL) {
            // ����ǰͷ�ڵ��ƶ�������β��
            readyTaskLists[highestPriority] = nextTaskToRun->pNextGeneric;
            nextTaskToRun->pNextGeneric->pPrevGeneric = NULL;
            Task_t *pLast = readyTaskLists[highestPriority];
            if (pLast != NULL) {
                while (pLast->pNextGeneric != NULL) pLast = pLast->pNextGeneric;
                pLast->pNextGeneric = nextTaskToRun;
                nextTaskToRun->pPrevGeneric = pLast;
                nextTaskToRun->pNextGeneric = NULL;
            } else {
                // ���������ֻ��һ��Ԫ�أ�������Ȼ��ͷ
                readyTaskLists[highestPriority] = nextTaskToRun;
                nextTaskToRun->pPrevGeneric = NULL;
                nextTaskToRun->pNextGeneric = NULL;
            }
        }
    }
    // ���µ�ǰ����
    currentTask = nextTaskToRun;
    // �㲥���������¼�
    if (currentTask) {
        KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_SWITCH_IN, .task = currentTask};
        broadcast_event(&eventData);
    }
    if (currentTask == NULL) return NULL; // �����ϲ�Ӧ��������Ϊ����idleTask
    // ����������Ķ�ջָ�����ֲ��
    return currentTask->sp;
}

/**
 * @brief �ֶ�����һ���������
 * @note  ͨ��������״̬�����ı�����磬���ӳٱ�Ϊ���������ã���ȷ��������ȼ��������ܹ����С�
 *        ��RTOS��API�ڲ�����ͨ��ͨ�� `MyRTOS_Port_Yield()` ʵ�֡�
 */
void MyRTOS_Schedule(void) {
    schedule_next_task();
}

// =============================
// Memory Management API
// =============================
/**
 * @brief ��̬�����ڴ�
 * @note  �˺������̰߳�ȫ�ġ��ڲ�ʹ�� `rtos_malloc` ���㲥һ���ڴ�����¼���
 * @param wantedSize Ҫ������ڴ��С���ֽڣ�
 * @return �ɹ��򷵻�ָ���ѷ����ڴ��ָ�룬ʧ���򷵻�NULL
 */
void *MyRTOS_Malloc(size_t wantedSize) {
    void *pv = rtos_malloc(wantedSize);
    // �㲥�ڴ�����¼�
    KernelEventData_t eventData = {
        .eventType = KERNEL_EVENT_MALLOC,
        .mem = {.ptr = pv, .size = wantedSize}
    };
    broadcast_event(&eventData);
    return pv;
}

/**
 * @brief �ͷ���ǰ������ڴ�
 * @note  �˺������̰߳�ȫ�ġ��ڲ�ʹ�� `rtos_free` ���㲥һ���ڴ��ͷ��¼���
 * @param pv Ҫ�ͷŵ��ڴ�ָ�룬������ͨ�� `MyRTOS_Malloc` �����
 */
void MyRTOS_Free(void *pv) {
    if (pv) {
        // ���ͷ�ǰ��ȡ���С�������¼��㲥
        BlockLink_t *link = (BlockLink_t *) ((uint8_t *) pv - heapStructSize);
        KernelEventData_t eventData = {
            .eventType = KERNEL_EVENT_FREE,
            .mem = {.ptr = pv, .size = (link->blockSize & ~blockAllocatedBit)}
        };
        broadcast_event(&eventData);
    }
    rtos_free(pv);
}

/**
 * @brief ����һ��������
 * @param func ������ָ��
 * @param taskName �������ƣ��ַ����������ڵ���
 * @param stack_size �����ջ��С����StackType_tΪ��λ��ͨ����4�ֽڣ�
 * @param param ���ݸ��������Ĳ���
 * @param priority �������ȼ� (0��������ȼ�)
 * @return �ɹ��򷵻���������ʧ���򷵻�NULL
 */
TaskHandle_t Task_Create(void (*func)(void *), const char *taskName, uint16_t stack_size, void *param,
                         uint8_t priority) {
    if (priority >= MYRTOS_MAX_PRIORITIES || func == NULL) return NULL;
    // Ϊ������ƿ飨TCB�������ڴ�
    Task_t *t = MyRTOS_Malloc(sizeof(Task_t));
    if (t == NULL) return NULL;
    // Ϊ�����ջ�����ڴ�
    StackType_t *stack = MyRTOS_Malloc(stack_size * sizeof(StackType_t));
    if (stack == NULL) {
        MyRTOS_Free(t);
        return NULL;
    }
    // ����һ��Ψһ������ID
    uint32_t newTaskId = (uint32_t) -1;
    MyRTOS_Port_EnterCritical();
    if (taskIdBitmap != 0xFFFFFFFFFFFFFFFFULL) {
        newTaskId = __builtin_ctzll(~taskIdBitmap); // ����λͼ�е�һ��Ϊ0��λ
        taskIdBitmap |= (1ULL << newTaskId);
    }
    MyRTOS_Port_ExitCritical();
    if (newTaskId == (uint32_t) -1) {
        // ���û�п��õ�����ID
        MyRTOS_Free(stack);
        MyRTOS_Free(t);
        return NULL;
    }
    // ��ʼ��������ƿ飨TCB��
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    t->taskId = newTaskId;
    t->stack_base = stack;
    t->priority = priority;
    t->basePriority = priority;
    t->pNextTask = NULL;
    t->pNextGeneric = NULL;
    t->pPrevGeneric = NULL;
    t->pNextEvent = NULL;
    t->pEventList = NULL;
    t->held_mutexes_head = NULL;
    t->eventData = NULL;
    t->taskName = taskName;
    t->stackSize_words = stack_size;
    // ʹ��ħ����������ջ�����ڶ�ջ������
    for (uint16_t i = 0; i < stack_size; ++i) {
        stack[i] = 0xA5A5A5A5;
    }
    // ������ֲ������ʼ�������ջ��ģ��CPU�����ģ�
    t->sp = MyRTOS_Port_InitialiseStack(stack + stack_size, func, param);
    MyRTOS_Port_EnterCritical(); {
        // ����������ӵ�ȫ�������б���
        if (allTaskListHead == NULL) {
            allTaskListHead = t;
        } else {
            Task_t *p = allTaskListHead;
            while (p->pNextTask != NULL) p = p->pNextTask;
            p->pNextTask = t;
        }
        // ����������ӵ������б�
        addTaskToReadyList(t);
    }
    MyRTOS_Port_ExitCritical();
    // �㲥���񴴽��¼�
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_CREATE, .task = t};
    broadcast_event(&eventData);
    return t;
}

/**
 * @brief ɾ��һ������
 * @param task_h Ҫɾ���������������ΪNULL����ɾ����ǰ����
 * @return �ɹ�����0��ʧ�ܷ���-1 (���磬����ɾ����������)
 */
int Task_Delete(TaskHandle_t task_h) {
    Task_t *task_to_delete = (task_h == NULL) ? currentTask : task_h;
    // ������ɾ����������
    if (task_to_delete == idleTask || task_to_delete == NULL) return -1;
    uint32_t deleted_task_id = task_to_delete->taskId;
    // �㲥����ɾ���¼�
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_TASK_DELETE, .task = task_to_delete};
    broadcast_event(&eventData);
    MyRTOS_Port_EnterCritical(); {
        // �������ڵ��κ��������Ƴ�����
        if (task_to_delete->state == TASK_STATE_READY) {
            removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
        } else if (task_to_delete->state == TASK_STATE_DELAYED || task_to_delete->state == TASK_STATE_BLOCKED) {
            removeTaskFromList(&delayedTaskListHead, task_to_delete);
        }
        // ����������ڵȴ��¼���Ҳ���¼��б����Ƴ�
        if (task_to_delete->pEventList != NULL) {
            eventListRemove(task_to_delete);
        }
        // �ͷ�������е����л�����
        while (task_to_delete->held_mutexes_head != NULL) {
            Mutex_Unlock(task_to_delete->held_mutexes_head);
        }
        task_to_delete->state = TASK_STATE_UNUSED;
        // ��ȫ�������б����Ƴ�
        Task_t *prev = NULL, *curr = allTaskListHead;
        while (curr != NULL && curr != task_to_delete) {
            prev = curr;
            curr = curr->pNextTask;
        }
        if (curr != NULL) {
            if (prev == NULL) allTaskListHead = curr->pNextTask;
            else prev->pNextTask = curr->pNextTask;
        }
        void *stack_to_free = task_to_delete->stack_base;
        // �����ɾ������
        if (task_h == NULL) {
            currentTask = NULL; // ��ǵ�ǰ����Ϊ�գ���������ѡ��������
            taskIdBitmap &= ~(1ULL << deleted_task_id); // ��������ID
            MyRTOS_Free(task_to_delete);
            MyRTOS_Free(stack_to_free);
            MyRTOS_Port_ExitCritical();
            MyRTOS_Port_Yield(); // �������ȣ������񽫲���ִ��
        } else {
            // �����ɾ����������
            taskIdBitmap &= ~(1ULL << deleted_task_id); // ��������ID
            MyRTOS_Free(task_to_delete);
            MyRTOS_Free(stack_to_free);
            MyRTOS_Port_ExitCritical();
        }
    }
    return 0;
}

/**
 * @brief ����ǰ�����ӳ�ָ���ĵδ���
 * @param tick Ҫ�ӳٵ�ϵͳ�δ���
 */
void Task_Delay(uint32_t tick) {
    if (tick == 0 || g_scheduler_started == 0) return;
    MyRTOS_Port_EnterCritical(); {
        // �Ӿ����������Ƴ���ǰ����
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        // ���㻽��ʱ�䲢��������״̬
        currentTask->delay = MyRTOS_GetTick() + tick;
        currentTask->state = TASK_STATE_DELAYED;
        // ��������ӵ�������ӳ�������
        addTaskToSortedDelayList(currentTask);
    }
    MyRTOS_Port_ExitCritical();
    // ��������
    MyRTOS_Port_Yield();
}

/**
 * @brief ��һ��������֪ͨ���������ڵȴ�������
 * @param task_h Ŀ������ľ��
 * @return �ɹ�����0
 */
int Task_Notify(TaskHandle_t task_h) {
    int trigger_yield = 0;
    MyRTOS_Port_EnterCritical(); {
        // ���Ŀ�������Ƿ����ڵȴ�֪ͨ
        if (task_h->is_waiting_notification && task_h->state == TASK_STATE_BLOCKED) {
            task_h->is_waiting_notification = 0;
            addTaskToReadyList(task_h);
            // ��������ѵ��������ȼ����ߣ�����Ҫ���е���
            if (task_h->priority > currentTask->priority) {
                trigger_yield = 1;
            }
        }
    }
    MyRTOS_Port_ExitCritical();
    if (trigger_yield)
        MyRTOS_Port_Yield();
    return 0;
}

/**
 * @brief ���жϷ������(ISR)����������֪ͨ
 * @param task_h Ŀ������ľ��
 * @param higherPriorityTaskWoken ָ�룬���ڷ����Ƿ��и������ȼ������񱻻���
 * @return �ɹ�����0��ʧ�ܷ���-1
 */
int Task_NotifyFromISR(TaskHandle_t task_h, int *higherPriorityTaskWoken) {
    if (higherPriorityTaskWoken == NULL) return -1;
    *higherPriorityTaskWoken = 0;
    MyRTOS_Port_EnterCritical(); {
        if (task_h->is_waiting_notification && task_h->state == TASK_STATE_BLOCKED) {
            task_h->is_waiting_notification = 0;
            addTaskToReadyList(task_h);
            if (task_h->priority > currentTask->priority) {
                *higherPriorityTaskWoken = 1;
            }
        }
    }
    MyRTOS_Port_ExitCritical();
    return 0;
}

/**
 * @brief ʹ��ǰ�����������״̬���ȴ�֪ͨ
 * @note  ����һֱ������ֱ�� `Task_Notify` �� `Task_NotifyFromISR` ������
 */
void Task_Wait(void) {
    MyRTOS_Port_EnterCritical(); {
        // �Ӿ��������Ƴ�
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        // ���õȴ�֪ͨ��־������״̬
        currentTask->is_waiting_notification = 1;
        currentTask->state = TASK_STATE_BLOCKED;
    }
    MyRTOS_Port_ExitCritical();
    // ��������
    MyRTOS_Port_Yield();
}

/**
 * @brief ��ȡ����ĵ�ǰ״̬
 * @param task_h Ŀ������ľ��
 * @return ���������״̬ (TaskState_t)
 */
TaskState_t Task_GetState(TaskHandle_t task_h) {
    return task_h ? ((Task_t *) task_h)->state : TASK_STATE_UNUSED;
}

/**
 * @brief ��ȡ����ĵ�ǰ���ȼ�
 * @param task_h Ŀ������ľ��
 * @return ������������ȼ�
 */
uint8_t Task_GetPriority(TaskHandle_t task_h) {
    return task_h ? ((Task_t *) task_h)->priority : 0;
}

/**
 * @brief ��ȡ��ǰ�������е�����ľ��
 * @return ���ص�ǰ����ľ��
 */
TaskHandle_t Task_GetCurrentTaskHandle(void) {
    return currentTask;
}

/**
 * @brief ��ȡ�����ΨһID
 * @param task_h Ŀ������ľ��
 * @return ���������ID
 */
uint32_t Task_GetId(TaskHandle_t task_h) {
    if (!task_h) return (uint32_t) -1;
    return ((Task_t *) task_h)->taskId;
}

/**
 * @brief ����һ����Ϣ����
 * @param length �����ܹ��洢�������Ŀ��
 * @param itemSize ÿ����Ŀ�Ĵ�С���ֽڣ�
 * @return �ɹ��򷵻ض��о����ʧ���򷵻�NULL
 */
QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize) {
    if (length == 0 || itemSize == 0) return NULL;
    // ������п��ƽṹ�ڴ�
    Queue_t *queue = MyRTOS_Malloc(sizeof(Queue_t));
    if (queue == NULL) return NULL;
    // ������д洢���ڴ�
    queue->storage = (uint8_t *) MyRTOS_Malloc(length * itemSize);
    if (queue->storage == NULL) {
        MyRTOS_Free(queue);
        return NULL;
    }
    // ��ʼ����������
    queue->length = length;
    queue->itemSize = itemSize;
    queue->waitingCount = 0;
    queue->writePtr = queue->storage;
    queue->readPtr = queue->storage;
    eventListInit(&queue->sendEventList); // ��ʼ���ȴ����͵������б�
    eventListInit(&queue->receiveEventList); // ��ʼ���ȴ����յ������б�
    return queue;
}

/**
 * @brief ɾ��һ����Ϣ����
 * @note  �ỽ�����еȴ��ö��е�����
 * @param delQueue Ҫɾ���Ķ��о��
 */
void Queue_Delete(QueueHandle_t delQueue) {
    Queue_t *queue = delQueue;
    if (queue == NULL) return;
    MyRTOS_Port_EnterCritical(); {
        // �������еȴ����͵�����
        while (queue->sendEventList.head != NULL) {
            Task_t *taskToWake = queue->sendEventList.head;
            eventListRemove(taskToWake);
            addTaskToReadyList(taskToWake);
        }
        // �������еȴ����յ�����
        while (queue->receiveEventList.head != NULL) {
            Task_t *taskToWake = queue->receiveEventList.head;
            eventListRemove(taskToWake);
            addTaskToReadyList(taskToWake);
        }
        // �ͷ��ڴ�
        MyRTOS_Free(queue->storage);
        MyRTOS_Free(queue);
    }
    MyRTOS_Port_ExitCritical();
}

/**
 * @brief ����з���һ����Ŀ
 * @param queue Ŀ����о��
 * @param item ָ��Ҫ���͵���Ŀ��ָ��
 * @param block_ticks ����������������������ȴ������δ�����0��ʾ���ȴ���MYRTOS_MAX_DELAY��ʾ���õȴ���
 * @return �ɹ����ͷ���1��ʧ�ܻ�ʱ����0
 */
int Queue_Send(QueueHandle_t queue, const void *item, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) return 0;
    while (1) {
        MyRTOS_Port_EnterCritical();
        // ���1: ���������ڵȴ���������
        if (pQueue->receiveEventList.head != NULL) {
            Task_t *taskToWake = pQueue->receiveEventList.head;
            eventListRemove(taskToWake);
            // ���������ͬʱҲ���ӳ��б��У������Ƴ�
            if (taskToWake->delay > 0) {
                removeTaskFromList(&delayedTaskListHead, taskToWake);
                taskToWake->delay = 0;
            }
            // ֱ�ӽ����ݿ������ȴ�������
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            taskToWake->eventData = NULL;
            addTaskToReadyList(taskToWake);
            // ��������ѵ��������ȼ����ߣ���������
            if (taskToWake->priority > currentTask->priority)
                MyRTOS_Port_Yield();
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // ���2: ����δ��
        if (pQueue->waitingCount < pQueue->length) {
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            // дָ��ػ�
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // ���3: �����������Ҳ���������
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
        // ���4: ������������Ҫ����
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&pQueue->sendEventList, currentTask); // ���뷢�͵ȴ��б�
        currentTask->delay = 0;
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_ExitCritical();
        MyRTOS_Port_Yield(); // �������ȣ������������
        // ���񱻻��Ѻ󣬼���Ƿ����������ѣ������ǳ�ʱ��
        if (currentTask->pEventList == NULL) continue; // ������������ѣ�pEventList�ᱻ��ΪNULL��ѭ�����Է���
        // ����ǳ�ʱ����
        MyRTOS_Port_EnterCritical();
        eventListRemove(currentTask);
        MyRTOS_Port_ExitCritical();
        return 0;
    }
}

/**
 * @brief �Ӷ��н���һ����Ŀ
 * @param queue Ŀ����о��
 * @param buffer ���ڴ洢���յ�����Ŀ�Ļ�����ָ��
 * @param block_ticks �������Ϊ�գ����������ȴ������δ�����0��ʾ���ȴ���MYRTOS_MAX_DELAY��ʾ���õȴ���
 * @return �ɹ����շ���1��ʧ�ܻ�ʱ����0
 */
int Queue_Receive(QueueHandle_t queue, void *buffer, uint32_t block_ticks) {
    Queue_t *pQueue = queue;
    if (pQueue == NULL) return 0;
    while (1) {
        MyRTOS_Port_EnterCritical();
        // ���1: ������������
        if (pQueue->waitingCount > 0) {
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            // ��ָ��ػ�
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;
            // ����������ڵȴ����ͣ�����һ��
            if (pQueue->sendEventList.head != NULL) {
                Task_t *taskToWake = pQueue->sendEventList.head;
                eventListRemove(taskToWake);
                // ���������ͬʱҲ���ӳ��б��У������Ƴ�
                if (taskToWake->delay > 0) {
                    removeTaskFromList(&delayedTaskListHead, taskToWake);
                    taskToWake->delay = 0;
                }
                addTaskToReadyList(taskToWake);
                // ��������ѵ��������ȼ����ߣ���������
                if (taskToWake->priority > currentTask->priority)
                    MyRTOS_Port_Yield();
            }
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // ���2: ����Ϊ�գ��Ҳ���������
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
        // ���3: ����Ϊ�գ���Ҫ����
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        currentTask->eventData = buffer; // ��ʱ�洢���ջ�����ָ��
        eventListInsert(&pQueue->receiveEventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_ExitCritical();
        MyRTOS_Port_Yield(); // �������ȣ������������
        // ���񱻻��Ѻ󣬼���Ƿ����������ѣ������ѱ�ֱ�ӿ�����buffer��
        if (currentTask->pEventList == NULL) return 1;
        // ����ǳ�ʱ����
        MyRTOS_Port_EnterCritical();
        eventListRemove(currentTask);
        currentTask->eventData = NULL;
        MyRTOS_Port_ExitCritical();
        return 0;
    }
}

// =============================
// Mutex Management API
// =============================
/**
 * @brief ����һ��������
 * @return �ɹ��򷵻ػ����������ʧ���򷵻�NULL
 */
MutexHandle_t Mutex_Create(void) {
    Mutex_t *mutex = MyRTOS_Malloc(sizeof(Mutex_t));
    if (mutex != NULL) {
        mutex->locked = 0;
        mutex->owner_tcb = NULL;
        mutex->next_held_mutex = NULL;
        mutex->recursion_count = 0;
        eventListInit(&mutex->eventList);
    }
    return mutex;
}

/**
 * @brief ���Ի�ȡһ��������������ʱ
 * @param mutex Ŀ�껥�������
 * @param block_ticks ������ѱ�ռ�ã����������ȴ������δ�����0��ʾ���ȴ���MYRTOS_MAX_DELAY��ʾ���õȴ���
 * @return �ɹ���ȡ����1��ʧ�ܻ�ʱ����0
 */
int Mutex_Lock_Timeout(MutexHandle_t mutex, uint32_t block_ticks) {
    while (1) {
        MyRTOS_Port_EnterCritical();
        // ���1: ��δ��ռ�ã��ɹ���ȡ
        if (!mutex->locked) {
            mutex->locked = 1;
            mutex->owner_tcb = currentTask;
            // ���˻��������뵱ǰ������еĻ�����������
            mutex->next_held_mutex = currentTask->held_mutexes_head;
            currentTask->held_mutexes_head = mutex;
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // ���2: ���ѱ�ռ�ã��Ҳ���������
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
        // ���3: ���ѱ�ռ�ã���Ҫ����
        // ʵ�����ȼ��̳У������ǰ�������ȼ��������ĳ����ߣ������������ߵ����ȼ�
        TaskHandle_t owner_tcb = mutex->owner_tcb;
        if (owner_tcb != NULL && currentTask->priority > owner_tcb->priority) {
            task_set_priority(owner_tcb, currentTask->priority);
        }
        // ����ǰ����Ӿ����б��Ƴ��������뻥�����ĵȴ��б�
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&mutex->eventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_ExitCritical();
        MyRTOS_Port_Yield(); // �������ȣ���������
        // ���񱻻��Ѻ�
        if (mutex->owner_tcb == currentTask) return 1; // ����Ƿ��ѳ�Ϊ�µĳ�����
        // ����ǳ�ʱ����
        if (currentTask->pEventList != NULL) {
            MyRTOS_Port_EnterCritical();
            eventListRemove(currentTask);
            MyRTOS_Port_ExitCritical();
            return 0;
        }
    }
}

/**
 * @brief ��ȡһ�������������õȴ���
 * @param mutex Ŀ�껥�������
 */
void Mutex_Lock(MutexHandle_t mutex) {
    Mutex_Lock_Timeout(mutex, MYRTOS_MAX_DELAY);
}

/**
 * @brief �ͷ�һ��������
 * @param mutex Ŀ�껥�������
 */
void Mutex_Unlock(MutexHandle_t mutex) {
    int trigger_yield = 0;
    MyRTOS_Port_EnterCritical();
    // ����Ƿ������ĳ�����
    if (!mutex->locked || mutex->owner_tcb != currentTask) {
        MyRTOS_Port_ExitCritical();
        return;
    }
    // �ӵ�ǰ������еĻ������������Ƴ�����
    if (currentTask->held_mutexes_head == mutex) {
        currentTask->held_mutexes_head = mutex->next_held_mutex;
    } else {
        Mutex_t *p_iterator = currentTask->held_mutexes_head;
        while (p_iterator != NULL && p_iterator->next_held_mutex != mutex) p_iterator = p_iterator->next_held_mutex;
        if (p_iterator != NULL) p_iterator->next_held_mutex = mutex->next_held_mutex;
    }
    mutex->next_held_mutex = NULL;
    // ���ȼ��ָ������������ȼ��ָ�����������ȼ���������Ȼ���е�������������Ҫ���������ȼ�
    uint8_t new_priority = currentTask->basePriority;
    Mutex_t *p_held_mutex = currentTask->held_mutexes_head;
    while (p_held_mutex != NULL) {
        if (p_held_mutex->eventList.head != NULL && p_held_mutex->eventList.head->priority > new_priority) {
            new_priority = p_held_mutex->eventList.head->priority;
        }
        p_held_mutex = p_held_mutex->next_held_mutex;
    }
    task_set_priority(currentTask, new_priority);
    // �����Ϊδ����
    mutex->locked = 0;
    mutex->owner_tcb = NULL;
    // ����������ڵȴ��������������ȼ���ߵ��Ǹ�
    if (mutex->eventList.head != NULL) {
        Task_t *taskToWake = mutex->eventList.head;
        eventListRemove(taskToWake);
        // ����������Ȩֱ��ת�Ƹ������ѵ�����
        mutex->locked = 1;
        mutex->owner_tcb = taskToWake;
        mutex->next_held_mutex = taskToWake->held_mutexes_head;
        taskToWake->held_mutexes_head = mutex;
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) trigger_yield = 1;
    }
    MyRTOS_Port_ExitCritical();
    if (trigger_yield)
        MyRTOS_Port_Yield();
}

/**
 * @brief �ݹ�ػ�ȡһ��������
 * @note  �����ǰ�������Ǹ����ĳ����ߣ������ӵݹ������������Ϊ�� `Mutex_Lock` ��ͬ��
 * @param mutex Ŀ�껥�������
 */
void Mutex_Lock_Recursive(MutexHandle_t mutex) {
    MyRTOS_Port_EnterCritical();
    // ����Ѿ����и��������ӵݹ����
    if (mutex->locked && mutex->owner_tcb == currentTask) {
        mutex->recursion_count++;
        MyRTOS_Port_ExitCritical();
        return;
    }
    MyRTOS_Port_ExitCritical();
    // �״λ�ȡ����
    Mutex_Lock(mutex);
    MyRTOS_Port_EnterCritical();
    if (mutex->owner_tcb == currentTask) mutex->recursion_count = 1;
    MyRTOS_Port_ExitCritical();
}

/**
 * @brief �ݹ���ͷ�һ��������
 * @note  ����ݹ��������1������ݼ��������������Ϊ1������ȫ�ͷŸ�����
 * @param mutex Ŀ�껥�������
 */
void Mutex_Unlock_Recursive(MutexHandle_t mutex) {
    MyRTOS_Port_EnterCritical();
    if (mutex->locked && mutex->owner_tcb == currentTask) {
        mutex->recursion_count--;
        if (mutex->recursion_count == 0) {
            // ���ݹ��������ʱ���������ͷ���
            MyRTOS_Port_ExitCritical();
            Mutex_Unlock(mutex);
        } else {
            MyRTOS_Port_ExitCritical();
        }
    } else {
        MyRTOS_Port_ExitCritical();
    }
}

/**
 * @brief ����һ�������ź���
 * @param maxCount �ź�����������ֵ
 * @param initialCount �ź����ĳ�ʼ����ֵ
 * @return �ɹ��򷵻��ź��������ʧ���򷵻�NULL
 */
SemaphoreHandle_t Semaphore_Create(uint32_t maxCount, uint32_t initialCount) {
    if (maxCount == 0 || initialCount > maxCount) return NULL;
    Semaphore_t *semaphore = MyRTOS_Malloc(sizeof(Semaphore_t));
    if (semaphore != NULL) {
        semaphore->count = initialCount;
        semaphore->maxCount = maxCount;
        eventListInit(&semaphore->eventList);
    }
    return semaphore;
}

/**
 * @brief ɾ��һ���ź���
 * @param semaphore Ҫɾ�����ź������
 */
void Semaphore_Delete(SemaphoreHandle_t semaphore) {
    if (semaphore == NULL) return;
    MyRTOS_Port_EnterCritical();
    // �������еȴ����ź���������
    while (semaphore->eventList.head != NULL) {
        Task_t *taskToWake = semaphore->eventList.head;
        eventListRemove(taskToWake);
        addTaskToReadyList(taskToWake);
    }
    MyRTOS_Free(semaphore);
    MyRTOS_Port_ExitCritical();
}

/**
 * @brief ��ȡ��P������һ���ź���
 * @param semaphore Ŀ���ź������
 * @param block_ticks ����ź�������Ϊ0�����������ȴ������δ�����0��ʾ���ȴ���MYRTOS_MAX_DELAY��ʾ���õȴ���
 * @return �ɹ���ȡ����1��ʧ�ܻ�ʱ����0
 */
int Semaphore_Take(SemaphoreHandle_t semaphore, uint32_t block_ticks) {
    if (semaphore == NULL) return 0;
    while (1) {
        MyRTOS_Port_EnterCritical();
        // ���1: �ź�����������0���ɹ���ȡ
        if (semaphore->count > 0) {
            semaphore->count--;
            MyRTOS_Port_ExitCritical();
            return 1;
        }
        // ���2: �ź���Ϊ0���Ҳ�����
        if (block_ticks == 0) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
        // ���3: �ź���Ϊ0����Ҫ����
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        eventListInsert(&semaphore->eventList, currentTask);
        currentTask->delay = 0;
        if (block_ticks != MYRTOS_MAX_DELAY) {
            currentTask->delay = MyRTOS_GetTick() + block_ticks;
            addTaskToSortedDelayList(currentTask);
        }
        MyRTOS_Port_ExitCritical();
        MyRTOS_Port_Yield(); // �������ȣ���������
        // �����Ѻ󣬼���Ƿ�����������
        if (currentTask->pEventList == NULL) return 1;
        // ����ǳ�ʱ����
        MyRTOS_Port_EnterCritical();
        eventListRemove(currentTask);
        MyRTOS_Port_ExitCritical();
        return 0;
    }
}

/**
 * @brief �ͷţ�V������һ���ź���
 * @param semaphore Ŀ���ź������
 * @return �ɹ��ͷŷ���1��ʧ�ܣ������ź����Ѵ����ֵ������0
 */
int Semaphore_Give(SemaphoreHandle_t semaphore) {
    if (semaphore == NULL) return 0;
    int trigger_yield = 0;
    MyRTOS_Port_EnterCritical();
    // ����������ڵȴ��ź�������ֱ�ӻ���һ�����������Ӽ���ֵ
    if (semaphore->eventList.head != NULL) {
        Task_t *taskToWake = semaphore->eventList.head;
        eventListRemove(taskToWake);
        if (taskToWake->delay > 0) {
            removeTaskFromList(&delayedTaskListHead, taskToWake);
            taskToWake->delay = 0;
        }
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) trigger_yield = 1;
    } else {
        // ���û������ȴ��������Ӽ���ֵ
        if (semaphore->count < semaphore->maxCount) {
            semaphore->count++;
        } else {
            // �Ѵ����ֵ���ͷ�ʧ��
            MyRTOS_Port_ExitCritical();
            return 0;
        }
    }
    MyRTOS_Port_ExitCritical();
    if (trigger_yield)
        MyRTOS_Port_Yield();
    return 1;
}

/**
 * @brief ���жϷ������(ISR)���ͷ�һ���ź���
 * @param semaphore Ŀ���ź������
 * @param pxHigherPriorityTaskWoken ָ�룬���ڷ����Ƿ��и������ȼ������񱻻���
 * @return �ɹ�����1��ʧ�ܷ���0
 */
int Semaphore_GiveFromISR(SemaphoreHandle_t semaphore, int *pxHigherPriorityTaskWoken) {
    if (semaphore == NULL || pxHigherPriorityTaskWoken == NULL) return 0;
    *pxHigherPriorityTaskWoken = 0;
    int result = 0;
    MyRTOS_Port_EnterCritical();
    // �߼��� Semaphore_Give ����
    if (semaphore->eventList.head != NULL) {
        Task_t *taskToWake = semaphore->eventList.head;
        eventListRemove(taskToWake);
        if (taskToWake->delay > 0) {
            removeTaskFromList(&delayedTaskListHead, taskToWake);
            taskToWake->delay = 0;
        }
        addTaskToReadyList(taskToWake);
        if (taskToWake->priority > currentTask->priority) {
            *pxHigherPriorityTaskWoken = 1;
        }
        result = 1;
    } else {
        if (semaphore->count < semaphore->maxCount) {
            semaphore->count++;
            result = 1;
        }
    }
    MyRTOS_Port_ExitCritical();
    return result;
}

/**
 * @brief ע��һ���ں���չ�ص�����
 * @note  �ں���չ�����ڵ��ԡ����ٻ�ʵ���Զ��幦�ܣ��������ض��ں��¼�����ʱ�����á�
 * @param callback Ҫע��Ļص�����ָ��
 * @return �ɹ�����0��ʧ�ܷ���-1
 */
int MyRTOS_RegisterExtension(KernelExtensionCallback_t callback) {
    MyRTOS_Port_EnterCritical();
    // �����չ���Ƿ�������ص������Ƿ�Ϊ��
    if (g_extension_count >= MAX_KERNEL_EXTENSIONS || callback == NULL) {
        MyRTOS_Port_ExitCritical();
        return -1;
    }
    // �����ظ�ע��
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i] == callback) {
            MyRTOS_Port_ExitCritical();
            return 0;
        }
    }
    // ����µĻص�
    g_extensions[g_extension_count++] = callback;
    MyRTOS_Port_ExitCritical();
    return 0;
}

/**
 * @brief ע��һ���ں���չ�ص�����
 * @param callback Ҫע���Ļص�����ָ��
 * @return �ɹ�����0��ʧ�ܣ�δ�ҵ��ûص�������-1
 */
int MyRTOS_UnregisterExtension(KernelExtensionCallback_t callback) {
    int found = 0;
    MyRTOS_Port_EnterCritical();
    if (callback == NULL) {
        MyRTOS_Port_ExitCritical();
        return -1;
    }
    for (uint8_t i = 0; i < g_extension_count; ++i) {
        if (g_extensions[i] == callback) {
            // �����һ��Ԫ���Ƶ���ǰλ�ã�Ȼ����������
            g_extensions[i] = g_extensions[g_extension_count - 1];
            g_extensions[g_extension_count - 1] = NULL;
            g_extension_count--;
            found = 1;
            break;
        }
    }
    MyRTOS_Port_ExitCritical();
    return found ? 0 : -1;
}
