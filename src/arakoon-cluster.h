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

#ifndef __ARAKOON_CLUSTER_H__
#define __ARAKOON_CLUSTER_H__

#include "arakoon.h"
#include "arakoon-cluster-node.h"

ARAKOON_BEGIN_DECLS

ArakoonClusterNode * _arakoon_cluster_get_master(const ArakoonCluster * const cluster);

#define ARAKOON_CLUSTER_GET_MASTER(c, m)                \
        STMT_START                                      \
        m = _arakoon_cluster_get_master(c);             \
        if(m == NULL) {                                 \
                return ARAKOON_RC_CLIENT_NOT_CONNECTED; \
        }                                               \
        STMT_END

ARAKOON_END_DECLS

#endif /* ifndef __ARAKOON_CLUSTER_H__ */
