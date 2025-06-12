#ifndef UDMABUF_H
#define UDMABUF_H

#include <stddef.h>

void* alloc_u_dma_buf(size_t size, int index);
void free_u_dma_buf(void *addr, size_t size);

#endif