#include <stdio.h>
#include <stdlib.h>

#include <check.h>

#include "arakoon.h"
#include "memory.h"

#define SENTINEL (0xdeadbeef)

/*static void * verbose_malloc(size_t s) {
        void *ret = NULL;
        size_t len = 0;

        fprintf(stderr, "[MEM] malloc(%lu)\n", s);
        len = s + sizeof(size_t) + (2 * sizeof(uint32_t));
        ret = malloc(len);

        if(ret == NULL) {
                fprintf(stderr, "[MEM] malloc returned NULL\n");
                abort();
        }

        *((size_t *)ret) = s;
        *((uint32_t *)((char *)ret + sizeof(size_t))) = SENTINEL;
        *((uint32_t *)((char *)ret + len - sizeof(uint32_t))) = SENTINEL;

        return ((char *)ret + sizeof(size_t) + sizeof(uint32_t));
}

static void verbose_free(void *ptr) {
        size_t s = 0;
        uint32_t s0 = 0, s1 = 0;

        ptr = (char *)ptr - sizeof(size_t) - sizeof(uint32_t);

        s = *(size_t *)ptr;

        fprintf(stderr, "[MEM] free(%lu)\n", s);

        s0 = *((uint32_t *)((char *)ptr + sizeof(size_t)));
        s1 = *((uint32_t *)((char *)ptr + sizeof(size_t) + sizeof(uint32_t) + s));

        if(s0 != SENTINEL) {
                abort();
        }
        if(s1 != SENTINEL) {
                abort();
        }

        free(ptr);
}

static void * verbose_realloc(void *ptr, size_t s) {
        size_t s0 = 0;
        void *ret = NULL;

        ptr = (char *)ptr - sizeof(size_t) - sizeof(uint32_t);
        s0 = *(size_t *)ptr;

        fprintf(stderr, "[MEM] realloc(%lu, %lu)\n", s0, s);

        ret = realloc(ptr, s + sizeof(size_t) + (2 * sizeof(uint32_t)));
        if(ret == NULL) {
                fprintf(stderr, "[MEM] realloc returned NULL\n");
                abort();
        }

        *((size_t *)ret) = s;
        *((uint32_t *)((char *)ret + sizeof(size_t) + sizeof(uint32_t) + s)) = SENTINEL;

        return ((char *)ret + sizeof(size_t) + sizeof(uint32_t));
}*/

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

        c = arakoon_cluster_new("test");
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

int main(int argc, char **argv) {
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
