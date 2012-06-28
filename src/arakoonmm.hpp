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

#ifndef ARAKOON_HPP
#define ARAKOON_HPP

#include "arakoon.h"

// use arakoon::memory_set_hooks instead
#undef arakoon_memory_set_hooks

#include <memory>
#include <string>
#include <exception>

namespace arakoon {

void memory_set_hooks(ArakoonMemoryHooks const * const hooks);

//// buffer

class buffer
{
  public:
    buffer();

    buffer(std::string const & s);

    buffer(
        void * const data,
        std::size_t const size,
        bool const take_ownership = false);

    ~buffer();

    void reset();
    void set(void * const data, std::size_t const size, bool const take_ownership);

    void * data();
    void const * data() const;

    std::size_t size() const;

  private:
    buffer(buffer const &) = delete;
    buffer & operator=(buffer const &) = delete;

    void * data_;
    std::size_t size_;
    bool owner_;
};

typedef std::shared_ptr<buffer> buffer_ptr;
typedef std::shared_ptr<buffer const> buffer_const_ptr;

//// error

typedef arakoon_rc rc;

class error : public std::exception
{
  public:
    error();
    explicit error(buffer_ptr);
    virtual ~error() throw ();
    virtual rc rc_get() const = 0;
    virtual buffer_const_ptr buffer_ptr_get() const;
    char const * what() const throw();

  protected:
    buffer_ptr buffer_ptr_;
};

#define DEF_ERROR(__name, __rc) \
    class __name : public error \
    { \
      public: \
        __name() : error() {} \
        explicit __name(buffer_ptr const buffer_ptr) : error(buffer_ptr) {} \
        rc rc_get() const { return __rc; } \
        __name(__name const & rhs) : error() { buffer_ptr_ = rhs.buffer_ptr_; } \
        __name & operator=(__name const & rhs) { if (this != &rhs) { buffer_ptr_ = rhs.buffer_ptr_; } return *this; } \
    };

DEF_ERROR(error_no_magic, ARAKOON_RC_NO_MAGIC);
DEF_ERROR(error_too_many_dead_nodes, ARAKOON_RC_TOO_MANY_DEAD_NODES);
DEF_ERROR(error_no_hello, ARAKOON_RC_NO_HELLO);
DEF_ERROR(error_not_master, ARAKOON_RC_NOT_MASTER);
DEF_ERROR(error_not_found, ARAKOON_RC_NOT_FOUND);
DEF_ERROR(error_wrong_cluster, ARAKOON_RC_WRONG_CLUSTER);
DEF_ERROR(error_nursery_range_error, ARAKOON_RC_NURSERY_RANGE_ERROR);
DEF_ERROR(error_assertion_failed, ARAKOON_RC_ASSERTION_FAILED);
DEF_ERROR(error_read_only, ARAKOON_RC_READ_ONLY);
DEF_ERROR(error_unknown_failure, ARAKOON_RC_UNKNOWN_FAILURE);

DEF_ERROR(error_client_network_error, ARAKOON_RC_CLIENT_NETWORK_ERROR);
DEF_ERROR(error_client_unknown_node, ARAKOON_RC_CLIENT_UNKNOWN_NODE);
DEF_ERROR(error_client_master_not_found, ARAKOON_RC_CLIENT_MASTER_NOT_FOUND);
DEF_ERROR(error_client_not_connected, ARAKOON_RC_CLIENT_NOT_CONNECTED);
DEF_ERROR(error_client_timeout, ARAKOON_RC_CLIENT_TIMEOUT);
DEF_ERROR(error_client_nursery_invalid_routing, ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING);
DEF_ERROR(error_client_nursery_invalid_config, ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG);

#undef DEF_ERROR

class value_list_iterator
{
  public:
    value_list_iterator(ArakoonValueListIter * const iter);

    ~value_list_iterator();

    void reset();
    void next();
    bool at_end() const;
    buffer const & value() const;

  private:
    value_list_iterator(value_list_iterator const &) = delete;
    value_list_iterator & operator=(value_list_iterator const &) = delete;

    ArakoonValueListIter * iter_;
    buffer value_;
};

typedef std::shared_ptr<value_list_iterator> value_list_iterator_ptr;

class value_list
{
  public:
    value_list();

    value_list(ArakoonValueList * const list);

    ~value_list();

    void add(buffer const & value);

    value_list_iterator_ptr begin() const;

    ArakoonValueList const * get() const;

    size_t size() const;

  private:
    value_list(value_list const &) = delete;
    value_list & operator=(value_list const &) = delete;

    ArakoonValueList * list_;
};

typedef std::shared_ptr<value_list const> value_list_const_ptr;

class key_value_list_iterator
{
  public:
    key_value_list_iterator(ArakoonKeyValueListIter * const iter);

