#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stddef.h>

enum dma_direction {
    DMA_DIRECTION_TX,
    DMA_DIRECTION_RX
};

typedef struct _dma_env {
    uintptr_t addr;
    enum dma_direction direction;
    int index;
} DMAEnv;

void* fnAllocUdmaBuf(uintptr_t addr, size_t size);
void fnFreeUdmaBuf(uintptr_t addr, void *buf, size_t size);
uint32_t fnInitDMA(uintptr_t addr, enum dma_direction direction, int dmaInterrupt);
int fnStartDMATransfer(uintptr_t addr, void *buf, size_t size);
int fnIsDMATransferComplete(uintptr_t addr);
void fnDestroyDMA(uintptr_t addr);

#endif // DMA_H
