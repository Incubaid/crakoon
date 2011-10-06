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

/* TODO
 * ====
 * - Plugable communication channels (like Pyrakoon)
 * - Use TCP_CORK when using TCP sockets, wrapped around command submission
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "arakoon.h"
#include "arakoon-networking.h"

#define ARAKOON_STRINGIFY(n) ARAKOON_STRINGIFY_ARG(n)
#define ARAKOON_STRINGIFY_ARG(n) #n

#define STMT_START \
        do {
#define STMT_END \
        } while(0)

#define ARAKOON_PROTOCOL_VERSION (1)
#define ARAKOON_PROTOCOL_COMMAND_LEN (4)
#define ARAKOON_PROTOCOL_MAGIC_MASK0 (0xff)
#define ARAKOON_PROTOCOL_MAGIC_MASK1 (0xb1)
#define ARAKOON_PROTOCOL_INT32_LEN (sizeof(int32_t))
#define ARAKOON_PROTOCOL_UINT32_LEN (sizeof(uint32_t))
#define ARAKOON_PROTOCOL_STRING_LEN(n) \
        (ARAKOON_PROTOCOL_UINT32_LEN + n)
#define ARAKOON_PROTOCOL_BOOL_LEN (sizeof(char))
#define ARAKOON_PROTOCOL_STRING_OPTION_LEN(s, l) \
        (sizeof(char) + ((s == NULL) ? 0 : ARAKOON_PROTOCOL_STRING_LEN(l)))

#define ARAKOON_PROTOCOL_WRITE_COMMAND(a, n0, n1)   \
        STMT_START                                  \
        a[0] = (char) n0;                           \
        a[1] = (char) n1;                           \
        a[2] = (char) ARAKOON_PROTOCOL_MAGIC_MASK0; \
        a[3] = (char) ARAKOON_PROTOCOL_MAGIC_MASK1; \
        a += ARAKOON_PROTOCOL_COMMAND_LEN;          \
        STMT_END

#define ARAKOON_PROTOCOL_WRITE_INT32(a, u) \
        STMT_START                         \
        *((int32_t *) a) = u;              \
        a += ARAKOON_PROTOCOL_INT32_LEN;   \
        STMT_END

#define ARAKOON_PROTOCOL_WRITE_UINT32(a, u) \
        STMT_START                          \
        *((uint32_t *) a) = u;              \
        a += ARAKOON_PROTOCOL_UINT32_LEN;   \
        STMT_END

#define ARAKOON_PROTOCOL_WRITE_STRING(a, s, n) \
        STMT_START                             \
        ARAKOON_PROTOCOL_WRITE_UINT32(a, n);   \
        memcpy(a, s, n);                       \
        a += n;                                \
        STMT_END

#define ARAKOON_PROTOCOL_WRITE_STRING_OPTION(a, s, n)               \
        STMT_START                                                  \
        if(s == NULL && n != 0) {                                   \
                log_error("Passed NULL string, but size != 0");     \
        }                                                           \
        if(s == NULL) {                                             \
                ARAKOON_PROTOCOL_WRITE_BOOL(a, ARAKOON_BOOL_FALSE); \
        }                                                           \
        else {                                                      \
                ARAKOON_PROTOCOL_WRITE_BOOL(a, ARAKOON_BOOL_TRUE);  \
                ARAKOON_PROTOCOL_WRITE_STRING(a, s, n);             \
        }                                                           \
        STMT_END

#define ARAKOON_PROTOCOL_WRITE_BOOL(a, b)                                                   \
        STMT_START                                                                          \
        *((char *) a) = (b == ARAKOON_BOOL_FALSE ? ARAKOON_BOOL_FALSE : ARAKOON_BOOL_TRUE); \
        a += ARAKOON_PROTOCOL_BOOL_LEN;                                                     \
        STMT_END

#define WRITE_BYTES(f, a, n, r, t)                          \
        STMT_START                                          \
        r = _arakoon_networking_poll_write(f->fd, a, n, t); \
        if(!ARAKOON_RC_IS_SUCCESS(r)) {                     \
                arakoon_cluster_node_disconnect(f);         \
        }                                                   \
        STMT_END

#define READ_BYTES(f, a, n, r, t)                          \
        STMT_START                                         \
        r = _arakoon_networking_poll_read(f->fd, a, n, t); \
        if(!ARAKOON_RC_IS_SUCCESS(r)) {                    \
                arakoon_cluster_node_disconnect(f);        \
        }                                                  \
        STMT_END

#define ARAKOON_PROTOCOL_READ_UINT32(fd, r, rc, t)    \
        STMT_START                                    \
        uint32_t _d = 0;                              \
        READ_BYTES(fd, &_d, sizeof(uint32_t), rc, t); \
        if(ARAKOON_RC_IS_SUCCESS(rc)) {               \
                r = _d;                               \
        }                                             \
        STMT_END
#define ARAKOON_PROTOCOL_READ_RC(fd, rc, t)           \
        STMT_START                                    \
        arakoon_rc _rc = 0;                           \
        ARAKOON_PROTOCOL_READ_UINT32(fd, rc, _rc, t); \
        if(!ARAKOON_RC_IS_SUCCESS(_rc)) {             \
                rc = _rc;                             \
        }                                             \
        STMT_END

#define ARAKOON_PROTOCOL_READ_STRING(fd, a, l, rc, t) \
        STMT_START                                    \
        uint32_t _l = 0;                              \
        char *_d = NULL;                              \
        arakoon_rc _rc = 0;                           \
        ARAKOON_PROTOCOL_READ_UINT32(fd, _l, _rc, t); \
        if(!ARAKOON_RC_IS_SUCCESS(_rc)) {             \
                rc = _rc;                             \
                break;                                \
        }                                             \
        _d = arakoon_mem_new(_l, char);               \
        if(_d == NULL) {                              \
                rc = -ENOMEM;                         \
                break;                                \
        }                                             \
                                                      \
        READ_BYTES(fd, _d, _l, _rc, t);               \
        rc = _rc;                                     \
        if(ARAKOON_RC_IS_SUCCESS(rc)) {               \
                a = _d;                               \
                l = _l;                               \
        }                                             \
        else {                                        \
                arakoon_mem_free(_d);                 \
                l = 0;                                \
                a = NULL;                             \
        }                                             \
        STMT_END

#define ARAKOON_PROTOCOL_READ_STRING_OPTION(fd, a, l, rc, t) \
        STMT_START                                           \
        char _v = 0;                                         \
        arakoon_rc _rc = 0;                                  \
        READ_BYTES(fd, &_v, 1, _rc, t);                      \
        if(!ARAKOON_RC_IS_SUCCESS(_rc)) {                    \
                a = NULL;                                    \
                l = 0;                                       \
                rc = _rc;                                    \
                break;                                       \
        }                                                    \
                                                             \
        if(_v == 0) {                                        \
                l = 0;                                       \
                a = NULL;                                    \
                rc = _rc;                                    \
                break;                                       \
        }                                                    \
                                                             \
        ARAKOON_PROTOCOL_READ_STRING(fd, a, l, rc, t);       \
        STMT_END

#define ARAKOON_PROTOCOL_READ_BOOL(fd, r, rc, t)                    \
        STMT_START                                                  \
        char _r  = 0;                                               \
        READ_BYTES(fd, &_r, 1, rc, t);                              \
        if(ARAKOON_RC_IS_SUCCESS(rc)) {                             \
                r = _r == ARAKOON_BOOL_FALSE ? ARAKOON_BOOL_FALSE : \
                    ARAKOON_BOOL_TRUE;                              \
        }                                                           \
        else {                                                      \
                r = 0;                                              \
        }                                                           \
        STMT_END

#define ARAKOON_PROTOCOL_READ_STRING_LIST(fd, a, rc, t)                     \
        STMT_START                                                          \
        uint32_t _rsl_cnt = 0, _rsl_i = 0;                                  \
        arakoon_rc _rsl_rc = 0;                                             \
        void *_rsl_s = NULL;                                                \
        size_t _rsl_l = 0;                                                  \
                                                                            \
        ARAKOON_PROTOCOL_READ_UINT32(fd, _rsl_cnt, _rsl_rc, t);             \
        if(ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {                                \
                for(_rsl_i = 0; _rsl_i < _rsl_cnt; _rsl_i++) {              \
                        _rsl_l = 0;                                         \
                        _rsl_s = NULL;                                      \
                        ARAKOON_PROTOCOL_READ_STRING(fd, _rsl_s, _rsl_l,    \
                                _rsl_rc, t);                                \
                                                                            \
                        if(!ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {               \
                                break;                                      \
                        }                                                   \
                        else {                                              \
                                /* TODO This introduces a useless memcpy */ \
                                _rsl_rc = arakoon_value_list_prepend(a,     \
                                        _rsl_l, _rsl_s);                    \
                                arakoon_mem_free(_rsl_s);                   \
                                                                            \
                                if(!ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {       \
                                        break;                              \
                                }                                           \
                        }                                                   \
                }                                                           \
        }                                                                   \
                                                                            \
        rc = _rsl_rc;                                                       \
        STMT_END

