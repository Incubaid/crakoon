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

#ifndef __ARAKOON_CLUSTER_NODE_H__
#define __ARAKOON_CLUSTER_NODE_H__

#include "arakoon.h"

ARAKOON_BEGIN_DECLS

typedef struct ArakoonClusterNode ArakoonClusterNode;

ArakoonClusterNode * _arakoon_cluster_node_new(const char * name,
    const ArakoonCluster * const cluster, struct addrinfo * address)
    ARAKOON_GNUC_NONNULL3(1, 2, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT
    ARAKOON_GNUC_MALLOC;
void _arakoon_cluster_node_free(ArakoonClusterNode *node);

arakoon_rc _arakoon_cluster_node_connect(ArakoonClusterNode *node,
    int *timeout)
    ARAKOON_GNUC_NONNULL1(1) ARAKOON_GNUC_WARN_UNUSED_RESULT;
void _arakoon_cluster_node_disconnect(ArakoonClusterNode *node)
    ARAKOON_GNUC_NONNULL;

arakoon_rc _arakoon_cluster_node_who_master(ArakoonClusterNode *node,
    int *timeout, char ** const master)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;

arakoon_rc _arakoon_cluster_node_write_bytes(ArakoonClusterNode *node,
    size_t len, void *data, int *timeout)
    ARAKOON_GNUC_NONNULL3(1, 3, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;

int _arakoon_cluster_node_get_fd(const ArakoonClusterNode * const node)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_WARN_UNUSED_RESULT;
const char * _arakoon_cluster_node_get_name(
    const ArakoonClusterNode * const node)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_PURE;
ArakoonClusterNode * _arakoon_cluster_node_get_next(
    const ArakoonClusterNode * const node)
    ARAKOON_GNUC_NONNULL;
void _arakoon_cluster_node_set_next(ArakoonClusterNode *node,
    ArakoonClusterNode *next)
    ARAKOON_GNUC_NONNULL;


ARAKOON_END_DECLS

#endif /* ifndef __ARAKOON_CLUSTER_NODE_H__ */
