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

#include "arakoon-key-value-list.h"
#include "arakoon-utils.h"

typedef struct ArakoonKeyValueListItem ArakoonKeyValueListItem;
struct ArakoonKeyValueListItem {
        ArakoonKeyValueListItem *next;

        size_t key_size;
        void * key;
        size_t value_size;
        void * value;
};

struct ArakoonKeyValueList {
        ssize_t size;
        ArakoonKeyValueListItem *first;
};

struct ArakoonKeyValueListIter {
        const ArakoonKeyValueList *list;
        ArakoonKeyValueListItem *current;
};

ArakoonKeyValueList * _arakoon_key_value_list_new(void) {
        ArakoonKeyValueList *list = NULL;

        list = arakoon_mem_new(1, ArakoonKeyValueList);
        RETURN_NULL_IF_NULL(list);

        list->size = 0;
        list->first = NULL;

        return list;
}

arakoon_rc _arakoon_key_value_list_prepend(ArakoonKeyValueList *list,
    const size_t key_size, const void * const key,
    const size_t value_size, const void * const value) {
        ArakoonKeyValueListItem *item = NULL;

        item = arakoon_mem_new(1, ArakoonKeyValueListItem);
        RETURN_ENOMEM_IF_NULL(item);

        item->key = arakoon_mem_malloc(key_size);
        if(item->key == NULL) {
                arakoon_mem_free(item);
                return -ENOMEM;
        }

        item->value = arakoon_mem_malloc(value_size);
        if(item->value == NULL) {
                arakoon_mem_free(item->key);
                arakoon_mem_free(item);
                return -ENOMEM;
        }

        item->next = list->first;
        item->key_size = key_size;
        memcpy(item->key, key, key_size);
        item->value_size = value_size;
        memcpy(item->value, value, value_size);

        list->first = item;
        list->size = list->size + 1;

        return ARAKOON_RC_SUCCESS;
}

ssize_t arakoon_key_value_list_size(const ArakoonKeyValueList * const list) {
        FUNCTION_ENTER(arakoon_key_value_list_size);

        return list->size;
}

static void arakoon_key_value_list_item_free(
    ArakoonKeyValueListItem * const item) {
        FUNCTION_ENTER(arakoon_key_value_list_item_free);

        RETURN_IF_NULL(item);

        item->key_size = 0;
        arakoon_mem_free(item->key);
        item->value_size = 0;
        arakoon_mem_free(item->value);

        arakoon_mem_free(item);
}

void arakoon_key_value_list_free(ArakoonKeyValueList * const list) {
        ArakoonKeyValueListItem *item = NULL, *olditem = NULL;

        FUNCTION_ENTER(arakoon_key_value_list_free);

        RETURN_IF_NULL(list);

        list->size = 0;
        item = list->first;
        list->first = NULL;

        while(item != NULL) {
                olditem = item;
                item = olditem->next;

                arakoon_key_value_list_item_free(olditem);
        }

        arakoon_mem_free(list);
}

ArakoonKeyValueListIter * arakoon_key_value_list_create_iter(
    const ArakoonKeyValueList * const list) {
        ArakoonKeyValueListIter * iter = NULL;

        FUNCTION_ENTER(arakoon_key_value_list_create_iter);

        iter = arakoon_mem_new(1, ArakoonKeyValueListIter);
        RETURN_NULL_IF_NULL(iter);

        iter->list = list;
        iter->current = list->first;

        return iter;
}

void arakoon_key_value_list_iter_free(ArakoonKeyValueListIter * const iter) {
        FUNCTION_ENTER(arakoon_key_value_list_iter_free);

        RETURN_IF_NULL(iter);

        iter->list = NULL;
        iter->current = NULL;

        arakoon_mem_free(iter);
}

arakoon_rc arakoon_key_value_list_iter_next(ArakoonKeyValueListIter * const iter,
    size_t * const key_size, const void ** const key,
    size_t * const value_size, const void ** const value) {
        FUNCTION_ENTER(arakoon_key_value_list_iter_next);

        if(iter->current != NULL) {
                *key_size = iter->current->key_size;
                *key = iter->current->key;
                *value_size = iter->current->value_size;
                *value = iter->current->value;

                iter->current = iter->current->next;
        }
        else {
                *key_size = 0;
                *key = NULL;
                *value_size = 0;
                *value = NULL;
        }

        return ARAKOON_RC_SUCCESS;
}

arakoon_rc arakoon_key_value_list_iter_reset(
    ArakoonKeyValueListIter * const iter) {
        FUNCTION_ENTER(arakoon_key_value_list_iter_reset);

        iter->current = iter->list->first;

        return ARAKOON_RC_SUCCESS;
}
