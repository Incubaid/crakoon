/* TODO
 * ====
 * - Timeouts, allow_dirty,... (use _ex)
 * - Plugable communication channels (like Pyrakoon)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "arakoon.h"

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

#define ARAKOON_PROTOCOL_WRITE_BOOL(a, b)                                                   \
        STMT_START                                                                          \
        *((char *) a) = (b == ARAKOON_BOOL_FALSE ? ARAKOON_BOOL_FALSE : ARAKOON_BOOL_TRUE); \
        a += ARAKOON_PROTOCOL_BOOL_LEN;                                                     \
        STMT_END

#define WRITE_BYTES(f, a, n, r)                        \
        STMT_START                                     \
        size_t _written = 0, _to_send = n;             \
        ssize_t _r = 0;                                \
        while(_to_send > 0) {                          \
                _r = write(f, a + _written, _to_send); \
                if(_r < 0) {                           \
                        /* TODO EINTR handling etc. */ \
                        r = -errno;                    \
                        close(f);                      \
                        f = -1;                        \
                        break;                         \
                }                                      \
                                                       \
                _to_send -= _r;                        \
                _written += _r;                        \
        }                                              \
        if(_to_send == 0) {                            \
                r = ARAKOON_RC_SUCCESS;                \
        }                                              \
        STMT_END

#define READ_BYTES(f, a, n, r)                                                  \
        STMT_START                                                              \
        size_t _read = 0, _to_read = n;                                         \
        ssize_t _r2 = 0;                                                        \
        while(_to_read > 0) {                                                   \
                _r2 = read(f, a + _read, _to_read);                             \
                if(_r2 <= 0) {                                                  \
                        /* TODO EINTR handling etc. */                          \
                        r = _r2 < 0 ? -errno : ARAKOON_RC_CLIENT_NETWORK_ERROR; \
                        close(f);                                               \
                        f = -1;                                                 \
                        break;                                                  \
                }                                                               \
                                                                                \
                _to_read -= _r2;                                                \
                _read += _r2;                                                   \
        }                                                                       \
        if(_to_read == 0) {                                                     \
                r = ARAKOON_RC_SUCCESS;                                         \
        }                                                                       \
        STMT_END

#define ARAKOON_PROTOCOL_READ_UINT32(fd, r, rc)    \
        STMT_START                                 \
        uint32_t _d = 0;                           \
        READ_BYTES(fd, &_d, sizeof(uint32_t), rc); \
        if(ARAKOON_RC_IS_SUCCESS(rc)) {            \
                r = _d;                            \
        }                                          \
        STMT_END
#define ARAKOON_PROTOCOL_READ_RC(fd, rc)           \
        STMT_START                                 \
        arakoon_rc _rc = 0;                        \
        ARAKOON_PROTOCOL_READ_UINT32(fd, rc, _rc); \
        if(!ARAKOON_RC_IS_SUCCESS(_rc)) {          \
                rc = _rc;                          \
        }                                          \
        STMT_END

#define ARAKOON_PROTOCOL_READ_STRING(fd, a, l, rc) \
        STMT_START                                 \
        uint32_t _l = 0;                           \
        char *_d = NULL;                           \
        arakoon_rc _rc = 0;                        \
        ARAKOON_PROTOCOL_READ_UINT32(fd, _l, _rc); \
        if(!ARAKOON_RC_IS_SUCCESS(_rc)) {          \
                rc = _rc;                          \
                break;                             \
        }                                          \
        _d = arakoon_mem_new(_l, char);            \
        if(_d == NULL) {                           \
                rc = -ENOMEM;                      \
                break;                             \
        }                                          \
                                                   \
        READ_BYTES(fd, _d, _l, _rc);               \
        rc = _rc;                                  \
        if(ARAKOON_RC_IS_SUCCESS(rc)) {            \
                a = _d;                            \
                l = _l;                            \
        }                                          \
        else {                                     \
                arakoon_mem_free(_d);              \
                l = 0;                             \
                a = NULL;                          \
        }                                          \
        STMT_END

#define ARAKOON_PROTOCOL_READ_STRING_OPTION(fd, a, l, rc) \
        STMT_START                                        \
        char _v = 0;                                      \
        arakoon_rc _rc = 0;                               \
        READ_BYTES(fd, &_v, 1, _rc);                      \
        if(!ARAKOON_RC_IS_SUCCESS(_rc)) {                 \
                a = NULL;                                 \
                l = 0;                                    \
                rc = _rc;                                 \
                break;                                    \
        }                                                 \
                                                          \
        if(_v == 0) {                                     \
                l = 0;                                    \
                a = NULL;                                 \
                rc = _rc;                                 \
                break;                                    \
        }                                                 \
                                                          \
        ARAKOON_PROTOCOL_READ_STRING(fd, a, l, rc);       \
        STMT_END

