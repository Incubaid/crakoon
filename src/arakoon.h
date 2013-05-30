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

/**
 * \mainpage
 *
 * \ref arakoon_c_api_group
 *
 * \ref arakoon_cpp_api_group
 */

/** \defgroup arakoon_c_api_group Arakoon C API
 * @{
 */
/**
 * Conventions
 *
 * \par Strings
 * We use 2 types of string-like values in Crakoon: null-terminated C-strings,
 * using `char *` as type, and plain chunks of memory, encoded as a pointer to
 * this memory (`void *`), and the value size (`size_t`). Whenever we're
 * dealing with human-readable strings, the first encoding is used (e.g. when
 * dealing with cluster or node names). Generic byte sequences, e.g. keys or
 * values, are encoded using the second system.
 *
 * Since some Arakoon calls take or return 'option' values, the use of `NULL`
 * can be overloaded. As such we decided on the following scheme:
 *
 * - The length of a `Nothing` value can be any value (including 0), and the
 *   data pointer is `NULL`.
 * - The length of a zero-length non-`Nothing` value is 0, and the data pointer
 *   is any non-`NULL` pointer (e.g. `ARAKOON_ZERO_LENGTH_DATA_PTR`).
 * - The length of a non-zero-length non-`Nothing` value is a non-0 value, and
 *   the data pointer is non-`NULL`.
 *
 * This implies it's not allowed to pass a `NULL`-pointer to `arakoon_set`,
 * since this doesn't take an 'option' value. As such, in case your code might
 * get into a path where a zero-length value could be stored, and the value
 * pointer might be `NULL` (due to other parts of the application), something
 * like this snippet could be used:
 *
 * \code{.c}
 * arakoon_rc set_mykey_value(ArakoonCluster *cluster,
 *     size_t len, const void * value) {
 *     return arakoon_set(cluster, NULL,
 *         "mykey", 5,
 *         len, ARAKOON_MAYBE_ZERO_LENGTH_DATA(len, value));
 * }
 * \endcode
 *
 * There's no reason to use this pattern in a code-path where a value is never
 * supposed to be of zero-length!
 *
 * Note this means some calls can return a non-`NULL` pointer which should never
 * be `free`'d!
 *
 * \par Memory handling
 * The memory handling procedures to be used by Crakoon can be set through
 * arakoon_memory_set_hooks. The `malloc`, `free` and `realloc` functions can be
 * specified. Crakoon should not use any other heap-allocation functions.
 *
 * The system *malloc(3)*, *free(3)* and *realloc(3)* procedures are used as default.
 * If possible, it is advisable to register your own heap allocation handler
 * functions before calling any other Crakoon procedures.
 *
 * Whenever an allocation fails (`malloc` or `realloc` returned `NULL`), this is
 * returned as-is to the caller, by returning `NULL` in case the procedure returns
 * a pointer, or `-ENOMEM` if an #arakoon_rc value is returned. When allocation
 * fails, any out-pointers should be set to NULL as well, although one should
 * not rely on this behaviour (always check the #arakoon_rc value).
 *
 * If you're not interested in handling out-of-memory situations and want to
 * abort on allocation failure, you can register simple wrappers around `malloc`
 * and `realloc` which e.g. call *abort(3)* on failure.
 *
 * \par Error handling
 * Most procedures return an #arakoon_rc value, which is an integer value. If
 * this value is negative, it's an *errno* value. If it is equal to 0, it
 * signifies success. A positive value is part of the #ArakoonReturnCode
 * enumeration.
 *
 * You can (and should!) use the #ARAKOON_RC_IS_ERRNO, #ARAKOON_RC_AS_ERRNO,
 * ARAKOON_RC_IS_SUCCESS, #ARAKOON_RC_IS_ARAKOONRETURNCODE and
 * ARAKOON_RC_AS_ARAKOONRETURNCODE macros to handle #arakoon_rc values.
 *
 * The #arakoon_strerror procedure can be used to turn an #arakoon_rc value into a
 * human-readable message. For *errno* values, the *strerror(3)* function is called,
 * so the caveats related to using this function apply to #arakoon_strerror as
 * well.
 */