    ~key_value_list_iterator();

    void reset();
    void next();
    bool at_end() const;
    buffer const & key() const;
    buffer const & value() const;

  private:
    key_value_list_iterator(key_value_list_iterator const &) = delete;
    key_value_list_iterator & operator=(key_value_list_iterator const &) = delete;

    ArakoonKeyValueListIter * iter_;
    buffer key_;
    buffer value_;
};

typedef std::shared_ptr<key_value_list_iterator> key_value_list_iterator_ptr;

class key_value_list
{
  public:
    key_value_list(ArakoonKeyValueList * const list);

    ~key_value_list();

    key_value_list_iterator_ptr begin() const;

    ArakoonKeyValueList const * get() const;

    size_t size() const;

  private:
    key_value_list(key_value_list const &) = delete;
    key_value_list & operator=(key_value_list const &) = delete;

    ArakoonKeyValueList * list_;
};

typedef std::shared_ptr<key_value_list const> key_value_list_const_ptr;

class sequence
{
  public:
    sequence();

    ~sequence();

    void add_set(
        buffer const & key,
        buffer const & value);

    void add_delete(
        buffer const & key);

    void add_assert(
        buffer const & key,
        buffer const & value);

    ArakoonSequence const * get() const;

  private:
    sequence(sequence const &) = delete;
    sequence & operator=(sequence const &) = delete;

    ArakoonSequence * sequence_;
};

typedef std::shared_ptr<sequence> sequence_ptr;

class client_call_options
{
  public:
    client_call_options();

    ~client_call_options();

    bool get_allow_dirty() const;
    void set_allow_dirty(bool const allow_dirty);

    int get_timeout() const;
    void set_timeout(int const timeout);

    ArakoonClientCallOptions const * get() const;

  private:
    client_call_options(client_call_options const &) = delete;
    client_call_options & operator=(client_call_options const &) = delete;

    ArakoonClientCallOptions * options_;
};

class cluster_node
{
  public:
    cluster_node(std::string const & name);

    ~cluster_node();

    void add_address_tcp(
        std::string const & host,
        std::string const & service);

  protected:
    friend class cluster;
    ArakoonClusterNode * get();
    void disown();

  private:
    cluster_node(cluster_node const &) = delete;
    cluster_node & operator=(cluster_node const &) = delete;

    ArakoonClusterNode * node_;
};

class cluster
{
  public:
    cluster(ArakoonProtocolVersion version, std::string const & cluster_name);

    ~cluster();

    void add_node(cluster_node & node);

    std::string get_cluster_name() const;

    void connect_master(
        client_call_options const & options);

    std::shared_ptr<std::string> hello(
        client_call_options const & options,
        std::string const & client_id,
        std::string const & cluster_id);

    std::shared_ptr<std::string> who_master(
        client_call_options const & options);

    bool expect_progress_possible(
        client_call_options const & options);

    bool exists(
        client_call_options const & options,
        buffer const & key);

    buffer_ptr get(
        client_call_options const & options,
        buffer const & key);

    value_list_const_ptr multi_get(
        client_call_options const & options,
        value_list const & keys);

    void set(
        client_call_options const & options,
        buffer const & key,
        buffer const & value);

    void remove(
        client_call_options const & options,
        buffer const & key);

    value_list_const_ptr range(
        client_call_options const & options,
        buffer const & begin_key,
        bool const begin_key_included,
        buffer const & end_key,
        bool const end_key_included,
        ssize_t const max_elements);

    key_value_list_const_ptr range_entries(
        client_call_options const & options,
        buffer const & begin_key,
        bool const begin_key_included,
        buffer const & end_key,
        bool const end_key_included,
        ssize_t const max_elements);

    key_value_list_const_ptr rev_range_entries(
        client_call_options const & options,
        buffer const & begin_key,
        bool const begin_key_included,
        buffer const & end_key,
        bool const end_key_included,
        ssize_t const max_elements);

    value_list_const_ptr prefix(
        client_call_options const & options,
        buffer const & begin_key,
        ssize_t const max_elements);

    buffer_ptr test_and_set(
        client_call_options const & options,
        buffer const & key,
        buffer const & old_value,
        buffer const & new_value);

    void sequence(
        client_call_options const & options,
        arakoon::sequence const & sequence);

    void synced_sequence(
        client_call_options const & options,
        arakoon::sequence const & sequence);

  private:
    void rc_to_error(rc const rc);

    cluster(cluster const &) = delete;
    cluster & operator=(cluster const &) = delete;

    ArakoonCluster * cluster_;
};

} // namespace arakoon

#endif // ARAKOON_HPP
