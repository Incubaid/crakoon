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

/* TODO
 * ====
 * - Plugable communication channels (like Pyrakoon)
 * - Use TCP_CORK when using TCP sockets, wrapped around command submission
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "arakoon.h"
#include "arakoon-utils.h"
#include "arakoon-protocol.h"
#include "arakoon-cluster.h"
#include "arakoon-cluster-node.h"
#include "arakoon-client-call-options.h"
#include "arakoon-value-list.h"
#include "arakoon-key-value-list.h"
#include "arakoon-assert.h"

/* Sequences */
typedef struct ArakoonSequenceItem ArakoonSequenceItem;
struct ArakoonSequenceItem {
        enum {
                ARAKOON_SEQUENCE_ITEM_TYPE_SET,
                ARAKOON_SEQUENCE_ITEM_TYPE_DELETE,
                ARAKOON_SEQUENCE_ITEM_TYPE_ASSERT
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
                        size_t value_size;
                        void * value;
                } assert;
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
                case ARAKOON_SEQUENCE_ITEM_TYPE_ASSERT: {
                        arakoon_mem_free(item->data.assert.key);
                        arakoon_mem_free(item->data.assert.value);
                }; break;
                default: {
                        _arakoon_log_fatal("Unknown sequence item type");
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

        ASSERT_NON_NULL_RC(sequence);
        ASSERT_NON_NULL_RC(key);
        ASSERT_NON_NULL_RC(value);

        OUVERTURE(ARAKOON_SEQUENCE_ITEM_TYPE_SET);

        COPY_STRING(set, key);
        COPY_STRING(set, value);

        POSTLUDIUM(arakoon_sequence_add_set);
}

arakoon_rc arakoon_sequence_add_delete(ArakoonSequence *sequence,
    const size_t key_size, const void * const key) {
        PRELUDE(arakoon_sequence_add_delete);

        ASSERT_NON_NULL_RC(sequence);
        ASSERT_NON_NULL_RC(key);

        OUVERTURE(ARAKOON_SEQUENCE_ITEM_TYPE_DELETE);

        COPY_STRING(delete, key);

        POSTLUDIUM(arakoon_sequence_add_delete);
}

arakoon_rc arakoon_sequence_add_assert(ArakoonSequence *sequence,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value) {
        PRELUDE(arakoon_sequence_add_assert);

        ASSERT_NON_NULL_RC(sequence);
        ASSERT_NON_NULL_RC(key);

        OUVERTURE(ARAKOON_SEQUENCE_ITEM_TYPE_ASSERT);

        COPY_STRING(assert, key);
        COPY_STRING_OPTION(assert, value);

        POSTLUDIUM(arakoon_sequence_add_assert);
}


#undef PRELUDE
#undef OUVERTURE
#undef POSTLUDIUM

#undef COPY_STRING
#undef COPY_STRING_OPTION

#define HANDLE_ERROR(rc, master, cluster, timeout)                            \
        STMT_START                                                            \
        if(rc != ARAKOON_RC_SUCCESS) {                                        \
                void *_err_msg = NULL;                                        \
                size_t _err_len = 0;                                          \
                arakoon_rc _err_rc = 0;                                       \
                _arakoon_log_debug("Error detected, reading message");        \
                ARAKOON_PROTOCOL_READ_STRING(                                 \
                    master, _err_msg, _err_len, _err_rc, timeout);            \
                if(_err_rc != ARAKOON_RC_SUCCESS) {                           \
                        _arakoon_log_fatal(                                   \
                            "Failed to read error message: %s",               \
                            arakoon_strerror(_err_rc));                       \
                        break;                                                \
                }                                                             \
                _arakoon_log_warning(                                         \
                    "%s: %.*s", arakoon_strerror(rc),                         \
                    _err_len < INT_MAX ? (int) _err_len : INT_MAX,            \
                    (char *) _err_msg);                                       \
                _arakoon_cluster_set_last_error(cluster, _err_len, _err_msg); \
        }                                                                     \
        STMT_END

/* Client operations */
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

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(client_id);
        ASSERT_NON_NULL_RC(cluster_id);
        ASSERT_NON_NULL_RC(result);

        _arakoon_cluster_reset_last_error(cluster);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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

