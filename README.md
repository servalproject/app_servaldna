Asterisk Channel Driver for Serval VoMP
=======================================

This repository contains the source code for the [Serval Project][]'s [VoMP][]
channel driver for [Asterisk][] implemented in [GNU C][], together with an
[AGI][] script in [Python][] and a set of configuration file templates for
Asterisk.

Dependencies
------------

Before the VoMP Asterisk channel driver can be built:

  * Asterisk must be built from source code and installed, eg:

        $ cd $HOME/src
        $ wget -q http://downloads.asterisk.org/pub/telephony/asterisk/asterisk-1.8-current.tar.gz
        $ tar xzf asterisk-1.8-current.tar.gz
        $ cd asterisk-1.8.20.0
        $ ./configure
        ...
        $ make
        ...
        $ make install
        ...
        $ make config
        ...
        $

    (You may have to edit `/etc/defaults/asterisk` to set the `AST_USER` and
    `AST_GROUP` variables appropriately.)

  * [Serval DNA][] must be built from source code, eg:

        $ cd $HOME/src
        $ git clone -q git://github.com/servalproject/serval-dna.git
        $ cd serval-dna
        $

    Follow the build instructions in [serval-dna/INSTALL.md][], then install
    the built executables, eg:

        $ cp -p servald directory_service /usr/sbin
        $

To build
--------

The current working directory must be `app_servaldna`.

The `AST_ROOT` and `SERVAL_ROOT` variables defined at the top of the
[Makefile](./Makefile) must be set to the directories containing the built
Asterisk source code and the built [serval-dna][] source code respectively.
This can be done in two ways:

  * EITHER edit the Makefile then run the `make` command,

  * OR pass arguments to the `make` command, eg:

        $ make AST_ROOT=$HOME/src/asterisk-1.8.20.0 SERVAL_ROOT=$HOME/src/serval-dna
        $

Built artifacts
---------------

The build process produces the following artifacts:

  * **app_servaldna.so** is the VoMP channel driver module shared library for
    Asterisk

  * **servaldnaagi.py** is an AGI script invoked by the call plan to resolve a DID
    (phone number) to a SID (Serval subscriber identifier) or SIP URI

  * the **conf_adv/** directory contains Asterisk configuration files

To install
----------

Copy the built artifacts into the proper Asterisk configuration directories:

    $ cp -p app_servaldna.so /usr/lib/asterisk/modules
    $ cp -p servaldnaagi.py /usr/lib/asterisk
    $ cp -a conf_adv/* /etc/asterisk

Edit the copied `/etc/asterisk/extensions.conf` to set the following variables:

    SERVAL_AGI = /usr/lib/asterisk/servaldnaagi.py
    SERVAL_BIN = /usr/sbin/servald
    SERVALD_INSTANCE = /var/serval-node

Edit the copied `/etc/asterisk/servaldna.conf` to set the following variables:

    instancepath = /var/serval-node

Create the [Serval DNA][] instance directory:

    $ mkdir /var/serval-node

Configure [Serval DNA][] by following the instructions in
[serval-dna/doc/Servald-Configuration.md][], and adding the following line to
`/var/serval-node/serval.conf`:

    dna.helper.executable=/usr/sbin/directory_service


About the examples
------------------

The examples in this document are [Bourne shell][] commands, using standard
quoting and variable expansion.  Commands issued by the user are prefixed with
the shell prompt `$` to distinguish them from the output of the command.
Single and double quotes around arguments are part of the shell syntax, so are
not seen by the command.  Lines ending in backslash `\` continue the command on
the next line.

The directory paths used in the examples are for illustrative purposes only,
and are not the recommended values for a production system.

Copyright and licensing
-----------------------

The VoMP Asterisk channel driver is [free software][] initially produced by the
[Serval Project][].  It is licensed to the public under the [GNU General Public
License version 2][GPL2].  All source code is freely available from the Serval
Project's [app\_servaldna][] Git repository on [GitHub][].

The copyright in the source code is held by [Serval Project Inc.][SPI], an
organisation incorporated in the state of South Australia in the Commonwealth
of Australia for the purpose of developing the Serval mesh software.

The [Serval Project][] will accept contributions from individual developers who
have agreed to the [Serval Project Developer Agreement - Individual][individ],
and from organisations that have agreed to the [Serval Project Developer
Agreement - Entity][entity].


[Serval Project]: http://www.servalproject.org/
[Serval DNA]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:servaldna:
[serval-dna]: https://github.com/servalproject/serval-dna
[serval-dna/INSTALL.md]: https://github.com/servalproject/serval-dna/blob/development/INSTALL.md
[serval-dna/doc/Servald-Configuration.md]: https://github.com/servalproject/serval-dna/blob/development/doc/Servald-Configuration.md
[app\_servaldna]: https://github.com/servalproject/app_servaldna
[SPI]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:spi
[VoMP]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:vomp
[Asterisk]: http://www.asterisk.org/
[AGI]: http://en.wikipedia.org/wiki/Asterisk_Gateway_Interface
[GNU C]: http://gcc.gnu.org/
[Python]: http://python.org/
[free software]: http://www.gnu.org/philosophy/free-sw.html
[GitHub]: https://github.com/servalproject
[GPL2]: http://www.gnu.org/licenses/gpl-2.0.html
[Bourne shell]: http://en.wikipedia.org/wiki/Bourne_shell
[individ]: http://developer.servalproject.org/files/serval_project_inc-individual.pdf
[entity]: http://developer.servalproject.org/files/serval_project_inc-entity.pdf
