#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

/* portable string functions for cross-platform compatibility */
char* cq_strndup(const char* s, size_t n);
size_t cq_strlcat(char* dst, const char* src, size_t size);
char* cq_strcasestr(const char* haystack, const char* needle);

#endif
