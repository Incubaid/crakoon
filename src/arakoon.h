/*
 * This file is part of Arakoon, a distributed key-value store.
 *
 * Copyright (C) 2010, 2012 Incubaid BVBA
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

#ifndef __ARAKOON_H__
#define __ARAKOON_H__

#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/* Conventions
 * ===========
 * Strings
 * -------
 * We use 2 types of string-like values in Crakoon: null-terminated C-strings,
 * using "char *" as type, and plain chunks of memory, encoded as a pointer to
 * this memory ("void *"), and the value size ("size_t"). Whenever we're
 * dealing with human-readable strings, the first encoding is used (e.g. when
 * dealing with cluster or node names). Generic byte sequences, e.g. keys or
 * values, are encoded using the second system.
 *
 * Memory handling
 * ---------------
 * The memory handling procedures to be used by Crakoon can be set through
 * arakoon_memory_set_hooks. The malloc, free and realloc functions can be
 * specified. Crakoon should not use any other heap-allocation functions.
 *
 * The system malloc(3), free(3) and realloc(3) procedures are used as default.
 * If possible, it is advisable to register your own heap allocation handler
 * functions before calling any other Crakoon procedures.
 *
 * Whenever an allocation fails (malloc or realloc returned NULL), this is
 * returned as-is to the caller, by returning NULL in case the procedure returns
 * a pointer, or -ENOMEM if an arakoon_rc value is returned. When allocation
 * fails, any out-pointers should be set to NULL as well, although one should
 * not rely on this behaviour (always check the arakoon_rc value).
 *
 * If you're not interested in handling out-of-memory situations and want to
 * abort on allocation failure, you can register simple wrappers around malloc
 * and realloc which e.g. call abort(3) on failure.
 *
 * Error handling
 * --------------
 * Most procedures return an arakoon_rc value, which is an integer value. If
 * this value is negative, it's an errno value. If it is equal to 0, it
 * signifies success. A positive value is part of the ArakoonReturnCode
 * enumeration.
 *
 * You can (and should!) use the ARAKOON_RC_IS_ERRNO, ARAKOON_RC_AS_ERRNO,
 * ARAKOON_RC_IS_SUCCESS, ARAKOON_RC_IS_ARAKOONRETURNCODE and
 * ARAKOON_RC_AS_ARAKOONRETURNCODE macros to handle arakoon_rc values.
 *
 * The arakoon_strerror procedure can be used to turn an arakoon_rc value into a
 * human-readable message. For errno values, the strerror(3) function is called,
 * so the caveats related to using this function apply to arakoon_strerror as
 * well.
 */

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
# define ARAKOON_GNUC_NONNULL1(x) __attribute__((__nonnull__(x)))
# define ARAKOON_GNUC_NONNULL2(x, y) __attribute__((__nonnull__(x, y)))
# define ARAKOON_GNUC_NONNULL3(x, y, z) \
   __attribute__((__nonnull__(x, y, z)))
# define ARAKOON_GNUC_NONNULL4(x, y, z, a) \
   __attribute__((__nonnull__(x, y, z, a)))
#else
# define ARAKOON_GNUC_NONNULL
# define ARAKOON_GNUC_NONNULL1(x)
# define ARAKOON_GNUC_NONNULL2(x, y)
# define ARAKOON_GNUC_NONNULL3(x, y, z)
# define ARAKOON_GNUC_NONNULL4(x, y, z, a)
#endif

#if __GNUC__ > 2 && defined(__OPTIMIZE__)
# define ARAKOON_GNUC_LIKELY(expr) (__builtin_expect(!!(expr), 1))
# define ARAKOON_GNUC_UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#else
# define ARAKOON_GNUC_LIKELY(expr) (expr)
# define ARAKOON_GNUC_UNLIKELY(expr) (expr)
#endif

ARAKOON_BEGIN_DECLS