/** \cond */
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
# define ARAKOON_GNUC_CONST __attribute__((__const__))
#else
# define ARAKOON_GNUC_UNUSED
# define ARAKOON_GNUC_CONST
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
# define ARAKOON_GNUC_NONNULL5(x, y, z, a, b) \
   __attribute__((__nonnull__(x, y, z, a, b)))
#else
# define ARAKOON_GNUC_NONNULL
# define ARAKOON_GNUC_NONNULL1(x)
# define ARAKOON_GNUC_NONNULL2(x, y)
# define ARAKOON_GNUC_NONNULL3(x, y, z)
# define ARAKOON_GNUC_NONNULL4(x, y, z, a)
# define ARAKOON_GNUC_NONNULL5(x, y, z, a, b)
#endif

#if __GNUC__ > 2 && defined(__OPTIMIZE__)
# define ARAKOON_GNUC_LIKELY(expr) (__builtin_expect(!!(expr), 1))
# define ARAKOON_GNUC_UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#else
# define ARAKOON_GNUC_LIKELY(expr) (expr)
# define ARAKOON_GNUC_UNLIKELY(expr) (expr)
#endif

/** \endcond */

#ifndef ARAKOON_H_EXPORT_TYPES
# define ARAKOON_H_EXPORT_TYPES 1
#endif
#ifndef ARAKOON_H_EXPORT_PROCEDURES
# define ARAKOON_H_EXPORT_PROCEDURES 1
#endif

ARAKOON_BEGIN_DECLS

#if ARAKOON_H_EXPORT_TYPES

/** \defgroup ResultCodes Result codes
 * @{
 */
/**
 * \brief Return code values contained in #arakoon_rc variables, if non-negative
 *
 * All *ARAKOON_RC_CLIENT_\** values are client-side errors. Others are error
 * codes returned by the Arakoon server.
 *
 * \since 1.0
 */
typedef enum {
    /* These are returned from the server */
    ARAKOON_RC_SUCCESS = 0, /**< Success */
    ARAKOON_RC_NO_MAGIC = 1, /**< No magic applied to given command */
    ARAKOON_RC_TOO_MANY_DEAD_NODES = 2, /**< Too many dead nodes */
    ARAKOON_RC_NO_HELLO = 3, /**< No hello received from client */
    ARAKOON_RC_NOT_MASTER = 4, /**< Node is not the master */
    ARAKOON_RC_NOT_FOUND = 5, /**< Not found */
    ARAKOON_RC_WRONG_CLUSTER = 6, /**< An invalid cluster name was specified */
    ARAKOON_RC_ASSERTION_FAILED = 7, /**< An assertion failed */
    ARAKOON_RC_READ_ONLY = 8, /**< Node is in read-only mode */
    ARAKOON_RC_NURSERY_RANGE_ERROR = 9, /**< Nursery range error */
    ARAKOON_RC_UNKNOWN_FAILURE = 0xff, /**< An unknown failure occurred */

    /* Internal client errors */
    ARAKOON_RC_CLIENT_NETWORK_ERROR = 0x0100, /**< A client-side network error occurred */
    ARAKOON_RC_CLIENT_UNKNOWN_NODE = 0x0200, /**< An unknown node name was received */
    ARAKOON_RC_CLIENT_MASTER_NOT_FOUND = 0x0300, /**< The master node could not be determined */
    ARAKOON_RC_CLIENT_NOT_CONNECTED = 0x0400, /**< The client is not connected to a master node */
    ARAKOON_RC_CLIENT_TIMEOUT = 0x0500, /**< A timeout was reached */
    ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING = 0x0600, /**< Unable to parse routing information */
    ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG = 0x0700 /**< Invalid client config (needs update?) */

} ArakoonReturnCode;

/**
 * \brief Status code type used throughout the API
 *
 * Values of type arakoon_rc denote one of the #ArakoonReturnCode codes if
 * positive or zero, or an errno value if negative.
 *
 * \ingroup ErrorHandling
 * \since 1.0
 */
typedef int arakoon_rc;

/**
 * \brief Check whether a given #arakoon_rc value is an *errno* value
 *
 * \since 1.0
 */
#define ARAKOON_RC_IS_ERRNO(n) (n < 0)
/**
 * \brief Convert and cast an #arakoon_rc value to the corresponding (positive!) *errno* value
 *
 * \since 1.0
 */
