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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include "arakoon.h"
#include "arakoon-utils.h"
#include "arakoon-assert.h"

const char * arakoon_strerror(arakoon_rc n) {
        if(ARAKOON_RC_IS_ERRNO(n)) {
                return strerror(n);
        }

        switch(ARAKOON_RC_AS_ARAKOONRETURNCODE(n)) {
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
                case ARAKOON_RC_ASSERTION_FAILED:
                        return "Assertion failed";
                        break;
                case ARAKOON_RC_READ_ONLY:
                        return "Read only";
                        break;
                case ARAKOON_RC_NURSERY_RANGE_ERROR:
                        return "Wrong range in nursery";
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

                case ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING:
                        return "Client unable to parse nursery routing table";
                        break;
                case ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG:
                        return "Client contains invalid nursery routing table";
                        break;
        }

        return "Unknown return code";
}

/* Logging */
static ArakoonLogHandler log_handler = NULL;

void arakoon_log_set_handler(const ArakoonLogHandler handler) {
        log_handler = handler;
}

static void arakoon_log_stderr_handler(ArakoonLogLevel level,
    const char * message) {
        const char *prefix = NULL;

        switch(level) {
                case ARAKOON_LOG_TRACE:
                    prefix = "[TRACE]";
                    break;
                case ARAKOON_LOG_DEBUG:
                    prefix = "[DEBUG]";
                    break;
                case ARAKOON_LOG_INFO:
                    prefix = "[INFO]";
                    break;
                case ARAKOON_LOG_WARNING:
                    prefix = "[WARNING]";
                    break;
                case ARAKOON_LOG_ERROR:
                    prefix = "[ERROR]";
                    break;
                case ARAKOON_LOG_FATAL:
                    prefix = "[FATAL]";
                    break;

                default:
                    prefix = "[LOG]";
                    break;
        }

        fprintf(stderr, "%s %s\n", prefix, message);
}

ArakoonLogHandler arakoon_log_get_stderr_handler(void) {
        return arakoon_log_stderr_handler;
}

#define DEFINE_LOG_FUNCTION(n, l)                         \
        void _arakoon_log_## n(const char *format, ...) { \
                va_list args;                             \
                char buffer[1024];                        \
                va_start(args, format);                   \
                                                          \
                if(log_handler == NULL) {                 \
                        return;                           \
                }                                         \
                                                          \
                vsnprintf(buffer, 1024, format, args);    \
                buffer[1023] = 0;                         \
                                                          \
                log_handler(l, buffer);                   \
                                                          \
                va_end(args);                             \
        }

#ifdef ENABLE_TRACE
DEFINE_LOG_FUNCTION(trace, ARAKOON_LOG_TRACE)
#else
#define _arakoon_log_trace(f) STMT_START STMT_END
#endif
DEFINE_LOG_FUNCTION(debug, ARAKOON_LOG_DEBUG)
DEFINE_LOG_FUNCTION(info, ARAKOON_LOG_INFO)
DEFINE_LOG_FUNCTION(warning, ARAKOON_LOG_WARNING)
DEFINE_LOG_FUNCTION(error, ARAKOON_LOG_ERROR)
DEFINE_LOG_FUNCTION(fatal, ARAKOON_LOG_FATAL)

#undef DEFINE_LOG_FUNCTION


static ArakoonClientErrorHandler client_error_handler = NULL;

void arakoon_log_set_client_error_handler(
    const ArakoonClientErrorHandler handler) {
        client_error_handler = handler;
}

void _arakoon_log_client_error(arakoon_rc rc, size_t message_size,
    const void * const message) {
        if(client_error_handler) {
                client_error_handler(rc, message_size, message);
        }
        else {
                _arakoon_log_debug("%s: %.*s", arakoon_strerror(rc),
                        message_size < INT_MAX ? (int) message_size : INT_MAX,
                        (const char *) message);
        }
}


/* Memory management */
ArakoonMemoryHooks memory_hooks = {
        malloc,
        free,
        realloc
};

arakoon_rc arakoon_memory_set_hooks(const ArakoonMemoryHooks * const hooks) {
        FUNCTION_ENTER(arakoon_memory_set_hooks);

        ASSERT_NON_NULL_RC(hooks);

        memory_hooks.malloc = hooks->malloc;
        memory_hooks.free = hooks->free;
        memory_hooks.realloc = hooks->realloc;

        return ARAKOON_RC_SUCCESS;
}

const ArakoonMemoryHooks * arakoon_memory_get_hooks(void)
{
    return &memory_hooks;
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

        ASSERT_NON_NULL(data);

        s = (char *)arakoon_mem_realloc(data, length + 1);
        if(s == NULL) {
                arakoon_mem_free(data);

                return NULL;
        }

        s[length] = 0;

        return s;
}