#define ARAKOON_PROTOCOL_READ_BOOL(fd, r, rc)                                          \
        STMT_START                                                                     \
        char _r  = 0;                                                                  \
        READ_BYTES(fd, &_r, 1, rc);                                                    \
        if(ARAKOON_RC_IS_SUCCESS(rc)) {                                                \
                r = _r == ARAKOON_BOOL_FALSE ? ARAKOON_BOOL_FALSE : ARAKOON_BOOL_TRUE; \
        }                                                                              \
        else {                                                                         \
                r = 0;                                                                 \
        }                                                                              \
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

                default:
                        return "Unknown return code";
        }
}

/* Logging */
static ArakoonLogHandler log_handler = NULL;

void arakoon_log_set_handler(const ArakoonLogHandler handler) {
        log_handler = handler;
}

static void log_va(ArakoonLogLevel level, const char *format, va_list args) {
        char buffer[1024];

        if(log_handler == NULL) {
                return;
        }

        vsnprintf(buffer, 1024, format, args);
        buffer[1023] = '\0';

        log_handler(level, buffer);
}

#define DEFINE_LOG_FUNCTION(n, l)                \
        static void n(const char *format, ...) { \
                va_list args;                    \
                va_start(args, format);          \
                log_va(l, format, args);         \
                va_end(args);                    \
        }

#ifdef ENABLE_TRACE
DEFINE_LOG_FUNCTION(log_trace, ARAKOON_LOG_TRACE)
#else
#define log_trace(f) STMT_START STMT_END
#endif
DEFINE_LOG_FUNCTION(log_debug, ARAKOON_LOG_DEBUG)
DEFINE_LOG_FUNCTION(log_warning, ARAKOON_LOG_WARNING)
DEFINE_LOG_FUNCTION(log_error, ARAKOON_LOG_ERROR)
DEFINE_LOG_FUNCTION(log_fatal, ARAKOON_LOG_FATAL)

#undef DEFINE_LOG_FUNCTION

#ifdef ENABLE_TRACE
# define FUNCTION_ENTER(n)                            \
        STMT_START                                    \
        log_trace("[T] Enter " ARAKOON_STRINGIFY(n)); \
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
typedef struct _ArakoonValueListItem ArakoonValueListItem;
struct _ArakoonValueListItem {
        ArakoonValueListItem *next;

        size_t value_size;
        void * value;
};

struct _ArakoonValueList {
        size_t size;
        ArakoonValueListItem *first;
};

struct _ArakoonValueListIter {
        const ArakoonValueList *list;
        ArakoonValueListItem *current;
};

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
typedef struct _ArakoonKeyValueListItem ArakoonKeyValueListItem;
struct _ArakoonKeyValueListItem {
        ArakoonKeyValueListItem *next;

        size_t key_size;
        void * key;
        size_t value_size;
        void * value;
};

struct _ArakoonKeyValueList {
        size_t size;
        ArakoonKeyValueListItem *first;
};

struct _ArakoonKeyValueListIter {
        const ArakoonKeyValueList *list;
        ArakoonKeyValueListItem *current;
};

size_t arakoon_key_value_list_size(const ArakoonKeyValueList * const list) {
        FUNCTION_ENTER(arakoon_key_value_list_size);

        return list->size;
}

static void arakoon_key_value_list_item_free(
    ArakoonKeyValueListItem * const item) {
        FUNCTION_ENTER(arakoon_key_value_list_item_free);

        RETURN_IF_NULL(item);

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
typedef struct _ArakoonSequenceItem ArakoonSequenceItem;
struct _ArakoonSequenceItem {
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

struct _ArakoonSequence {
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

typedef struct _ArakoonClusterNode ArakoonClusterNode;

struct _ArakoonClusterNode {
        char * name;
        const ArakoonCluster * cluster;
        struct addrinfo * address;
        int fd;

        ArakoonClusterNode * next;
};

struct _ArakoonCluster {
        char * name;

        ArakoonClusterNode * nodes;
        ArakoonClusterNode * master;
};

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
                close(node->fd);
        }

        arakoon_mem_free(node->name);
        freeaddrinfo(node->address);
        arakoon_mem_free(node);
}

static arakoon_rc arakoon_cluster_node_connect(ArakoonClusterNode *node) {
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

        WRITE_BYTES(node->fd, prologue, len, rc);

        arakoon_mem_free(prologue);

        return rc;
}

static void arakoon_cluster_node_disconnect(ArakoonClusterNode *node) {
        FUNCTION_ENTER(arakoon_cluster_node_disconnect);

        if(node->fd >= 0) {
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
                arakoon_cluster_node_free(node);
                node = next_node;
        }

        arakoon_mem_free(cluster);
}

/*arakoon_rc arakoon_cluster_load_config(ArakoonCluster *cluster,
    const char * const path) {
        abort();
}*/

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

/* Client operations */
arakoon_rc arakoon_hello(ArakoonCluster *cluster,
    const char * const client_id, const char * const cluster_id,
    char ** const result) {
        size_t len = 0, client_id_len = 0, cluster_id_len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        void *result_data = NULL;
        size_t result_size = 0;

        FUNCTION_ENTER(arakoon_hello);

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

        if(cluster->nodes->fd < 0) {
                rc = arakoon_cluster_node_connect(cluster->nodes);
                if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                        arakoon_mem_free(command);
                }

                RETURN_IF_NOT_SUCCESS(rc);
        }

        WRITE_BYTES(cluster->nodes->fd, command, len, rc);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(cluster->nodes->fd, rc);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_STRING(cluster->nodes->fd, result_data,
                result_size, rc);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result = NULL;
                return rc;
        }

        *result = arakoon_utils_make_string(result_data, result_size);
        RETURN_ENOMEM_IF_NULL(*result);

        return rc;
}