#define ARAKOON_PROTOCOL_READ_STRING_STRING_LIST(fd, a, rc, t)                    \
        STMT_START                                                                \
        uint32_t _rsl_cnt = 0, _rsl_i = 0;                                        \
        arakoon_rc _rsl_rc = 0;                                                   \
        void *_rsl_s0 = NULL, *_rsl_s1 = NULL;                                    \
        size_t _rsl_l0 = 0, _rsl_l1 = 0;                                          \
                                                                                  \
        ARAKOON_PROTOCOL_READ_UINT32(fd, _rsl_cnt, _rsl_rc, t);                   \
        if(ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {                                      \
                for(_rsl_i = 0; _rsl_i < _rsl_cnt; _rsl_i++) {                    \
                        _rsl_l0 = 0;                                              \
                        _rsl_s0 = NULL;                                           \
                        _rsl_l1 = 0;                                              \
                        _rsl_s1 = NULL;                                           \
                                                                                  \
                        ARAKOON_PROTOCOL_READ_STRING(fd, _rsl_s0, _rsl_l0,        \
                                _rsl_rc, t);                                      \
                                                                                  \
                        if(!ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {                     \
                                break;                                            \
                        }                                                         \
                        else {                                                    \
                                ARAKOON_PROTOCOL_READ_STRING(fd, _rsl_s1,         \
                                        _rsl_l1, _rsl_rc, t);                     \
                                if(!ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {             \
                                        arakoon_mem_free(_rsl_s0);                \
                                        break;                                    \
                                }                                                 \
                                else {                                            \
                                        /* TODO This introduces a useless
                                         * memcpy */                              \
                                        _rsl_rc = arakoon_key_value_list_prepend( \
                                                a, _rsl_l0, _rsl_s0, _rsl_l1,     \
                                                _rsl_s1);                         \
                                        arakoon_mem_free(_rsl_s0);                \
                                        arakoon_mem_free(_rsl_s1);                \
                                                                                  \
                                        if(!ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {     \
                                                break;                            \
                                        }                                         \
                                }                                                 \
                        }                                                         \
                }                                                                 \
        }                                                                         \
                                                                                  \
        rc = _rsl_rc;                                                             \
        STMT_END


const char * arakoon_strerror(arakoon_rc n) {
        if(ARAKOON_RC_IS_ERRNO(n)) {
                return strerror(n);
        }

        switch(n) {
                case ARAKOON_RC_SUCCESS:
                        return "Success";
                        break;
                case ARAKOON_RC_NO_MAGIC:
                        return "No magic";
                        break;
                case ARAKOON_RC_TOO_MANY_DEAD_NODES:
                        return "Too many dead nodes";
                        break;
                case ARAKOON_RC_NO_HELLO:
                        return "No hello";
                        break;
                case ARAKOON_RC_NOT_MASTER:
                        return "Not master";
                        break;
                case ARAKOON_RC_NOT_FOUND:
                        return "Not found";
                        break;
                case ARAKOON_RC_WRONG_CLUSTER:
                        return "Wrong cluster";
                        break;
                case ARAKOON_RC_UNKNOWN_FAILURE:
                        return "Unknown failure";
                        break;

                case ARAKOON_RC_CLIENT_NETWORK_ERROR:
                        return "Network error in client";
                        break;
                case ARAKOON_RC_CLIENT_UNKNOWN_NODE:
                        return "Unknown node";
                        break;
                case ARAKOON_RC_CLIENT_MASTER_NOT_FOUND:
                        return "Unable to determine master";
                        break;
                case ARAKOON_RC_CLIENT_NOT_CONNECTED:
                        return "Client not connected";
                        break;
                case ARAKOON_RC_CLIENT_TIMEOUT:
                        return "Client timeout";
                        break;

                default:
                        return "Unknown return code";
        }
}

/* Logging */
static ArakoonLogHandler log_handler = NULL;

void arakoon_log_set_handler(const ArakoonLogHandler handler) {
        log_handler = handler;
}

#define DEFINE_LOG_FUNCTION(n, l)                      \
        static void n(const char *format, ...) {       \
                va_list args;                          \
                char buffer[1024];                     \
                va_start(args, format);                \
                                                       \
                if(log_handler == NULL) {              \
                        return;                        \
                }                                      \
                                                       \
                vsnprintf(buffer, 1024, format, args); \
                buffer[1023] = 0;                      \
                                                       \
                log_handler(l, buffer);                \
                                                       \
                va_end(args);                          \
        }

#ifdef ENABLE_TRACE
DEFINE_LOG_FUNCTION(log_trace, ARAKOON_LOG_TRACE)
#else
#define log_trace(f) STMT_START STMT_END
#endif
DEFINE_LOG_FUNCTION(log_debug, ARAKOON_LOG_DEBUG)
DEFINE_LOG_FUNCTION(log_info, ARAKOON_LOG_INFO)
DEFINE_LOG_FUNCTION(log_warning, ARAKOON_LOG_WARNING)
DEFINE_LOG_FUNCTION(log_error, ARAKOON_LOG_ERROR)
DEFINE_LOG_FUNCTION(log_fatal, ARAKOON_LOG_FATAL)

#undef DEFINE_LOG_FUNCTION

#ifdef ENABLE_TRACE
# define FUNCTION_ENTER(n)                        \
        STMT_START                                \
        log_trace("Enter " ARAKOON_STRINGIFY(n)); \
        STMT_END
#else
# define FUNCTION_ENTER(n)  \
        STMT_START STMT_END
#endif

/* Memory management */
static ArakoonMemoryHooks memory_hooks = {
        malloc,
        free,
        realloc
};

#define arakoon_mem_malloc(s) (memory_hooks.malloc(s))
#define arakoon_mem_free(p) (memory_hooks.free(p))
#define arakoon_mem_realloc(p, s) (memory_hooks.realloc(p, s))

#define arakoon_mem_new(c, t) \
        (t *)(arakoon_mem_malloc(c * sizeof(t)))

#define RETURN_NULL_IF_NULL(v) \
        STMT_START             \
        if(v == NULL) {        \
                return NULL;   \
        }                      \
        STMT_END
#define RETURN_ENOMEM_IF_NULL(v) \
        STMT_START               \
        if(v == NULL) {          \
                return -ENOMEM;  \
        }                        \
        STMT_END
#define RETURN_IF_NULL(v) \
        STMT_START        \
        if(v == NULL) {   \
                return;   \
        }                 \
        STMT_END

#define RETURN_IF_NOT_SUCCESS(r)        \
        STMT_START                      \
        if(!ARAKOON_RC_IS_SUCCESS(r)) { \
                return r;               \
        }                               \
        STMT_END

void arakoon_memory_set_hooks(const ArakoonMemoryHooks * const hooks) {
        FUNCTION_ENTER(arakoon_memory_set_hooks);

        memory_hooks.malloc = hooks->malloc;
        memory_hooks.free = hooks->free;
        memory_hooks.realloc = hooks->realloc;
}

static void * arakoon_memory_abort_malloc(size_t s)
        ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_MALLOC;
static void * arakoon_memory_abort_malloc(size_t s) {
        void *ret = NULL;

        ret = malloc(s);

        if(ret == NULL) {
                abort();
        }

        return ret;
}

static void * arakoon_memory_abort_realloc(void *ptr, size_t s)
        ARAKOON_GNUC_WARN_UNUSED_RESULT;
static void * arakoon_memory_abort_realloc(void *ptr, size_t s) {
        void *ret = NULL;

        ret = realloc(ptr, s);

        if(ret == NULL) {
                abort();
        }

        return ret;
}

const ArakoonMemoryHooks * arakoon_memory_get_abort_hooks(void) {
        static const ArakoonMemoryHooks hooks = {
                arakoon_memory_abort_malloc,
                free,
                arakoon_memory_abort_realloc
        };

        FUNCTION_ENTER(arakoon_memory_get_abort_hooks);

        return &hooks;
}

/* Utils */
char * arakoon_utils_make_string(void *data, size_t length) {
        char *s = NULL;

        FUNCTION_ENTER(arakoon_utils_make_string);

        s = arakoon_mem_realloc(data, length + 1);
        if(s == NULL) {
                arakoon_mem_free(data);

                return NULL;
        }

        s[length] = 0;

        return s;
}

/* Value lists */
typedef struct ArakoonValueListItem ArakoonValueListItem;
struct ArakoonValueListItem {
        ArakoonValueListItem *next;

        size_t value_size;
        void * value;
};

struct ArakoonValueList {
        size_t size;
        ArakoonValueListItem *first;
        ArakoonValueListItem *last;
};

struct ArakoonValueListIter {
        const ArakoonValueList *list;
        ArakoonValueListItem *current;
};

ArakoonValueList * arakoon_value_list_new(void) {
        ArakoonValueList *list = NULL;

        list = arakoon_mem_new(1, ArakoonValueList);
        RETURN_NULL_IF_NULL(list);

        list->size = 0;
        list->first = NULL;
        list->last = NULL;

        return list;
}

static arakoon_rc arakoon_value_list_prepend(ArakoonValueList *list,
    const size_t value_size, const void * const value) {
        ArakoonValueListItem *item = NULL;

        item = arakoon_mem_new(1, ArakoonValueListItem);
        RETURN_ENOMEM_IF_NULL(item);

        item->value = arakoon_mem_malloc(value_size);
        if(item->value == NULL) {
                arakoon_mem_free(item);
                return -ENOMEM;
        }

        item->next = list->first;
        item->value_size = value_size;
        memcpy(item->value, value, value_size);

        list->first = item;
        list->size = list->size + 1;

        if(list->last == NULL) {
                list->last = item;
        }

        return ARAKOON_RC_SUCCESS;
}

static arakoon_rc arakoon_value_list_append(ArakoonValueList *list,
    const size_t value_size, const void * const value) {
        ArakoonValueListItem *item = NULL;

        item = arakoon_mem_new(1, ArakoonValueListItem);
        RETURN_ENOMEM_IF_NULL(item);

        item->value = arakoon_mem_malloc(value_size);
        if(item->value == NULL) {
                arakoon_mem_free(item);
                return -ENOMEM;
        }

        item->next = NULL;
        item->value_size = value_size;
        memcpy(item->value, value, value_size);

        if(list->last != NULL) {
                list->last->next = item;
        }
        list->last = item;
        if(list->first == NULL) {
                list->first = item;
        }

        list->size = list->size + 1;

        return ARAKOON_RC_SUCCESS;
}

arakoon_rc arakoon_value_list_add(ArakoonValueList *list,
    const size_t value_size, const void * const value) {
        FUNCTION_ENTER(arakoon_value_list_add);

        return arakoon_value_list_append(list, value_size, value);
}

size_t arakoon_value_list_size(const ArakoonValueList * const list) {
        FUNCTION_ENTER(arakoon_value_list_size);

        return list->size;
}

static void arakoon_value_list_item_free(ArakoonValueListItem * const item) {
        FUNCTION_ENTER(arakoon_value_list_item_free);

        RETURN_IF_NULL(item);

        item->value_size = 0;
        arakoon_mem_free(item->value);

        arakoon_mem_free(item);
}

void arakoon_value_list_free(ArakoonValueList * const list) {
        ArakoonValueListItem *item = NULL, *olditem = NULL;

        FUNCTION_ENTER(arakoon_value_list_free);

        RETURN_IF_NULL(list);

        list->size = 0;
        item = list->first;
        list->first = NULL;

        while(item != NULL) {
                olditem = item;
                item = olditem->next;

                arakoon_value_list_item_free(olditem);
        }

        arakoon_mem_free(list);
}

ArakoonValueListIter * arakoon_value_list_create_iter(
    const ArakoonValueList * const list) {
        ArakoonValueListIter * iter = NULL;

        FUNCTION_ENTER(arakoon_value_list_create_iter);

        iter = arakoon_mem_new(1, ArakoonValueListIter);
        RETURN_NULL_IF_NULL(iter);

        iter->list = list;
        iter->current = list->first;

        return iter;
}

void arakoon_value_list_iter_free(ArakoonValueListIter * const iter) {
        FUNCTION_ENTER(arakoon_value_list_iter_free);

        RETURN_IF_NULL(iter);

        iter->list = NULL;
        iter->current = NULL;

        arakoon_mem_free(iter);
}

void arakoon_value_list_iter_next(ArakoonValueListIter * const iter,
    size_t * const value_size, const void ** const value) {
        FUNCTION_ENTER(arakoon_value_list_iter_next);

        if(iter->current != NULL) {
                *value_size = iter->current->value_size;
                *value = iter->current->value;

                iter->current = iter->current->next;
        }
        else {
                *value_size = 0;
                *value = NULL;
        }
}

void arakoon_value_list_iter_reset(ArakoonValueListIter * const iter) {
        FUNCTION_ENTER(arakoon_value_list_iter_reset);

        iter->current = iter->list->first;
}

/* Key-value list */
typedef struct ArakoonKeyValueListItem ArakoonKeyValueListItem;
struct ArakoonKeyValueListItem {
        ArakoonKeyValueListItem *next;

        size_t key_size;
        void * key;
        size_t value_size;
        void * value;
};

struct ArakoonKeyValueList {
        size_t size;
        ArakoonKeyValueListItem *first;
};

struct ArakoonKeyValueListIter {
        const ArakoonKeyValueList *list;
        ArakoonKeyValueListItem *current;
};

static ArakoonKeyValueList * arakoon_key_value_list_new(void)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_MALLOC;
static ArakoonKeyValueList * arakoon_key_value_list_new(void) {
        ArakoonKeyValueList *list = NULL;

        list = arakoon_mem_new(1, ArakoonKeyValueList);
        RETURN_NULL_IF_NULL(list);

        list->size = 0;
        list->first = NULL;

        return list;
}

static arakoon_rc arakoon_key_value_list_prepend(ArakoonKeyValueList *list,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_NONNULL3(1, 3, 5);
static arakoon_rc arakoon_key_value_list_prepend(ArakoonKeyValueList *list,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value) {
        ArakoonKeyValueListItem *item = NULL;

        item = arakoon_mem_new(1, ArakoonKeyValueListItem);
        RETURN_ENOMEM_IF_NULL(item);

        item->key = arakoon_mem_malloc(key_size);
        if(item->key == NULL) {
                arakoon_mem_free(item);
                return -ENOMEM;
        }

        item->value = arakoon_mem_malloc(value_size);
        if(item->value == NULL) {
                arakoon_mem_free(item->key);
                arakoon_mem_free(item);
                return -ENOMEM;
        }

        item->next = list->first;
        item->key_size = key_size;
        memcpy(item->key, key, key_size);
        item->value_size = value_size;
        memcpy(item->value, value, value_size);

        list->first = item;
        list->size = list->size + 1;

        return ARAKOON_RC_SUCCESS;
}

size_t arakoon_key_value_list_size(const ArakoonKeyValueList * const list) {
        FUNCTION_ENTER(arakoon_key_value_list_size);

        return list->size;
}

static void arakoon_key_value_list_item_free(
    ArakoonKeyValueListItem * const item) {
        FUNCTION_ENTER(arakoon_key_value_list_item_free);

        RETURN_IF_NULL(item);

        item->key_size = 0;
        arakoon_mem_free(item->key);
        item->value_size = 0;
        arakoon_mem_free(item->value);

        arakoon_mem_free(item);
}

void arakoon_key_value_list_free(ArakoonKeyValueList * const list) {
        ArakoonKeyValueListItem *item = NULL, *olditem = NULL;

        FUNCTION_ENTER(arakoon_key_value_list_free);

        RETURN_IF_NULL(list);

        list->size = 0;
        item = list->first;
        list->first = NULL;

        while(item != NULL) {
                olditem = item;
                item = olditem->next;

                arakoon_key_value_list_item_free(olditem);
        }

        arakoon_mem_free(list);
}

ArakoonKeyValueListIter * arakoon_key_value_list_create_iter(
    const ArakoonKeyValueList * const list) {
        ArakoonKeyValueListIter * iter = NULL;

        FUNCTION_ENTER(arakoon_key_value_list_create_iter);

        iter = arakoon_mem_new(1, ArakoonKeyValueListIter);
        RETURN_NULL_IF_NULL(iter);

        iter->list = list;
        iter->current = list->first;

        return iter;
}

void arakoon_key_value_list_iter_free(ArakoonKeyValueListIter * const iter) {
        FUNCTION_ENTER(arakoon_key_value_list_iter_free);

        RETURN_IF_NULL(iter);

        iter->list = NULL;
        iter->current = NULL;

        arakoon_mem_free(iter);
}

void arakoon_key_value_list_iter_next(ArakoonKeyValueListIter * const iter,
    size_t * const key_size, const void ** const key,
    size_t * const value_size, const void ** const value) {
        FUNCTION_ENTER(arakoon_key_value_list_iter_next);

        if(iter->current != NULL) {
                *key_size = iter->current->key_size;
                *key = iter->current->key;
                *value_size = iter->current->value_size;
                *value = iter->current->value;

                iter->current = iter->current->next;
        }
        else {
                *key_size = 0;
                *key = NULL;
                *value_size = 0;
                *value = NULL;
        }
}

void arakoon_key_value_list_iter_reset(ArakoonKeyValueListIter * const iter) {
        FUNCTION_ENTER(arakoon_key_value_list_iter_reset);

        iter->current = iter->list->first;
}

/* Sequences */
typedef struct ArakoonSequenceItem ArakoonSequenceItem;
struct ArakoonSequenceItem {
        enum {
                ARAKOON_SEQUENCE_ITEM_TYPE_SET,
                ARAKOON_SEQUENCE_ITEM_TYPE_DELETE,
                ARAKOON_SEQUENCE_ITEM_TYPE_TEST_AND_SET
        } type;

        union {
                struct {
                        size_t key_size;
                        void * key;
                        size_t value_size;
                        void * value;
                } set;

                struct {
                        size_t key_size;
                        void * key;
                } delete;

                struct {
                        size_t key_size;
                        void * key;
                        size_t old_value_size;
                        void * old_value;
                        size_t new_value_size;
                        void * new_value;
                } test_and_set;
        } data;

        ArakoonSequenceItem * next;
};

static void arakoon_sequence_item_free(ArakoonSequenceItem *item) {
        RETURN_IF_NULL(item);

        switch(item->type) {
                case ARAKOON_SEQUENCE_ITEM_TYPE_SET: {
                        arakoon_mem_free(item->data.set.key);
                        arakoon_mem_free(item->data.set.value);
                }; break;
                case ARAKOON_SEQUENCE_ITEM_TYPE_DELETE: {
                        arakoon_mem_free(item->data.delete.key);
                }; break;
                case ARAKOON_SEQUENCE_ITEM_TYPE_TEST_AND_SET: {
                        arakoon_mem_free(item->data.test_and_set.key);
                        arakoon_mem_free(item->data.test_and_set.old_value);
                        arakoon_mem_free(item->data.test_and_set.new_value);
                }; break;
                default: {
                        log_fatal("Unknown sequence item type");
                        abort();
                }; break;
        }

        arakoon_mem_free(item);
}

struct ArakoonSequence {
        ArakoonSequenceItem *item;
};

ArakoonSequence * arakoon_sequence_new(void) {
        ArakoonSequence * sequence = NULL;

        FUNCTION_ENTER(arakoon_sequence_new);
        
        sequence = arakoon_mem_new(1, ArakoonSequence);
        RETURN_NULL_IF_NULL(sequence);

        sequence->item = NULL;

        return sequence;
}

void arakoon_sequence_free(ArakoonSequence *sequence) {
        ArakoonSequenceItem *item = NULL, *next = NULL;

        FUNCTION_ENTER(arakoon_sequence_free);

        RETURN_IF_NULL(sequence);

        next = sequence->item;

        while(next != NULL) {
                item = next;
                next = item->next;

                arakoon_sequence_item_free(item);
        }

        arakoon_mem_free(sequence);
}

#define PRELUDE(n)                        \
        ArakoonSequenceItem *item = NULL; \
        FUNCTION_ENTER(n)
#define OUVERTURE(t)                                    \
        STMT_START                                      \
        item = arakoon_mem_new(1, ArakoonSequenceItem); \
        RETURN_ENOMEM_IF_NULL(item);                    \
        item->type = t;                                 \
        STMT_END
#define POSTLUDIUM(n)                \
        STMT_START                   \
        item->next = sequence->item; \
        sequence->item = item;       \
        return ARAKOON_RC_SUCCESS;   \
        STMT_END

#define COPY_STRING(t, n)                                 \
        STMT_START                                        \
        item->data.t.n##_size = n##_size;                 \
        /* TODO This alloc could fail, handle it! */      \
        item->data.t.n = arakoon_mem_new(n##_size, char); \
        if(item->data.t.n == NULL) {                      \
                abort();                                  \
        }                                                 \
        memcpy(item->data.t.n, n, n##_size);              \
        STMT_END

#define COPY_STRING_OPTION(t, n)           \
        STMT_START                         \
        item->data.t.n##_size = n##_size;  \
        if(n == NULL) {                    \
                if(n##_size != 0) {        \
                        /* TODO */         \
                        abort();           \
                }                          \
                                           \
                item->data.t.n##_size = 0; \
                item->data.t.n = NULL;     \
        }                                  \
        else {                             \
                COPY_STRING(t, n);         \
        }                                  \
        STMT_END

arakoon_rc arakoon_sequence_add_set(ArakoonSequence *sequence,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value) {
        PRELUDE(arakoon_sequence_add_set);
        OUVERTURE(ARAKOON_SEQUENCE_ITEM_TYPE_SET);

        COPY_STRING(set, key);
        COPY_STRING(set, value);

        POSTLUDIUM(arakoon_sequence_add_set);
}

arakoon_rc arakoon_sequence_add_delete(ArakoonSequence *sequence,
    const size_t key_size, const void * const key) {
        PRELUDE(arakoon_sequence_add_delete);
        OUVERTURE(ARAKOON_SEQUENCE_ITEM_TYPE_DELETE);

        COPY_STRING(delete, key);

        POSTLUDIUM(arakoon_sequence_add_delete);
}

arakoon_rc arakoon_sequence_add_test_and_set(ArakoonSequence *sequence,
    const size_t key_size, const void * const key,
    const size_t old_value_size, const void * const old_value,
    const size_t new_value_size, const void * const new_value) {
        PRELUDE(arakoon_sequence_add_test_and_set);
        OUVERTURE(ARAKOON_SEQUENCE_ITEM_TYPE_TEST_AND_SET);

        COPY_STRING(test_and_set, key);

        COPY_STRING_OPTION(test_and_set, old_value);
        COPY_STRING_OPTION(test_and_set, new_value);

        POSTLUDIUM(arakoon_sequence_add_test_and_set);
}

#undef PRELUDE
#undef OUVERTURE
#undef POSTLUDIUM

#undef COPY_STRING
#undef COPY_STRING_OPTION

/* Cluster */

typedef struct ArakoonClusterNode ArakoonClusterNode;

struct ArakoonClusterNode {
        char * name;
        const ArakoonCluster * cluster;
        struct addrinfo * address;
        int fd;

        ArakoonClusterNode * next;
};

struct ArakoonCluster {
        char * name;

        ArakoonClusterNode * nodes;
        ArakoonClusterNode * master;
};

static void arakoon_cluster_node_disconnect(ArakoonClusterNode *node);

static ArakoonClusterNode * arakoon_cluster_node_new(const char * name,
    const ArakoonCluster * const cluster, struct addrinfo * address) {
        ArakoonClusterNode *ret = NULL;
        size_t len = 0;

        FUNCTION_ENTER(arakoon_cluster_node_new);

        ret = arakoon_mem_new(1, ArakoonClusterNode);
        RETURN_NULL_IF_NULL(ret);

        memset(ret, 0, sizeof(ArakoonClusterNode));

        len = strlen(name) + 1;
        ret->name = arakoon_mem_new(len, char);
        if(ret->name == NULL) {
                goto nomem;
        }

        strncpy(ret->name, name, len);
        ret->cluster = cluster;

        ret->address = address;
        ret->fd = -1;
        ret->next = NULL;

        return ret;

nomem:
        if(ret != NULL) {
                arakoon_mem_free(ret->name);
        }

        arakoon_mem_free(ret);

        return NULL;
}

static void arakoon_cluster_node_free(ArakoonClusterNode *node) {
        FUNCTION_ENTER(arakoon_cluster_node_free);

        RETURN_IF_NULL(node);

        if(node->fd >= 0) {
                log_warning("Freeing a cluster node which wasn't "
                        "disconnected before");
                arakoon_cluster_node_disconnect(node);
        }

        arakoon_mem_free(node->name);
        freeaddrinfo(node->address);
        arakoon_mem_free(node);
}

static arakoon_rc arakoon_cluster_node_connect(ArakoonClusterNode *node,
    int *timeout) {
        struct addrinfo *rp = NULL;
        size_t n = 0, len = 0;
        char *prologue = NULL, *p = NULL;
        arakoon_rc rc = 0;

        FUNCTION_ENTER(arakoon_cluster_node_connect);

        if(node->fd >= 0) {
                log_warning("arakoon_cluster_node_connect called, but FD >= 0");

                return ARAKOON_RC_SUCCESS;
        }

        for(rp = node->address; rp != NULL; rp = rp->ai_next) {
                node->fd = socket(rp->ai_family, rp->ai_socktype,
                        rp->ai_protocol);

                if(node->fd == -1) {
                        continue;
                }

                if(connect(node->fd, rp->ai_addr, rp->ai_addrlen) != -1) {
                        break;
                }

                shutdown(node->fd, SHUT_RDWR);
                close(node->fd);
                node->fd = -1;
        }

        if(rp == NULL) {
                log_error("Unable to connect to node %s", node->name);

                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }

        /* Send prologue */
        n = strlen(node->cluster->name);
        len = ARAKOON_PROTOCOL_COMMAND_LEN + ARAKOON_PROTOCOL_INT32_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(n);

        prologue = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(prologue);

        p = prologue;

        ARAKOON_PROTOCOL_WRITE_COMMAND(p, 0, 0);
        ARAKOON_PROTOCOL_WRITE_INT32(p, ARAKOON_PROTOCOL_VERSION);
        ARAKOON_PROTOCOL_WRITE_STRING(p, node->cluster->name, n);

        WRITE_BYTES(node, prologue, len, rc, timeout);

        arakoon_mem_free(prologue);

        return rc;
}

#define ASSERT_ALL_WRITTEN(command, c, len)                              \
        STMT_START                                                       \
        if(c != command + len) {                                         \
                log_fatal("Unexpected number of characters in command"); \
                abort();                                                 \
        }                                                                \
        STMT_END

static arakoon_rc arakoon_cluster_node_who_master(ArakoonClusterNode *node,
    int *timeout, char ** const master) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        void *result_data = NULL;
        size_t result_size = 0;

        FUNCTION_ENTER(arakoon_cluster_node_who_master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x02, 0x00);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(node, command, len, rc, timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(node, rc, timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_STRING_OPTION(node, result_data, result_size,
                rc, timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *master = NULL;
                return rc;
        }

        *master = arakoon_utils_make_string(result_data, result_size);
        RETURN_ENOMEM_IF_NULL(*master);

        return rc;
}

static void arakoon_cluster_node_disconnect(ArakoonClusterNode *node) {
        FUNCTION_ENTER(arakoon_cluster_node_disconnect);

        if(node->fd >= 0) {
                shutdown(node->fd, SHUT_RDWR);
                close(node->fd);
        }

        node->fd = -1;
}


ArakoonCluster * arakoon_cluster_new(const char * const name) {
        ArakoonCluster *ret = NULL;
        size_t len = 0;

        FUNCTION_ENTER(arakoon_cluster_new);

        ret = arakoon_mem_new(1, ArakoonCluster);
        RETURN_NULL_IF_NULL(ret);

        memset(ret, 0, sizeof(ArakoonCluster));

        len = strlen(name) + 1;
        ret->name = arakoon_mem_new(len, char);
        if(ret->name == NULL) {
                goto nomem;
        }

        strncpy(ret->name, name, len);

        ret->nodes = NULL;
        ret->master = NULL;

        return ret;

nomem:
        if(ret != NULL) {
                arakoon_mem_free(ret->name);
        }

        arakoon_mem_free(ret);

        return NULL;
}

void arakoon_cluster_free(ArakoonCluster *cluster) {
        ArakoonClusterNode *node = NULL, *next_node = NULL;

        FUNCTION_ENTER(arakoon_cluster_free);

        RETURN_IF_NULL(cluster);

        arakoon_mem_free(cluster->name);

        node = cluster->nodes;
        while(node != NULL) {
                next_node = node->next;
                arakoon_cluster_node_disconnect(node);
                arakoon_cluster_node_free(node);
                node = next_node;
        }

        arakoon_mem_free(cluster);
}



arakoon_rc arakoon_cluster_connect_master(ArakoonCluster * const cluster,
    const ArakoonClientCallOptions * const options) {
        ArakoonClusterNode *node = NULL;
        arakoon_rc rc = 0;
        char *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_cluster_connect_master);

        log_debug("Looking up master node");

        timeout = options != NULL ?
                arakoon_client_call_options_get_timeout(options) :
                ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        /* Find a node to which we can connect */
        node = cluster->nodes;
        while(node != NULL) {
                rc = arakoon_cluster_node_connect(node, &timeout);

                if(ARAKOON_RC_IS_SUCCESS(rc)) {
                        log_debug("Connected to node %s", node->name);
                        break;
                }

                node = node->next;
        }

        if(node == NULL) {
                log_warning("Unable to connect to any node");
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }

        /* Retrieve master, according to the node */
        rc = arakoon_cluster_node_who_master(node, &timeout, &master);
        RETURN_IF_NOT_SUCCESS(rc);

        if(strcmp(node->name, master) == 0) {
                /* The node is master */
                cluster->master = node;
                arakoon_mem_free(master);

                log_info("Found master node %s", cluster->master->name);

                return ARAKOON_RC_SUCCESS;
        }

        /* Find master node */
        node = cluster->nodes;
        while(node) {
                if(strcmp(node->name, master) == 0) {
                        break;
                }
                node = node->next;
        }

        arakoon_mem_free(master);

        if(node == NULL) {
                return ARAKOON_RC_CLIENT_UNKNOWN_NODE;
        }

        log_debug("Connecting to master node %s", node->name);

        rc = arakoon_cluster_node_connect(node, &timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        /* Check whether master thinks it's master */
        log_debug("Validating master node");

        rc = arakoon_cluster_node_who_master(node, &timeout, &master);
        RETURN_IF_NOT_SUCCESS(rc);

        if(strcmp(node->name, master) != 0) {
                rc = ARAKOON_RC_CLIENT_MASTER_NOT_FOUND;
        }
        else {
                rc = ARAKOON_RC_SUCCESS;
                cluster->master = node;
        }

        arakoon_mem_free(master);

        log_debug("Found master node %s", cluster->master->name);

        return rc;
}

const char * arakoon_cluster_get_name(const ArakoonCluster * const cluster) {
        FUNCTION_ENTER(arakoon_cluster_get_name);

        return cluster->name;
}

arakoon_rc arakoon_cluster_add_node(ArakoonCluster *cluster,
    const char * const name, struct addrinfo * const address) {
        ArakoonClusterNode *node = NULL;

        FUNCTION_ENTER(arakoon_cluster_add_node);

        log_debug("Adding node %s to cluster %s", name, cluster->name);

        node = arakoon_cluster_node_new(name, cluster, address);
        RETURN_ENOMEM_IF_NULL(node);

        node->next = cluster->nodes;
        cluster->nodes = node;

        return ARAKOON_RC_SUCCESS;
}

arakoon_rc arakoon_cluster_add_node_tcp(ArakoonCluster *cluster,
    const char * const name, const char * const host, const char * const service) {
        struct addrinfo hints;
        struct addrinfo *result;
        int rc = 0;

        FUNCTION_ENTER(arakoon_cluster_add_node_tcp);

        log_debug("Looking up node %s at %s:%s", name, host, service);

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC; /* IPv4 and IPv6, whatever */
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0; /* Any protocol */

        rc = getaddrinfo(host, service, &hints, &result);

        if(rc != 0) {
                log_error("Address lookup failed: %s", gai_strerror(rc));
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }

        rc = arakoon_cluster_add_node(cluster, name, result);

        return rc;
}

/* Client call options */
struct ArakoonClientCallOptions {
        arakoon_bool allow_dirty;
        int timeout;
};

static const ArakoonClientCallOptions *
    arakoon_client_call_options_get_default(void) {
        static const ArakoonClientCallOptions options = {
                ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_ALLOW_DIRTY,
                ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT
        };

        return &options;
}

ArakoonClientCallOptions * arakoon_client_call_options_new(void) {
        ArakoonClientCallOptions *options = NULL;

        FUNCTION_ENTER(arakoon_client_call_options_new);

        options = arakoon_mem_new(1, ArakoonClientCallOptions);
        RETURN_NULL_IF_NULL(options);

        memcpy(options, arakoon_client_call_options_get_default(),
                sizeof(ArakoonClientCallOptions));

        return options;
}

void arakoon_client_call_options_free(ArakoonClientCallOptions *options) {
        FUNCTION_ENTER(arakoon_client_call_options_free);

        arakoon_mem_free(options);
}

arakoon_bool arakoon_client_call_options_get_allow_dirty(
    const ArakoonClientCallOptions * const options) {
        FUNCTION_ENTER(arakoon_client_call_options_get_allow_dirty);

        return options->allow_dirty;
}

void arakoon_client_call_options_set_allow_dirty(
    ArakoonClientCallOptions * const options, arakoon_bool allow_dirty) {
        FUNCTION_ENTER(arakoon_client_call_options_set_allow_dirty);

        options->allow_dirty = allow_dirty;
}

int arakoon_client_call_options_get_timeout(
    const ArakoonClientCallOptions * const options) {
        FUNCTION_ENTER(arakoon_client_call_options_get_timeout);

        return options->timeout;
}

void arakoon_client_call_options_set_timeout(
    ArakoonClientCallOptions * const options, int timeout) {
        FUNCTION_ENTER(arakoon_client_call_options_set_timeout);

        options->timeout = timeout;
}

/* Client operations */
#define READ_OPTIONS \
        const ArakoonClientCallOptions *options_ = \
                (options == NULL ? arakoon_client_call_options_get_default() : options)

#define GET_CLUSTER_MASTER(c, m)                        \
        STMT_START                                      \
        if(c->master == NULL) {                         \
                return ARAKOON_RC_CLIENT_NOT_CONNECTED; \
        }                                               \
        if(c->master->fd < 0) {                         \
                return ARAKOON_RC_CLIENT_NOT_CONNECTED; \
        }                                               \
                                                        \
        m = c->master;                                  \
        STMT_END

arakoon_rc arakoon_hello(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const char * const client_id, const char * const cluster_id,
    char ** const result) {
        size_t len = 0, client_id_len = 0, cluster_id_len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        void *result_data = NULL;
        size_t result_size = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_hello);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        GET_CLUSTER_MASTER(cluster, master);

        client_id_len = strlen(client_id);
        cluster_id_len = strlen(cluster_id);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(client_id_len)
                + ARAKOON_PROTOCOL_STRING_LEN(cluster_id_len);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x01, 0x00);
        ARAKOON_PROTOCOL_WRITE_STRING(c, client_id, client_id_len);
        ARAKOON_PROTOCOL_WRITE_STRING(c, cluster_id, cluster_id_len);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_STRING(master, result_data,
                result_size, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result = NULL;
                return rc;
        }

        *result = arakoon_utils_make_string(result_data, result_size);
        RETURN_ENOMEM_IF_NULL(*result);

        return rc;
}

arakoon_rc arakoon_who_master(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    char ** const master) {
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_who_master);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        return arakoon_cluster_node_who_master(cluster->master, &timeout,
                master);
}

arakoon_rc arakoon_expect_progress_possible(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    arakoon_bool *result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_expect_progress_possible);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x12, 0x00);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_BOOL(master, *result, rc, &timeout);

        return rc;
}

arakoon_rc arakoon_exists(ArakoonCluster * const cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key, arakoon_bool *result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_exists);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x07, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c,
                arakoon_client_call_options_get_allow_dirty(options_));
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_BOOL(master, *result, rc, &timeout);

        return rc;
}

arakoon_rc arakoon_get(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    size_t *result_size, void **result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_get);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x08, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c,
                arakoon_client_call_options_get_allow_dirty(options_));
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result_size = 0;
                *result = NULL;
                return rc;
        }

        ARAKOON_PROTOCOL_READ_STRING(master, *result, *result_size, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result_size = 0;
                *result = NULL;
        }

        return rc;
}

