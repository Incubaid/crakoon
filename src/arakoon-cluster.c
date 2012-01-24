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

#include "arakoon-cluster.h"
#include "arakoon-utils.h"

struct ArakoonCluster {
        char * name;

        ArakoonClusterNode * nodes;
        ArakoonClusterNode * master;
};

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
        ArakoonClusterNode *node = NULL;
        ArakoonClusterNode *next_node = NULL;

        FUNCTION_ENTER(arakoon_cluster_free);

        RETURN_IF_NULL(cluster);

        arakoon_mem_free(cluster->name);

        node = cluster->nodes;
        while(node != NULL) {
                next_node = _arakoon_cluster_node_get_next(node);
                _arakoon_cluster_node_disconnect(node);
                _arakoon_cluster_node_free(node);
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
                        break;
                }

                node = _arakoon_cluster_node_get_next(node);
        }

        if(node == NULL) {
                _arakoon_log_warning("Unable to connect to any node");
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }

        /* Retrieve master, according to the node */
        rc = _arakoon_cluster_node_who_master(node, &timeout, &master);
        RETURN_IF_NOT_SUCCESS(rc);

        if(strcmp(_arakoon_cluster_node_get_name(node), master) == 0) {
                /* The node is master */
                cluster->master = node;
                arakoon_mem_free(master);

                _arakoon_log_info("Found master node %s",
                        _arakoon_cluster_node_get_name(_arakoon_cluster_get_master(cluster)));

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
        }
        else {
                rc = ARAKOON_RC_SUCCESS;
                cluster->master = node;
        }

        arakoon_mem_free(master);

        _arakoon_log_debug("Found master node %s",
                _arakoon_cluster_node_get_name(_arakoon_cluster_get_master(cluster)));

        return rc;
}

const char * arakoon_cluster_get_name(const ArakoonCluster * const cluster) {
        FUNCTION_ENTER(arakoon_cluster_get_name);

        return cluster->name;
}

arakoon_rc arakoon_cluster_add_node(ArakoonCluster *cluster,
    const char * const name, struct addrinfo * const address) {
        ArakoonClusterNode *node = NULL;

        FUNCTION_ENTER(arakoon_cluster_add_node);

        _arakoon_log_debug("Adding node %s to cluster %s", name, cluster->name);

        node = _arakoon_cluster_node_new(name, cluster, address);
        RETURN_ENOMEM_IF_NULL(node);

        _arakoon_cluster_node_set_next(node, cluster->nodes);
        cluster->nodes = node;

        return ARAKOON_RC_SUCCESS;
}

arakoon_rc arakoon_cluster_add_node_tcp(ArakoonCluster *cluster,
    const char * const name, const char * const host, const char * const service) {
        struct addrinfo hints;
        struct addrinfo *result;
        int rc = 0;

        FUNCTION_ENTER(arakoon_cluster_add_node_tcp);

        _arakoon_log_debug("Looking up node %s at %s:%s", name, host, service);

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC; /* IPv4 and IPv6, whatever */
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0; /* Any protocol */

        rc = getaddrinfo(host, service, &hints, &result);

        if(rc != 0) {
                _arakoon_log_error("Address lookup failed: %s",
                        gai_strerror(rc));
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }

        rc = arakoon_cluster_add_node(cluster, name, result);

        return rc;
}

ArakoonClusterNode * _arakoon_cluster_get_master(
    const ArakoonCluster * const cluster) {
        if(cluster->master == NULL) {
                return NULL;
        }
        if(_arakoon_cluster_node_get_fd(cluster->master) < 0) {
                return NULL;
        }

        return cluster->master;
}