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
#include <string.h>

#include "arakoon.h"
#include "arakoon-nursery.h"
#include "arakoon-utils.h"
#include "arakoon-protocol.h"
#include "arakoon-cluster.h"
#include "arakoon-cluster-node.h"
#include "arakoon-client-call-options.h"
#include "arakoon-nursery-routing.h"
#include "arakoon-assert.h"

struct ArakoonNursery {
        const ArakoonCluster *keeper;
        ArakoonNurseryRouting *routing;
};

ArakoonNursery * arakoon_nursery_new(const ArakoonCluster * const keeper) {
        ArakoonNursery *ret = NULL;

        FUNCTION_ENTER(arakoon_nursery_new);

        ASSERT_NON_NULL(keeper);

        ret = arakoon_mem_new(1, ArakoonNursery);
        RETURN_NULL_IF_NULL(ret);

        memset(ret, 0, sizeof(ArakoonNursery));

        ret->keeper = keeper;
        ret->routing = NULL;

        return ret;
}

void arakoon_nursery_free(ArakoonNursery *nursery) {
        RETURN_IF_NULL(nursery);

        _arakoon_nursery_routing_free(nursery->routing);
        arakoon_mem_free(nursery);
}

arakoon_rc arakoon_nursery_update_routing(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;
        void *routing_data = NULL;
        size_t routing_length = 0;
        ArakoonNurseryRouting *routing = NULL;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_nursery_update_routing);

        ASSERT_NON_NULL_RC(nursery);

        if(nursery->routing != NULL) {
                _arakoon_nursery_routing_free(nursery->routing);
                nursery->routing = NULL;
        }

        ARAKOON_CLUSTER_GET_MASTER(nursery->keeper, master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN;

        /* TODO Use stack value? */
        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x20, 0x00);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(master, command, len, rc, &timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(master, rc, &timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_STRING(master, routing_data, routing_length, rc,
                &timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                arakoon_mem_free(routing);
                return rc;
        }

        rc = _arakoon_nursery_routing_parse(routing_length, routing_data,
                &routing);
        RETURN_IF_NOT_SUCCESS(rc);

        arakoon_mem_free(routing_data);

        nursery->routing = routing;

        return rc;
}

arakoon_rc arakoon_nursery_get(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    size_t *result_size, void **result) {
        ArakoonCluster *cluster = NULL;

        FUNCTION_ENTER(arakoon_nursery_get);

        ASSERT_NON_NULL_RC(nursery);
        ASSERT_NON_NULL_RC(key);
        ASSERT_NON_NULL_RC(result_size);
        ASSERT_NON_NULL_RC(result);

        cluster = _arakoon_nursery_routing_lookup(nursery->routing, key_size, key);
        if(cluster == NULL) {
                return ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG;
        }

        return arakoon_get(cluster, options, key_size, key, result_size, result);
}

arakoon_rc arakoon_nursery_set(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value) {
        ArakoonCluster *cluster = NULL;

        FUNCTION_ENTER(arakoon_nursery_set);

        ASSERT_NON_NULL_RC(nursery);
        ASSERT_NON_NULL_RC(key);
        ASSERT_NON_NULL_RC(value);

        cluster = _arakoon_nursery_routing_lookup(nursery->routing, key_size, key);
        if(cluster == NULL) {
                return ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG;
        }

        return arakoon_set(cluster, options, key_size, key, value_size, value);
}

arakoon_rc arakoon_nursery_delete(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key) {
        ArakoonCluster *cluster = NULL;

        FUNCTION_ENTER(arakoon_nursery_delete);

        ASSERT_NON_NULL_RC(nursery);
        ASSERT_NON_NULL_RC(key);

        cluster = _arakoon_nursery_routing_lookup(nursery->routing, key_size, key);
        if(cluster == NULL) {
                return ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG;
        }

        return arakoon_delete(cluster, options, key_size, key);
}

arakoon_rc arakoon_nursery_reconnect_master(const ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key) {
        ArakoonCluster *cluster = NULL;

        FUNCTION_ENTER(arakoon_nursery_reconnect_master);

        ASSERT_NON_NULL_RC(nursery);
        ASSERT_NON_NULL_RC(key);

        cluster = _arakoon_nursery_routing_lookup(nursery->routing, key_size, key);
        if(cluster == NULL) {
                return ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG;
        }

        return arakoon_cluster_connect_master(cluster, options);
}
