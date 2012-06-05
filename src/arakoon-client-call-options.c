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
#include "arakoon-utils.h"
#include "arakoon-assert.h"

struct ArakoonClientCallOptions {
        arakoon_bool allow_dirty;
        int timeout;
};

const ArakoonClientCallOptions *
    _arakoon_client_call_options_get_default(void) {
        static const ArakoonClientCallOptions options = {
                ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_ALLOW_DIRTY,
                ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT
        };

        return &options;
}

ArakoonClientCallOptions * arakoon_client_call_options_new(void) {
        ArakoonClientCallOptions *options = NULL;

        FUNCTION_ENTER(arakoon_client_call_options_new);

        options = arakoon_mem_new(1, ArakoonClientCallOptions);
        RETURN_NULL_IF_NULL(options);

        memcpy(options, _arakoon_client_call_options_get_default(),
                sizeof(ArakoonClientCallOptions));

        return options;
}

void arakoon_client_call_options_free(ArakoonClientCallOptions *options) {
        FUNCTION_ENTER(arakoon_client_call_options_free);

        arakoon_mem_free(options);
}

arakoon_bool arakoon_client_call_options_get_allow_dirty(
    const ArakoonClientCallOptions * const options) {
        FUNCTION_ENTER(arakoon_client_call_options_get_allow_dirty);

        ASSERT_NON_NULL_RC(options);

        return options->allow_dirty;
}

arakoon_rc arakoon_client_call_options_set_allow_dirty(
    ArakoonClientCallOptions * const options, arakoon_bool allow_dirty) {
        FUNCTION_ENTER(arakoon_client_call_options_set_allow_dirty);

        ASSERT_NON_NULL_RC(options);

        options->allow_dirty = allow_dirty;

        return ARAKOON_RC_SUCCESS;
}

int arakoon_client_call_options_get_timeout(
    const ArakoonClientCallOptions * const options) {
        FUNCTION_ENTER(arakoon_client_call_options_get_timeout);

        ASSERT_NON_NULL_RC(options);

        return options->timeout;
}

arakoon_rc arakoon_client_call_options_set_timeout(
    ArakoonClientCallOptions * const options, int timeout) {
        FUNCTION_ENTER(arakoon_client_call_options_set_timeout);

        ASSERT_NON_NULL_RC(options);

        options->timeout = timeout;

        return ARAKOON_RC_SUCCESS;
}
