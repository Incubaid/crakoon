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

#ifndef __ARAKOON_UTILS_H__
#define __ARAKOON_UTILS_H__

#include <stdlib.h>

#include "arakoon.h"

ARAKOON_BEGIN_DECLS

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
# define ARAKOON_GNUC_PRINTF( format_idx, arg_idx ) \
  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#else
# define ARAKOON_GNUC_PRINTF
#endif

#define ARAKOON_STRINGIFY(n) ARAKOON_STRINGIFY_ARG(n)
#define ARAKOON_STRINGIFY_ARG(n) #n

#define STMT_START \
        do {
#define STMT_END \
        } while(0)

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

extern ArakoonMemoryHooks memory_hooks;

#define arakoon_mem_malloc(s) (memory_hooks.malloc(s))
#define arakoon_mem_free(p) (memory_hooks.free(p))
#define arakoon_mem_realloc(p, s) (memory_hooks.realloc(p, s))

#define arakoon_mem_new(c, t) \
        (t *)(arakoon_mem_malloc((c) * sizeof(t)))

#ifdef ENABLE_TRACE
void _arakoon_log_trace(const char *format, ...) ARAKOON_GNUC_PRINTF(1, 2);

# define FUNCTION_ENTER(n)                        \
        STMT_START                                \
        _arakoon_log_trace("Enter " ARAKOON_STRINGIFY(n)); \
        STMT_END

#else
# define _arakoon_log_trace(f) STMT_START STMT_END
# define FUNCTION_ENTER(n) STMT_START STMT_END
#endif
void _arakoon_log_debug(const char *format, ...) ARAKOON_GNUC_PRINTF(1, 2);
void _arakoon_log_info(const char *format, ...) ARAKOON_GNUC_PRINTF(1, 2);
void _arakoon_log_warning(const char *format, ...) ARAKOON_GNUC_PRINTF(1, 2);
void _arakoon_log_error(const char *format, ...) ARAKOON_GNUC_PRINTF(1, 2);
void _arakoon_log_fatal(const char *format, ...) ARAKOON_GNUC_PRINTF(1, 2);

void _arakoon_log_client_error(arakoon_rc rc,
        size_t message_size, const void * message) ARAKOON_GNUC_NONNULL1(3);

#define ASSERT_ALL_WRITTEN(command, c, len)                            \
        STMT_START                                                     \
        if(c != command + len) {                                       \
                _arakoon_log_fatal(                                    \
                        "Unexpected number of characters in command"); \
                abort();                                               \
        }                                                              \
        STMT_END

ARAKOON_END_DECLS

#endif /* ifndef __ARAKOON_UTILS_H__ */