/* Return code values contained in arakoon_rc variables, if non-negative */
typedef enum {
    /* These are returned from the server */
    ARAKOON_RC_SUCCESS = 0, /* Success */
    ARAKOON_RC_NO_MAGIC = 1, /* No magic applied to given command */
    ARAKOON_RC_TOO_MANY_DEAD_NODES = 2, /* Too many dead nodes */
    ARAKOON_RC_NO_HELLO = 3, /* No hello received from client */
    ARAKOON_RC_NOT_MASTER = 4, /* Node is not the master */
    ARAKOON_RC_NOT_FOUND = 5, /* Not found */
    ARAKOON_RC_WRONG_CLUSTER = 6, /* An invalid cluster name was specified */
    ARAKOON_RC_ASSERTION_FAILED = 7, /* An assertion failed */
    ARAKOON_RC_READ_ONLY = 8, /* Node is in read-only mode */
    ARAKOON_RC_NURSERY_RANGE_ERROR = 9, /* Nursery range error */
    ARAKOON_RC_UNKNOWN_FAILURE = 0xff, /* An unknown failure occurred */

    /* Internal client errors */
    ARAKOON_RC_CLIENT_NETWORK_ERROR = 0x0100, /* A client-side network error occurred */
    ARAKOON_RC_CLIENT_UNKNOWN_NODE = 0x0200, /* An unknown node name was received */
    ARAKOON_RC_CLIENT_MASTER_NOT_FOUND = 0x0300, /* The master node could not be determined */
    ARAKOON_RC_CLIENT_NOT_CONNECTED = 0x0400, /* The client is not connected to a master node */
    ARAKOON_RC_CLIENT_TIMEOUT = 0x0500, /* A timeout was reached */
    ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING = 0x0600, /* Unable to parse routing information */
    ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG = 0x0700 /* Invalid client config (needs update?) */

} ArakoonReturnCode;

typedef int arakoon_rc;

/* Check whether a given arakoon_rc value is an errno value */
#define ARAKOON_RC_IS_ERRNO(n) (n < 0)
/* Convert and cast an arakoon_rc value to the corresponding (positive!) errno value */
#define ARAKOON_RC_AS_ERRNO(n) ((typeof(errno))(-n))
/*Check whether a given arakoon_rc value denotes success */
#define ARAKOON_RC_IS_SUCCESS(n) (ARAKOON_GNUC_LIKELY(n == 0))
/* Check whether a given arakoon_rc value is an ArakoonReturnCode */
#define ARAKOON_RC_IS_ARAKOONRETURNCODE(n) (n >= 0)
/* Convert and cast an arakoon_rc value into an ArakoonReturnCode */
#define ARAKOON_RC_AS_ARAKOONRETURNCODE(n) ((ArakoonReturnCode) n)

/* Turn an arakoon_rc value into a human-readable string representation
 *
 * This uses strerror(3) internally when arakoon_rc is an errno value. The
 * corresponding caveats apply.
 */
const char * arakoon_strerror(arakoon_rc n);

/* Utility stuff */
typedef char arakoon_bool;
#define ARAKOON_BOOL_TRUE (1)
#define ARAKOON_BOOL_FALSE (0)

/* Turn a generic byte sequence into a C-string
 *
 * This procedure turns a given data blob into a null-terminated C-string. The
 * allocated string is returned. A call to realloc is used to achieve this, so
 * the original data pointer will be invalid after calling this procedure.
 *
 * The caller is in charge of releasing the returned memory.
 */
char * arakoon_utils_make_string(void *data, size_t length)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;

/* Memory handling */
/* A vtable containing function pointers to be used for heap management */
typedef struct {
    void * (*malloc) (size_t size); /* malloc(3) */
    void (*free) (void *ptr); /* free(3) */
    void * (*realloc) (void *ptr, size_t size); /* realloc(3) */
} ArakoonMemoryHooks;

