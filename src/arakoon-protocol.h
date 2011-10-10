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

#ifndef __ARAKOON_PROTOCOL_H__
#define __ARAKOON_PROTOCOL_H__

#include <stdint.h>

#include "arakoon.h"
#include "arakoon-utils.h"
#include "arakoon-networking.h"

ARAKOON_BEGIN_DECLS

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

#define ARAKOON_PROTOCOL_WRITE_STRING_OPTION(a, s, n)                    \
        STMT_START                                                       \
        if(s == NULL && n != 0) {                                        \
                _arakoon_log_error("Passed NULL string, but size != 0"); \
        }                                                                \
        if(s == NULL) {                                                  \
                ARAKOON_PROTOCOL_WRITE_BOOL(a, ARAKOON_BOOL_FALSE);      \
        }                                                                \
        else {                                                           \
                ARAKOON_PROTOCOL_WRITE_BOOL(a, ARAKOON_BOOL_TRUE);       \
                ARAKOON_PROTOCOL_WRITE_STRING(a, s, n);                  \
        }                                                                \
        STMT_END

#define ARAKOON_PROTOCOL_WRITE_BOOL(a, b)                                                   \
        STMT_START                                                                          \
        *((char *) a) = (b == ARAKOON_BOOL_FALSE ? ARAKOON_BOOL_FALSE : ARAKOON_BOOL_TRUE); \
        a += ARAKOON_PROTOCOL_BOOL_LEN;                                                     \
        STMT_END

#define WRITE_BYTES(f, a, n, r, t)                                                    \
        STMT_START                                                                    \
        r = _arakoon_networking_poll_write(_arakoon_cluster_node_get_fd(f), a, n, t); \
        if(!ARAKOON_RC_IS_SUCCESS(r)) {                                               \
                _arakoon_cluster_node_disconnect(f);                                  \
        }                                                                             \
        STMT_END

#define READ_BYTES(f, a, n, r, t)                                                    \
        STMT_START                                                                   \
        r = _arakoon_networking_poll_read(_arakoon_cluster_node_get_fd(f), a, n, t); \
        if(!ARAKOON_RC_IS_SUCCESS(r)) {                                              \
                _arakoon_cluster_node_disconnect(f);                                 \
        }                                                                            \
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
                                _rsl_rc = _arakoon_value_list_prepend(a,    \
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

#define ARAKOON_PROTOCOL_READ_STRING_STRING_LIST(fd, a, rc, t)                     \
        STMT_START                                                                 \
        uint32_t _rsl_cnt = 0, _rsl_i = 0;                                         \
        arakoon_rc _rsl_rc = 0;                                                    \
        void *_rsl_s0 = NULL, *_rsl_s1 = NULL;                                     \
        size_t _rsl_l0 = 0, _rsl_l1 = 0;                                           \
                                                                                   \
        ARAKOON_PROTOCOL_READ_UINT32(fd, _rsl_cnt, _rsl_rc, t);                    \
        if(ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {                                       \
                for(_rsl_i = 0; _rsl_i < _rsl_cnt; _rsl_i++) {                     \
                        _rsl_l0 = 0;                                               \
                        _rsl_s0 = NULL;                                            \
                        _rsl_l1 = 0;                                               \
                        _rsl_s1 = NULL;                                            \
                                                                                   \
                        ARAKOON_PROTOCOL_READ_STRING(fd, _rsl_s0, _rsl_l0,         \
                                _rsl_rc, t);                                       \
                                                                                   \
                        if(!ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {                      \
                                break;                                             \
                        }                                                          \
                        else {                                                     \
                                ARAKOON_PROTOCOL_READ_STRING(fd, _rsl_s1,          \
                                        _rsl_l1, _rsl_rc, t);                      \
                                if(!ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {              \
                                        arakoon_mem_free(_rsl_s0);                 \
                                        break;                                     \
                                }                                                  \
                                else {                                             \
                                        /* TODO This introduces a useless
                                         * memcpy */                               \
                                        _rsl_rc = _arakoon_key_value_list_prepend( \
                                                a, _rsl_l0, _rsl_s0, _rsl_l1,      \
                                                _rsl_s1);                          \
                                        arakoon_mem_free(_rsl_s0);                 \
                                        arakoon_mem_free(_rsl_s1);                 \
                                                                                   \
                                        if(!ARAKOON_RC_IS_SUCCESS(_rsl_rc)) {      \
                                                break;                             \
                                        }                                          \
                                }                                                  \
                        }                                                          \
                }                                                                  \
        }                                                                          \
                                                                                   \
        rc = _rsl_rc;                                                              \
        STMT_END

ARAKOON_END_DECLS

#endif /* ifndef __ARAKOON_PROTOCOL_H__ */
