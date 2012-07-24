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

#include "arakoonmm.hpp"

#include <stdexcept>
#include <system_error>
#include <string.h>

extern arakoon_rc arakoon_memory_set_hooks(const ArakoonMemoryHooks * const hooks) ARAKOON_GNUC_NONNULL;

namespace arakoon {

//// error

error::error()
    :   std::exception(),
        buffer_ptr_()
{
}

error::error(buffer_ptr const buffer_ptr)
    :   std::exception(),
        buffer_ptr_(buffer_ptr)
{
}

error::~error() throw ()
{
}

buffer_const_ptr
error::buffer_ptr_get() const
{
    return buffer_ptr_;
}

char const *
error::what() const throw()
{
    return arakoon_strerror(rc_get());
}

static void
rc_to_error(
    rc const rc,
    buffer_ptr const buffer_ptr = buffer_ptr())
{
    if (ARAKOON_RC_IS_SUCCESS(rc))
    {
        return;
    }

    if (ARAKOON_RC_IS_ERRNO(rc))
    {
        if (errno == ENOMEM)
        {
            throw std::bad_alloc();
        }
        else
        {
            throw std::system_error(ARAKOON_RC_AS_ERRNO(rc), std::system_category());
        }
    }

    switch ((ArakoonReturnCode) (rc))
    {
        case ARAKOON_RC_SUCCESS:
            return; // should be unreachable code
        case ARAKOON_RC_NO_MAGIC:
            throw error_no_magic(buffer_ptr);
        case ARAKOON_RC_TOO_MANY_DEAD_NODES:
            throw error_too_many_dead_nodes(buffer_ptr);
        case ARAKOON_RC_NO_HELLO:
            throw error_no_hello(buffer_ptr);
        case ARAKOON_RC_NOT_MASTER:
            throw error_not_master(buffer_ptr);
        case ARAKOON_RC_NOT_FOUND:
            throw error_not_found(buffer_ptr);
        case ARAKOON_RC_WRONG_CLUSTER:
            throw error_wrong_cluster(buffer_ptr);
        case ARAKOON_RC_NURSERY_RANGE_ERROR:
            throw error_nursery_range_error(buffer_ptr);
        case ARAKOON_RC_ASSERTION_FAILED:
            throw error_assertion_failed(buffer_ptr);
        case ARAKOON_RC_READ_ONLY:
            throw error_read_only(buffer_ptr);
        case ARAKOON_RC_UNKNOWN_FAILURE:
            throw error_unknown_failure(buffer_ptr);
        case ARAKOON_RC_CLIENT_NETWORK_ERROR:
            throw error_client_network_error(buffer_ptr);
        case ARAKOON_RC_CLIENT_UNKNOWN_NODE:
            throw error_client_unknown_node(buffer_ptr);
        case ARAKOON_RC_CLIENT_MASTER_NOT_FOUND:
            throw error_client_master_not_found(buffer_ptr);
        case ARAKOON_RC_CLIENT_NOT_CONNECTED:
            throw error_client_not_connected(buffer_ptr);
        case ARAKOON_RC_CLIENT_TIMEOUT:
            throw error_client_timeout(buffer_ptr);
        case ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING:
            throw error_client_nursery_invalid_routing(buffer_ptr);
        case ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG:
            throw error_client_nursery_invalid_config(buffer_ptr);

        // no default, so that the compiler can warn if a case is missing
    }

    throw error_unknown_failure(buffer_ptr);
}

//// memory hooks

static ArakoonMemoryHooks memory_hooks = { ::malloc, ::free, ::realloc };

void
memory_set_hooks(ArakoonMemoryHooks const * const hooks)
{
    rc_to_error(arakoon_memory_set_hooks(hooks));

    memcpy(&memory_hooks, hooks, sizeof(ArakoonMemoryHooks));
}

//// buffer

buffer::buffer()
    :   data_(NULL),
        size_(0),
        owner_(false)
{
}

buffer::buffer(std::string const & s)
    :   data_((void *) s.data()),
        size_(s.size()),
        owner_(false)
{
}

buffer::buffer(
    void * const data,
    std::size_t const size,
    bool const take_ownership)
    :   data_(data),
        size_(size),
        owner_(take_ownership)
{
}

buffer::~buffer()
{
    reset();
}

void
buffer::reset()
{
    if (owner_)
    {
        memory_hooks.free(data_);
        data_ = NULL;
        owner_ = false;
    }

    size_ = 0;
}

void
buffer::set(
    void * const data,
    std::size_t const size,
    bool const take_ownership)
{
    reset();
    data_ = data;
    size_ = size;
    owner_ = take_ownership;
}

void *
buffer::data()
{
    return data_;
}

void const *
buffer::data() const
{
    return data_;
}

std::size_t
buffer::size() const
{
    return size_;
}

//// value_list_iterator

value_list_iterator::value_list_iterator(
    ArakoonValueListIter * const iter)
    :   iter_(iter),
        value_()
{
    if (iter == NULL)
    {
        throw std::invalid_argument("value_list_iterator::value_list_iterator");
    }

    next();
}

value_list_iterator::~value_list_iterator()
{
    reset();
    arakoon_value_list_iter_free(iter_);
    iter_ = NULL;
}

void
value_list_iterator::reset()
{
    value_.reset();
    rc_to_error(arakoon_value_list_iter_reset(iter_));
}

void
value_list_iterator::next()
{
    void const * value_data = NULL;
    size_t value_size = 0;

    rc_to_error(arakoon_value_list_iter_next(iter_, &value_size, &value_data));

    value_.set((void *) value_data, value_size, false);
}

bool
value_list_iterator::at_end() const
{
    return (value_.data() == NULL);
}

buffer const &
value_list_iterator::value() const
{
    return value_;
}

//// value_list

value_list::value_list()
    :   list_(NULL)
{
    list_ = arakoon_value_list_new();
    if (list_ == NULL)
    {
        throw std::bad_alloc();
    }
}

value_list::value_list(
    ArakoonValueList * const list)
    :   list_(list)
{
    if (list == NULL)
    {
        throw std::invalid_argument("value_list::value_list");
    }
}

value_list::~value_list()
{
    if (list_)
    {
        arakoon_value_list_free(list_);
        list_ = NULL;
    }
}

void
value_list::add(
    buffer const & value)
{
    rc_to_error(arakoon_value_list_add(list_, value.size(), value.data()));
}

value_list_iterator_ptr
value_list::begin() const
{
    ArakoonValueListIter * iter = arakoon_value_list_create_iter(list_);
    if (iter == NULL)
    {
        throw std::bad_alloc();
    }

    try
    {
        return value_list_iterator_ptr(new value_list_iterator(iter));
    }
    catch (...)
    {
        arakoon_value_list_iter_free(iter);
        throw;
    }
}

ArakoonValueList const *
value_list::get() const
{
    return list_;
}

size_t
value_list::size() const
{
    ssize_t size = arakoon_value_list_size(list_);
    if (size < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    return (size_t) size;
}

//// key_value_list_iterator

key_value_list_iterator::key_value_list_iterator(
    ArakoonKeyValueListIter * const iter)
    :   iter_(iter),
        key_(),
        value_()
{
    if (iter == NULL)
    {
        throw std::invalid_argument("key_value_list_iterator::key_value_list_iterator");
    }

    next();
}

key_value_list_iterator::~key_value_list_iterator()
{
    reset();
    arakoon_key_value_list_iter_free(iter_);
    iter_ = NULL;
}

void
key_value_list_iterator::reset()
{
    key_.reset();
    value_.reset();
    rc_to_error(arakoon_key_value_list_iter_reset(iter_));
}

void
key_value_list_iterator::next()
{
    void const * key_data = NULL;
    size_t key_size = 0;
    void const * value_data = NULL;
    size_t value_size = 0;

    rc_to_error(arakoon_key_value_list_iter_next(iter_, &key_size, &key_data, &value_size, &value_data));

    key_.set((void *) key_data, key_size, false);
    value_.set((void *) value_data, value_size, false);
}

bool
key_value_list_iterator::at_end() const
{
    return (key_.data() == NULL);
}

buffer const &
key_value_list_iterator::key() const
{
    return key_;
}

buffer const &
key_value_list_iterator::value() const
{
    return value_;
}

//// key_value_list

key_value_list::key_value_list(
    ArakoonKeyValueList * const list)
    :   list_(list)
{
    if (list == NULL)
    {
        throw std::invalid_argument("key_value_list::key_value_list");
    }
}

key_value_list::~key_value_list()
{
    if (list_)
    {
        arakoon_key_value_list_free(list_);
        list_ = NULL;
    }
}

key_value_list_iterator_ptr
key_value_list::begin() const
{
    ArakoonKeyValueListIter * iter = arakoon_key_value_list_create_iter(list_);
    if (iter == NULL)
    {
        throw std::bad_alloc();
    }

    try
    {
        return key_value_list_iterator_ptr(new key_value_list_iterator(iter));
    }
    catch (...)
    {
        arakoon_key_value_list_iter_free(iter);
        throw;
    }
}

ArakoonKeyValueList const *
key_value_list::get() const
{
    return list_;
}

size_t
key_value_list::size() const
{
    ssize_t size = arakoon_key_value_list_size(list_);
    if (size < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    return (size_t) size;
}

//// sequence

sequence::sequence()
{
    sequence_ = arakoon_sequence_new();
    if (sequence_ == NULL)
    {
        throw std::bad_alloc();
    }
}

sequence::~sequence()
{
    arakoon_sequence_free(sequence_);
    sequence_ = NULL;
}

void
sequence::add_set(
    buffer const & key,
    buffer const & value)
{
    rc_to_error(arakoon_sequence_add_set(sequence_, key.size(), key.data(), value.size(), value.data()));
}

void
sequence::add_delete(
    buffer const & key)
{
    rc_to_error(arakoon_sequence_add_delete(sequence_, key.size(), key.data()));
}

void
sequence::add_assert(
    buffer const & key,
    buffer const & value)
{
    rc_to_error(arakoon_sequence_add_assert(sequence_, key.size(), key.data(), value.size(), value.data()));
}

ArakoonSequence const *
sequence::get() const
{
    return sequence_;
}

//// client_call_options

client_call_options::client_call_options()
{
    options_ = arakoon_client_call_options_new();
    if (options_ == NULL)
    {
        throw std::bad_alloc();
    }
}

client_call_options::~client_call_options()
{
    arakoon_client_call_options_free(options_);
    options_ = NULL;
}

bool
client_call_options::get_allow_dirty() const
{
    return (arakoon_client_call_options_get_allow_dirty(options_) == ARAKOON_BOOL_TRUE);
}

void
client_call_options::set_allow_dirty(
    bool const allow_dirty)
{
    rc_to_error(arakoon_client_call_options_set_allow_dirty(options_, (allow_dirty ? ARAKOON_BOOL_TRUE : ARAKOON_BOOL_FALSE)));
}

int
client_call_options::get_timeout() const
{
    return arakoon_client_call_options_get_timeout(options_);
}

void
client_call_options::set_timeout(
    int const timeout)
{
    rc_to_error(arakoon_client_call_options_set_timeout(options_, timeout));
}

ArakoonClientCallOptions const *
client_call_options::get() const
{
    return options_;
}

//// cluster_node

cluster_node::cluster_node(
    std::string const & name)
    :   node_(NULL)
{
    node_ = arakoon_cluster_node_new(name.c_str());
    if (node_ == NULL)
    {
        throw std::bad_alloc();
    }
}

cluster_node::~cluster_node()
{
    if (node_)
    {
        arakoon_cluster_node_free(node_);
        node_ = NULL;
    }
}

void
cluster_node::add_address_tcp(
    std::string const & host,
    std::string const & service)
{
    if (node_)
    {
        rc_to_error(arakoon_cluster_node_add_address_tcp(node_, host.c_str(), service.c_str()));
    }
    else
    {
        rc_to_error(ARAKOON_RC_CLIENT_UNKNOWN_NODE);
    }
}

ArakoonClusterNode *
cluster_node::get()
{
    if (node_ == NULL)
    {
        rc_to_error(ARAKOON_RC_CLIENT_UNKNOWN_NODE);
    }

    return node_;
}

void
cluster_node::disown()
{
    node_ = NULL;
}

//// cluster

cluster::cluster(
    ArakoonProtocolVersion version,
    std::string const & cluster_name)
    :   cluster_(NULL)
{
    cluster_ = arakoon_cluster_new(version, cluster_name.c_str());
    if (cluster_ == NULL)
    {
        throw std::bad_alloc();
    }
}

cluster::~cluster()
{
    arakoon_cluster_free(cluster_);
    cluster_ = NULL;
}

void
cluster::rc_to_error(
    rc const rc1)
{
    if (!ARAKOON_RC_IS_SUCCESS(rc1))
    {
        void const * data = NULL;
        size_t size = 0;
        rc const rc2 = arakoon_cluster_get_last_error(cluster_, &size, &data);
        if (ARAKOON_RC_IS_SUCCESS(rc2) && (data != NULL) && (size != 0))
        {
            buffer_ptr buffer_ptr;
            void * data_cpy = NULL;

            try
            {
                data_cpy = memory_hooks.malloc(size);
                if (data_cpy == NULL)
                {
                    throw std::bad_alloc();
                }
                memcpy(data_cpy, data, size);

                buffer_ptr.reset(new buffer(data_cpy, size, true));
            }
            catch (...)
            {
                memory_hooks.free(data_cpy);
                throw;
            }

            arakoon::rc_to_error(rc1, buffer_ptr);
        }
        else
        {
            arakoon::rc_to_error(rc1);
        }
    }
}

std::string
cluster::get_cluster_name() const
{
    char const * cluster_name = arakoon_cluster_get_name(cluster_);
    return std::string(cluster_name ? cluster_name : "");
}

void
cluster::add_node(cluster_node & node)
{
    rc_to_error(arakoon_cluster_add_node(cluster_, node.get()));
    node.disown();
}

void
cluster::connect_master(
    client_call_options const * const options)
{
    rc_to_error(arakoon_cluster_connect_master(cluster_, (options ? options->get() : NULL)));
}

std::shared_ptr<std::string>
cluster::hello(
    client_call_options const * const options,
    std::string const & client_id,
    std::string const & cluster_id)
{
    char * result = NULL;

    rc_to_error(arakoon_hello(cluster_, (options ? options->get() : NULL), client_id.c_str(), cluster_id.c_str(), &result));

    std::shared_ptr<std::string> result_ptr(new std::string(result));

    memory_hooks.free(result);
    result = NULL;

    return result_ptr;
}

std::shared_ptr<std::string>
cluster::who_master(
    client_call_options const * const options)
{
    char * result = NULL;

    rc_to_error(arakoon_who_master(cluster_, (options ? options->get() : NULL), &result));

    std::shared_ptr<std::string> result_ptr(new std::string(result));

    memory_hooks.free(result);
    result = NULL;

    return result_ptr;
}

bool
cluster::expect_progress_possible(
    client_call_options const * const options)
{
    arakoon_bool result = ARAKOON_BOOL_FALSE;

    rc_to_error(arakoon_expect_progress_possible(cluster_, (options ? options->get() : NULL), &result));

    return (result == ARAKOON_BOOL_TRUE);
}

bool
cluster::exists(
    client_call_options const * const options,
    buffer const & key)
{
    arakoon_bool result = ARAKOON_BOOL_FALSE;

    rc_to_error(arakoon_exists(cluster_, (options ? options->get() : NULL), key.size(), key.data(), &result));

    return (result == ARAKOON_BOOL_TRUE);
}

buffer_ptr
cluster::get(
    client_call_options const * const options,
    buffer const & key)
{
    size_t result_value_size = 0;
    void * result_value_data = NULL;
    rc_to_error(arakoon_get(cluster_, (options ? options->get() : NULL), key.size(), key.data(), &result_value_size, &result_value_data));

    return buffer_ptr(new buffer(result_value_data, result_value_size, true));
}

value_list_const_ptr
cluster::multi_get(
    client_call_options const * const options,
    value_list const & keys)
{
    ArakoonValueList * result = NULL;

    rc_to_error(arakoon_multi_get(cluster_, (options ? options->get() : NULL), keys.get(), &result));

    return value_list_const_ptr(new value_list(result));
}

void
cluster::set(
    client_call_options const * const options,
    buffer const & key,
    buffer const & value)
{
    rc_to_error(arakoon_set(cluster_, (options ? options->get() : NULL), key.size(), key.data(), value.size(), value.data()));
}

void
cluster::remove(
    client_call_options const * const options,
    buffer const & key)
{
    rc_to_error(arakoon_delete(cluster_, (options ? options->get() : NULL), key.size(), key.data()));
}

value_list_const_ptr
cluster::range(
    client_call_options const * const options,
    buffer const & begin_key,
    bool const begin_key_included,
    buffer const & end_key,
    bool const end_key_included,
    ssize_t const max_elements)
{
    ArakoonValueList * result = NULL;

    rc_to_error(arakoon_range(cluster_, (options ? options->get() : NULL),
        begin_key.size(), begin_key.data(),
        (begin_key_included ? ARAKOON_BOOL_TRUE : ARAKOON_BOOL_FALSE),
        end_key.size(), end_key.data(),
        (end_key_included ? ARAKOON_BOOL_TRUE : ARAKOON_BOOL_FALSE),
        max_elements,
        &result));

    return value_list_const_ptr(new value_list(result));
}

key_value_list_const_ptr
cluster::range_entries(
    client_call_options const * const options,
    buffer const & begin_key,
    bool const begin_key_included,
    buffer const & end_key,
    bool const end_key_included,
    ssize_t const max_elements)
{
    ArakoonKeyValueList * result = NULL;

    rc_to_error(arakoon_range_entries(cluster_, (options ? options->get() : NULL),
        begin_key.size(), begin_key.data(),
        (begin_key_included ? ARAKOON_BOOL_TRUE : ARAKOON_BOOL_FALSE),
        end_key.size(), end_key.data(),
        (end_key_included ? ARAKOON_BOOL_TRUE : ARAKOON_BOOL_FALSE),
        max_elements,
        &result));

    return key_value_list_const_ptr(new key_value_list(result));
}

key_value_list_const_ptr
cluster::rev_range_entries(
    client_call_options const * const options,
    buffer const & begin_key,
    bool const begin_key_included,
    buffer const & end_key,
    bool const end_key_included,
    ssize_t const max_elements)
{
    ArakoonKeyValueList * result = NULL;

    rc_to_error(arakoon_rev_range_entries(cluster_, (options ? options->get() : NULL),
        begin_key.size(), begin_key.data(),
        (begin_key_included ? ARAKOON_BOOL_TRUE : ARAKOON_BOOL_FALSE),
        end_key.size(), end_key.data(),
        (end_key_included ? ARAKOON_BOOL_TRUE : ARAKOON_BOOL_FALSE),
        max_elements,
        &result));

    return key_value_list_const_ptr(new key_value_list(result));
}

value_list_const_ptr
cluster::prefix(
    client_call_options const * const options,
    buffer const & begin_key,
    ssize_t const max_elements)
{
    ArakoonValueList * result = NULL;

    rc_to_error(arakoon_prefix(cluster_, (options ? options->get() : NULL), begin_key.size(), begin_key.data(), max_elements, &result));

    return value_list_const_ptr(new value_list(result));
}

buffer_ptr
cluster::test_and_set(
    client_call_options const * const options,
    buffer const & key,
    buffer const & old_value,
    buffer const & new_value)
{
    void const * const old_value_data = old_value.data();
    size_t const old_value_size = old_value_data ? old_value.size() : 0;
    void const * const new_value_data = new_value.data();
    size_t const new_value_size = new_value_data ? new_value.size() : 0;
    size_t result_value_size = 0;
    void * result_value_data = NULL;

    rc_to_error(arakoon_test_and_set(cluster_, (options ? options->get() : NULL), key.size(), key.data(), old_value_size, old_value_data, new_value_size, new_value_data, &result_value_size, &result_value_data));

    return buffer_ptr(new buffer(result_value_data, result_value_size, true));
}

void
cluster::sequence(
    client_call_options const * const options,
    arakoon::sequence const & sequence)
{
    rc_to_error(arakoon_sequence(cluster_, (options ? options->get() : NULL), sequence.get()));
}

void cluster::synced_sequence(
    client_call_options const * const options,
    arakoon::sequence const & sequence)
{
    rc_to_error(arakoon_synced_sequence(cluster_, (options ? options->get() : NULL), sequence.get()));
}

} // namespace arakoon
