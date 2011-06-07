#include <check.h>

#include "arakoon.h"
#include "memory.h"

START_TEST(test_memory_malloc) {
        void *d = check_arakoon_malloc(1024);
        check_arakoon_free(d);
} END_TEST

START_TEST(test_memory_last_malloc_address) {
        void *d = check_arakoon_malloc(1024);

        fail_unless(check_arakoon_last_malloc_address() == d);

        check_arakoon_free(d);
} END_TEST

START_TEST(test_memory_free_null) {
        check_arakoon_free(NULL);
} END_TEST

START_TEST(test_memory_last_free_address) {
        void *d = check_arakoon_malloc(1024);
        check_arakoon_free(d);

        fail_unless(check_arakoon_last_free_address() == d);

        check_arakoon_free(NULL);

        fail_unless(check_arakoon_last_free_address() == NULL);
} END_TEST

START_TEST(test_memory_realloc) {
        void *d = check_arakoon_malloc(1024);

        d = check_arakoon_realloc(d, 2048);
        d = check_arakoon_realloc(d, 2048);
        d = check_arakoon_realloc(d, 1024);

        check_arakoon_free(d);
} END_TEST

START_TEST(test_memory_realloc_null) {
        void *d = check_arakoon_realloc(NULL, 1024);

        fail_unless(d != NULL);

        check_arakoon_free(d);
} END_TEST

START_TEST(test_memory_realloc_zero) {
        void *d = NULL;
        
        d = check_arakoon_malloc(1024);
        d = check_arakoon_realloc(d, 0);

        fail_unless(d == NULL);

} END_TEST

static Suite * memory_suite() {
        TCase *c = NULL;
        Suite *s = NULL;

        s = suite_create("Memory");

        c = tcase_create("malloc");
        tcase_add_test(c, test_memory_malloc);
        tcase_add_test(c, test_memory_last_malloc_address);
        suite_add_tcase(s, c);

        c = tcase_create("free");
        tcase_add_test(c, test_memory_free_null);
        tcase_add_test(c, test_memory_last_free_address);
        suite_add_tcase(s, c);

        c = tcase_create("realloc");
        tcase_add_test(c, test_memory_realloc);
        tcase_add_test(c, test_memory_realloc_null);
        tcase_add_test(c, test_memory_realloc_zero);
        suite_add_tcase(s, c);

        return s;
}

int main(int argc ARAKOON_GNUC_UNUSED, char **argv ARAKOON_GNUC_UNUSED) {
        int failures = 0;
        Suite *s = NULL;
        SRunner *r = NULL;

        s = memory_suite();
        r = srunner_create(s);

        srunner_run_all(r, CK_VERBOSE);
        failures = srunner_ntests_failed(r);

        srunner_free(r);

        return (failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
