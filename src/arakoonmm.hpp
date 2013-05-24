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

#ifndef ARAKOON_H_EXPORT_PROCEDURES
# define ARAKOON_H_EXPORT_PROCEDURES 0
#endif
#ifndef ARAKOON_H_EXPORT_TYPES
# define ARAKOON_H_EXPORT_TYPES 1
#endif

#include "arakoon.h"

#include <memory>
#include <string>
#include <exception>
#include <utility>

/**
 * \namespace arakoon
 *
 * \par Requirements
 * The Crakoon C++ API uses the C++11 standard, which may require specific
 * compiler options, for example: 'g++ -std=c++0x'.
 * The C++ API is a wrapper around the Crakoon C API, so both the Crakoon C API
 * library and the Crakoon C++ API library should be linked to.
 *
 * \par Buffers
 * Arakoon can store keys and values containing NUL bytes. The C implementation
 * underlying the C++ implementation does its own memory allocations. Currently,
 * in C++, there is no way to may a std::string of a blob of data without
 * copying the data, and copying can be expensive. To avoid the copy, a 'buffer'
 * class is provided instead.
 *
 * \par Memory handling
 * The memory handling procedures to be used by Crakoon can be set through
 * ::arakoon::memory_set_hooks.
 * All memory management has been simplified greatly by use of std::shared_ptr.
 * All returned objects will take care of deallocating themselves properly when
 * going out of scope.
 *
 * \par Error handling
 * Arakoon errors are translated to one of the specific ::arakoon::error_*
 * exceptions.
 * Memory allocation errors are translated to std::bad_alloc; any other
 * errno code from the underlying Crakoon C API is translated to
 * a std::runtime_error with strerror(errno) as what().
 */
namespace arakoon {

/** \defgroup arakoon_cpp_api_group Arakoon C++ API
 * @{
 */

/**
 * \brief
 * Set the memory handling procedures to be used by Crakoon. The `malloc`,
 * `free` and `realloc` functions can be specified. These functions should *NOT*
 * throw any exceptions, since they are called from C code. Crakoon should not
 * use any other heap-allocation functions.
 *
 * The system *malloc(3)*, *free(3)* and *realloc(3)* procedures are used as default.
 * If possible, it is advisable to register your own heap allocation handler
 * functions before calling any other Crakoon procedures.
 *
 * For the definition of #ArakoonMemoryHooks, please see arakoon.h.
 */
void memory_set_hooks(ArakoonMemoryHooks const * const hooks);

/** \defgroup buffer_group Buffers
 * @{
 */
/**
 * \class buffer
 *
 * Arakoon can store keys and values containing NUL bytes. The C implementation
 * underlying the C++ implementation does its own memory allocations. Currently,
 * in C++, there is no way to may a std::string of a blob of data without
 * copying the data, and copying can be expensive. To avoid the copy, a 'buffer'
 * implementation is provided instead.
 *
 * For ease of use, class buffer provides a constructor to wrap a std::string,
 * without copying the data. The ownership of the data remains with the string,
 * so the caller should take care that the lifetime of the string exceeds the
 * lifetime of the buffer.
 *
 * A buffer can be told to take ownership of the raw data provided to its
 * constructor, in which case it will free it on destruction using the
 * deallocation function in use (see 'Memory handling' below).
 */
class buffer
{
  public:
    /**
     * \brief Construct a buffer that represents the value 'None', with
     *        data == NULL and size == 0.
     */
    buffer();

    /**
     * \brief Construct a buffer from a string's data and size. The ownership
     *        of the data remains with the string, so the caller should take
     *        care that the lifetime of the string exceeds the lifetime of the
     *        buffer.
     */
    buffer(std::string const & s);

    /**
     * \brief Construct a buffer from specified data and size. If told to take
     *        ownership of the data, the buffer will deallocate the data on
     *        reset, set, or destruction using the deallocation function in use
     *        (see ::arakoon::memory_set_hooks).
     */
    buffer(
        void * const data,
        std::size_t const size,
        bool const take_ownership = false);

    /**
     * \brief If the buffer currently has ownership of its data, the destructor
     *        deallocates it using the deallocation function in use (see
     *        ::arakoon::memory_set_hooks).
     */
    ~buffer();