#ifndef __cplusplus /* C++ doesn't like typeof */
# define ARAKOON_RC_AS_ERRNO(n) ((typeof(errno))(-n))
#else /* C++ hack. According to 'man 3 errno', errno is an integer */
# define ARAKOON_RC_AS_ERRNO(n) ((int)(-n))
#endif
/**
 * \brief Check whether a given #arakoon_rc value denotes success
 *
 * \since 1.0
 */
#define ARAKOON_RC_IS_SUCCESS(n) (ARAKOON_GNUC_LIKELY(n == 0))
/**
 * \brief Check whether a given #arakoon_rc value is an #ArakoonReturnCode
 *
 * \since 1.0
 */
#define ARAKOON_RC_IS_ARAKOONRETURNCODE(n) (n >= 0)
/**
 * \brief Convert and cast an #arakoon_rc value into an #ArakoonReturnCode
 *
 * \since 1.0
 */
#define ARAKOON_RC_AS_ARAKOONRETURNCODE(n) ((ArakoonReturnCode) n)

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES

/**
 * \brief Turn an #arakoon_rc value into a human-readable string representation
 *
 * This uses *strerror(3)* internally when the argument is an *errno* value.
 * The corresponding caveats apply.
 *
 * \param n status code to represent
 *
 * \return Human-readable string for the given status code
 *
 * \since 1.0
 */
const char * arakoon_strerror(arakoon_rc n);

#endif /* ARAKOON_H_EXPORT_PROCEDURES */

/** @} */


/** \defgroup utils Utilities
 * @{
 */

/* Utility stuff */
#if ARAKOON_H_EXPORT_TYPES

typedef char arakoon_bool;
#define ARAKOON_BOOL_TRUE ((arakoon_bool)(1))
#define ARAKOON_BOOL_FALSE ((arakoon_bool)(0))

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES

/* Library version information */
/**
 * \brief Retrieve the major version number of the library
 *
 * \since 1.0
 */
unsigned int arakoon_library_version_major(void) ARAKOON_GNUC_CONST;
/**
 * \brief Retrieve the minor version number of the library
 *
 * \since 1.0
 */
unsigned int arakoon_library_version_minor(void) ARAKOON_GNUC_CONST;
/**
 * \brief Retrieve the micro version number of the library
 *
 * \since 1.0
 */
unsigned int arakoon_library_version_micro(void) ARAKOON_GNUC_CONST;
/**
 * \brief Retrieve a human-readable version string of the library
 *
 * \note Don't assume any formatting of this string, it could contain anything at
 * all.
 *
 * \since 1.0
 */
const char * arakoon_library_version_info(void) ARAKOON_GNUC_CONST;

/**
 * \brief Turn a generic byte sequence into a C-string
 *
 * This procedure turns a given data blob into a null-terminated C-string. The
 * allocated string is returned. A call to `realloc` is used to achieve this, so
 * the original data pointer will be invalid after calling this procedure.
 *
 * The caller is in charge of releasing the returned memory.
 *
 * \param data pointer to the string data
 * \param length length of the string
 *
 * \return Null-terminated string version of the original data
 *
 * \since 1.0
 */
char * arakoon_utils_make_string(void *data, size_t length)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;

#endif /* ARAKOON_H_EXPORT_PROCEDURES */

#if ARAKOON_H_EXPORT_TYPES
/**
 * \brief Placeholder for zero-length strings
 *
 * This is (intentionally) a pointer to 0x01, which is not equal to NULL, but
 * should, similar to a NULL-pointer, result in a segfault if dereferenced.
 *
 * It is not guaranteed this value will be used for all zero-length data
 * pointers!
 *
 * \since 1.2
 */
#define ARAKOON_ZERO_LENGTH_DATA_PTR ((void *)(0x01))

/**
 * \brief Turn a potential NULL-pointer into a valid address for zero-length
 * strings
 *
 * \since 1.2
 */
#define ARAKOON_MAYBE_ZERO_LENGTH_DATA(l, s) \
        ((((l) == 0) && ((s) == NULL)) ? ARAKOON_ZERO_LENGTH_DATA_PTR : s)
#endif

/** @} */


/** \defgroup MemoryHandling Memory handling
 * @{
 */

/* Memory handling */
#if ARAKOON_H_EXPORT_TYPES

/**
 * \brief A vtable containing function pointers to be used for heap management
 *
 * \since 1.0
 */