arakoon_rc arakoon_set(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_set);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size)
                + ARAKOON_PROTOCOL_STRING_LEN(value_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x09, 0x00);
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);
        ARAKOON_PROTOCOL_WRITE_STRING(c, value, value_size);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);

        return rc;
}

arakoon_rc arakoon_multi_get(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const ArakoonValueList * const keys, ArakoonValueList **result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonValueListItem *item = NULL;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_multi_get);

        GET_CLUSTER_MASTER(cluster, master);

        *result = NULL;

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_UINT32_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x11, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c,
                arakoon_client_call_options_get_allow_dirty(options_));
        ARAKOON_PROTOCOL_WRITE_UINT32(c, arakoon_value_list_size(keys));

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        for(item = keys->first; item != NULL; item = item->next) {
                /* TODO Multi syscall vs memory copies... */
                WRITE_BYTES(master, &(item->value_size),
                        ARAKOON_PROTOCOL_UINT32_LEN, rc, &timeout);
                RETURN_IF_NOT_SUCCESS(rc);

                WRITE_BYTES(master, item->value, item->value_size, rc,
                        &timeout);
                RETURN_IF_NOT_SUCCESS(rc);
        }

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        *result = arakoon_value_list_new();
        RETURN_ENOMEM_IF_NULL(*result);

        ARAKOON_PROTOCOL_READ_STRING_LIST(master, *result, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                arakoon_value_list_free(*result);
                *result = NULL;
        }

        return rc;
}