    /**
     * \brief If the buffer currently has ownership of its data, deallocate it.
     *        Set the buffer to value 'None', with data == NULL and size == 0.
     */
    void reset();

    /**
     * \brief If the buffer currently has ownership of its data, deallocate it.
     *        Set the buffer to the specified parameters as if newly constructed.
     */
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
/** @} */ // buffer_group

/** \defgroup error_group Errors
 * @{
 */
typedef arakoon_rc rc;

/**
 * \class error
 *
 * Class 'error' is the base class for all arakoon errors. It has been derived
 * from std::exception.
 *
 * All specific exception classes ::arakoon::error_* for the different arakoon
 * #ArakoonReturnCode enumeration values from crakoon derive from 'error'.
 *
 * The #ArakoonReturnCode corresponding with the exception can be retrieved
 * from the ::arakoon::error* exception through method #rc_get(), and the
 * corresponding error string can be retrieved through method #what().
 *
 * Any data the arakoon server returned with the #ArakoonReturnCode can be
 * retrieved through method buffer_ptr_get().
 */
class error : public std::exception
{
  public:
    error();
    explicit error(buffer_ptr);
    virtual ~error() throw ();

    /**
     * \brief Return the #ArakoonReturnCode corresponding to the exception.
     */
    virtual rc rc_get() const = 0;

    /**
     * \brief Return a buffer with any data the arakoon server returned
     *        together with the #ArakoonReturnCode. This data may contain
     *        keys and/or values, which can contain NUL bytes.
     */
    virtual buffer_const_ptr buffer_ptr_get() const;

    /**
     * \brief Return the error string corresponding to the #ArakoonReturnCode
     *        corresponding to the exception, see also arakoon_strerror() in
     *        arakoon.h.
     */
    char const * what() const throw();

  protected:
    buffer_ptr buffer_ptr_;
};

template <rc Rc_code>
class specific_error : public error
{
  public:
    specific_error()
        :   error()
    {
    }

    explicit specific_error(buffer_ptr const buffer_ptr)
        :   error(buffer_ptr)
    {
    }

    rc rc_get() const
    {
        return Rc_code;
    }
};

typedef specific_error<ARAKOON_RC_NO_MAGIC> error_no_magic;
typedef specific_error<ARAKOON_RC_TOO_MANY_DEAD_NODES> error_too_many_dead_nodes;
typedef specific_error<ARAKOON_RC_NO_HELLO> error_no_hello;
typedef specific_error<ARAKOON_RC_NOT_MASTER> error_not_master;
typedef specific_error<ARAKOON_RC_NOT_FOUND> error_not_found;
typedef specific_error<ARAKOON_RC_WRONG_CLUSTER> error_wrong_cluster;
typedef specific_error<ARAKOON_RC_NURSERY_RANGE_ERROR> error_nursery_range_error;
typedef specific_error<ARAKOON_RC_ASSERTION_FAILED> error_assertion_failed;
typedef specific_error<ARAKOON_RC_READ_ONLY> error_read_only;
typedef specific_error<ARAKOON_RC_UNKNOWN_FAILURE> error_unknown_failure;

typedef specific_error<ARAKOON_RC_CLIENT_NETWORK_ERROR> error_client_network_error;
typedef specific_error<ARAKOON_RC_CLIENT_UNKNOWN_NODE> error_client_unknown_node;
typedef specific_error<ARAKOON_RC_CLIENT_MASTER_NOT_FOUND> error_client_master_not_found;
typedef specific_error<ARAKOON_RC_CLIENT_NOT_CONNECTED> error_client_not_connected;
typedef specific_error<ARAKOON_RC_CLIENT_TIMEOUT> error_client_timeout;
typedef specific_error<ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING> error_client_nursery_invalid_routing;
typedef specific_error<ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG> error_client_nursery_invalid_config;

void rc_to_error(
    rc const rc);
void rc_to_error(
    rc const rc,
    buffer_ptr const buffer_ptr);

/** @} */ // error_group

/** \defgroup data_structures_group Data structures
 *
 * \brief Some generic data structures used throughout the API.
 *
 * @{
 */

/** \defgroup value_list_group Lists of keys or values
 * @{
 */
/**
 * \class value_list_iterator
 *
 * Iterator for iteration over a value_list.
 */
class value_list_iterator
{
  public:
    value_list_iterator(ArakoonValueListIter * const iter);