typedef struct {
    void * (*malloc) (size_t size); /**< *malloc(3)* */
    void (*free) (void *ptr); /**< *free(3)* */
    void * (*realloc) (void *ptr, size_t size); /**< *realloc(3)* */
} ArakoonMemoryHooks;

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES

/**
 * \brief Register memory-management functions to be used by Crakoon
 *
 * \param hooks heap management hooks to use inside the library
 *
 * \since 1.0
 */
arakoon_rc arakoon_memory_set_hooks(const ArakoonMemoryHooks * const hooks)
    ARAKOON_GNUC_NONNULL;

/**
 * \brief Get registered memory-management functions used by Crakoon
 *
 * \since 1.2
 */
const ArakoonMemoryHooks * arakoon_memory_get_hooks(void);

/**
 * Retrieve an #ArakoonMemoryHooks table containing wrappers around the system
 * *malloc(3)* and *realloc(3)* which `abort()` on allocation failure. Pass
 * this to #arakoon_memory_set_hooks in case you don't bother handling heap
 * management failures (i.e. don't want to care about `NULL` checks or
 * `ENOMEM`) and just die.
 *
 * \since 1.0
 */
const ArakoonMemoryHooks * arakoon_memory_get_abort_hooks(void);

#endif /* ARAKOON_H_EXPORT_PROCEDURES */

/** @} */

/** \defgroup Logging
 * @{
 */
#if ARAKOON_H_EXPORT_TYPES

/**
 * \brief Enumeration of log levels
 *
 * \since 1.0
 */
typedef enum {
    ARAKOON_LOG_TRACE,
    ARAKOON_LOG_DEBUG,
    ARAKOON_LOG_INFO,
    ARAKOON_LOG_WARNING,
    ARAKOON_LOG_ERROR,
    ARAKOON_LOG_FATAL
} ArakoonLogLevel;

/**
 * \brief Log handler callback prototype
 *
 * \since 1.0
 */
typedef void (*ArakoonLogHandler) (ArakoonLogLevel level,
    const char * message);

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES

/**
 * \brief Set a log message handler procedure
 *
 * \since 1.0
 */
void arakoon_log_set_handler(const ArakoonLogHandler handler);
/**
 * \brief A handler which logs all messages to stderr
 *
 * \since 1.0
 */
ArakoonLogHandler arakoon_log_get_stderr_handler(void);

#endif /* ARAKOON_H_EXPORT_PROCEDURES */

#if ARAKOON_H_EXPORT_TYPES
/**
 * \brief Prototype for client error handlers
 *
 * This is mainly meant to separate client error logging from standard library
 * functionality logging.
 *
 * By default, client errors will be logged to the default logger at
 * #ARAKOON_LOG_DEBUG level. If the message contains a NUL-byte, it will be
 * truncated.
 *
 * If a specific handler is set using #arakoon_log_set_client_error_handler,
 * this will be used instead.
 *
 * \since 1.1
 */
typedef void (*ArakoonClientErrorHandler) (arakoon_rc rc,
    const size_t message_size, const void * const message);

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES

/**
 * \brief Set a client error log handler
 *
 * \since 1.1
 */
void arakoon_log_set_client_error_handler(const ArakoonClientErrorHandler);

#endif /* ARAKOON_H_EXPORT_PROCEDURES */


/** @} */

/** \defgroup DataStructures Data structures
 *
 * \brief Some generic data structures used throughout the API.
 *
 * @{
 */

/** \defgroup ValueList Lists of keys or values
 * @{
 */
#if ARAKOON_H_EXPORT_TYPES
/**
 * \brief Abstract representation of a value list
 *
 * \since 1.0
 */
typedef struct ArakoonValueList ArakoonValueList;

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES

/**
 * \brief Create a new #ArakoonValueList
 *
 * The list should be released using #arakoon_value_list_free when no longer
 * needed.
 *
 * \since 1.0
 */
ArakoonValueList * arakoon_value_list_new(void)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_MALLOC;
/**
 * \brief Add a value to the list (at the tail)
 *
 * The given value is copied and will be freed when #arakoon_value_list_free is
 * used.
 *
 * \since 1.0
 */
