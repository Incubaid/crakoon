#include "arakoon-nursery-routing.h"
#include "arakoon-utils.h"

struct ArakoonNurseryRouting {
};

static ArakoonNurseryRouting * _arakoon_nursery_routing_new(void)
    ARAKOON_GNUC_MALLOC ARAKOON_GNUC_WARN_UNUSED_RESULT;
static ArakoonNurseryRouting * _arakoon_nursery_routing_new(void) {
        return arakoon_mem_new(1, ArakoonNurseryRouting);
}

void _arakoon_nursery_routing_free(ArakoonNurseryRouting *routing) {
        RETURN_IF_NULL(routing);

        arakoon_mem_free(routing);
}

arakoon_rc _arakoon_nursery_routing_parse(size_t length,
        const void *data, ArakoonNurseryRouting **routing) {
        arakoon_rc rc = 0;
        ArakoonNurseryRouting *ret = NULL;

        *routing = NULL;

        ret = _arakoon_nursery_routing_new();

        *routing = ret;

        length -= length;
        data++;
        return rc;
}