    ~value_list_iterator();

    /** \brief Reset the iterator to the first value. */
    void reset();

    /** \brief Advance the iterator to the next value. */
    void next();

    /** \brief Check if the iterator has reached the end of the list. */
    bool at_end() const;

    /** \brief Get the value the iterator is currently positioned at.
     *         If at the end, the returned buffer has size 0 and data NULL (if
     *         not at the end, the returned buffer has size 0 but data != NULL).
     */
    buffer const & value() const;

  private:
    value_list_iterator(value_list_iterator const &) = delete;
    value_list_iterator & operator=(value_list_iterator const &) = delete;

    ArakoonValueListIter * iter_;
    buffer value_;
};

typedef std::shared_ptr<value_list_iterator> value_list_iterator_ptr;

/**
 * \class value_list
 *
 * A list of arakoon values, also usable as a list of arakoon keys.
 */
class value_list
{
  public:
    /** \brief Construct an empty value list. */
    value_list();

    value_list(ArakoonValueList * const list);

    ~value_list();

    /** \brief Add a copy of the specified value to the list. */
    void add(buffer const & value);

    /** \brief Return an iterator positioned at the first element of the list,
     *         or if the list is empty at the end of the list.
     */
    value_list_iterator_ptr begin() const;

    ArakoonValueList const * get() const;

    /** \brief Return the number of items in the value_list. */
    size_t size() const;

  private:
    value_list(value_list const &) = delete;
    value_list & operator=(value_list const &) = delete;

    ArakoonValueList * list_;
};

typedef std::shared_ptr<value_list const> value_list_const_ptr;
/** @} */ // value_list_group

/** \defgroup key_value_list_group Lists of (key, value) pairs
 * @{
 */
/**
 * \class key_value_list_iterator
 *
 * Iterator for iteration over a key_value_list.
 */
class key_value_list_iterator
{
  public:
    key_value_list_iterator(ArakoonKeyValueListIter * const iter);

    ~key_value_list_iterator();

    /** \brief Reset the iterator to the first (key, value) pair. */
    void reset();

    /** \brief Advance the iterator to the next (key, value) pair. */
    void next();

    /** \brief Check if the iterator has reached the end of the list. */
    bool at_end() const;

    /** \brief Get the key the iterator is currently positioned at.
     *         If at the end, the returned buffer has size 0 and data NULL (if
     *         not at the end, the returned buffer has size 0 but data != NULL).
     */
    buffer const & key() const;

    /** \brief Get the value the iterator is currently positioned at.
     *         If at the end, the returned buffer has size 0 and data NULL (if
     *         not at the end, the returned buffer has size 0 but data != NULL).
     */
    buffer const & value() const;

  private:
    key_value_list_iterator(key_value_list_iterator const &) = delete;
    key_value_list_iterator & operator=(key_value_list_iterator const &) = delete;

    ArakoonKeyValueListIter * iter_;
    buffer key_;
    buffer value_;
};

typedef std::shared_ptr<key_value_list_iterator> key_value_list_iterator_ptr;

/**
 * \class key_value_list
 *
 * A list of (key, value) pairs.
 */
class key_value_list
{
  public:
    key_value_list(ArakoonKeyValueList * const list);

    ~key_value_list();

    /** \brief Return an iterator positioned at the first element of the list,
     *         or if the list is empty at the end of the list.
     */
    key_value_list_iterator_ptr begin() const;

    ArakoonKeyValueList const * get() const;

    /** \brief Return the number of items in the value_list. */
    size_t size() const;

  private:
    key_value_list(key_value_list const &) = delete;
    key_value_list & operator=(key_value_list const &) = delete;

    ArakoonKeyValueList * list_;
};

typedef std::shared_ptr<key_value_list const> key_value_list_const_ptr;
/** @} */ // key_value_list_group
/** @} */ // data_structures_group

/** \defgroup sequence_group Sequences
 * @{
 */
/**
 * \class sequence
 *
 * A sequence of arakoon commands.
 */
class sequence
{
  public:
    /** \brief Construct an empty sequence. */
    sequence();