arakoon_rc arakoon_value_list_add(ArakoonValueList *list,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_WARN_UNUSED_RESULT ARAKOON_GNUC_NONNULL2(1, 3);
/**
 * \brief Retrieve the number of items in the list
 *
 * \since 1.0
 */
ssize_t arakoon_value_list_size(const ArakoonValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;
/**
 * \brief Free a value list
 *
 * \note Any iters over the list will become invalid after this call.
 *
 * \since 1.0
 */
void arakoon_value_list_free(ArakoonValueList * const list);

#endif /* ARAKOON_H_EXPORT_PROCEDURES */

#if ARAKOON_H_EXPORT_TYPES
/**
 * \brief Abstract representation of an #ArakoonValueList iterator
 *
 * \since 1.0
 */
typedef struct ArakoonValueListIter ArakoonValueListIter;

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES
/**
 * \brief Create an iter for the given value list
 *
 * \since 1.0
 */
ArakoonValueListIter * arakoon_value_list_create_iter(
    const ArakoonValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT
    ARAKOON_GNUC_MALLOC;
/**
 * \brief Release an #ArakoonValueListIter
 *
 * \since 1.0
 */
void arakoon_value_list_iter_free(ArakoonValueListIter * const iter);
/**
 * \brief Retrieve the next item
 *
 * `value` will point to `NULL` when the last item was reached.
 *
 * \since 1.0
 */
arakoon_rc arakoon_value_list_iter_next(ArakoonValueListIter * const iter,
    size_t * const value_size, const void ** const value)
    ARAKOON_GNUC_NONNULL;
/**
 * \brief Reset the list cursor to the first entry
 *
 * \since 1.0
 */
arakoon_rc arakoon_value_list_iter_reset(ArakoonValueListIter * const iter)
    ARAKOON_GNUC_NONNULL;

#endif /* ARAKOON_H_EXPORT_PROCEDURES */

/**
 * \brief Helper to map over an #ArakoonValueList
 *
 * Example usage:
 *
 * \code
 * const void * v = NULL;
 * size_t l = 0;
 * ArakoonValueIter *iter = NULL;
 *
 * iter = get_iter();
 * FOR_ARAKOON_VALUE_ITER(iter, &l, &v) {
 *     do_something(l, v);
 * }
 * \endcode
 *
 * \since 1.0
 */
#define FOR_ARAKOON_VALUE_ITER(i, l, v)        \
    for(arakoon_value_list_iter_next(i, l, v); \
        *v != NULL;                            \
        arakoon_value_list_iter_next(i, l, v))
/** @} */

/** \defgroup KeyValueList Lists of (key, value) pairs
 *
 * \brief Similar to \ref ValueList "ValueLists", but with pairs of values, sort-of
 *
 * @{
 */
#if ARAKOON_H_EXPORT_TYPES
/**
 * \brief Abstract representation of a key-value list
 *
 * \since 1.0
 */
typedef struct ArakoonKeyValueList ArakoonKeyValueList;

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES
/**
 * \brief Retrieve the number of items in the list
 *
 * \since 1.0
 */
ssize_t arakoon_key_value_list_size(const ArakoonKeyValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;
/**
 * \brief Free a key-value list
 *
 * \note Any iters over the list will become invalid after this call.
 *
 * \since 1.0
 */
void arakoon_key_value_list_free(ArakoonKeyValueList * const list);

#endif /* ARAKOON_H_EXPORT_PROCEDURES */

#if ARAKOON_H_EXPORT_TYPES
/**
 * \brief Abstract representation of an #ArakoonKeyValueList iterator
 *
 * \since 1.0
 */
typedef struct ArakoonKeyValueListIter ArakoonKeyValueListIter;

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES
/**
 * \brief Create an iter for the given key-value list
 *
 * \since 1.0
 */
ArakoonKeyValueListIter * arakoon_key_value_list_create_iter(
    const ArakoonKeyValueList * const list)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT
    ARAKOON_GNUC_MALLOC;
/**
 * \brief Release an #ArakoonKeyValueListIter
 *
 * \since 1.0
 */
void arakoon_key_value_list_iter_free(ArakoonKeyValueListIter * const iter);
/**
 * \brief Retrieve the next item
 *
 * `key` and `value` will point to `NULL` when the last item was reached.
 *
 * \since 1.0
 */
arakoon_rc arakoon_key_value_list_iter_next(ArakoonKeyValueListIter * const iter,
    size_t * const key_size, const void ** const key,
    size_t * const value_size, const void ** const value)
    ARAKOON_GNUC_NONNULL;
/**
 * \brief Reset the list cursor to the first entry
 *
 * \since 1.0
 */
arakoon_rc arakoon_key_value_list_iter_reset(ArakoonKeyValueListIter * const iter)
    ARAKOON_GNUC_NONNULL;

#endif /* ARAKOON_H_EXPORT_PROCEDURES */
/**
 * \brief Helper to map over an #ArakoonValueList
 *
 * \since 1.0
 */
#define FOR_ARAKOON_KEY_VALUE_ITER(i, kl, k, vl, v)        \
    for(arakoon_key_value_list_iter_next(i, kl, k, vl, v); \
        (*k != NULL && *v != NULL);                        \
        arakoon_key_value_list_iter_next(i, kl, k, vl, v))

/** @} */
/** @} */

#if ARAKOON_H_EXPORT_TYPES
/** \defgroup Sequences Sequences
 * @{
 */
typedef struct ArakoonSequence ArakoonSequence;

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES
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
/* Add an 'assert_exists' action to the sequence
 *
 * Key will be copied and released on arakoon_sequence_free.
 */
arakoon_rc arakoon_sequence_add_assert_exists(ArakoonSequence *sequence,
    const size_t key_size, const void * const key)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;

#endif /* ARAKOON_H_EXPORT_PROCEDURES */
/** @} */


/** \defgroup ClientCallOptions Client call options
 * @{
 */
#if ARAKOON_H_EXPORT_TYPES
/* This struct contains some settings (to be set using accessor procedures)
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

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES
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

#endif /* ARAKOON_H_EXPORT_PROCEDURES */
/** @} */


/** \defgroup ClusterNode Cluster nodes
 * @{
 */

/* ArakoonClusterNode */
#if ARAKOON_H_EXPORT_TYPES
typedef struct ArakoonClusterNode ArakoonClusterNode;
#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES
/* Allocate a new ArakoonClusterNode
 *
 * This can be released using arakoon_cluster_node_free, unless the node has
 * been attached to an ArakoonCluster using arakoon_cluster_add_node, which
 * transfers ownership to the cluster, unless the arakoon_cluster_add_node call
 * fails.
 *
 * The given name will be copied and released in arakoon_cluster_node_free.
 */
ArakoonClusterNode * arakoon_cluster_node_new(const char * const name)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_MALLOC ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Release an ArakoonClusterNode
 *
 * This will also close any connections related to the node object.
 *
 * Do not call this after attaching a node to an ArakoonCluster using
 * arakoon_cluster_add_node (unless this call failed).
 */
void arakoon_cluster_node_free(ArakoonClusterNode *node);

/* Add an address to an ArakoonClusterNode
 *
 * The given address struct should *not* be freed by the caller. It should
 * remain valid as long as the ArakoonClusterNode (or, the ArakoonCluster it's
 * attached to) exists. It *will* be released using freeaddrinfo(3) when
 * arakoon_cluster_node_free is used. As such, a caller should not re-use the
 * pointer after calling arakoon_cluster_node_add_address, nor pass it to any
 * other functions (in general).
 *
 * If some addresses were attached to the node before, the new one will be added
 * to this list (ordering is undefined).
 */
arakoon_rc arakoon_cluster_node_add_address(ArakoonClusterNode *node,
    struct addrinfo * const address)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Helper around arakoon_cluster_node_add_address
 *
 * This function is a helper arond arakoon_cluster_node_add_addres to easily
 * add TCP-based connections to server nodes. The function will not reference
 * any of the given pointers after returning.
 *
 * It uses getaddrinfo(3) to look up socket connection information. Refer to the
 * corresponding manual page to know the value of 'host' and 'service'.
 */
arakoon_rc arakoon_cluster_node_add_address_tcp(ArakoonClusterNode *node,
    const char * const host, const char * const service)
    ARAKOON_GNUC_NONNULL3(1, 2, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;

#endif /* ARAKOON_H_EXPORT_PROCEDURES */
/** @} */

/** \defgroup Cluster Clusters
 * @{
 */
#if ARAKOON_H_EXPORT_TYPES

typedef enum {
        ARAKOON_PROTOCOL_VERSION_1
} ArakoonProtocolVersion;

/* ArakoonCluster */
typedef struct ArakoonCluster ArakoonCluster;

#endif /* ARAKOON_H_EXPORT_TYPES */

#if ARAKOON_H_EXPORT_PROCEDURES
/* Allocate a new ArakoonCluster, to be released using arakoon_cluster_free
 *
 * The given name will be copied and released in arakoon_cluster_free
 */
ArakoonCluster * arakoon_cluster_new(ArakoonProtocolVersion version,
    const char * const name)
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
/**
 * \brief Retrieve the last error message received through the cluster
 *
 * Note you'll get a reference to a string which will be free'd upon the next
 * call using the cluster, or whenever the cluster is free'd. As such you
 * should create your own copy of the string for further usage, if required.
 *
 * \since 1.0
 */
arakoon_rc arakoon_cluster_get_last_error(
    const ArakoonCluster * const cluster, size_t *len, const void ** data)
    ARAKOON_GNUC_NONNULL;

/* Add a node to the cluster
 *
 * The ownership of the ArakoonClusterNode transfers to the ArakoonCluster it
 * is attached to, so you shouldn't free an ArakoonClusterNode after a
 * successful call to arakoon_cluster_add_node.
 *
 * You *are* allowed to add addresses to a node (using
 * arakoon_cluster_node_add_address or arakoon_cluster_node_add_address_tcp)
 * after attaching it to a cluster.
 */
arakoon_rc arakoon_cluster_add_node(ArakoonCluster *cluster,
    ArakoonClusterNode *node)
    ARAKOON_GNUC_NONNULL2(1, 2) ARAKOON_GNUC_WARN_UNUSED_RESULT;

/** @} */

/** \defgroup ClientOperations Client operations
 * @{
 */
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
/* Send a 'synced_sequence' call to the server */
arakoon_rc arakoon_synced_sequence(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const ArakoonSequence * const sequence)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send an 'assert' call to the server
 *
 * 'value' can be NULL to denote 'None', in which case 'size' should be 0 as
 * well.
 *
 * This call doesn't produce any result, but will return
 * ARAKOON_RC_ASSERTION_FAILED if the assertion failed.
 */
arakoon_rc arakoon_assert(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_NONNULL2(1, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send an 'assert_exists' call to the server
 *
 * This call doesn't produce any result, but will return
 * ARAKOON_RC_ASSERTION_FAILED if the assertion failed.
 */
arakoon_rc arakoon_assert_exists(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key)
    ARAKOON_GNUC_NONNULL2(1, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'rev_range_entries' call to the server
 *
 * 'begin_key' and 'end_key' can be set to NULL to denote 'None'.
 * 'begin_key_size' and 'end_key_size' should be set to 0 accordingly.
 *
 * If 'max_elements' is negative, all matches will be returned.
 *
 * The result will be stored at 'result', and should be released using
 * arakoon_key_value_list_free by the caller when no longer required.
 */
arakoon_rc arakoon_rev_range_entries(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t begin_key_size, const void * const begin_key,
    const arakoon_bool begin_key_included,
    const size_t end_key_size, const void * const end_key,
    const arakoon_bool end_key_included,
    const ssize_t max_elements,
    ArakoonKeyValueList **result) ARAKOON_GNUC_NONNULL2(1, 10)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'delete_prefix' call to the server
 *
 * Remove all key-value pairs from the store for which the key matches the given
 * prefix.
 *
 * The result (the number of entries removed from the store) will be stored at
 * 'result'.
 */
arakoon_rc arakoon_delete_prefix(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t prefix_size, const void * const prefix,
    uint32_t * result) ARAKOON_GNUC_NONNULL3(1, 4, 5)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'version' call to the server
 *
 * Retrieve a version number triplet from the server, as well as a free-form
 * string containing version and build options.
 */
arakoon_rc arakoon_version(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    int32_t * major, int32_t * minor, int32_t * patch,
    char ** const version_info) ARAKOON_GNUC_NONNULL5(1, 3, 4, 5, 6)
    ARAKOON_GNUC_WARN_UNUSED_RESULT;

#endif /* ARAKOON_H_EXPORT_PROCEDURES */

/** @} */

ARAKOON_END_DECLS
/** @} */

#endif /* ifndef __ARAKOON_H__ */
