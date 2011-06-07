#ifndef __ARAKOON_H__
#define __ARAKOON_H__

#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef  __cplusplus
# define ARAKOON_BEGIN_DECLS \
    extern "C" {
# define ARAKOON_END_DECLS \
    }
#else
# define ARAKOON_BEGIN_DECLS
# define ARAKOON_END_DECLS
#endif

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
# define ARAKOON_GNUC_WARN_UNUSED_RESULT \
   __attribute__((warn_unused_result))
#else
# define ARAKOON_GNUC_WARN_UNUSED_RESULT
#endif

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
# define ARAKOON_GNUC_UNUSED __attribute__ ((__unused__))
#else
# define ARAKOON_GNUC_UNUSED
#endif

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
# define ARAKOON_GNUC_MALLOC __attribute__((__malloc__))
# define ARAKOON_GNUC_PURE __attribute__((__pure__))
#else
# define ARAKOON_GNUC_MALLOC
# define ARAKOON_GNUC_PURE
#endif

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
# define ARAKOON_GNUC_NONNULL __attribute__((__nonnull__))
# define ARAKOON_GNUC_NONNULL2(x, y) __attribute__((__nonnull__(x, y)))
# define ARAKOON_GNUC_NONNULL3(x, y, z) \
   __attribute__((__nonnull__(x, y, z)))
# define ARAKOON_GNUC_NONNULL4(x, y, z, a) \
   __attribute__((__nonnull__(x, y, z, a)))
#else
# define ARAKOON_GNUC_NONNULL
# define ARAKOON_GNUC_NONNULL2(x, y)
# define ARAKOON_GNUC_NONNULL3(x, y, z)
# define ARAKOON_GNUC_NONNULL4(x, y, z, a)
#endif

ARAKOON_BEGIN_DECLS

/* Conventions
 * ===========
 * Strings
 * -------
 * Everytime a char pointer is used, this is expected to be a standard,
 * null-terminated C-string. Whenever a non-null-terminated byte sequence is
 * used, a combination of a size_t value and a void pointer is used.
 *
 * Error handling
 * --------------
 * Several calls return an arakoon_rc value, which is typedef'ed to an int.
 * If the value equals 0, the call succeeded. If the value is negative, its
 * value is taken from errno, and as such denotes an E* value. If it's
 * positive, it's an Arakoon-specific error. See the ArakoonReturnCode.
 * enumeration for all possible values.
 *
 * You can use arakoon_strerror to turn a non-zero arakoon_rc value into
 * a more human-friendly string. It returns strerror(-rc) if rc is a
 * negative value.
 */

/**
 * \brief Crakoon return codes
 *
 * The Crakoon API contains functions which return TODO
 */
typedef enum {
    /* These are returned from the server */
    ARAKOON_RC_SUCCESS = 0, /* Success */
    ARAKOON_RC_NO_MAGIC = 1,
    ARAKOON_RC_TOO_MANY_DEAD_NODES = 2,
    ARAKOON_RC_NO_HELLO = 3,
    ARAKOON_RC_NOT_MASTER = 4, /* Node is not the master */
    ARAKOON_RC_NOT_FOUND = 5, /* Not found */
    ARAKOON_RC_WRONG_CLUSTER = 6,
    ARAKOON_RC_UNKNOWN_FAILURE = 0xff,

    /* Internal client errors */
    ARAKOON_RC_CLIENT_NETWORK_ERROR = 0x0100
} ArakoonReturnCode;

typedef int arakoon_rc;

#define ARAKOON_RC_IS_ERRNO(n) (n < 0)
#define ARAKOON_RC_AS_ERRNO(n) ((typeof(errno))(-n))
#define ARAKOON_RC_IS_SUCCESS(n) (n == 0)
#define ARAKOON_RC_IS_ARAKOONRETURNCODE (n >= 0)
#define ARAKOON_RC_AS_ARAKOONRETURNCODE ((ArakoonReturnCode) n)

const char * arakoon_strerror(arakoon_rc n);

/* Utility stuff */
typedef char arakoon_bool;
#define ARAKOON_BOOL_TRUE (1)
#define ARAKOON_BOOL_FALSE (0)