    ~sequence();

    /**
     * \brief Add a 'set' action to the sequence. Key and value will be
     *        copied and released on destruction of the sequence.
     */
    void add_set(
        buffer const & key,
        buffer const & value);

    /**
     * \brief Add a 'delete' action to the sequence. Key will be
     *        copied and released on destruction of the sequence.
     */
    void add_delete(
        buffer const & key);

    /**
     * \brief Add an 'assert' action to the sequence. Key and value will be
     *        copied and released on destruction of the sequence.
     */
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
/** @} */ // sequence_group

/** \defgroup client_call_options_group Client call options
 * @{
 */
/**
 * \class client_call_options
 *
 * General options for any call to the arakoon server.
 */
class client_call_options
{
  public:
    /**
     * \brief Construct default client call options. See also
     *        ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_ALLOW_DIRTY and
     *        ARAKOON_CLIENT_CALL_OPTIONS_DEFAULT_TIMEOUT in arakoon.h.
     */
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
/** @} */ // client_call_options_group

/** \defgroup cluster_node_group Cluster nodes
 * @{
 */
/**
 * \class cluster_node
 *
 * Representation of an arakoon cluster node.
 */
class cluster_node
{
  public:
    /** \brief Construct a cluster node with the specified name.
     *         The name will be copied.
     */
    cluster_node(std::string const & name);

    ~cluster_node();

    /* \brief Add a TCP address to use to connect to the node.
     *        See the manual page of getaddrinfo(3) for the value of
     *        'host' and 'service'.
     */
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
/** @} */ // cluster_node_group

/** \defgroup cluster_group Clusters
 * @{
 */
/**
 * \class cluster
 *
 * Representation of an arakoon cluster node.
 */
class cluster
{
  public:
    /** \brief Construct a cluster with the specified cluster name.
     *         The cluster name will be copied.
     */
    cluster(ArakoonProtocolVersion version, std::string const & cluster_name);

    ~cluster();

    /**
     * \brief Attach a node to the cluster. The cluster assumes ownership of the
     *        cluster node's internals. Afterward, the cluster node should be
     *        destructed by the caller.
     */
    void add_node(cluster_node & node);

    /** \brief Return the cluster name. */
    std::string get_cluster_name() const;

    /**
     * \brief Look up the master node of the arakoon cluster and connect to it.
     * \param options Options, or NULL for default options.
     */
    void connect_master(
        client_call_options const * const options);

    /**
     * \brief Send a 'hello' call to the server, using 'client_id' and
     *        'cluster_id', and return the response.
     * \param options Options, or NULL for default options.
     */
    std::shared_ptr<std::string> hello(
        client_call_options const * const options,
        std::string const & client_id,
        std::string const & cluster_id);

    /**
     * \brief Send a 'who_master' call to the server, and return the name of
     *        the node that is currently the master node of the cluster.
     * \param options Options, or NULL for default options.
     */
    std::shared_ptr<std::string> who_master(
        client_call_options const * const options);

    /**
     * \brief Send an 'expect_progress_possible' call to the server.
     * \param options Options, or NULL for default options.
     */
    bool expect_progress_possible(
        client_call_options const * const options);

    /**
     * \brief Send an 'exists' call to the server.
     * \param options Options, or NULL for default options.
     */
    bool exists(
        client_call_options const * const options,
        buffer const & key);

    /**
     * \brief Send a 'get' call to the server.
     * \param options Options, or NULL for default options.
     */
    buffer_ptr get(
        client_call_options const * const options,
        buffer const & key);

    /**
     * \brief Send a 'get' call to the server.
     * \param options Options, or NULL for default options.
     * \return On success: a pair of ARAKOON_RC_SUCCESS and the value associated with the key.
     *         On error: a pair of error code and, if available, error buffer.
     */
    std::pair<rc, buffer_ptr> get_no_exc(
        client_call_options const * const options,
        buffer const & key);

    /**
     * \brief Send a 'multi-get' call to the server.
     * \param options Options, or NULL for default options.
     */
    value_list_const_ptr multi_get(
        client_call_options const * const options,
        value_list const & keys);

    /**
     * \brief Send a 'set' call to the server.
     * \param options Options, or NULL for default options.
     */
    void set(
        client_call_options const * const options,
        buffer const & key,
        buffer const & value);

