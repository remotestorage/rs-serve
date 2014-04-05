rs-serve - A remoteStorage server implementation
================================================

Content:

1. Introduction
2. Overview
    1. remoteStorage
    2. Webfinger
    3. Authorization tools
    4. Storage system
3. Installing
    1. Dependencies
    2. Getting the code
    3. Building
    4. Installing system-wide
    5. Setting options
    6. Integrating authorization

1) Introduction
---------------

remoteStorage is an open specification for personal data storage. It is supposed
to replace the currently popular proprietary "cloud storage" protocols using an
open standard and thereby promoting the seperation of applications and their
data on the web.

For more information, check out these links:

  * http://remotestorage.io/ - Information about the remotestorage protocol
                               and current implementations.
  * http://unhosted.org/     - Philosophy, hands-on Tutorials and App collection.

2) Overview
-----------

rs-serve brings 3 things:

* HTTP endpoint implementing remoteStorage: `/storage/{user}`
* HTTP endpoint implementing Webfinger: `/.well-known/webfinger`
* A collection of scripts to manage authorizations: add/remove/list token(s)

The user management is taken care of by the system. Each system user with an
allowed user id (default: >= 1000. Minimum defined by `RS_MIN_UID` in
`src/config.h`) can access their `~/storage/ directory` (configurable via
`--dir` option)  using the remoteStorage endpoint.

rs-serve is written entirely in C, using mostly POSIX library functions. It
relies on a few portable libraries (see the list under "Dependencies" below).

It does however currently use the `signalfd()` system call, which is only
available on Linux. (this is a solvable problem though, if you want to
be able to run on another system, please open an issue to ask for help.)

2.1) remoteStorage
------------------

The currently implemented protocol version is "draft-dejong-remotestorage-01".

Currently the following features are supported:

* CORS support for all verbs
* GET, PUT, DELETE requests on files and folders
* Opaque version strings (in directory listings and `ETag` header)
* Conditional GET, PUT and DELETE requests (`If-Match`, `If-None-Match` headers)
* Protection of all non-public paths via Bearer token authorization.
* Special handling of public paths (i.e. those starting with `/public/`), such that
  requests on non-directory paths succeed without authorization.
* HEAD requests on files and folders with `Content-Length` header
  (not part of remotestorage-01, only enabled when `--experimental` flag is given)

2.2) Webfinger
--------------

The Webfinger implementation only serves information about remoteStorage
and is currently not extensible.

The hostname part of user addresses is expected to be the hostname set for
the rs-serve instance. This currently defaults to `local.dev` and can be
overridden with the `--hostname` option.

Virtual hosting (== hosting storage for multiple domains from a single
instance) is currently not supported.

2.3) Authorization
------------------

rs-serve now comes with an authorization backend and frontend, supporting the
implicit bearer flow as described by OAuth 2. Authentication happens through PAM,
so you can use any authentication backend supported by PAM (such as
passwd/shadow files, LDAP, SQL...).

The authorization server is written in JavaScript. To run the server-side
component you need node.js. Also you need to build the bindings to the
authorization tools of rs-serve.

You can do this by running:

    make bindings

To start the server run

    node auth/backend/server.js

It runs on port 8888 by default, you can change this by tweaking the
auth/backend/server.js file. Note that you also need to configure the backend
URL in auth/frontend/app.js.

The frontend part is an unhosted web app (i.e. completely client side), so you
can use any webserver to serve it. However, for simplicity the backend server
will also automatically serve all files from auth/frontend/.

Once you got that all running, set the `--auth-uri` option of rs-serve to point
to where you're serving the frontend, e.g. `https://example.com/?username=%s`.

Note that while the remotestorage server itself needs to run on port 80, the
authorization frontend and backend can run on any port you like.

2.4) Storage system
-------------------

The payload data of the remotestorage endpoint is stored on the local filesystem
within the respective user's home directory.

Thus a few restrictions apply:

* The remotestorage endpoint cannot be used to store both a directory and a file
  under the same path (ignoring the trailing slash). That means you cannot store
  `/foo/bar/baz` and `/foo/bar`, but only one of them. This is a natural restriction
  of traditional filesystems, that is currently well adhered to by all apps using
  remotestorage (as far as I know).

