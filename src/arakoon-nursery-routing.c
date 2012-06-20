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

#include "arakoon.h"
#include "arakoon-nursery-routing.h"
#include "arakoon-utils.h"

typedef struct ArakoonNurseryRoutingNode ArakoonNurseryRoutingNode;
struct ArakoonNurseryRouting {
        ArakoonNurseryRoutingNode *root;
        /* Null-terminated array of clusters */
        ArakoonCluster **clusters;
};

static ArakoonNurseryRouting * _arakoon_nursery_routing_new(void)
    ARAKOON_GNUC_MALLOC ARAKOON_GNUC_WARN_UNUSED_RESULT;
static ArakoonNurseryRouting * _arakoon_nursery_routing_new(void) {
        return arakoon_mem_new(1, ArakoonNurseryRouting);
}

typedef enum {
        ARAKOON_NURSERY_ROUTING_NODE_LEAF,
        ARAKOON_NURSERY_ROUTING_NODE_INTERNAL
} ArakoonNurseryRoutingNodeType;

typedef struct ArakoonNurseryRoutingLeafNode ArakoonNurseryRoutingLeafNode;
typedef struct ArakoonNurseryRoutingInternalNode ArakoonNurseryRoutingInternalNode;

struct ArakoonNurseryRoutingLeafNode {
        char *cluster;
};
struct ArakoonNurseryRoutingInternalNode {
        ArakoonNurseryRoutingNode *left;
        ArakoonNurseryRoutingNode *right;
        char *boundary;
};

struct ArakoonNurseryRoutingNode {
        ArakoonNurseryRoutingNodeType type;

        union {
                ArakoonNurseryRoutingLeafNode leaf;
                ArakoonNurseryRoutingInternalNode internal;
        } node;
};

static void arakoon_nursery_routing_node_free(ArakoonNurseryRoutingNode *node);
static ArakoonNurseryRoutingNode * arakoon_nursery_routing_parse_node(
    const void **data);
static arakoon_rc arakoon_nursery_routing_parse_clusters(
    const void **data, ArakoonCluster ***result);

void _arakoon_nursery_routing_free(ArakoonNurseryRouting *routing) {
        int i = 0;

        RETURN_IF_NULL(routing);

        arakoon_nursery_routing_node_free(routing->root);

        for(i = 0; routing->clusters[i] != NULL; i++) {
                arakoon_cluster_free(routing->clusters[i]);
        }
        arakoon_mem_free(routing->clusters);

        arakoon_mem_free(routing);
}

arakoon_rc _arakoon_nursery_routing_parse(size_t length ARAKOON_GNUC_UNUSED,
        const void *data, ArakoonNurseryRouting **routing) {
        arakoon_rc rc = 0;
        ArakoonNurseryRouting *ret = NULL;
        ArakoonNurseryRoutingNode *root = NULL;
        ArakoonCluster **clusters = NULL;
        const void *iter = NULL;
        int i = 0;

        *routing = NULL;

        iter = data;

        root = arakoon_nursery_routing_parse_node(&iter);
        if(root == NULL) {
                return ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING;
        }

        rc = arakoon_nursery_routing_parse_clusters(&iter, &clusters);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                if(*clusters == NULL) {
                        for(i = 0; clusters[i] != NULL; i++) {
                                arakoon_cluster_free(clusters[i]);
                        }
                        arakoon_mem_free(clusters);
                }

                return rc;
        }
        if(clusters == NULL) {
                arakoon_nursery_routing_node_free(root);
                return ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING;
        }

        if(iter != (char *)data + length) {
                arakoon_nursery_routing_node_free(root);

                for(i = 0; clusters[i] != NULL; i++) {
                        arakoon_cluster_free(clusters[i]);
                }
                arakoon_mem_free(clusters);

                return ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING;
        }

        ret = _arakoon_nursery_routing_new();
        ret->root = root;
        ret->clusters = clusters;

        *routing = ret;

        return rc;
}

static ArakoonNurseryRoutingNode * arakoon_nursery_routing_node_new(
    ArakoonNurseryRoutingNodeType type)
    ARAKOON_GNUC_MALLOC ARAKOON_GNUC_WARN_UNUSED_RESULT;
static ArakoonNurseryRoutingNode * arakoon_nursery_routing_node_new(
    ArakoonNurseryRoutingNodeType type) {
        ArakoonNurseryRoutingNode * node = arakoon_mem_new(1,
                ArakoonNurseryRoutingNode);
        memset(node, 0, sizeof(ArakoonNurseryRoutingNode));

        node->type = type;

        return node;
}

static void arakoon_nursery_routing_node_free(ArakoonNurseryRoutingNode *node) {
        RETURN_IF_NULL(node);

        switch(node->type) {
                case ARAKOON_NURSERY_ROUTING_NODE_LEAF:
                        arakoon_mem_free(node->node.leaf.cluster);
                        break;
                case ARAKOON_NURSERY_ROUTING_NODE_INTERNAL:
                        {
                                arakoon_nursery_routing_node_free(
                                                node->node.internal.left);
                                arakoon_nursery_routing_node_free(
                                                node->node.internal.right);
                                arakoon_mem_free(node->node.internal.boundary);
                        }
                        break;

                default:
                        abort(); /* Or return? */
        }

        arakoon_mem_free(node);
}