arakoon_rc arakoon_delete(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_delete);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x0a, 0x00);
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);

        return rc;
}

arakoon_rc arakoon_range(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t begin_key_size, const void * const begin_key,
    const arakoon_bool begin_key_included,
    const size_t end_key_size, const void * const end_key,
    const arakoon_bool end_key_included,
    const ssize_t max_elements,
    ArakoonValueList **result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_range);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_OPTION_LEN(begin_key, begin_key_size)
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_OPTION_LEN(end_key, end_key_size)
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_INT32_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x0b, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c,
                arakoon_client_call_options_get_allow_dirty(options_));
        ARAKOON_PROTOCOL_WRITE_STRING_OPTION(c, begin_key, begin_key_size);
        ARAKOON_PROTOCOL_WRITE_BOOL(c, begin_key_included);
        ARAKOON_PROTOCOL_WRITE_STRING_OPTION(c, end_key, end_key_size);
        ARAKOON_PROTOCOL_WRITE_BOOL(c, end_key_included);
        ARAKOON_PROTOCOL_WRITE_INT32(c, max_elements);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result = NULL;
                return rc;
        }

        *result = arakoon_value_list_new();
        RETURN_ENOMEM_IF_NULL(*result);

        ARAKOON_PROTOCOL_READ_STRING_LIST(master, *result, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                arakoon_value_list_free(*result);
                *result = NULL;
        }

        return rc;
}

