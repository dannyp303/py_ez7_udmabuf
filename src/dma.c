
#include "dma.h"
#include "udmabuf.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <errno.h>

#define DMA_MAP_SIZE 0x1000
#define S2MM_CONTROL   0x30
#define S2MM_STATUS    0x34
#define S2MM_DEST_ADDR 0x48
#define S2MM_LENGTH    0x58
#define S2MM_IRQ_MASK  0x54

#define DMA_CONTROL_RUNSTOP    (1 << 0)
#define DMA_CONTROL_RESET      (1 << 2)
#define DMA_STATUS_HALTED      (1 << 0)
#define DMA_STATUS_IDLE        (1 << 1)
#define DMA_STATUS_ERROR_MASK  0xF000

uintptr_t get_udmabuf_phys(int index) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/u-dma-buf/udmabuf%d/phys_addr", index);
    printf("Path: %s\n", path);
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("open phys_addr");
        return 0;
    }
    uintptr_t phys;
    fscanf(f, "%lx", &phys);
    fclose(f);
    return phys;
}

void* fnAllocUdmaBuf(uintptr_t addr, size_t size) {
    DMAEnv *dma_env = (DMAEnv *)(uintptr_t)addr;
    if (!dma_env) return NULL;
    return alloc_u_dma_buf(size, dma_env->index);
}

void fnFreeUdmaBuf(uintptr_t addr, void *buf, size_t size) {
    free_u_dma_buf(buf, size);
}

uint32_t fnInitDMA(uintptr_t addr, enum dma_direction direction, int dmaInterrupt) {
    DMAEnv *dma_env = (DMAEnv *)malloc(sizeof(DMAEnv));
    if (!dma_env) return 0;
    if (addr == 0x40400000) {
        dma_env->index = 0;
    } else if (addr == 0x40410000) {
        dma_env->index = 1;
    } else {
        return 0;
    }
    dma_env->addr = addr;
    dma_env->direction = direction;
    return (uint32_t)(uintptr_t)dma_env;
}

int fnStartDMATransfer(uintptr_t addr, void *buf, size_t size) {
    DMAEnv *dma_env = (DMAEnv *)(uintptr_t)addr;
    if (!dma_env || !buf) return -1;

    uintptr_t phys_addr = get_udmabuf_phys(dma_env->index);
    if (!phys_addr) return -1;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        return -1;
    }

    void *mapped = mmap(NULL, DMA_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, dma_env->addr);
    if (mapped == MAP_FAILED) {
        perror("mmap dma regs");
        close(fd);
        return -1;
    }

    volatile uint32_t *dma_regs = (volatile uint32_t *)mapped;

    // Reset and start DMA
    dma_regs[S2MM_CONTROL >> 2] = DMA_CONTROL_RESET;
    usleep(1000);
    dma_regs[S2MM_CONTROL >> 2] = DMA_CONTROL_RUNSTOP;
    dma_regs[S2MM_IRQ_MASK >> 2] = 0x0; // Enable all IRQs
    dma_regs[S2MM_DEST_ADDR >> 2] = phys_addr;
    dma_regs[S2MM_LENGTH >> 2] = size;

    munmap((void *)dma_regs, DMA_MAP_SIZE);
    close(fd);
    return 0;
}

int fnIsDMATransferComplete(uintptr_t addr) {
    DMAEnv *dma_env = (DMAEnv *)(uintptr_t)addr;
    if (!dma_env) return -1;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        return -1;
    }

    void *mapped = mmap(NULL, DMA_MAP_SIZE, PROT_READ, MAP_SHARED, fd, dma_env->addr);
    if (mapped == MAP_FAILED) {
        perror("mmap dma regs");
        close(fd);
        return -1;
    }

    volatile uint32_t *dma_regs = (volatile uint32_t *)mapped;

    uint32_t status = dma_regs[S2MM_STATUS >> 2];
    munmap((void *)dma_regs, DMA_MAP_SIZE);
    close(fd);

    if (status & DMA_STATUS_ERROR_MASK) {
        fprintf(stderr, "DMA Error! Status: 0x%08x\n", status);
        return -2; // Error
    }

    return (status & DMA_STATUS_IDLE) ? 1 : 0; // 1 = complete, 0 = in progress
}

void fnDestroyDMA(uintptr_t addr) {
    if (addr) free((void *)(uintptr_t)addr);
}
