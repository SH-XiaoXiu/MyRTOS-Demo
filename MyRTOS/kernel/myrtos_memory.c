/**
 * @file myrtos_memory.c
 * @brief MyRTOS 内存管理模块
 */

#include "myrtos_kernel.h"

/*===========================================================================*
 * 私有变量
 *===========================================================================*/

// 内存块管理结构的大小，已考虑内存对齐
static const size_t heapStructSize =
        (sizeof(BlockLink_t) + (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) & ~(((size_t) MYRTOS_HEAP_BYTE_ALIGNMENT - 1));

// 内存堆管理
//  静态分配的内存池，用于RTOS的动态内存分配
static uint8_t rtos_memory_pool[MYRTOS_MEMORY_POOL_SIZE] __attribute__((aligned(MYRTOS_HEAP_BYTE_ALIGNMENT)));
// 空闲内存块链表的起始哨兵节点
static BlockLink_t start;
// 内存堆的结束哨兵节点
static BlockLink_t *blockLinkEnd = NULL;
// 当前剩余的空闲字节数
size_t freeBytesRemaining = 0U;
// 用于标记内存块是否已被分配的位掩码 (最高位)
static size_t blockAllocatedBit = 0;

/*===========================================================================*
 * 私有函数
 *===========================================================================*/

/**
 * @brief 初始化内存堆
 * @note  此函数负责设置内存池，创建初始的空闲内存块，并设置起始和结束哨兵节点。
 */
static void heapInit(void) {
    BlockLink_t *firstFreeBlock;
    uint8_t *alignedHeap;
    size_t address = (size_t) rtos_memory_pool;
    size_t totalHeapSize = MYRTOS_MEMORY_POOL_SIZE;
    // 确保堆的起始地址是对齐的
    if ((address & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) != 0) {
        address += (MYRTOS_HEAP_BYTE_ALIGNMENT - (address & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)));
        totalHeapSize -= address - (size_t) rtos_memory_pool;
    }
    alignedHeap = (uint8_t *) address;
    // 设置起始哨兵节点
    start.nextFreeBlock = (BlockLink_t *) alignedHeap;
    start.blockSize = (size_t) 0;
    // 设置结束哨兵节点
    address = ((size_t) alignedHeap) + totalHeapSize - heapStructSize;
    blockLinkEnd = (BlockLink_t *) address;
    blockLinkEnd->blockSize = 0;
    blockLinkEnd->nextFreeBlock = NULL;
    // 创建第一个大的空闲内存块
    firstFreeBlock = (BlockLink_t *) alignedHeap;
    firstFreeBlock->blockSize = address - (size_t) firstFreeBlock;
    firstFreeBlock->nextFreeBlock = blockLinkEnd;
    // 初始化剩余空闲字节数
    freeBytesRemaining = firstFreeBlock->blockSize;
    // 设置用于标记"已分配"的位（最高位）
    blockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * 8) - 1);
}

/**
 * @brief 将一个内存块插入到空闲链表中
 * @note  此函数会按地址顺序插入内存块，并尝试与相邻的空闲块合并。
 * @param blockToInsert 要插入的内存块指针
 */
static void insertBlockIntoFreeList(BlockLink_t *blockToInsert) {
    BlockLink_t *iterator;
    uint8_t *puc;
    // 遍历空闲链表，找到合适的插入位置
    for (iterator = &start; iterator->nextFreeBlock < blockToInsert; iterator = iterator->nextFreeBlock) {
        // 空循环，仅为移动迭代器
    }
    // 尝试与前一个空闲块合并
    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        iterator->blockSize += blockToInsert->blockSize;
        blockToInsert = iterator;
    } else {
        blockToInsert->nextFreeBlock = iterator->nextFreeBlock;
    }
    // 尝试与后一个空闲块合并
    puc = (uint8_t *) blockToInsert;
    if ((puc + blockToInsert->blockSize) == (uint8_t *) iterator->nextFreeBlock) {
        if (iterator->nextFreeBlock != blockLinkEnd) {
            blockToInsert->blockSize += iterator->nextFreeBlock->blockSize;
            blockToInsert->nextFreeBlock = iterator->nextFreeBlock->nextFreeBlock;
        }
    }
    // 如果没有发生前向合并，则将当前块链接到链表中
    if (iterator != blockToInsert) {
        iterator->nextFreeBlock = blockToInsert;
    }
}

/**
 * @brief RTOS内部使用的内存分配函数
 * @note  实现了首次适应(First Fit)算法来查找合适的内存块。
 * @param wantedSize 请求分配的字节数
 * @return 成功则返回分配的内存指针，失败则返回NULL
 */
