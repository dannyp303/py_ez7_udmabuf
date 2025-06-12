#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

void* alloc_u_dma_buf(size_t size, int index)
{
    char udmabuf_name[32];
    snprintf(udmabuf_name, sizeof(udmabuf_name), "/dev/udmabuf%d", index);

    // Read actual udmabuf size from sysfs
    char size_path[64];
    snprintf(size_path, sizeof(size_path),
             "/sys/class/u-dma-buf/udmabuf%d/size", index);
    printf("Size path: %s\n", size_path);
    FILE *f = fopen(size_path, "r");
    if (!f) {
        perror("Failed to open udmabuf size sysfs entry");
        return NULL;
    }

    size_t device_size;
    if (fscanf(f, "%zu", &device_size) != 1) {
        perror("Failed to read size from sysfs");
        fclose(f);
        return NULL;
    }
    fclose(f);

    if (size > device_size) {
        fprintf(stderr, "Requested size %zu exceeds udmabuf size %zu\n", size, device_size);
        return NULL;
    }

    int fd = open(udmabuf_name, O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", udmabuf_name, strerror(errno));
        return NULL;
    }

    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return NULL;
    }

    close(fd);
    return addr;
}

void free_u_dma_buf(void *addr, size_t size)
{

    printf("Entering function: axidma_free\n");
    printf("[DEBUG] Freeing DMA Memory: Size: %zu\n", size);
    if (munmap(addr, size) < 0) {
        perror("Failed to free the AXI DMA memory mapped region");
        return;
    }

    return;
}