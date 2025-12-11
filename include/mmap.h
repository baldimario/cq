#ifndef _MMAP_H
#define _MMAP_H

#include <stdlib.h>

char *portable_mmap(const char *filename, size_t *out_size, int *out_fd);

void portable_munmap(char *data, size_t size, int fd);

#endif