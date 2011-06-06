#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdlib.h>

#include "arakoon.h"

void * check_arakoon_malloc(size_t s)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_MALLOC;
void check_arakoon_free(void *ptr);
void * check_arakoon_realloc(void *ptr, size_t s)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;

const void * check_arakoon_last_free_address(void);
const void * check_arakoon_last_malloc_address(void);

void * check_arakoon_realloc_null(void *ptr, size_t s)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
#endif