/* Register memory-management functions to be used by Crakoon */
arakoon_rc arakoon_memory_set_hooks(const ArakoonMemoryHooks * const hooks)
    ARAKOON_GNUC_NONNULL;
/* Retrieve an ArakoonMemoryHooks table containing wrappers around the system
 * malloc(3) and realloc(3) which abort() on allocation failure. Pass this to
 * arakoon_memory_set_hooks in case you don't bother handling heap management
 * failures (i.e. don't want to care about NULL checks or ENOMEM) and just die.
 */
const ArakoonMemoryHooks * arakoon_memory_get_abort_hooks(void);

/* Logging */
/* Enumeration of log levels */
typedef enum {
    ARAKOON_LOG_TRACE,
    ARAKOON_LOG_DEBUG,
    ARAKOON_LOG_INFO,
    ARAKOON_LOG_WARNING,
    ARAKOON_LOG_ERROR,
    ARAKOON_LOG_FATAL
} ArakoonLogLevel;

/* Log handler callback prototype */
typedef void (*ArakoonLogHandler) (ArakoonLogLevel level,
    const char * message);

/* Set a log message handler procedure */
void arakoon_log_set_handler(const ArakoonLogHandler handler);
/* A handler which logs all messages to stderr */
ArakoonLogHandler arakoon_log_get_stderr_handler(void);

/* Value list
 *
 * The list can be iterated and should be free'd when done (or whenever
 * seems fit).
 */
typedef struct ArakoonValueList ArakoonValueList;
/* Create a new ArakoonValueList
 *
 * The list should be released using arakoon_value_list_free when no longer
 * needed.
 */
ArakoonValueList * arakoon_value_list_new(void)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_MALLOC;
/* Add a value to the list (at the tail) 
 *
 * The given value is copied and will be freed when arakoon_value_list_free is
 * used.
 */