/* TODO Move into protocol */
static arakoon_bool _read_bool(const void **data) {
        const char *b = (char *)*data;

        *(char **)data += 1;

        if(*b == ARAKOON_BOOL_FALSE) {
                return ARAKOON_BOOL_FALSE;
        }
        else {
                return ARAKOON_BOOL_TRUE;
        }
}

static uint32_t _read_uint32(const void **data) {
        uint32_t res = 0;

        res = *(uint32_t *)(*data);
        *data = (char *)(*data) + sizeof(uint32_t);

        return res;
}

static char * _read_string(const void **data) {
        uint32_t length = 0;
        char *ret = NULL;

        length = _read_uint32(data);

        ret = arakoon_mem_new(length + 1, char);
        memcpy(ret, *data, length);
        ret[length] = 0;

        *(char **)data += length;

        return ret;
}

static ArakoonNurseryRoutingNode * arakoon_nursery_routing_parse_node(
    const void **data) {
        ArakoonNurseryRoutingNode *node = NULL;
        arakoon_bool is_leaf = ARAKOON_BOOL_FALSE;

        is_leaf = _read_bool(data);
        if(is_leaf) {
                node = arakoon_nursery_routing_node_new(
                        ARAKOON_NURSERY_ROUTING_NODE_LEAF);
                node->node.leaf.cluster = _read_string(data);
        }
        else {
                node = arakoon_nursery_routing_node_new(
                        ARAKOON_NURSERY_ROUTING_NODE_INTERNAL);
                node->node.internal.boundary = _read_string(data);

                node->node.internal.left = arakoon_nursery_routing_parse_node(data);
                if(node->node.internal.left == NULL) {
                        arakoon_nursery_routing_node_free(node);
                        return NULL;
                }

                node->node.internal.right = arakoon_nursery_routing_parse_node(
                        data);

                if(node->node.internal.right == NULL) {
                        arakoon_nursery_routing_node_free(node);
                        return NULL;
                }
        }

        return node;
}

#define UINT32_MAX_LENGTH (10)
static arakoon_rc arakoon_nursery_routing_parse_clusters(
    const void **data, ArakoonCluster *** result) {
        uint32_t count = 0;
        ArakoonCluster **ret = NULL;
        ArakoonClusterNode *node = NULL;
        char *cluster_id = NULL;
        uint32_t cluster_size = 0;
        char *node_id = NULL, *ip = NULL;
        char service[UINT32_MAX_LENGTH + 1];
        uint32_t port = 0;
        ArakoonCluster *cluster = NULL;
        uint32_t i = 0, j = 0;
        arakoon_rc rc = 0;

        count = _read_uint32(data);

        ret = arakoon_mem_new(count + 1, ArakoonCluster *);
        ret[count] = NULL;

        for(i = 0; i < count; i++) {
                cluster_id = _read_string(data);
                cluster_size = _read_uint32(data);

                cluster = arakoon_cluster_new(cluster_id);
                arakoon_mem_free(cluster_id);

                ret[i] = cluster;

                for(j = 0; j < cluster_size; j++) {
                        node_id = _read_string(data);
                        ip = _read_string(data);
                        port = _read_uint32(data);

                        snprintf(service, sizeof(service), "%u", port);

                        node = arakoon_cluster_node_new(node_id);
                        if(node == NULL) {
                                goto failure;
                        }

                        rc = arakoon_cluster_node_add_address_tcp(node, ip,
                                service);

                        arakoon_mem_free(node_id);
                        arakoon_mem_free(ip);

                        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                                arakoon_cluster_node_free(node);
                                goto failure;
                        }

                        rc = arakoon_cluster_add_node(cluster, node);

                        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                                goto failure;
                        }
                }
        }

        *result = ret;

        return rc;

failure:
        for(i = 0; ret[i] != NULL; i++) {
                arakoon_cluster_free(ret[i]);
        }

        arakoon_mem_free(ret);

        return rc;
}

static const char * _arakoon_nursery_routing_node_lookup(
    const ArakoonNurseryRoutingNode * node, size_t key_size, const void *key) {
        switch(node->type) {
                case ARAKOON_NURSERY_ROUTING_NODE_LEAF:
                        return node->node.leaf.cluster;
                        break;

                case ARAKOON_NURSERY_ROUTING_NODE_INTERNAL:
                        {
                                if(memcmp(key, node->node.internal.boundary, key_size) < 0) {
                                        return _arakoon_nursery_routing_node_lookup(
                                                node->node.internal.left, key_size, key);
                                }
                                else {
                                        return _arakoon_nursery_routing_node_lookup(
                                                node->node.internal.right, key_size, key);
                                }
                        }
                        break;

                default:
                        return NULL;
        }
}

ArakoonCluster * _arakoon_nursery_routing_lookup(
    const ArakoonNurseryRouting *routing, size_t key_size, const void *key) {
        const char * cluster_name = NULL;
        int i = 0;

        cluster_name = _arakoon_nursery_routing_node_lookup(routing->root, key_size, key);

        if(cluster_name == NULL) {
                return NULL;
        }

        for(i = 0; routing->clusters[i] != NULL; i++) {
                if(strcmp(arakoon_cluster_get_name(routing->clusters[i]), cluster_name) == 0) {
                        return routing->clusters[i];
                }
        }

        return NULL;
}