arakoon_rc arakoon_who_master(ArakoonCluster *cluster,
    char ** const master) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        void *result_data = NULL;
        size_t result_size = 0;

        FUNCTION_ENTER(arakoon_who_master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x02, 0x00);

        if(cluster->nodes->fd < 0) {
                rc = arakoon_cluster_node_connect(cluster->nodes);
                if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                        arakoon_mem_free(command);
                }

                RETURN_IF_NOT_SUCCESS(rc);
        }

        WRITE_BYTES(cluster->nodes->fd, command, len, rc);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(cluster->nodes->fd, rc);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_STRING_OPTION(cluster->nodes->fd, result_data,
                result_size, rc);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *master = NULL;
                return rc;
        }

        *master = arakoon_utils_make_string(result_data, result_size);
        RETURN_ENOMEM_IF_NULL(*master);

        return rc;
}

arakoon_rc arakoon_expect_progress_possible(ArakoonCluster *cluster,
        arakoon_bool *result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;

        FUNCTION_ENTER(arakoon_expect_progress_possible);

        len = ARAKOON_PROTOCOL_COMMAND_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x12, 0x00);

        if(cluster->nodes->fd < 0) {
                rc = arakoon_cluster_node_connect(cluster->nodes);
                if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                        arakoon_mem_free(command);
                }

                RETURN_IF_NOT_SUCCESS(rc);
        }

        WRITE_BYTES(cluster->nodes->fd, command, len, rc);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(cluster->nodes->fd, rc);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_BOOL(cluster->nodes->fd, *result, rc);

        return rc;
}

arakoon_rc arakoon_exists(ArakoonCluster * const cluster,
    const size_t key_size, const void * const key, arakoon_bool *result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;

        FUNCTION_ENTER(arakoon_exists);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x07, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c, ARAKOON_BOOL_FALSE);
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);

        if(cluster->nodes->fd < 0) {
                rc = arakoon_cluster_node_connect(cluster->nodes);
                if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                        arakoon_mem_free(command);
                }

                RETURN_IF_NOT_SUCCESS(rc);
        }

        WRITE_BYTES(cluster->nodes->fd, command, len, rc);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(cluster->nodes->fd, rc);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_BOOL(cluster->nodes->fd, *result, rc);

        return rc;
}

arakoon_rc arakoon_get(ArakoonCluster *cluster, const size_t key_size,
    const void * const key, size_t *result_size, void **result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;

        FUNCTION_ENTER(arakoon_get);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x08, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c, ARAKOON_BOOL_FALSE);
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);

        if(cluster->nodes->fd < 0) {
                rc = arakoon_cluster_node_connect(cluster->nodes);
                if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                        arakoon_mem_free(command);
                }

                RETURN_IF_NOT_SUCCESS(rc);
        }

        WRITE_BYTES(cluster->nodes->fd, command, len, rc);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(cluster->nodes->fd, rc);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result_size = 0;
                *result = NULL;
                return rc;
        }

        ARAKOON_PROTOCOL_READ_STRING(cluster->nodes->fd, *result, *result_size, rc);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result_size = 0;
                *result = NULL;
        }

        return rc;
}

arakoon_rc arakoon_set(ArakoonCluster *cluster, const size_t key_size,
    const void * const key, const size_t value_size, const void * const value) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;

        FUNCTION_ENTER(arakoon_set);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size)
                + ARAKOON_PROTOCOL_STRING_LEN(value_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x09, 0x00);
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);
        ARAKOON_PROTOCOL_WRITE_STRING(c, value, value_size);

        if(cluster->nodes->fd < 0) {
                rc = arakoon_cluster_node_connect(cluster->nodes);
                if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                        arakoon_mem_free(command);
                }

                RETURN_IF_NOT_SUCCESS(rc);
        }

        WRITE_BYTES(cluster->nodes->fd, command, len, rc);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(cluster->nodes->fd, rc);

        return rc;
}
