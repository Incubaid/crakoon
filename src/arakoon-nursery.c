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