arakoon_rc arakoon_range_entries(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t begin_key_size, const void * const begin_key,
    const arakoon_bool begin_key_included,
    const size_t end_key_size, const void * const end_key,
    const arakoon_bool end_key_included,
    const ssize_t max_elements,
    ArakoonKeyValueList **result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_range_entries);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_OPTION_LEN(begin_key, begin_key_size)
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_OPTION_LEN(end_key, end_key_size)
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_INT32_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x0f, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c,
                arakoon_client_call_options_get_allow_dirty(options_));
        ARAKOON_PROTOCOL_WRITE_STRING_OPTION(c, begin_key, begin_key_size);
        ARAKOON_PROTOCOL_WRITE_BOOL(c, begin_key_included);
        ARAKOON_PROTOCOL_WRITE_STRING_OPTION(c, end_key, end_key_size);
        ARAKOON_PROTOCOL_WRITE_BOOL(c, end_key_included);
        ARAKOON_PROTOCOL_WRITE_INT32(c, max_elements);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result = NULL;
                return rc;
        }

        *result = arakoon_key_value_list_new();
        RETURN_ENOMEM_IF_NULL(*result);

        ARAKOON_PROTOCOL_READ_STRING_STRING_LIST(master, *result, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                arakoon_key_value_list_free(*result);
                *result = NULL;
        }

        return rc;
}