char * arakoon_utils_make_string(void *data, size_t length)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;

/* Memory management */
typedef struct {
    void * (*malloc) (size_t size);
    void (*free) (void *ptr);
    void * (*realloc) (void *ptr, size_t size);
} ArakoonMemoryHooks;

/** \brief Register memory-management functions to be used by Crakoon
 */
void arakoon_memory_set_hooks(const ArakoonMemoryHooks * const hooks)
    ARAKOON_GNUC_NONNULL;

/* Logging */
typedef enum {
    ARAKOON_LOG_TRACE,
    ARAKOON_LOG_DEBUG,
    ARAKOON_LOG_WARNING,
    ARAKOON_LOG_ERROR,
    ARAKOON_LOG_FATAL
} ArakoonLogLevel;

typedef void (*ArakoonLogHandler) (ArakoonLogLevel level,
    const char * message);

/** \brief Set a log message handler procedure
 */
void arakoon_log_set_handler(const ArakoonLogHandler handler);

/* Value list
 * The list can be iterated and should be free'd when done (or whenever
 * seems fit). Concurrent iteration is not possible.
 */
typedef struct _ArakoonValueList ArakoonValueList;
ArakoonValueList * arakoon_value_list_new(void)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_MALLOC;
/* TODO Do we want/need append? */
arakoon_rc arakoon_value_list_prepend(ArakoonValueList *list,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_NONNULL2(1, 3);
/* Retrieve the number of items in the list */
size_t arakoon_value_list_size(const ArakoonValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;
/* Free a value list
 * Note any iters over the list will become invalid after this call
 */
void arakoon_value_list_free(ArakoonValueList * const list);

typedef struct _ArakoonValueListIter ArakoonValueListIter;
/* Create an iter for the given value list */
ArakoonValueListIter * arakoon_value_list_create_iter(
    const ArakoonValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT
    ARAKOON_GNUC_MALLOC;
void arakoon_value_list_iter_free(ArakoonValueListIter * const iter);
/* Retrieve the next item. Returns NULL if the last item was reached
 * before. */
void arakoon_value_list_iter_next(ArakoonValueListIter * const iter,
    size_t * const value_size, const void ** const value)
    ARAKOON_GNUC_NONNULL;
/* Reset the list cursor to the first entry */
void arakoon_value_list_iter_reset(ArakoonValueListIter * const iter)
    ARAKOON_GNUC_NONNULL;

#define FOR_ARAKOON_VALUE_ITER(i, l, v)        \
    for(arakoon_value_list_iter_next(i, l, v); \
        *v != NULL;                             \
        arakoon_value_list_iter_next(i, l, v))

/* Key Value list
 * Same as ValueList, but with pairs of values, sort-of
 */
typedef struct _ArakoonKeyValueList ArakoonKeyValueList;
size_t arakoon_key_value_list_size(const ArakoonKeyValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;
void arakoon_key_value_list_free(ArakoonKeyValueList * const list);

typedef struct _ArakoonKeyValueListIter ArakoonKeyValueListIter;
ArakoonKeyValueListIter * arakoon_key_value_list_create_iter(
    const ArakoonKeyValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT
    ARAKOON_GNUC_MALLOC;
void arakoon_key_value_list_iter_free(ArakoonKeyValueListIter * const iter);
void arakoon_key_value_list_iter_next(ArakoonKeyValueListIter * const iter,
    size_t * const key_size, const void ** const key,
    size_t * const value_size, const void ** const value)
    ARAKOON_GNUC_NONNULL;
void arakoon_key_value_list_iter_reset(ArakoonKeyValueListIter * const iter)
    ARAKOON_GNUC_NONNULL;

#define FOR_ARAKOON_KEY_VALUE_ITER(i, kl, k, vl, v)        \
    for(arakoon_key_value_list_iter_next(i, kl, k, vl, v); \
        (*k != NULL && *v != NULL);                          \
        arakoon_key_value_list_iter_next(i, kl, k, vl, v))

/* Sequence support */
typedef struct _ArakoonSequence ArakoonSequence;

ArakoonSequence * arakoon_sequence_new(void)
    ARAKOON_GNUC_MALLOC;
void arakoon_sequence_free(ArakoonSequence *sequence);
arakoon_rc arakoon_sequence_add_set(ArakoonSequence *sequence,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_NONNULL3(1, 3, 5) ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_sequence_add_delete(ArakoonSequence *sequence,
    const size_t key_size, const void * const key)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_sequence_add_test_and_set(ArakoonSequence *sequence,
    const size_t key_size, const void * const key,
    const size_t old_value_size, const void * const old_value,
    const size_t new_value_size, const void * const new_value)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;


/* ArakoonCluster */
typedef struct _ArakoonCluster ArakoonCluster;

ArakoonCluster * arakoon_cluster_new(const char * const name)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_MALLOC
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
void arakoon_cluster_free(ArakoonCluster *cluster);
const char * arakoon_cluster_get_name(const ArakoonCluster * const cluster)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;

arakoon_rc arakoon_cluster_add_node(ArakoonCluster *cluster,
    const char * const name, struct addrinfo * const address)
    ARAKOON_GNUC_NONNULL3(1, 2, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Helper around arakoon_cluster_add_node */
arakoon_rc arakoon_cluster_add_node_tcp(ArakoonCluster *cluster,
    const char * const name, const char * const host,
    const char * const service)
    ARAKOON_GNUC_NONNULL4(1, 2, 3, 4)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;

/* Client calls */
arakoon_rc arakoon_hello(ArakoonCluster *cluster,
    const char * const client_id, const char * const cluster_id,
    char ** const result)
    ARAKOON_GNUC_NONNULL4(1, 2, 3, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_who_master(ArakoonCluster *cluster,
    char ** master)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_expect_progress_possible(ArakoonCluster *cluster,
    arakoon_bool *result)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_exists(ArakoonCluster *cluster, const size_t key_size,
    const void * const key, arakoon_bool *result)
    ARAKOON_GNUC_NONNULL3(1, 3, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_get(ArakoonCluster *cluster, const size_t key_size,
    const void * const key, size_t *result_size, void **result)
    ARAKOON_GNUC_NONNULL4(1, 3, 4, 5) ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_multi_get(ArakoonCluster *cluster,
    const ArakoonValueList * const keys, ArakoonValueList **result)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_set(ArakoonCluster *cluster, const size_t key_size,
    const void * const key, const size_t value_size,
    const void * const value) ARAKOON_GNUC_NONNULL3(1, 3, 5)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_delete(ArakoonCluster *cluster, const size_t key_size,
    const void * const key) ARAKOON_GNUC_NONNULL2(1, 3)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_range(ArakoonCluster *cluster,
    const size_t begin_key_size, const void * const begin_key,
    const arakoon_bool begin_key_included,
    const size_t end_key_size, const void * const end_key,
    const arakoon_bool end_key_included,
    const ssize_t max_elements, /* TODO Use int ? */
    ArakoonValueList **result) ARAKOON_GNUC_NONNULL2(1, 9)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_range_entries(ArakoonCluster *cluster,
    const size_t begin_key_size, const void * const begin_key,
    const arakoon_bool begin_key_included,
    const size_t end_key_size, const void * const end_key,
    const arakoon_bool end_key_included,
    const ssize_t max_elements, /* TODO Use int ? */
    ArakoonKeyValueList **result) ARAKOON_GNUC_NONNULL2(1, 9)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_prefix(ArakoonCluster *cluster,
    const size_t begin_key_size, const void * const begin_key,
    const ssize_t max_elements,
    ArakoonValueList **result) ARAKOON_GNUC_NONNULL3(1, 3, 5)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Note: old_value can be NULL, new_value can be NULL */
arakoon_rc arakoon_test_and_set(ArakoonCluster *cluster,
    const size_t key_size, const void * const key,
    const size_t old_value_size, const void * const old_value,
    const size_t new_value_size, const void * const new_value,
    size_t *result_size, void **result)
    ARAKOON_GNUC_NONNULL4(1, 3, 8, 9) ARAKOON_GNUC_WARN_UNUSED_RESULT;
arakoon_rc arakoon_sequence(ArakoonCluster *cluster,
    const ArakoonSequence * const sequence)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;

ARAKOON_END_DECLS

#endif /* ifndef __ARAKOON_H__ */