    /**
     * \brief Send a 'delete' call to the server.
     * \param options Options, or NULL for default options.
     */
    void remove(
        client_call_options const * const options,
        buffer const & key);

    /**
     * \brief Send a 'range' call to the server.
     *        'begin_key' and 'end_key' can be set to (NULL, 0) to denote 'None'.
     *        If 'max_elements' is negative, all matches will be returned.
     * \param options Options, or NULL for default options.
     */
    value_list_const_ptr range(
        client_call_options const * const options,
        buffer const & begin_key,
        bool const begin_key_included,
        buffer const & end_key,
        bool const end_key_included,
        ssize_t const max_elements);

    /**
     * \brief Send a 'range_entries' call to the server.
     *        'begin_key' and 'end_key' can be set to (NULL, 0) to denote 'None'.
     *        If 'max_elements' is negative, all matches will be returned.
     * \param options Options, or NULL for default options.
     */
    key_value_list_const_ptr range_entries(
        client_call_options const * const options,
        buffer const & begin_key,
        bool const begin_key_included,
        buffer const & end_key,
        bool const end_key_included,
        ssize_t const max_elements);

    /**
     * \brief Send a 'rev_range_entries' call to the server.
     *        'begin_key' and 'end_key' can be set to (NULL, 0) to denote 'None'.
     *        If 'max_elements' is negative, all matches will be returned.
     * \param options Options, or NULL for default options.
     */
    key_value_list_const_ptr rev_range_entries(
        client_call_options const * const options,
        buffer const & begin_key,
        bool const begin_key_included,
        buffer const & end_key,
        bool const end_key_included,
        ssize_t const max_elements);

    /**
     * \brief Send a 'prefix' call to the server
     *        If 'max_elements' is negative, all matches will be returned.
     * \param options Options, or NULL for default options.
     */
    value_list_const_ptr prefix(
        client_call_options const * const options,
        buffer const & begin_key,
        ssize_t const max_elements);

    /**
     * \brief Send a 'test_and_set' call to the server
     *        'old_value' and 'new_value' can be (NULL, 0) to denote 'None'.
     *        A result of (NULL, 0) indicates that the server returned 'None'.
     * \param options Options, or NULL for default options.
     * \param key The key.
     * \param old_value The old value that should be tested.
     * \param new_value The new value that should be set if the test succeeds.
     * \return The
     */
    buffer_ptr test_and_set(
        client_call_options const * const options,
        buffer const & key,
        buffer const & old_value,
        buffer const & new_value);

    /**
     * \brief Send a 'sequence' call to the server.
     * \param options Options, or NULL for default options.
     * \param sequence The command sequence to execute.
     */
    void sequence(
        client_call_options const * const options,
        arakoon::sequence const & sequence);

    /**
     * \brief Send a 'sequence' call to the server.
     * \param options Options, or NULL for default options.
     * \param sequence The command sequence to execute.
     * \return A pair of error code and, if available, error buffer.
     */
    std::pair<rc, buffer_ptr> sequence_no_exc(
        client_call_options const * const options,
        arakoon::sequence const & sequence);

    /**
     * \brief Send a 'synced_sequence' call to the server.
     * \param options Options, or NULL for default options.
     * \param sequence The command sequence to execute.
     */
    void synced_sequence(
        client_call_options const * const options,
        arakoon::sequence const & sequence);

    /**
     * \brief Send a 'synced_sequence' call to the server.
     * \param options Options, or NULL for default options.
     * \param sequence The command sequence to execute.
     * \return A pair of error code and, if available, error buffer.
     */
    std::pair<rc, buffer_ptr> synced_sequence_no_exc(
        client_call_options const * const options,
        arakoon::sequence const & sequence);

  private:
    void rc_to_error(rc const rc);
    std::pair<rc, buffer_ptr> rc_to_error_no_exc(rc const rc);

    cluster(cluster const &) = delete;
    cluster & operator=(cluster const &) = delete;

    ArakoonCluster * cluster_;
};
/** @} */ // cluster_group

/** @} */ // arakoon_cpp_api_group

} // namespace arakoon

#endif // ARAKOON_HPP
