#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arakoon.h"
#include "memory.h"

#define ABORT_IF_NULL(p)         \
        do {                     \
                if(p == NULL) {  \
                        abort(); \
                }                \
        } while(0)
#define ABORT_IF_NOT_SUCCESS(rc)                 \
        do {                                     \
                if(!ARAKOON_RC_IS_SUCCESS(rc)) { \
                        abort();                 \
                }                                \
        } while(0)

int main(int argc, char **argv) {
        ArakoonCluster *c = NULL;
        arakoon_rc rc = 0;
        ArakoonValueList *r0 = NULL;
        ArakoonValueListIter *iter0 = NULL;
        ArakoonKeyValueList *r1 = NULL;
        ArakoonKeyValueListIter *iter1 = NULL;
        size_t l0 = 0, l1 = 0;
        const void *v0 = NULL, *v1 = NULL;
        char *s0 = NULL, *s1 = NULL;

        ArakoonMemoryHooks hooks = {
                check_arakoon_malloc,
                check_arakoon_free,
                check_arakoon_realloc
        };

        if(argc < 5) {
                fprintf(stderr, "Usage: %s name host port\n", argv[0]);
                return 1;
        }

        arakoon_memory_set_hooks(&hooks);

        c = arakoon_cluster_new(argv[1]);
        ABORT_IF_NULL(c);

        rc = arakoon_cluster_add_node_tcp(c, argv[2], argv[3], argv[4]);
        ABORT_IF_NOT_SUCCESS(rc);

        rc = arakoon_prefix(c, 1, "g", -1, &r0);
        ABORT_IF_NOT_SUCCESS(rc);

        iter0 = arakoon_value_list_create_iter(r0);
        ABORT_IF_NULL(iter0);

        FOR_ARAKOON_VALUE_ITER(iter0, &l0, &v0) {
                s0 = check_arakoon_malloc((l0 + 1) * sizeof(char));
                ABORT_IF_NULL(s0);
                memcpy(s0, v0, l0);
                s0[l0] = 0;
                printf("Prefix: %s\n", s0);
                check_arakoon_free(s0);
        }

        arakoon_value_list_iter_free(iter0);
        arakoon_value_list_free(r0);

        arakoon_cluster_free(c);

        return 0;
}