        rc = _arakoon_cluster_node_write_bytes(master, len, command, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        HANDLE_ERROR(rc, master, cluster, &timeout);
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
        ArakoonClusterNode *master_ = NULL;

        FUNCTION_ENTER(arakoon_who_master);

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(master);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master_);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        return _arakoon_cluster_node_who_master(master_, &timeout,
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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(result);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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
        HANDLE_ERROR(rc, master, cluster, &timeout);
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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(key);
        ASSERT_NON_NULL_RC(result);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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
        HANDLE_ERROR(rc, master, cluster, &timeout);
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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(key);
        ASSERT_NON_NULL_RC(result_size);
        ASSERT_NON_NULL_RC(result);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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

        HANDLE_ERROR(rc, master, cluster, &timeout);

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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(key);
        ASSERT_NON_NULL_RC(value);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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

        HANDLE_ERROR(rc, master, cluster, &timeout);

        return rc;
}

arakoon_rc arakoon_multi_get(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const ArakoonValueList * const keys, ArakoonValueList **result) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonValueListIter *iter = NULL;
        size_t value_size = 0;
        const void *value = NULL;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_multi_get);

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(keys);
        ASSERT_NON_NULL_RC(result);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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

        iter = arakoon_value_list_create_iter(keys);
        FOR_ARAKOON_VALUE_ITER(iter, &value_size, &value) {
                /* TODO Multi syscall vs memory copies... */
                WRITE_BYTES(master, &value_size,
                        ARAKOON_PROTOCOL_UINT32_LEN, rc, &timeout);
                RETURN_IF_NOT_SUCCESS(rc);

                WRITE_BYTES(master, value, value_size, rc,
                        &timeout);
                RETURN_IF_NOT_SUCCESS(rc);
        }
        arakoon_value_list_iter_free(iter);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        HANDLE_ERROR(rc, master, cluster, &timeout);
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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(key);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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

        HANDLE_ERROR(rc, master, cluster, &timeout);

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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(result);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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
        HANDLE_ERROR(rc, master, cluster, &timeout);
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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(result);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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
        HANDLE_ERROR(rc, master, cluster, &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *result = NULL;
                return rc;
        }

        *result = _arakoon_key_value_list_new();
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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(begin_key);
        ASSERT_NON_NULL_RC(result);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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
        HANDLE_ERROR(rc, master, cluster, &timeout);
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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(key);
        ASSERT_NON_NULL_RC(result_size);
        ASSERT_NON_NULL_RC(result);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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
        HANDLE_ERROR(rc, master, cluster, &timeout);
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

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(sequence);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

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
                        case ARAKOON_SEQUENCE_ITEM_TYPE_ASSERT: {
                                I(ARAKOON_PROTOCOL_STRING_LEN(item->data.assert.key_size));
                                I(ARAKOON_PROTOCOL_STRING_OPTION_LEN(
                                    item->data.assert.value, item->data.assert.value_size));
                        }; break;
                        default: {
                                _arakoon_log_fatal("Invalid sequence type");
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

#define WRITE_STRING_OPTION(s, l)                                          \
        STMT_START                                                         \
        if(s == NULL) {                                                    \
                if(l != 0) {                                               \
                        _arakoon_log_error(                                \
                                "String is NULL, but length is non-zero"); \
                }                                                          \
                i -= ARAKOON_PROTOCOL_BOOL_LEN;                            \
                command[i] = ARAKOON_BOOL_FALSE;                           \
        }                                                                  \
        else {                                                             \
                WRITE_STRING(s, l);                                        \
                i -= ARAKOON_PROTOCOL_BOOL_LEN;                            \
                command[i] = ARAKOON_BOOL_TRUE;                            \
        }                                                                  \
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
                        case ARAKOON_SEQUENCE_ITEM_TYPE_ASSERT: {
                                WRITE_STRING_OPTION(item->data.assert.value,
                                        item->data.assert.value_size);
                                WRITE_STRING(item->data.assert.key,
                                        item->data.assert.key_size);
                                WRITE_UINT32(8);
                        }; break;
                        default: {
                                _arakoon_log_fatal("Invalid sequence type");
                                abort();
                        }; break;
                }
        }

        if(i != ARAKOON_PROTOCOL_COMMAND_LEN /* Command */
                        + ARAKOON_PROTOCOL_UINT32_LEN /* String length */
                        + ARAKOON_PROTOCOL_UINT32_LEN /* Outer sequence */
                        + ARAKOON_PROTOCOL_UINT32_LEN /* Item count */
          ) {
                _arakoon_log_fatal("Incorrect count in sequence construction");
                abort();
        }

        WRITE_UINT32(count);
        WRITE_UINT32(5);
        WRITE_UINT32(len - ARAKOON_PROTOCOL_UINT32_LEN - ARAKOON_PROTOCOL_COMMAND_LEN);

#undef WRITE_UINT32
#undef WRITE_STRING
#undef WRITE_STRING_OPTION

        if(i != ARAKOON_PROTOCOL_COMMAND_LEN) {
                _arakoon_log_fatal("Incorrect count in sequence construction");
                abort();
        }

        ARAKOON_PROTOCOL_WRITE_COMMAND(command, 0x10, 0x00);
        /* Macro changes our pointer... */
        command -= ARAKOON_PROTOCOL_COMMAND_LEN;

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        HANDLE_ERROR(rc, master, cluster, &timeout);

        return rc;
}

arakoon_rc arakoon_assert(ArakoonCluster *cluster,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        FUNCTION_ENTER(arakoon_assert);

        _arakoon_cluster_reset_last_error(cluster);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(key);

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        ARAKOON_CLUSTER_GET_MASTER(cluster, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN
                + ARAKOON_PROTOCOL_BOOL_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(key_size)
                + ARAKOON_PROTOCOL_STRING_OPTION_LEN(value, value_size);

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x16, 0x00);
        ARAKOON_PROTOCOL_WRITE_BOOL(c,
                arakoon_client_call_options_get_allow_dirty(options_));
        ARAKOON_PROTOCOL_WRITE_STRING(c, key, key_size);
        ARAKOON_PROTOCOL_WRITE_STRING_OPTION(c, value, value_size);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);

        HANDLE_ERROR(rc, master, cluster, &timeout);

        return rc;
}
