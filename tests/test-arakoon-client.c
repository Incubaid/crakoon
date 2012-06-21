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
#include "memory.h"
#include "utils.h"

typedef struct Node Node;
struct Node {
        const char *name;
        ArakoonClusterNode *node;
        Node *next;
};

int main(int argc, char **argv) {
        ArakoonCluster *c = NULL;
        arakoon_rc rc = 0;
        ArakoonClientCallOptions *options = NULL;
        ArakoonValueList *r0 = NULL, *r2 = NULL;
        ArakoonValueListIter *iter0 = NULL;
        ArakoonKeyValueList *r1 = NULL;
        ArakoonKeyValueListIter *iter1 = NULL;
        size_t l0 = 0, l1 = 0;
        void *d0 = NULL;
        const void *v0 = NULL, *v1 = NULL;
        char *s0 = NULL, *s1 = NULL;
        ArakoonSequence *seq = NULL;
        int i = 0;

        Node *fst = NULL, *n = NULL, *the_node = NULL;
        const char *name = NULL;

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

        printf("Working with crakoon version %u.%u.%u (%s)\n",
                arakoon_library_version_major(),
                arakoon_library_version_minor(),
                arakoon_library_version_micro(),
                arakoon_library_version_info());

        arakoon_memory_set_hooks(&hooks);
        arakoon_log_set_handler(arakoon_log_get_stderr_handler());

        options = arakoon_client_call_options_new();
        arakoon_client_call_options_set_timeout(options, 400);

        c = arakoon_cluster_new(ARAKOON_PROTOCOL_VERSION_1, argv[1]);
        ABORT_IF_NULL(c, "arakoon_cluster_new");

        for(i = 2; i < argc; i++) {
                the_node = NULL;
                name = argv[i];

                for(n = fst; n != NULL;) {
                        if(strcmp(n->name, name) == 0) {
                                the_node = n;
                                break;
                        }

                        n = n->next;
                }

                if(the_node == NULL) {
                        the_node = (Node *)check_arakoon_malloc(sizeof(Node));
                        ABORT_IF_NULL(the_node, "Allocate Node");

                        the_node->node = arakoon_cluster_node_new(name);
                        ABORT_IF_NULL(the_node->node,
                                "arakoon_cluster_node_new");
                        the_node->name = name;

                        the_node->next = fst;
                        fst = the_node;
                }

                rc = arakoon_cluster_node_add_address_tcp(the_node->node,
                        argv[i + 1], argv[i + 2]);
                ABORT_IF_NOT_SUCCESS(rc,
                        "arakoon_cluster_node_add_address_tcp");

                i += 2;
        }

        for(n = fst; n != NULL;) {
                rc = arakoon_cluster_add_node(c, n->node);
                ABORT_IF_NOT_SUCCESS(rc, "arakoon_cluster_add_node");

                the_node = n;
                n = n->next;
                check_arakoon_free(the_node);
        }

        rc = arakoon_cluster_connect_master(c, options);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_cluster_connect_master");

        rc = arakoon_get(c, NULL, 3, "foo", &l0, &d0);
        if(rc != ARAKOON_RC_NOT_FOUND) {
                fprintf(stderr, "Unset value found\n");
                abort();
        }

        rc = arakoon_set(c, NULL, 3, "foo", 3, "bar");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_set");
        rc = arakoon_set(c, options, 4, "foo2", 4, "bar2");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_set");
        rc = arakoon_set(c, NULL, 7, "testkey", 9, "testvalue");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_set");

        r0 = arakoon_value_list_new();
        rc = arakoon_value_list_add(r0, 3, "foo");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_value_list_add");
        rc = arakoon_value_list_add(r0, 4, "foo2");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_value_list_add");
        rc = arakoon_multi_get(c, options, r0, &r2);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_multi_get");
        arakoon_value_list_free(r0);
        iter0 = arakoon_value_list_create_iter(r2);
        ABORT_IF_NULL(iter0, "arakoon_value_list_create_iter");

        FOR_ARAKOON_VALUE_ITER(iter0, &l0, &v0) {
                s0 = (char *)check_arakoon_malloc((l0 + 1) * sizeof(char));
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
                s0 = (char *)check_arakoon_malloc((l0 + 1) * sizeof(char));
                ABORT_IF_NULL(s0, "check_arakoon_malloc");
                memcpy(s0, v0, l0);
                s0[l0] = 0;
                printf("Prefix: %s\n", s0);
                check_arakoon_free(s0);
        }

        arakoon_value_list_iter_free(iter0);
        arakoon_value_list_free(r0);

        rc = arakoon_range_entries(c, options, 0, NULL, ARAKOON_BOOL_TRUE,
                0, NULL, ARAKOON_BOOL_TRUE, -1, &r1);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_range_entries");

        iter1 = arakoon_key_value_list_create_iter(r1);
        ABORT_IF_NULL(iter1, "arakoon_key_value_list_create_iter");

        FOR_ARAKOON_KEY_VALUE_ITER(iter1, &l0, &v0, &l1, &v1) {
                s0 = (char *)check_arakoon_malloc((l0 + 1) * sizeof(char));
                ABORT_IF_NULL(s0, "check_arakoon_malloc");
                s1 = (char *)check_arakoon_malloc((l1 + 1) * sizeof(char));
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
        rc = arakoon_sequence_add_assert(seq, 3, "foo", 3, "baz");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence_add_assert");
        rc = arakoon_sequence_add_delete(seq, 3, "foz");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence_add_delete");
        rc = arakoon_sequence_add_assert(seq, 3, "foz", 0, NULL);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence_add_assert");
        rc = arakoon_sequence(c, NULL, seq);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence");
        arakoon_sequence_free(seq);

        seq = arakoon_sequence_new();
        rc = arakoon_sequence_add_assert(seq, 4, "fail", 1, "a");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_sequence_add_assert");
        rc = arakoon_sequence(c, NULL, seq);
        if(rc != ARAKOON_RC_ASSERTION_FAILED) {
                fprintf(stderr, "Assertion didn't fail: %s\n", arakoon_strerror(rc));
                abort();
        }
        arakoon_sequence_free(seq);

        rc = arakoon_assert(c, NULL, 11, "assert_test", 0, NULL);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_assert");
        rc = arakoon_assert(c, NULL, 11, "assert_test", 3, "foo");
        if(rc != ARAKOON_RC_ASSERTION_FAILED) {
                fprintf(stderr, "Assertion didn't fail: %s\n", arakoon_strerror(rc));
                abort();
        }
        rc = arakoon_set(c, NULL, 11, "assert_test", 3, "foo");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_set");
        rc = arakoon_assert(c, NULL, 11, "assert_test", 0, NULL);
        if(rc != ARAKOON_RC_ASSERTION_FAILED) {
                fprintf(stderr, "Assertion didn't fail: %s\n", arakoon_strerror(rc));
                abort();
        }
        rc = arakoon_assert(c, NULL, 11, "assert_test", 3, "foo");
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_assert");


        rc = arakoon_rev_range_entries(c, options, 0, NULL, ARAKOON_BOOL_TRUE,
                0, NULL, ARAKOON_BOOL_TRUE, -1, &r1);
        ABORT_IF_NOT_SUCCESS(rc, "arakoon_rev_range_entries");

        iter1 = arakoon_key_value_list_create_iter(r1);
        ABORT_IF_NULL(iter1, "arakoon_key_value_list_create_iter");

        FOR_ARAKOON_KEY_VALUE_ITER(iter1, &l0, &v0, &l1, &v1) {
                s0 = (char *)check_arakoon_malloc((l0 + 1) * sizeof(char));
                ABORT_IF_NULL(s0, "check_arakoon_malloc");
                s1 = (char *)check_arakoon_malloc((l1 + 1) * sizeof(char));
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


        arakoon_client_call_options_free(options);
        arakoon_cluster_free(c);

        return 0;
}
