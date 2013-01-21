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
#include <string.h>

#include "arakoon-cluster.h"
#include "arakoon-utils.h"
#include "arakoon-assert.h"

struct ArakoonCluster {
        ArakoonProtocolVersion version;
        char * name;

        struct {
                size_t len;
                void *data;
        } last_error;

        ArakoonClusterNode * nodes;
        ArakoonClusterNode * master;
};

ArakoonCluster * arakoon_cluster_new(ArakoonProtocolVersion version,
    const char * const name) {
        ArakoonCluster *ret = NULL;
        size_t len = 0;

        FUNCTION_ENTER(arakoon_cluster_new);

        if(version != ARAKOON_PROTOCOL_VERSION_1) {
                _arakoon_log_fatal("Unknown protocol version requested");

                errno = EINVAL;
                return NULL;
        }

        ASSERT_NON_NULL(name);

        ret = arakoon_mem_new(1, ArakoonCluster);
        RETURN_NULL_IF_NULL(ret);

        memset(ret, 0, sizeof(ArakoonCluster));

        len = strlen(name) + 1;
        ret->name = arakoon_mem_new(len, char);
        if(ret->name == NULL) {
                goto nomem;
        }

        strncpy(ret->name, name, len);

        ret->last_error.len = 0;
        ret->last_error.data = NULL;
        ret->nodes = NULL;
        ret->master = NULL;
        ret->version = version;

        return ret;

nomem:
        if(ret != NULL) {
                arakoon_mem_free(ret->name);
        }

        arakoon_mem_free(ret);

        return NULL;
}

void arakoon_cluster_free(ArakoonCluster *cluster) {
        ArakoonClusterNode *node = NULL;
        ArakoonClusterNode *next_node = NULL;

        FUNCTION_ENTER(arakoon_cluster_free);

        RETURN_IF_NULL(cluster);

        arakoon_mem_free(cluster->name);
        arakoon_mem_free(cluster->last_error.data);

        node = cluster->nodes;
        while(node != NULL) {
                next_node = _arakoon_cluster_node_get_next(node);
                _arakoon_cluster_node_disconnect(node);
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

        ASSERT_NON_NULL_RC(cluster);

        _arakoon_log_debug("Looking up master node");

        timeout = options != NULL ?
                arakoon_client_call_options_get_timeout(options) :
                ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        /* Find a node to which we can connect */
        node = cluster->nodes;
        while(node != NULL) {
                rc = _arakoon_cluster_node_connect(node, &timeout);

                if(ARAKOON_RC_IS_SUCCESS(rc)) {
                        _arakoon_log_debug("Connected to node %s",
                                _arakoon_cluster_node_get_name(node));

                        rc = _arakoon_cluster_node_who_master(node, &timeout,
                                &master);

                        if(ARAKOON_RC_IS_SUCCESS(rc) && master != NULL) {
                                break;
                        }
                        else if(ARAKOON_RC_IS_SUCCESS(rc)) {
                                _arakoon_log_debug(
                                        "Node %s doesn't know who's master",
                                        _arakoon_cluster_node_get_name(node));
                        }
                        else {
                                _arakoon_log_info(
                                        "Error during who_master call to %s: %s",
                                        _arakoon_cluster_node_get_name(node),
                                        arakoon_strerror(rc));
                        }
                }

                node = _arakoon_cluster_node_get_next(node);
        }

        if(node == NULL) {
                _arakoon_log_warning("Unable to connect to any node");
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }

        if(strcmp(_arakoon_cluster_node_get_name(node), master) == 0) {
                /* The node is master */
                cluster->master = node;
                arakoon_mem_free(master);

                _arakoon_log_info("Found master node %s",
                        _arakoon_cluster_node_get_name(node));

                return ARAKOON_RC_SUCCESS;
        }

        /* Find master node */
        node = cluster->nodes;
        while(node) {
                if(strcmp(_arakoon_cluster_node_get_name(node), master) == 0) {
                        break;
                }
                node = _arakoon_cluster_node_get_next(node);
        }

        arakoon_mem_free(master);

        if(node == NULL) {
                return ARAKOON_RC_CLIENT_UNKNOWN_NODE;
        }

        _arakoon_log_debug("Connecting to master node %s", _arakoon_cluster_node_get_name(node));

        rc = _arakoon_cluster_node_connect(node, &timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        /* Check whether master thinks it's master */
        _arakoon_log_debug("Validating master node");

        rc = _arakoon_cluster_node_who_master(node, &timeout, &master);
        RETURN_IF_NOT_SUCCESS(rc);

        if(strcmp(_arakoon_cluster_node_get_name(node), master) != 0) {
                rc = ARAKOON_RC_CLIENT_MASTER_NOT_FOUND;

                _arakoon_log_debug("Unable to determine master node");
        }
        else {
                rc = ARAKOON_RC_SUCCESS;
                cluster->master = node;

                _arakoon_log_debug("Found master node %s",
                        _arakoon_cluster_node_get_name(node));
        }

        arakoon_mem_free(master);

        return rc;
}

const char * arakoon_cluster_get_name(const ArakoonCluster * const cluster) {
        FUNCTION_ENTER(arakoon_cluster_get_name);

        ASSERT_NON_NULL(cluster);

        return cluster->name;
}

arakoon_rc arakoon_cluster_get_last_error(
    const ArakoonCluster * const cluster, size_t *len, const void **data) {
        FUNCTION_ENTER(arakoon_cluster_get_last_error);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(len);
        ASSERT_NON_NULL_RC(data);

        *len = cluster->last_error.len;
        *data = cluster->last_error.data;

        return ARAKOON_RC_SUCCESS;
}

void _arakoon_cluster_set_last_error(ArakoonCluster * const cluster,
    size_t len, void *data) {
        FUNCTION_ENTER(_arakoon_cluster_set_last_error);

        cluster->last_error.len = len;
        cluster->last_error.data = data;
}

arakoon_rc arakoon_cluster_add_node(ArakoonCluster *cluster,
    ArakoonClusterNode *node) {
        arakoon_rc rc = ARAKOON_RC_SUCCESS;

        FUNCTION_ENTER(arakoon_cluster_add_node);

        ASSERT_NON_NULL_RC(cluster);
        ASSERT_NON_NULL_RC(node);

        _arakoon_log_debug("Adding node %s to cluster %s",
                _arakoon_cluster_node_get_name(node), cluster->name);

        rc = _arakoon_cluster_node_set_cluster(node, cluster);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                return rc;
        }

        _arakoon_cluster_node_set_next(node, cluster->nodes);
        cluster->nodes = node;

        return ARAKOON_RC_SUCCESS;
}

ArakoonClusterNode * _arakoon_cluster_get_master(
    const ArakoonCluster * const cluster) {
        ASSERT_NON_NULL(cluster);

        if(cluster->master == NULL) {
                return NULL;
        }
        if(_arakoon_cluster_node_get_fd(cluster->master) < 0) {
                return NULL;
        }

        return cluster->master;
}

void _arakoon_cluster_reset_last_error(ArakoonCluster * const cluster) {
        FUNCTION_ENTER(_arakoon_cluster_reset_error);

        cluster->last_error.len = 0;

        arakoon_mem_free(cluster->last_error.data);
        cluster->last_error.data = NULL;
}

ArakoonProtocolVersion _arakoon_cluster_get_protocol_version(
    const ArakoonCluster * const cluster) {
        FUNCTION_ENTER(_arakoon_cluster_get_protocol_version);

        return cluster->version;
}
