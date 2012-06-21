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
#include <stdlib.h>
#include <string.h>

#include "arakoon.h"
#include "arakoon-nursery.h"

#include "memory.h"
#include "utils.h"

#define TEST_KEY ("nursery_foo")
#define TEST_KEY_LENGTH (strlen(TEST_KEY) - 1)
#define TEST_VALUE ("nursery_bar")
#define TEST_VALUE_LENGTH (strlen(TEST_VALUE) - 1)

int main(int argc, char **argv) {
        ArakoonCluster *keeper = NULL;
        ArakoonNursery *nursery = NULL;
        ArakoonClusterNode *node = NULL;
        arakoon_rc rc = 0;
        int i = 0;
        size_t value_length = 0;
        void *value = NULL;

        const ArakoonMemoryHooks hooks = {
                check_arakoon_malloc,
                check_arakoon_free,
                check_arakoon_realloc
        };

        if((argc - 2) % 3 != 0 || argc < 5) {
                fprintf(stderr, "Usage: %s cluster [name host port]*\n",
                        argv[0]);
                return 1;
        }

        arakoon_memory_set_hooks(&hooks);
        arakoon_log_set_handler(arakoon_log_get_stderr_handler());

        keeper = arakoon_cluster_new(ARAKOON_PROTOCOL_VERSION_1, argv[1]);
        ABORT_IF_NULL(keeper, "arakoon_cluster_new");

        for(i = 2; i < argc; i++) {
                node = arakoon_cluster_node_new(argv[i]);
                ABORT_IF_NULL(node, "arakoon_cluster_node_new");

                rc = arakoon_cluster_node_add_address_tcp(node, argv[i + 1],
                        argv[i + 2]);
                ABORT_IF_NOT_SUCCESS(rc,
                        "arakoon_cluster_node_add_address_tcp");

                rc = arakoon_cluster_add_node(keeper, node);
                ABORT_IF_NOT_SUCCESS(rc, "arakoon_cluster_add_node");
                i += 2;
        }

        rc = arakoon_cluster_connect_master(keeper, NULL);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_cluster_connect_master");

        nursery = arakoon_nursery_new(keeper);
        ABORT_IF_NULL(nursery, "arakoon_nursery_new");

        rc = arakoon_nursery_update_routing(nursery, NULL);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_nursery_update_config");

        rc = arakoon_nursery_set(nursery, NULL, TEST_KEY_LENGTH, TEST_KEY,
                TEST_VALUE_LENGTH, TEST_VALUE);

        if(rc != ARAKOON_RC_CLIENT_NOT_CONNECTED) {
                fprintf(stderr, "Unexpected rc: %s\n", arakoon_strerror(rc));
                abort();
        }

        rc = arakoon_nursery_reconnect_master(nursery, NULL, TEST_KEY_LENGTH,
                TEST_KEY);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_nursery_reconnect_master");

        rc = arakoon_nursery_set(nursery, NULL, TEST_KEY_LENGTH, TEST_KEY,
                TEST_VALUE_LENGTH, TEST_VALUE);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_nursery_set");

        rc = arakoon_nursery_get(nursery, NULL, TEST_KEY_LENGTH, TEST_KEY,
                &value_length, &value);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_nursery_get");

        if(value_length != TEST_VALUE_LENGTH) {
                fprintf(stderr, "Unexpected value length: %zu\n", value_length);
                abort();
        }
        if(memcmp(TEST_VALUE, value, value_length) != 0) {
                fprintf(stderr, "Unexpected value\n");
                abort();
        }

        check_arakoon_free(value);

        rc = arakoon_nursery_delete(nursery, NULL, TEST_KEY_LENGTH, TEST_KEY);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_nursery_delete");

        arakoon_nursery_free(nursery);
        arakoon_cluster_free(keeper);

        return 0;
}