* MIME types may not be exact for files that were added "out-of-band", that is
  not added via the remoteStorage protocol, but by copying to the `~/storage/`
  directory by other means. rs-serve stores MIME type and character encoding
  under the `user.mime_type` and `user.charset` extended attributes, given these
  are supported by the underlying filesystem. When these attributes aren't set,
  a MIME type is guessed using libmagic, which may not always yield desirable
  results. (for example an empty file, created using `touch` will be transmitted
  via remoteStorage with a Content-Type header of `inode/x-empty; charset=binary`)
  If even libmagic fails to make sense of a file, the `Content-Type` is set to
  `application/octet-stream; charset=binary`.

3) Installing
-------------

These steps should enable you to install rs-serve.

3.1) Dependencies
-----------------

- GNU make
- cmake
- pkg-config (or tweak the Makefile)
- gcc
- libc
- libevent (>= 2.0)
- libmagic
- libattr
- BerkeleyDB

On Debian based systems, this should give you all you need:

    apt-get install build-essential cmake libevent-dev libmagic-dev libattr1-dev libssl-dev libdb-dev pkg-config

If you want to develop, you may also want debug symbols and valgrind (required by
leakcheck.sh script):

    apt-get install libevent-dbg valgrind

3.2) Getting the code
---------------------

Given you are reading this file, you probably have the code already, but just to
be sure:

Currently the rs-serve code is hosted on GitHub.

You can browse it online at https://github.com/remotestorage/rs-serve or clone it using git:

    git clone git://github.com/remotestorage/rs-serve.git

3.3) Building
-------------

Given you have all dependencies installed, simply run

    make

and you should be good to go.

3.4) Installing system-wide
---------------------------

To install the rs-serve binary to `/usr/bin`, run

    make install

as a privileged user.

To install somewhere else, tweak the Makefile first.

This will also install an init script to `/etc/init.d/rs-serve` and a default
configuration to `/etc/default/rs-serve`.

On Debian based systems (i.e. when `update-rc.d` is present), `make install`
will also install the rs-serve init script into `/etc/rc*.d/`.

3.5) Setting options
--------------------

There are a variety of options.

If you want to use the init script, you can set options in `/etc/default/rs-serve`,
otherwise just pass them on the command line.

Run:

    rs-serve --help

to get a list of supported options.

3.6) Integrating authorization
------------------------------

To integrate an authorization endpoint, you need to do two things:

* Configure endpoint URI

  Set the `--auth-uri` option to a printf style format string. `%s` will be
  replaced with the username.

* Configure your authorization endpoint to manage rs-serve tokens

  rs-serve doesn't care where tokens come from, but it need to know them to
  decide whether a given request is authorized or not. It maintains an internal
  store for authorizations (i.e. structures of [user-name, token, scopes]),
  which must be managed from the outside.

  The tools to do this are:

  * `rs-add-token`:

        Usage: rs-add-token <user> <token> <scope1> [<scope2> ... <scopeN>]

    - `<user>` is the login name of the user (rs-serve must be able to resolve
      it using getpwnam() in order to find the home directory)
    - `<token>` is the token string authenticating future requests. For
      rs-serve it is an opaque string.
    - `<scope1>..<scopeN>` are scope strings in the same form as described in
      draft-dejong-remotestorage-01, Section 9.

  * `rs-remove-token`:

        Usage: rs-remove-token <user> <token>

    `<user>` and `<token>` must both be given. If the token cannot be found,
    `rs-remove-token` terminates with non-zero status.

  * `rs-list-tokens`:

    Lists all currently installed tokens and their respective scopes.

    The output format is primarily meant for (human) debugging and subject to change.

4) Contributing
---------------

* If you've found a bug, or have any questions, please open an issue on GitHub:
  https://github.com/remotestoage/rs-serve/issues

* If you want to contribute, fork the project on GitHub and send pull requests.

* In any case, don't hesitate to talk with us on IRC: #remotestorage and #unhosted,
  both on irc.freenode.org