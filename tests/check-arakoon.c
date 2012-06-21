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

#include <check.h>

#include "arakoon.h"
#include "memory.h"

#define SENTINEL (0xdeadbeef)

#define S "abcdef"
#define L (strlen(S))

START_TEST(test_arakoon_utils_make_string) {

        void *d = NULL;
        char *s = NULL;
        const char *t = S;

        d = malloc((L + 1) * sizeof(char));
        memcpy(d, t, L);
        ((char *)d)[L] = 'a';

        s = arakoon_utils_make_string(d, L);

        fail_unless(s[L] == 0, NULL);
        fail_unless(strcmp(s, t) == 0, NULL);

        free(s);

} END_TEST

START_TEST(test_arakoon_utils_make_string_frees_on_realloc_error) {
        const ArakoonMemoryHooks hooks = {
                NULL,
                check_arakoon_free,
                check_arakoon_realloc_null
        };

        void *d = NULL;
        char *s = NULL;
        const char *t = S;

        arakoon_memory_set_hooks(&hooks);

        d = check_arakoon_malloc((L + 1) * sizeof(char));
        memcpy(d, t, L);
        ((char *)d)[L] = 'a';

        s = arakoon_utils_make_string(d, L);

        fail_unless(s == NULL, NULL);
        fail_unless(check_arakoon_last_free_address() == d, NULL);

} END_TEST
#undef S
#undef L

START_TEST(test_arakoon_memory_set_hooks) {
        const ArakoonMemoryHooks hooks = {
                check_arakoon_malloc,
                check_arakoon_free,
                check_arakoon_realloc
        };

        arakoon_memory_set_hooks(&hooks);
} END_TEST

START_TEST(test_arakoon_memory_set_hooks_usage) {
        const ArakoonMemoryHooks hooks = {
                check_arakoon_malloc,
                check_arakoon_free,
                /* realloc is tested in
                 * test_arakoon_utils_make_string_frees_on_realloc_error
                 * */
                NULL
        };

        ArakoonCluster *c = NULL;

        arakoon_memory_set_hooks(&hooks);

        c = arakoon_cluster_new(ARAKOON_PROTOCOL_VERSION_1, "test");
        fail_unless(check_arakoon_last_malloc_address() ==
                arakoon_cluster_get_name(c), NULL);

        arakoon_cluster_free(c);
        fail_unless(check_arakoon_last_free_address() == c, NULL);

} END_TEST

static Suite * arakoon_suite() {
        TCase *c = NULL;
        Suite *s = NULL;

        s = suite_create("Arakoon");

        c = tcase_create("arakoon_memory");
        tcase_add_test(c, test_arakoon_memory_set_hooks);
        tcase_add_test(c, test_arakoon_memory_set_hooks_usage);
        suite_add_tcase(s, c);

        c = tcase_create("arakoon_utils");
        tcase_add_test(c, test_arakoon_utils_make_string);
        tcase_add_test(c, test_arakoon_utils_make_string_frees_on_realloc_error);
        suite_add_tcase(s, c);

        return s;
}

int main(int argc ARAKOON_GNUC_UNUSED, char **argv ARAKOON_GNUC_UNUSED) {
        int failures = 0;
        Suite *s = NULL;
        SRunner *r = NULL;

        s = arakoon_suite();
        r = srunner_create(s);

        srunner_run_all(r, CK_VERBOSE);
        failures = srunner_ntests_failed(r);

        srunner_free(r);

        return (failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
