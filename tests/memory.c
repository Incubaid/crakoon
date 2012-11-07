/*
 * This file is part of Arakoon, a distributed key-value store.
 *
 * Copyright (C) 2010 Incubaid BVBA
 *
 * Licensees holding a valid Incubaid license may use this file in
 * accordance with Incubaid's Arakoon commercial license agreement. For
 * more information on how to enter into this agreement, please contact
 * Incubaid (contact details can be found on http://www.arakoon.org/licensing).
 *
 * Alternatively, this file may be redistributed and/or modified under
 * the terms of the GNU Affero General Public License version 3, as
 * published by the Free Software Foundation. Under this license, this
 * file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Affero General Public License for more details.
 * You should have received a copy of the
 * GNU Affero General Public License along with this program (file "COPYING").
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory.h"

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
                fprintf(stderr, "malloc: malloc returned NULL");
                fflush(stderr);
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
                fprintf(stderr, "free: Prefix sentinel corrupted");
                fflush(stderr);
                abort();
        }
        if(s1 != SENTINEL2) {
                fprintf(stderr, "free: Suffix sentinel corrupted");
                fflush(stderr);
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
                        fprintf(stderr, "realloc: Prefix sentinel corrupted");
                        fflush(stderr);
                        abort();
                }
                if(GET_SENTINEL2(ptr, s0) != SENTINEL2) {
                        fprintf(stderr, "realloc: Suffix sentinel corrupted");
                        fflush(stderr);
                        abort();
                }
        }

        len = MALLOC_SIZE(s);
        ret = realloc(ptr, len);
        if(ret == NULL) {
                fprintf(stderr, "realloc: realloc returned NULL");
                fflush(stderr);
                abort();
        }

        SET_SIZE(ret, len);
        SET_SENTINEL1(ret);
        SET_SENTINEL2(ret, len);

        return USER_ADDRESS(ret);
}

void * check_arakoon_realloc_null(void *ptr ARAKOON_GNUC_UNUSED,
    size_t s ARAKOON_GNUC_UNUSED) {
        return NULL;
}

const void * check_arakoon_last_malloc_address(void) {
        return last_malloc;
}

const void * check_arakoon_last_free_address(void) {
        return last_free;
}
