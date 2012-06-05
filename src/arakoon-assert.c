/*
 * This file is part of Arakoon, a distributed key-value store.
 *
 * Copyright (C) 2012 Incubaid BVBA
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

#include "arakoon-utils.h"
#include "arakoon-assert.h"

#if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)) && (ARAKOON_ASSERT == 0)
# define MAYBE_UNUSED __attribute__((__unused__))
#else
# define MAYBE_UNUSED
#endif

arakoon_bool _arakoon_assert_non_null(const void *value,
    const char *name MAYBE_UNUSED, const char *file MAYBE_UNUSED,
    unsigned int line MAYBE_UNUSED, const char *function MAYBE_UNUSED) {
        if(value != NULL) {
                return ARAKOON_BOOL_TRUE;
        }

#if ARAKOON_ASSERT == 0
        ;
#elif ARAKOON_ASSERT == 1 || ARAKOON_ASSERT == 2
        _arakoon_log_error("Invalid NULL value for `%s' at %s:%u:%s", name,
            file, line, function);
# if ARAKOON_ASSERT == 2
        fprintf(stderr,
            "Error: invalid NULL value for `%s' at %s:%u:%s, abort\n", name,
            file, line, function);
        fflush(stderr);
        abort();
# endif
#else
# error Invalid value for ARAKOON_ASSERT
#endif

        return ARAKOON_BOOL_FALSE;
}
