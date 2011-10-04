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
#include <stdlib.h>
#include <string.h>

#include "arakoon.h"
#include "memory.h"

#define ABORT_IF_NULL(p, msg)                      \
        do {                                       \
                if(p == NULL) {                    \
                        fprintf(stderr, msg "\n"); \
                        abort();                   \
                }                                  \
        } while(0)
#define ABORT_IF_NOT_SUCCESS(rc, msg)              \
        do {                                       \
                if(!ARAKOON_RC_IS_SUCCESS(rc)) {   \
                        fprintf(stderr, msg "\n"); \
                        abort();                   \
                }                                  \
        } while(0)

static void log_message(ArakoonLogLevel level ARAKOON_GNUC_UNUSED,
    const char * message) {
        fprintf(stderr, "[LOG] %s\n", message);
}

int main(int argc, char **argv) {
        ArakoonCluster *c = NULL;
        arakoon_rc rc = 0;
        ArakoonValueList *r0 = NULL, *r2 = NULL;
        ArakoonValueListIter *iter0 = NULL;
        ArakoonKeyValueList *r1 = NULL;
        ArakoonKeyValueListIter *iter1 = NULL;
        size_t l0 = 0, l1 = 0;
        const void *v0 = NULL, *v1 = NULL;
        char *s0 = NULL, *s1 = NULL;
        ArakoonSequence *seq = NULL;

        const ArakoonMemoryHooks hooks = {
                check_arakoon_malloc,
                check_arakoon_free,
                check_arakoon_realloc
        };

        if(argc < 5) {
                fprintf(stderr, "Usage: %s cluster name host port\n", argv[0]);
                return 1;
        }

        arakoon_memory_set_hooks(&hooks);
        arakoon_log_set_handler(log_message);

        c = arakoon_cluster_new(argv[1]);
        ABORT_IF_NULL(c, "arakoon_cluster_new");

        rc = arakoon_cluster_add_node_tcp(c, argv[2], argv[3], argv[4]);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_cluster_add_node_tcp");

        rc = arakoon_cluster_connect_master(c, NULL);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_cluster_connect_master");

        rc = arakoon_set(c, NULL, 3, "foo", 3, "bar");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_set");
        rc = arakoon_set(c, NULL, 4, "foo2", 4, "bar2");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_set");

        r0 = arakoon_value_list_new();
        rc = arakoon_value_list_add(r0, 3, "foo");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_value_list_add");
        rc = arakoon_value_list_add(r0, 4, "foo2");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_value_list_add");
        rc = arakoon_multi_get(c, NULL, r0, &r2);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_multi_get");
        arakoon_value_list_free(r0);
        iter0 = arakoon_value_list_create_iter(r2);
        ABORT_IF_NULL(iter0, "arakoon_value_list_create_iter");

        FOR_ARAKOON_VALUE_ITER(iter0, &l0, &v0) {
                s0 = check_arakoon_malloc((l0 + 1) * sizeof(char));
                ABORT_IF_NULL(s0, "check_arakoon_malloc");
                memcpy(s0, v0, l0);
                s0[l0] = 0;
                printf("Multi-get value: %s\n", s0);
                check_arakoon_free(s0);
        }

        arakoon_value_list_iter_free(iter0);
        arakoon_value_list_free(r2);

        rc = arakoon_prefix(c, NULL, 1, "f", -1, &r0);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_prefix");

        iter0 = arakoon_value_list_create_iter(r0);
        ABORT_IF_NULL(iter0, "arakoon_value_list_create_iter");

        FOR_ARAKOON_VALUE_ITER(iter0, &l0, &v0) {
                s0 = check_arakoon_malloc((l0 + 1) * sizeof(char));
                ABORT_IF_NULL(s0, "check_arakoon_malloc");
                memcpy(s0, v0, l0);
                s0[l0] = 0;
                printf("Prefix: %s\n", s0);
                check_arakoon_free(s0);
        }

        arakoon_value_list_iter_free(iter0);
        arakoon_value_list_free(r0);

        rc = arakoon_range_entries(c, NULL, 0, NULL, ARAKOON_BOOL_TRUE,
                0, NULL, ARAKOON_BOOL_TRUE, -1, &r1);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_range_entries");

        iter1 = arakoon_key_value_list_create_iter(r1);
        ABORT_IF_NULL(iter1, "arakoon_key_value_list_create_iter");

        FOR_ARAKOON_KEY_VALUE_ITER(iter1, &l0, &v0, &l1, &v1) {
                s0 = check_arakoon_malloc((l0 + 1) * sizeof(char));
                ABORT_IF_NULL(s0, "check_arakoon_malloc");
                s1 = check_arakoon_malloc((l1 + 1) * sizeof(char));
                ABORT_IF_NULL(s1, "check_arakoon_malloc");
                memcpy(s0, v0, l0);
                s0[l0] = 0;
                memcpy(s1, v1, l1);
                s1[l1] = 0;
                printf("Key: %s, value: %s\n", s0, s1);
                check_arakoon_free(s0);
                check_arakoon_free(s1);
        }

        arakoon_key_value_list_iter_free(iter1);
        arakoon_key_value_list_free(r1);

        seq = arakoon_sequence_new();
        rc = arakoon_sequence_add_set(seq, 3, "foo", 3, "baz");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence_add_set");
        rc = arakoon_sequence_add_set(seq, 3, "foz", 3, "bat");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence_add_set");
        rc = arakoon_sequence_add_test_and_set(seq, 3, "foz", 3, "bat", 3, "baz");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence_add_test_and_set");
        rc = arakoon_sequence_add_delete(seq, 3, "foz");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence_add_delete");
        rc = arakoon_sequence(c, NULL, seq);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence");
        arakoon_sequence_free(seq);

        arakoon_cluster_free(c);

        return 0;
}