arakoon_rc arakoon_prefix(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t begin_key_size, const void * const begin_key,
    const ssize_t max_elements,
    ArakoonValueList **result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_prefix);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(begin_key_size)
                + ARAKOON_PROTOCOL_UINT32_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x0c, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c,
                arakoon_client_call_options_get_allow_dirty(options_));
        ARAKOON_PROTOCOL_WRITE_STRING(c, begin_key, begin_key_size);
        ARAKOON_PROTOCOL_WRITE_INT32(c, max_elements);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result = NULL;
                return rc;
        }

        *result = arakoon_value_list_new();
        RETURN_ENOMEM_IF_NULL(*result);

        ARAKOON_PROTOCOL_READ_STRING_LIST(master, *result, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                arakoon_value_list_free(*result);
                *result = NULL;
        }

        return rc;
}

arakoon_rc arakoon_test_and_set(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    const size_t old_value_size, const void * const old_value,
    const size_t new_value_size, const void * const new_value,
    size_t *result_size, void **result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_test_and_set);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        GET_CLUSTER_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size)
                + ARAKOON_PROTOCOL_STRING_OPTION_LEN(old_value, old_value_size)
                + ARAKOON_PROTOCOL_STRING_OPTION_LEN(new_value, new_value_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x0d, 0x00);
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);
        ARAKOON_PROTOCOL_WRITE_STRING_OPTION(c, old_value, old_value_size);
        ARAKOON_PROTOCOL_WRITE_STRING_OPTION(c, new_value, new_value_size);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_STRING_OPTION(master, *result,
                *result_size, rc, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result = NULL;
                *result_size = 0;
        }

        return rc;
}

