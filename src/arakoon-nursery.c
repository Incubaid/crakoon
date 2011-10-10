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

struct ArakoonNursery {
        const ArakoonCluster * keeper;
};

ArakoonNursery * arakoon_nursery_new(const ArakoonCluster * const keeper) {
        ArakoonNursery *ret = NULL;

        FUNCTION_ENTER(arakoon_nursery_new);

        ret = arakoon_mem_new(1, ArakoonNursery);
        RETURN_NULL_IF_NULL(ret);

        memset(ret, 0, sizeof(ArakoonNursery));

        ret->keeper = keeper;

        return ret;
}

void arakoon_nursery_free(ArakoonNursery *nursery) {
        RETURN_IF_NULL(nursery);

        arakoon_mem_free(nursery);
}

arakoon_rc arakoon_nursery_update_config(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClusterNode *master = NULL;
        int timeout = ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT;

        READ_OPTIONS;
        timeout = arakoon_client_call_options_get_timeout(options_);

        FUNCTION_ENTER(arakoon_nursery_update_config);

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

        return rc;
}
