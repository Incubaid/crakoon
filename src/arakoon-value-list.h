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

#ifndef __ARAKOON_VALUE_LIST_H__
#define __ARAKOON_VALUE_LIST_H__

#include "arakoon.h"

ARAKOON_BEGIN_DECLS

typedef struct ArakoonValueListItem ArakoonValueListItem;

arakoon_rc _arakoon_value_list_prepend(ArakoonValueList *list,
    const size_t value_size, const void * const value)
    ARAKOON_GNUC_NONNULL2(1, 3) ARAKOON_GNUC_WARN_UNUSED_RESULT;

ARAKOON_END_DECLS

#endif /* ifndef __ARAKOON_VALUE_LIST_H__ */
