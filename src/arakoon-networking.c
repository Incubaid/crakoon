/*
 * This file is part of Arakoon, a distributed key-value store.
 *
 * Copyright (C) 2010, 2012 Incubaid BVBA
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
#include <string.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "arakoon.h"
#include "arakoon-utils.h"
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

typedef ssize_t (*NetworkActionProto) (int fd, void *buf, size_t count);

typedef enum {
        NETWORK_ACTION_READ,
        NETWORK_ACTION_WRITE
} NetworkAction;

static arakoon_rc _arakoon_networking_poll_act(NetworkAction action,
    int event, int fd, void *data, size_t count, int *timeout) {
        size_t done = 0, todo = count;
        ssize_t cnt = 0;

        int rc = 0;

        struct timespec start = {0, 0}, now = {0, 0};
        arakoon_bool with_timeout = (timeout != NULL &&
                *timeout != ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT);
        int timeout_ = 0, time_left = 0;
        struct pollfd ev;
        int ev_cnt = 0;

        if(fd < 0) {
                return ARAKOON_RC_CLIENT_NOT_CONNECTED;
        }

        NetworkActionProto action_ = NULL;

        switch(action) {
                case NETWORK_ACTION_READ:
                        action_ = read;
                        break;
                case NETWORK_ACTION_WRITE:
                        action_ = (NetworkActionProto) write;
                        break;

                default:
                        abort();
                        break;
        }

        memset(&ev, 0, sizeof(ev));

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
                ev.events = event;
        }

        while(todo > 0) {
                if(with_timeout) {
                        /* Wait until we can write, or timeout occurs */
                        rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
                        if(rc != 0) {
                                return -errno;
                        }

                        time_left = timeout_ - time_delta(&start, &now);

                        ev_cnt = poll(&ev, 1, time_left);

                        if(ev_cnt < 0) {
                                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                        &now);
                                if(rc != 0) {
                                        return -errno;
                                }

                                *timeout = timeout_ - time_delta(&start, &now);

                                return -errno;
                        }

                        if(ev_cnt == 0) {
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

                        if(!(ev.revents & event)) {
                                continue;
                        }
                }

                cnt = action_(fd, data + done, todo);

                if(action == NETWORK_ACTION_WRITE && cnt < 0) {
                        if(errno == EINTR || errno == EAGAIN) {
                                continue;
                        }

                        return -errno;
                }
                else if(action == NETWORK_ACTION_READ && cnt <= 0) {
                        /* We were unable to read a single byte, even though
                         * thanks to poll we know some data is available. This
                         * implies something went very wrong, or we got a
                         * 'normal' error case (EINTR, EAGAIN,...)
                         */
                        if(errno == EINTR || errno == EAGAIN) {
                                continue;
                        }

                        return (cnt < 0 ? -errno :
                                ARAKOON_RC_CLIENT_NETWORK_ERROR);
                }

                todo -= cnt;
                done += cnt;
        }

        if(with_timeout) {
                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
                if(rc != 0) {
                        return -errno;
                }

                *timeout = timeout_ - time_delta(&start, &now);
        }

        if(todo == 0) {
                return ARAKOON_RC_SUCCESS;
        }
        else {
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }
}

arakoon_rc _arakoon_networking_poll_write(int fd, const void *buf,
    size_t count, int *timeout) {
        return _arakoon_networking_poll_act(NETWORK_ACTION_WRITE, POLLOUT,
                fd, (void *) buf, count, timeout);
}

arakoon_rc _arakoon_networking_poll_read(int fd, void *buf, size_t count,
    int *timeout) {
        return _arakoon_networking_poll_act(NETWORK_ACTION_READ, POLLIN,
                fd, buf, count, timeout);
}


arakoon_rc _arakoon_networking_connect(const struct addrinfo *addr, int *fd,
    int *timeout) {
        int sock = -1, flags = 0, rc = -1;
        arakoon_bool with_timeout = (timeout != NULL &&
                *timeout != ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT);
        struct pollfd ev;
        struct timespec start = {0, 0}, now = {0, 0};
        int timeout_ = 0, time_left = 0;
        int ev_cnt = 0;
        socklen_t len = 0;
        int opt = 0;

        FUNCTION_ENTER(_arakoon_networking_connect);

        *fd = -1;

        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if(sock == -1) {
                _arakoon_log_error("Failed to create socket");

                return -errno;
        }

        if(!with_timeout) {
                if(connect(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
                        close(sock);

                        return -errno;
                }

                *fd = sock;

                return ARAKOON_RC_SUCCESS;
        }

        timeout_ = *timeout;

        if(timeout_ <= 0) {
                close(sock);

                return ARAKOON_RC_CLIENT_TIMEOUT;
        }

        rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
        if(rc != 0) {
                close(sock);

                return -errno;
        }

        memset(&ev, 0, sizeof(ev));

        ev.fd = sock;
        ev.events = POLLOUT;

        flags = fcntl(sock, F_GETFL, NULL);
        if(flags < 0) {
                _arakoon_log_error("Failed to retrieve socket flags");
                close(sock);

                return -errno;
        }

        if(fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
                _arakoon_log_error("Failed to set socket flags");
                close(sock);

                return -errno;
        }

        rc = connect(sock, addr->ai_addr, addr->ai_addrlen);

        /* TODO Since there's lots of overlap with code in
         * _arakoon_networking_poll_act. some refactoring might be in place
         */

        if(rc < 0) {
                if(errno != EINPROGRESS) {
                        _arakoon_log_error("Failed to connect socket: %s",
                                strerror(errno));

                        close(sock);

                        return -errno;
                }

                while(1) {
                        rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
                        if(rc != 0) {
                                close(sock);

                                return -errno;
                        }

                        time_left = timeout_ - time_delta(&start, &now);

                        if(time_left <= 0) {
                                close(sock);

                                return ARAKOON_RC_CLIENT_TIMEOUT;
                        }

                        ev_cnt = poll(&ev, 1, time_left);

                        if(ev_cnt < 0) {
                                if(errno == EINTR) {
                                        continue;
                                }

                                close(sock);

                                rc = clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                        &now);
                                if(rc != 0) {
                                        return -errno;
                                }

                                *timeout = timeout_ - time_delta(&start, &now);

                                return -errno;
                        }

                        if(ev_cnt == 0) {
                                close(sock);

                                *timeout = 0;
                                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
                        }

                        if(ev.revents & POLLERR || ev.revents & POLLHUP ||
                                ev.revents & POLLNVAL) {
                                close(sock);

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

                        if((ev.revents & POLLOUT) != 0) {
                                break;
                        }
                }
        }

        len = sizeof(opt);
        if(getsockopt(sock, SOL_SOCKET, SO_ERROR, &opt, &len) < 0) {
                close(sock);

                return -errno;
        }

        if(opt) {
                close(sock);

                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }

        if(fcntl(sock, F_SETFL, flags) < 0) {
                _arakoon_log_error("Failed to reset socket flags: %s",
                        strerror(errno));
                close(sock);

                return -errno;
        }

        *fd = sock;

        return ARAKOON_RC_SUCCESS;
}