static void *rtos_malloc(const size_t wantedSize) {
    BlockLink_t *block, *previousBlock, *newBlockLink;
    void *pvReturn = NULL;
    MyRTOS_Port_EnterCritical(); {
        // 如果堆尚未初始化，则进行初始化
        if (blockLinkEnd == NULL) {
            heapInit();
        }
        if ((wantedSize > 0) && ((wantedSize & blockAllocatedBit) == 0)) {
            // 计算包括管理结构和对齐后的总大小
            size_t totalSize = heapStructSize + wantedSize;
            if ((totalSize & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)) != 0) {
                totalSize += (MYRTOS_HEAP_BYTE_ALIGNMENT - (totalSize & (MYRTOS_HEAP_BYTE_ALIGNMENT - 1)));
            }
            if (totalSize <= freeBytesRemaining) {
                // 遍历空闲链表，查找足够大的内存块
                previousBlock = &start;
                block = start.nextFreeBlock;
                while ((block->blockSize < totalSize) && (block->nextFreeBlock != NULL)) {
                    previousBlock = block;
                    block = block->nextFreeBlock;
                }
                // 如果找到了合适的块
                if (block != blockLinkEnd) {
                    pvReturn = (void *) (((uint8_t *) block) + heapStructSize);
                    previousBlock->nextFreeBlock = block->nextFreeBlock;
                    // 如果剩余部分足够大，则分裂成一个新的空闲块
                    if ((block->blockSize - totalSize) > HEAP_MINIMUM_BLOCK_SIZE) {
                        newBlockLink = (BlockLink_t *) (((uint8_t *) block) + totalSize);
                        newBlockLink->blockSize = block->blockSize - totalSize;
                        block->blockSize = totalSize;
                        insertBlockIntoFreeList(newBlockLink);
                    }
                    freeBytesRemaining -= block->blockSize;
                    // 标记该块为已分配
                    block->blockSize |= blockAllocatedBit;
                    block->nextFreeBlock = NULL;
                }
            }
        }
        // 如果分配失败且请求大小大于0，报告错误
        if (pvReturn == NULL && wantedSize > 0) {
            MyRTOS_ReportError(KERNEL_ERROR_MALLOC_FAILED, (void *) wantedSize);
        }
    }
    MyRTOS_Port_ExitCritical();
    return pvReturn;
}

/**
 * @brief RTOS内部使用的内存释放函数
 * @note  将释放的内存块重新插入到空闲链表中，并尝试合并。
 * @param pv 要释放的内存指针
 */
static void rtos_free(void *pv) {
    if (pv == NULL)
        return;
    uint8_t *puc = (uint8_t *) pv;
    BlockLink_t *link;
    // 从用户指针回退到内存块的管理结构
    puc -= heapStructSize;
    link = (BlockLink_t *) puc;
    // 检查该块是否确实是已分配状态
    if (((link->blockSize & blockAllocatedBit) != 0) && (link->nextFreeBlock == NULL)) {
        // 清除已分配标志
        link->blockSize &= ~blockAllocatedBit;
        MyRTOS_Port_EnterCritical();
        // 更新剩余空闲字节数并将其插回空闲链表
        freeBytesRemaining += link->blockSize;
        insertBlockIntoFreeList(link);
        MyRTOS_Port_ExitCritical();
    }
}

/*===========================================================================*
 * 公开接口实现
 *===========================================================================*/

/**
 * @brief 动态分配内存
 * @note  此函数是线程安全的。内部使用 `rtos_malloc` 并广播一个内存分配事件。
 * @param wantedSize 要分配的内存大小（字节）
 * @return 成功则返回指向已分配内存的指针，失败则返回NULL
 */
void *MyRTOS_Malloc(size_t wantedSize) {
    void *pv = rtos_malloc(wantedSize);
    // 广播内存分配事件
    KernelEventData_t eventData = {.eventType = KERNEL_EVENT_MALLOC, .mem = {.ptr = pv, .size = wantedSize}};
    broadcast_event(&eventData);
    return pv;
}

/**
 * @brief 释放先前分配的内存
 * @note  此函数是线程安全的。内部使用 `rtos_free` 并广播一个内存释放事件。
 * @param pv 要释放的内存指针，必须是通过 `MyRTOS_Malloc` 分配的
 */
void MyRTOS_Free(void *pv) {
    if (pv) {
        // 在释放前获取块大小以用于事件广播
        BlockLink_t *link = (BlockLink_t *) ((uint8_t *) pv - heapStructSize);
        KernelEventData_t eventData = {
            .eventType = KERNEL_EVENT_FREE,
            .mem = {.ptr = pv, .size = (link->blockSize & ~blockAllocatedBit)}
        };
        broadcast_event(&eventData);
    }
    rtos_free(pv);
}