arakoon_rc arakoon_sequence(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const ArakoonSequence * const sequence) {
        size_t len = 0, i = 0;
        uint32_t count = 0;
        ArakoonSequenceItem *item = NULL;
        char *command = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_sequence);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        GET_CLUSTER_MASTER(cluster, master);

#define I(n) (len += n)

        I(ARAKOON_PROTOCOL_COMMAND_LEN); /* Command */
        I(ARAKOON_PROTOCOL_UINT32_LEN); /* Total string size */
        I(ARAKOON_PROTOCOL_UINT32_LEN); /* Outer sequence */
        I(ARAKOON_PROTOCOL_UINT32_LEN); /* Number of sequence items */

        for(item = sequence->item; item != NULL; item = item->next) {
                I(ARAKOON_PROTOCOL_UINT32_LEN); /* Command type */

                switch(item->type) {
                        case ARAKOON_SEQUENCE_ITEM_TYPE_SET: {
                                I(ARAKOON_PROTOCOL_STRING_LEN(item->data.set.key_size));
                                I(ARAKOON_PROTOCOL_STRING_LEN(item->data.set.value_size));
                        }; break;
                        case ARAKOON_SEQUENCE_ITEM_TYPE_DELETE: {
                                I(ARAKOON_PROTOCOL_STRING_LEN(item->data.delete.key_size));
                        }; break;
                        case ARAKOON_SEQUENCE_ITEM_TYPE_TEST_AND_SET: {
                                I(ARAKOON_PROTOCOL_STRING_LEN(item->data.test_and_set.key_size));
                                I(ARAKOON_PROTOCOL_STRING_OPTION_LEN(item->data.test_and_set.old_value,
                                        item->data.test_and_set.old_value_size));
                                I(ARAKOON_PROTOCOL_STRING_OPTION_LEN(item->data.test_and_set.new_value,
                                        item->data.test_and_set.new_value_size));
                        }; break;
                        default: {
                                log_fatal("Invalid sequence type");
                                abort();
                        }; break;
                }
        }

