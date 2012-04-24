Crakoon
=======
Crakoon_ is a client for the Arakoon_ key-value store implemented in C. It has
no special dependencies on most POSIX systems, although it's only been tested on
GNU/Linux.

.. _Crakoon: https://github.com/Incubaid/crakoon
.. _Arakoon: http://arakoon.org

Building
--------
To build the library from repository source, you need to initialize the
autotools environment by executing

::

    $ autoreconf -i

Now configure, build and install the library as usual::

    $ ./configure --prefix=...
    $ make
    $ make install

Testing
-------
Some basic tests are included which use the *check* library. Execute the
test-suite by running

::

    $ make check

Some test utilities running against a real Arakoon server are provided as well,
you can run them using

::

    $ ./tests/test-arakoon-client clustername node1 host1 4321 node2 host2 9876
    $ ./tests/test-nursery-client clustername node1 host1 4321 node2 host2 9876

Documentation
-------------
Some documentation is provided in the 2 public header files which you can find
in *src/arakoon.h* and *src/arakoon-nursery.h* and define the public API. Make
sure to read the error handling information.

Caveats & TODO
--------------
Abort handling
~~~~~~~~~~~~~~
As of now, the library is not free of *abort(3)* calls: even though in most
situations appropriate error codes should be returned (including *ENOMEM* on
memory allocation failure, which you can override using
*arakoon_memory_set_hooks(arakoon_memory_get_abort_hooks())* or similar) in
some circumstances the library will call *abort*:

- When internal corruption is detected, e.g. an enum field has an unknown value
- Failure to fully write a command string, i.e. when a command string is not of
  precalculated length at the end of construction
- At 2 memory allocation sites when allocation fails. This should be fixed.

Socket handling
~~~~~~~~~~~~~~~
Currently socket handling is hardcoded around an internal call to *poll(2)* (to
support timeouts). This could be changed so the library can be hooked into
existing mainloops later on, although this is a non-trivial effort (the current
design is not callback/context-based).

Supported features
~~~~~~~~~~~~~~~~~~
Crakoon doesn't aim to support all Arakoon features at all times. If a required
feature is not supported, please file a support ticket, or issue a pull
request!
