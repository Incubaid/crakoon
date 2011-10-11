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

#ifndef __ARAKOON_NURSERY_H__
#define __ARAKOON_NURSERY_H__

#include "arakoon.h"

ARAKOON_BEGIN_DECLS

/* ArakoonNursery */
typedef struct ArakoonNursery ArakoonNursery;

/* Allocate a new ArakoonNursery, to be released using arakoon_nursery_free
 *
 * The given cluster will not be copied, and should not be released until the
 * nursery is released. A call to arakoon_nursery_free will not release the
 * given cluster.
 */
ArakoonNursery * arakoon_nursery_new(const ArakoonCluster * const keeper)
    ARAKOON_GNUC_NONNULL ARAKOON_GNUC_MALLOC
    ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Release an ArakoonNursery
 *
 * This will also close any open connections.
 */
void arakoon_nursery_free(ArakoonNursery *nursery);

/* Update nursery config */
arakoon_rc arakoon_nursery_update_routing(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options)
    ARAKOON_GNUC_NONNULL1(1) ARAKOON_GNUC_WARN_UNUSED_RESULT;

/* Force a reconnect to the master serving a given key */
arakoon_rc arakoon_nursery_reconnect_master(const ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key)
    ARAKOON_GNUC_NONNULL2(1, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;

/* Actions */
/* Send a 'get' call to the nursery
 *
 * The resulting value will be stored at 'result', and its length at
 * 'result_size'. 'result' should be released by the caller when no longer
 * required.
 */
arakoon_rc arakoon_nursery_get(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    size_t *result_size, void **result)
    ARAKOON_GNUC_NONNULL4(1, 4, 5, 6) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'set' call to the nursery */
arakoon_rc arakoon_nursery_set(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_NONNULL3(1, 4, 6) ARAKOON_GNUC_WARN_UNUSED_RESULT;
/* Send a 'delete' call to the nursery */
arakoon_rc arakoon_nursery_delete(ArakoonNursery *nursery,
    const ArakoonClientCallOptions * const options,
    const size_t key_size, const void * const key)
    ARAKOON_GNUC_NONNULL2(1, 4) ARAKOON_GNUC_WARN_UNUSED_RESULT;

ARAKOON_END_DECLS

#endif /* ifndef __ARAKOON_NURSERY_H__ */