#undef I

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        i = len;

        /* NOTE This code builds the command back-to-front! */
#define WRITE_UINT32(n)                   \
        STMT_START                        \
        i -= ARAKOON_PROTOCOL_UINT32_LEN; \
        *((uint32_t *)&command[i]) = n;   \
        STMT_END

#define WRITE_STRING(s, l)         \
        STMT_START                 \
        i -= l;                    \
        memcpy(&command[i], s, l); \
        WRITE_UINT32(l);           \
        STMT_END

#define WRITE_STRING_OPTION(s, l)                                            \
        STMT_START                                                           \
        if(s == NULL) {                                                      \
                if(l != 0) {                                                 \
                        log_error("String is NULL, but length is non-zero"); \
                }                                                            \
                i -= ARAKOON_PROTOCOL_BOOL_LEN;                              \
                command[i] = ARAKOON_BOOL_FALSE;                             \
        }                                                                    \
        else {                                                               \
                WRITE_STRING(s, l);                                          \
                i -= ARAKOON_PROTOCOL_BOOL_LEN;                              \
                command[i] = ARAKOON_BOOL_TRUE;                              \
        }                                                                    \
        STMT_END

        for(item = sequence->item; item != NULL; item = item->next) {
                count++;

                switch(item->type) {
                        case ARAKOON_SEQUENCE_ITEM_TYPE_SET: {
                                WRITE_STRING(item->data.set.value,
                                        item->data.set.value_size);
                                WRITE_STRING(item->data.set.key,
                                        item->data.set.key_size);
                                WRITE_UINT32(1);
                        }; break;
                        case ARAKOON_SEQUENCE_ITEM_TYPE_DELETE: {
                                WRITE_STRING(item->data.delete.key,
                                        item->data.delete.key_size);
                                WRITE_UINT32(2);
                        }; break;
                        case ARAKOON_SEQUENCE_ITEM_TYPE_TEST_AND_SET: {
                                WRITE_STRING_OPTION(item->data.test_and_set.new_value,
                                        item->data.test_and_set.new_value_size);
                                WRITE_STRING_OPTION(item->data.test_and_set.old_value,
                                        item->data.test_and_set.old_value_size);
                                WRITE_STRING(item->data.test_and_set.key,
                                        item->data.test_and_set.key_size);
                                WRITE_UINT32(3);
                        }; break;
                        default: {
                                log_fatal("Invalid sequence type");
                                abort();
                        }; break;
                }
        }

        if(i != ARAKOON_PROTOCOL_COMMAND_LEN /* Command */
                        + ARAKOON_PROTOCOL_UINT32_LEN /* String length */
                        + ARAKOON_PROTOCOL_UINT32_LEN /* Outer sequence */
                        + ARAKOON_PROTOCOL_UINT32_LEN /* Item count */
          ) {
                log_fatal("Incorrect count in sequence construction");
                abort();
        }

        WRITE_UINT32(count);
        WRITE_UINT32(5);
        WRITE_UINT32(len - ARAKOON_PROTOCOL_UINT32_LEN - ARAKOON_PROTOCOL_COMMAND_LEN);

#undef WRITE_UINT32
#undef WRITE_STRING
#undef WRITE_STRING_OPTION

        if(i != ARAKOON_PROTOCOL_COMMAND_LEN) {
                log_fatal("Incorrect count in sequence construction");
                abort();
        }

        ARAKOON_PROTOCOL_WRITE_COMMAND(command, 0x10, 0x00);
        /* Macro changes our pointer... */
        command -= ARAKOON_PROTOCOL_COMMAND_LEN;

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);

        return rc;
}
