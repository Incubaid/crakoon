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
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

#include "arakoon.h"
#include "arakoon-networking.h"

#define NS_PER_MS (1000000)
#define MS_PER_S (1000)
#define NS_PER_S (MS_PER_S * NS_PER_MS)

static long time_delta(const struct timespec * const start,
    const struct timespec * const end) {
        long nd = 0;

        if(start->tv_sec == end->tv_sec) {
                return ((end->tv_nsec - start->tv_nsec) / NS_PER_MS);
        }

        nd = (NS_PER_S - start->tv_nsec) + end->tv_nsec;
        return (nd / NS_PER_MS) + ((end->tv_sec - start->tv_sec) * MS_PER_S);
}

arakoon_rc _arakoon_networking_poll_write(int fd, const void *data,
    size_t count, int *timeout) {
        size_t written = 0, to_send = count;
        ssize_t done = 0;
        struct timespec start = {0, 0}, now = {0, 0};
        int rc = 0;
        arakoon_bool with_timeout = (timeout != NULL &&
                *timeout != ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT);
        int timeout_ = 0, time_left = 0;
        int cnt = 0;
        struct pollfd ev;

        if(with_timeout) {
                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

                if(rc != 0) {
                        return -errno;
                }

                timeout_ = *timeout;

                ev.fd = fd;
                ev.events = POLLOUT;
        }

        while(to_send > 0) {
                if(with_timeout) {
                        /* Wait until we can write, or timeout occurs */
                        rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
                        if(rc != 0) {
                                return -errno;
                        }

                        time_left = timeout_ - time_delta(&start, &now);

                        cnt = poll(&ev, 1, time_left);

                        if(cnt < 0) {
                                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                        &now);
                                if(rc != 0) {
                                        return -errno;
                                }

                                *timeout = timeout_ - time_delta(&start, &now);

                                return -errno;
                        }

                        if(cnt == 0) {
                                *timeout = 0;
                                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
                        }

                        if(ev.revents & POLLERR || ev.revents & POLLHUP ||
                                ev.revents & POLLNVAL) {
                                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                        &now);
                                if(rc != 0) {
                                        return -errno;
                                }

                                *timeout = timeout_ - time_delta(&start, &now);

                                if(ev.revents & POLLERR) {
                                        return ARAKOON_RC_CLIENT_NETWORK_ERROR;
                                }
                                if(ev.revents & POLLHUP) {
                                        return ARAKOON_RC_CLIENT_NOT_CONNECTED;
                                }
                                if(ev.revents & POLLNVAL) {
                                        return ARAKOON_RC_CLIENT_NOT_CONNECTED;
                                }

                                /* Not reached */
                                abort();
                        }

                        if(!(ev.revents & POLLOUT)) {
                                continue;
                        }
                }

                done = write(fd, data + written, to_send);

                if(done < 0) {
                        /* TODO Handle EINTR, EAGAIN,... */
                        return -errno;
                }

                to_send -= done;
                written += done;
                done = 0;
        }

        if(with_timeout) {
                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
                if(rc != 0) {
                        return -errno;
                }

                *timeout = timeout_ - time_delta(&start, &now);
        }

        if(to_send == 0) {
                return ARAKOON_RC_SUCCESS;
        }
        else {
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }
}

arakoon_rc _arakoon_networking_poll_read(int fd, void *buf, size_t count,
    int *timeout) {
        size_t read_ = 0, to_read = count;
        ssize_t read2 = 0;

        struct timespec start = {0, 0}, now = {0, 0};
        int rc = 0;
        arakoon_bool with_timeout = (timeout != NULL &&
                *timeout != ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT);
        int timeout_ = 0, time_left = 0;
        int cnt = 0;
        struct pollfd ev;

        if(with_timeout) {
                timeout_ = *timeout;

                if(timeout_ <= 0) {
                        return ARAKOON_RC_CLIENT_TIMEOUT;
                }

                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

                if(rc != 0) {
                        return -errno;
                }

                ev.fd = fd;
                ev.events = POLLIN;

                usleep(timeout_ / 2);
        }

        while(to_read > 0) {
                if(with_timeout) {
                        /* Wait until we can write, or timeout occurs */
                        rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
                        if(rc != 0) {
                                return -errno;
                        }

                        time_left = timeout_ - time_delta(&start, &now);

                        cnt = poll(&ev, 1, time_left);

                        if(cnt < 0) {
                                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                        &now);
                                if(rc != 0) {
                                        return -errno;
                                }

                                *timeout = timeout_ - time_delta(&start, &now);

                                return -errno;
                        }

                        if(cnt == 0) {
                                *timeout = 0;
                                return ARAKOON_RC_CLIENT_TIMEOUT;
                        }

                        if(ev.revents & POLLERR || ev.revents & POLLHUP ||
                                ev.revents & POLLNVAL) {
                                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                        &now);
                                if(rc != 0) {
                                        return -errno;
                                }

                                *timeout = timeout_ - time_delta(&start, &now);

                                if(ev.revents & POLLERR) {
                                        return ARAKOON_RC_CLIENT_NETWORK_ERROR;
                                }
                                if(ev.revents & POLLHUP) {
                                        return ARAKOON_RC_CLIENT_NOT_CONNECTED;
                                }
                                if(ev.revents & POLLNVAL) {
                                        return ARAKOON_RC_CLIENT_NOT_CONNECTED;
                                }

                                /* Not reached */
                                abort();
                        }

                        if(!(ev.revents & POLLIN)) {
                                continue;
                        }
                }

                read2 = read(fd, buf + read_, to_read);

                if(read2 <= 0) {
                        /* TODO Handle EINTR, EAGAIN,... */
                        return (read2 < 0 ? -errno :
                                ARAKOON_RC_CLIENT_NETWORK_ERROR);
                }

                to_read -= read2;
                read_ += read2;
                read2 = 0;
        }

        if(with_timeout) {
                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
                if(rc != 0) {
                        return -errno;
                }

                *timeout = timeout_ - time_delta(&start, &now);
        }

        if(to_read == 0) {
                return ARAKOON_RC_SUCCESS;
        }
        else {
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }
}
