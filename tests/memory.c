#include <stdint.h>
#include <stdlib.h>

#define SENTINEL1 (0xdeadbeef)
#define SENTINEL2 (0xcafebabe)

#define MALLOC_SIZE(s) (s + sizeof(size_t) + sizeof(uint32_t) + sizeof(uint32_t))

#define ADDRESS_OF_SIZE(r) ((size_t *)r)
#define SET_SIZE(r, s)              \
        do {                        \
                *((size_t *)r) = s; \
        } while(0)
#define GET_SIZE(r) (*(ADDRESS_OF_SIZE(r)))
#define ADDRESS_OF_SENTINEL1(r) \
        ((uint32_t *)((char *)ADDRESS_OF_SIZE(r) + sizeof(size_t)))
#define SET_SENTINEL1(r)                                            \
        do {                                                        \
                *(ADDRESS_OF_SENTINEL1(r)) = SENTINEL1; \
        } while(0)
#define GET_SENTINEL1(r) (*(ADDRESS_OF_SENTINEL1(r)))
#define ADDRESS_OF_SENTINEL2(r, l) \
        ((uint32_t *)((char *)r + l - sizeof(uint32_t)))
#define SET_SENTINEL2(r, l) \
        do { \
                *(ADDRESS_OF_SENTINEL2(r, l)) = SENTINEL2; \
        } while(0)
#define GET_SENTINEL2(r, l) (*(ADDRESS_OF_SENTINEL2(r, l)))
#define USER_ADDRESS(r) ((void *)((char *)r + sizeof(size_t) + sizeof(uint32_t)))
#define MALLOC_ADDRESS(r) \
        ((void *)((char *)r - sizeof(uint32_t) - sizeof(size_t)))

static void * last_malloc = NULL;
static void * last_free = NULL;

void * check_arakoon_malloc(size_t s) {
        void *ret = NULL;
        size_t len = 0;

        len = MALLOC_SIZE(s);
        ret = malloc(len);

        if(ret == NULL) {
                abort();
        }

        SET_SIZE(ret, len);
        SET_SENTINEL1(ret);
        SET_SENTINEL2(ret, len);

        last_malloc = USER_ADDRESS(ret);
        return last_malloc;
}

void check_arakoon_free(void *ptr) {
        size_t s = 0;
        uint32_t s0 = 0, s1 = 0;

        last_free = ptr;

        if(ptr == NULL) {
                return;
        }

        ptr = MALLOC_ADDRESS(ptr);

        s = GET_SIZE(ptr);

        s0 = GET_SENTINEL1(ptr);
        s1 = GET_SENTINEL2(ptr, s);

        if(s0 != SENTINEL1) {
                abort();
        }
        if(s1 != SENTINEL2) {
                abort();
        }

        free(ptr);
}

void * check_arakoon_realloc(void *ptr, size_t s) {
        size_t s0 = 0, len = 0;
        void *ret = NULL;

        if(s == 0) {
                if(ptr != NULL) {
                        check_arakoon_free(ptr);
                }

                return NULL;
        }

        if(ptr != NULL) {
                ptr = MALLOC_ADDRESS(ptr);

                s0 = GET_SIZE(ptr);
                if(GET_SENTINEL1(ptr) != SENTINEL1) {
                        abort();
                }
                if(GET_SENTINEL2(ptr, s0) != SENTINEL2) {
                        abort();
                }
        }

        len = MALLOC_SIZE(s);
        ret = realloc(ptr, len);
        if(ret == NULL) {
                abort();
        }

        SET_SIZE(ret, len);
        SET_SENTINEL1(ret);
        SET_SENTINEL2(ret, len);

        return USER_ADDRESS(ret);
}

void * check_arakoon_realloc_null(void *ptr, size_t s) {
        return NULL;
}

const void * check_arakoon_last_malloc_address(void) {
        return last_malloc;
}

const void * check_arakoon_last_free_address(void) {
        return last_free;
}