arakoon_rc arakoon_value_list_add(ArakoonValueList *list,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_NONNULL2(1, 3);
/* Retrieve the number of items in the list */
ssize_t arakoon_value_list_size(const ArakoonValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;
/* Free a value list
 *
 * Note: any iters over the list will become invalid after this call.
 */
void arakoon_value_list_free(ArakoonValueList * const list);

typedef struct ArakoonValueListIter ArakoonValueListIter;
/* Create an iter for the given value list */
ArakoonValueListIter * arakoon_value_list_create_iter(
    const ArakoonValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT
    ARAKOON_GNUC_MALLOC;
/* Release an iter */
void arakoon_value_list_iter_free(ArakoonValueListIter * const iter);
/* Retrieve the next item
 *
 * value will point to NULL when the last item was reached.
 */
arakoon_rc arakoon_value_list_iter_next(ArakoonValueListIter * const iter,
    size_t * const value_size, const void ** const value)
    ARAKOON_GNUC_NONNULL;
/* Reset the list cursor to the first entry */
arakoon_rc arakoon_value_list_iter_reset(ArakoonValueListIter * const iter)
    ARAKOON_GNUC_NONNULL;

/* Helper to map over an ArakoonValueList
 *
 * Example usage:
 *
 * const void * v = NULL;
 * size_t l = 0;
 * ArakoonValueIter *iter = NULL;
 *
 * iter = get_iter();
 * FOR_ARAKOON_VALUE_ITER(iter, &l, &v) {
 *     do_something(l, v);
 * }
 */
#define FOR_ARAKOON_VALUE_ITER(i, l, v)        \
    for(arakoon_value_list_iter_next(i, l, v); \
        *v != NULL;                             \
        arakoon_value_list_iter_next(i, l, v))

/* Key Value list
 * Same as ValueList, but with pairs of values, sort-of
 */
typedef struct ArakoonKeyValueList ArakoonKeyValueList;
ssize_t arakoon_key_value_list_size(const ArakoonKeyValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;
void arakoon_key_value_list_free(ArakoonKeyValueList * const list);

typedef struct ArakoonKeyValueListIter ArakoonKeyValueListIter;
ArakoonKeyValueListIter * arakoon_key_value_list_create_iter(
    const ArakoonKeyValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT
    ARAKOON_GNUC_MALLOC;
void arakoon_key_value_list_iter_free(ArakoonKeyValueListIter * const iter);
arakoon_rc arakoon_key_value_list_iter_next(ArakoonKeyValueListIter * const iter,
    size_t * const key_size, const void ** const key,
    size_t * const value_size, const void ** const value)
    ARAKOON_GNUC_NONNULL;
arakoon_rc arakoon_key_value_list_iter_reset(ArakoonKeyValueListIter * const iter)
    ARAKOON_GNUC_NONNULL;

#define FOR_ARAKOON_KEY_VALUE_ITER(i, kl, k, vl, v)        \
    for(arakoon_key_value_list_iter_next(i, kl, k, vl, v); \
        (*k != NULL && *v != NULL);                          \
        arakoon_key_value_list_iter_next(i, kl, k, vl, v))

/* Sequence support */
typedef struct ArakoonSequence ArakoonSequence;

/* Allocate a new sequence
 *
 * The return value should be released using arakoon_sequence_free once no
 * longer needed.
 */
ArakoonSequence * arakoon_sequence_new(void)
    ARAKOON_GNUC_MALLOC;
/* Release an ArakoonSequence allocated using arakoon_sequence_new */
void arakoon_sequence_free(ArakoonSequence *sequence);
/* Add a 'set' action to the sequence
 *
 * Key and value will be copied and released on arakoon_sequence_free.
 */
arakoon_rc arakoon_sequence_add_set(ArakoonSequence *sequence,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_NONNULL3(1, 3, 5) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Add a 'delete' action to the sequence
 *
 * Key will be copied and released on arakoon_sequence_free.
 */
arakoon_rc arakoon_sequence_add_delete(ArakoonSequence *sequence,
    const size_t key_size, const void * const key)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Add an 'assert' action to the sequence
 *
 * Key and value will be copied and released on arakoon_sequence_free.
 */
arakoon_rc arakoon_sequence_add_assert(ArakoonSequence *sequence,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;


/* Client call options
 *
 * This struct contains some settings (to be set using accessor procedures)
 * which could be of use when performing a client call to a cluster.
 *
 * Not all options are applicable to all calls. Whenever NULL is passed to a
 * client operation, default values (as set in any returned value of
 * arakoon_client_call_options_new) will be used.
 */
typedef struct ArakoonClientCallOptions ArakoonClientCallOptions;

#define ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_ALLOW_DIRTY (ARAKOON_BOOL_FALSE)
#define ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT (-1)
#define ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT \
        ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT

/* Allocate a new ArakoonClientCallOptions structure
 *
 * The returned value should be released using arakoon_client_call_options_free
 * when no longer required.
 *
 * The returned structure will be pre-initialized using settings according to
 * the ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_* definitions.
 */
ArakoonClientCallOptions * arakoon_client_call_options_new(void)
    ARAKOON_GNUC_MALLOC ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Release an ArakoonClientCallOptions structure allocated using
 * arakoon_client_call_options_new
 */
void arakoon_client_call_options_free(ArakoonClientCallOptions *options);

/* Get the current 'allow_dirty' setting from an ArakoonClientCallOptions
 * structure
 */
arakoon_bool arakoon_client_call_options_get_allow_dirty(
    const ArakoonClientCallOptions * const options)
    ARAKOON_GNUC_NONNULL;
/* Set the 'allow_dirty' flag in an ArakoonClientCallOptions structure */
arakoon_rc arakoon_client_call_options_set_allow_dirty(
    ArakoonClientCallOptions * const options, arakoon_bool allow_dirty)
    ARAKOON_GNUC_NONNULL;

/* Get the current 'timeout' setting from an ArakoonClientCallOptions structure
 *
 * The timeout is an integer value of milliseconds. When equal to
 * ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT, no timeout will be used.
 */
int arakoon_client_call_options_get_timeout(
    const ArakoonClientCallOptions * const options)
    ARAKOON_GNUC_NONNULL;
/* Set the 'timeout' setting in an ArakoonClientCallOptions structure
 *
 * Use ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT to set an infinite (or, no)
 * timeout.
 */
arakoon_rc arakoon_client_call_options_set_timeout(
    ArakoonClientCallOptions * const options, int timeout)
    ARAKOON_GNUC_NONNULL1(1);

/* ArakoonCluster */
typedef struct ArakoonCluster ArakoonCluster;

/* Allocate a new ArakoonCluster, to be released using arakoon_cluster_free
 *
 * The given name will be copied and released in arakoon_cluster_free
 */
ArakoonCluster * arakoon_cluster_new(const char * const name)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_MALLOC
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Release an ArakoonCluster
 *
 * This will also close any open connections.
 */
void arakoon_cluster_free(ArakoonCluster *cluster);
/* Lookup the master node
 *
 * This should be called before performing any other operations, and whenever
 * ARAKOON_RC_NOT_MASTER is encountered.
 */
arakoon_rc arakoon_cluster_connect_master(ArakoonCluster * const cluster,
    const ArakoonClientCallOptions * const options);
/* Retrieve the name of the cluster */
const char * arakoon_cluster_get_name(const ArakoonCluster * const cluster)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;
/* Retrieve the last error message received through the cluster
 *
 * Note you'll get a reference to a string which will be free'd upon the next
 * call using the cluster, or whenever the cluster is free'd. As such you
 * should create your own copy of the string for further usage, if required.
 */
const char * arakoon_cluster_get_last_error(
    const ArakoonCluster * const cluster)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;

/* Add a node to the cluster
 *
 * The name will be copied and released on arakoon_cluster_free.
 * The given address struct should *not* be freed by the caller. It should
 * remain valid as long as the ArakoonCluster exists. It *will* be released
 * using freeaddrinfo(3) when arakoon_cluster_free is used. As such, a caller
 * should not re-use the pointer after calling arakoon_cluster_add_node, nor
 * pass it to any other functions (in general).
 */
arakoon_rc arakoon_cluster_add_node(ArakoonCluster *cluster,
    const char * const name, struct addrinfo * const address)
    ARAKOON_GNUC_NONNULL3(1, 2, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Helper around arakoon_cluster_add_node
 *
 * This function is a helper arond arakoon_cluster_add_node to easily add
 * TCP-based connections to server nodes. The function will not reference any
 * of the given pointers after returning.
 *
 * It uses getaddrinfo(3) to look up socket connection information. Refer to the
 * corresponding manual page to know the value of 'host' and 'service'.
 */
arakoon_rc arakoon_cluster_add_node_tcp(ArakoonCluster *cluster,
    const char * const name, const char * const host,
    const char * const service)
    ARAKOON_GNUC_NONNULL4(1, 2, 3, 4)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;


/* Client calls */
/* Send a 'hello' call to the server, using 'client_id' and 'cluster_id'
 *
 * The message returned by the server is stored at 'result', which should be
 * released by the caller when no longer required.
 */
arakoon_rc arakoon_hello(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const char * const client_id, const char * const cluster_id,
    char ** const result)
    ARAKOON_GNUC_NONNULL4(1, 3, 4, 5) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'who_master' call to the server
 *
 * The name of the master node as returned by the server will be stored at
 * 'master', which should be released by the caller when no longer required.
 * Do note the result as stored in 'master' can be 'NULL' even though the
 * return code is 'ARAKOON_RC_SUCCESS'. This denotes a 'None' return by the
 * server, which implies the master is unknown at the time of the request.
 */
arakoon_rc arakoon_who_master(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options, char ** master)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send an 'expect_progress_possible' call to the server
 *
 * The result value will be stored at 'result' on success.
 */
arakoon_rc arakoon_expect_progress_possible(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options, arakoon_bool *result)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send an 'exists' call to the server
 *
 * The result value will be stored at 'result' on success.
 */
arakoon_rc arakoon_exists(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key, arakoon_bool *result)
    ARAKOON_GNUC_NONNULL3(1, 4, 5) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'get' call to the server
 *
 * The resulting value will be stored at 'result', and its length at
 * 'result_size'. 'result' should be released by the caller when no longer
 * required.
 */
arakoon_rc arakoon_get(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    size_t *result_size, void **result)
    ARAKOON_GNUC_NONNULL4(1, 4, 5, 6) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'multi_get' call to the server
 *
 * The result value list will be stored at 'result'. This ArakoonValueList
 * should be released by the client using arakoon_value_list_free when no longer
 * required.
 */
arakoon_rc arakoon_multi_get(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const ArakoonValueList * const keys, ArakoonValueList **result)
    ARAKOON_GNUC_NONNULL3(1, 3, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'set' call to the server */
arakoon_rc arakoon_set(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_NONNULL3(1, 4, 6) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'delete' call to the server */
arakoon_rc arakoon_delete(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key)
    ARAKOON_GNUC_NONNULL2(1, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'range' call to the server
 *
 * 'begin_key' and 'end_key' can be set to NULL to denote 'None'.
 * 'begin_key_size' and 'end_key_size' should be set to 0 accordingly.
 *
 * If 'max_elements' is negative, all matches will be returned.
 *
 * The result will be stored at 'result', and should be released using
 * arakoon_value_list_free by the caller when no longer required.
 */
arakoon_rc arakoon_range(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t begin_key_size, const void * const begin_key,
    const arakoon_bool begin_key_included,
    const size_t end_key_size, const void * const end_key,
    const arakoon_bool end_key_included,
    const ssize_t max_elements,
    ArakoonValueList **result) ARAKOON_GNUC_NONNULL2(1, 10)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'range_entries' call to the server
 *
 * 'begin_key' and 'end_key' can be set to NULL to denote 'None'.
 * 'begin_key_size' and 'end_key_size' should be set to 0 accordingly.
 *
 * If 'max_elements' is negative, all matches will be returned.
 *
 * The result will be stored at 'result', and should be released using
 * arakoon_key_value_list_free by the caller when no longer required.
 */
arakoon_rc arakoon_range_entries(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t begin_key_size, const void * const begin_key,
    const arakoon_bool begin_key_included,
    const size_t end_key_size, const void * const end_key,
    const arakoon_bool end_key_included,
    const ssize_t max_elements,
    ArakoonKeyValueList **result) ARAKOON_GNUC_NONNULL2(1, 10)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'prefix' call to the server
 *
 * If 'max_elements' is negative, all matches will be returned.
 *
 * The result will be stored at 'result', and should be released using
 * arakoon_value_list_free by the caller when no longer required.
 */
arakoon_rc arakoon_prefix(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t begin_key_size, const void * const begin_key,
    const ssize_t max_elements,
    ArakoonValueList **result) ARAKOON_GNUC_NONNULL3(1, 4, 6)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'test_and_set' call to the server
 *
 * 'old_value' and 'new_value' can be NULL to denote 'None', in which case the
 * according size value should be 0 as well.
 *
 * The result will be stored at 'result', and should be released by the caller
 * when no longer required. The result can be NULL, in which case the server
 * returned 'None'.
 */
arakoon_rc arakoon_test_and_set(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    const size_t old_value_size, const void * const old_value,
    const size_t new_value_size, const void * const new_value,
    size_t *result_size, void **result)
    ARAKOON_GNUC_NONNULL4(1, 4, 9, 10) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'sequence' call to the server */
arakoon_rc arakoon_sequence(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const ArakoonSequence * const sequence)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;

ARAKOON_END_DECLS

#endif /* ifndef __ARAKOON_H__ */
