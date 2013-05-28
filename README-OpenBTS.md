Integration of Serval DNA with Commotion OpenBTS
================================================
[Serval Project][], May 2013

[Serval DNA][] has been [integrated][] into the [Commotion OpenBTS][] GSM base
station with [funding][] provided by the [Open Technology Institute][OTI] which
is part of the [New America Foundation][NAF].

The integration connects conventional [GSM][] phones to the [Serval mesh
network][], connects [Serval Mesh][] phones to the OpenBTS GSM network, and
extends the reach of Commotion's GSM coverage by providing a way to securely
mesh OpenBTS base stations together over Wi-Fi to form a single network instead
of a set of isolated cells.

The integration is done using the [VoMP Channel Driver for
Asterisk](./README.md) which allows [Asterisk 1.8][] to send and receive
[VoMP][] calls via the [Serval DNA][] daemon.  See the [README](./README.md)
file for more information about the channel driver, including concept of
operation, downloading, building and configuring.

The integration was originally tested using [OpenBTS][] 2.8 with Asterisk
1.8.14.1 and [Serval DNA 48c899d][].

Concept of operation
--------------------

An OpenBTS unit runs the following software:

 * The [OpenBTS][] software has three components: **OpenBTS**, **smqueue** and
   **sipauthserve**, which together allow a GSM phone to register with the
   OpenBTS unit and then make and receive [SIP][] calls.  Essentially, an
   OpenBTS unit acts as a single cell in a cellular mobile network.

 * The [Serval DNA][] daemon communicates with other daemons running on other
   nodes on the [Serval mesh network][] using [MDP][] encapsulated within
   [UDP/IP][] over [Wi-Fi][].

Every [Serval DNA][] daemon has its own [Serval Identity (SID)][SID], which is
a unique public key in an [elliptic curve cryptosystem][crypto].  The SID is
used as the mesh network address of the node.  Every node also has a [phone
number (DID)][DID] which may or may not be unique.

When placing a [VoMP][] phone call, the [Serval DNA][] daemon first uses the
[DNA][] protocol to resolve the called phone number to a SID by broadcasting a
DNA Lookup request and waiting for responses in the form of [URI][]s.  Every
[Serval DNA][] daemon on a reachable node responds to DNA requests for its own
phone number by returning a VoMP URI containing its own [SID][] and its own
[DID][].  If a daemon is configured with a [DNA Helper][] script, then it will
invoke the script whenever a DNA Lookup request is received, and return any URI
that the script produces to the requestor.  This allows a daemon to act as a
proxy for more phone numbers than just its own [DID][].

For OpenBTS integration, the [DNA Helper][] script [num2sip.py][] was
developed, which looks up the [DID][] in the OpenBTS subscriber registry (an
SQLite database).  If there is a match then `num2sip.py` returns a VoMP URI
containing the Serval DNA daemon's own [SID][] and the requested number,
indicating that the daemon will accept calls to that number.

For general Asterisk integration, the [AGI][] script [servaldnaagi.py][] was
developed, which asks the [Serval DNA][] to perform a [DNA][] lookup for a
given number, and returns the results in a form that can be used by an Asterisk
dial plan.

The [Serval DNA][] daemon exposes a local interface called the *monitor
interface* which provides operations for performing [VoMP][] calls with other
Serval nodes, including call establishment, codec negotiation, call state
management, bidirectional audio streaming, and call tear-down.  The OpenBTS
integration uses the [VoMP channel driver](./README.md) to pass calls to and
from the [Asterisk 1.8][] PBX daemon.

The following use cases consider two OpenBTS units, **A** and **B** which are
connected via Wi-Fi using the Serval mesh.  The [VoMP][] protocol is documented
in the [vomp.c][] source file in [serval-dna][].

### Establishment of GSM-to-GSM call via Serval mesh

This describes in detail the steps involved when a GSM phone in A's cell calls
a GSM phone in B's cell

0. The channel driver on each unit has already announced its set of supported
codecs to the [Serval DNA][] daemon using the *monitor interface* MONITOR VOMP
command, sent at start up.

1. The GSM phone initiates the call through the OpenBTS software on A.

2. The OpenBTS component on A asks Asterisk to resolve the number.  The number
is not found in A's subscriber registry, so the Asterisk dial plan invokes the
[AGI][] script which in turn invokes [Serval DNA][] to broadcast a [DNA][]
request for the number.

3. The [Serval DNA][] daemon on B receives the [DNA][] request and invokes the
[num2sip.py][] [DNA Helper][] script, which consults B's subscriber registry.
It finds the number and responds to A with a VoMP URI containing B's [SID][]
and the requested number.

4. The [Serval DNA][] daemon on A passes the URI to the waiting [AGI][] script,
which returns it to Asterisk.  The Asterisk dial plan handles the URI by
sending a CALL command via the VoMP channel driver to A'a [Serval DNA][]
daemon.

5. A's [Serval DNA][] daemon sends a VoMP CALLPREP message to B, listing its
supported codecs (from step 0), declaring a new per-session token, and giving
the [SID][]s and [DID][]s of the calling and called party.

6. The VoMP CALLPREP message is received by B's [Serval DNA][] daemon, which
replies with a matching NOCALL message to A, listing B's supported codecs.

7. A's [Serval DNA][] daemon receives the NOCALL message and chooses a codec
from the list of codecs that B supports.  A then responds with a VoMP RINGOUT
message to B.

8. B's [Serval DNA][] daemon receives the VoMP RINGOUT message from A,
whereupon it notifies the channel driver of A's supported codecs by sending the
CODECS command through the *monitor interface*, then immediately notifies of
the incoming call by sending the CALLFROM command via the *monitor interface*,
which contains the [DID][]s and [SID][]s of the calling and called parties.

9. The channel driver on B receives the CODECS notification and chooses a codec
from A's supported codec list, then receives the CALLFROM notification and
activates B's Asterisk's dial plan with the called [DID][].  Asterisk resolves
the [DID][] to one of B's connected GSM phones and initiates a call to that GSM
phone via the OpenBTS component.

10. When B's OpenBTS reports to Asterisk that the GSM phone is ringing,
Asterisk sends a RING command via the channel driver to B's [Serval DNA][]
daemon, which sends a VoMP RINGIN message to A.

11. A's [Serval DNA][] daemon receives the VoMP RINGIN message and sends a
notification via the *monitor interface* to A's channel driver, which causes
Asterisk or OpenBTS to start playing the “ring ring” audio signal to the
calling GSM phone.

12. When the called GSM phone is answered, the OpenBTS component in B signals
to Asterisk that the call is in progress, and Asterisk sends a PICKUP command
via the channel driver to B's [Serval DNA][] daemon.  The daemon sends a VoMP
INCALL message to A and starts streaming encoded audio via the *monitor
interface*.

13. The [Serval DNA][] daemon on A receives the VoMP INCALL message and replies
with its own INCALL message.  The [Serval DNA][] daemon on B receives the
message, and the VoMP call is now established.

14. Every VoMP audio stream embeds a VoMP codec identifier in every packet of
audio.  The channel driver on each unit translates this identifier into
Asterisk's internal codec number, and Asterisk encodes and decodes the audio.

### Establishment of GSM-to-Serval call

This describes the steps involved when a GSM phone in A's cell calls any
[Serval Mesh][] smart-phone on the [Serval mesh network][].

0. The channel driver on A has already announced its set of supported codecs to
the [Serval DNA][] daemon using the *monitor interface* MONITOR VOMP command,
sent at start up.

1. The GSM phone initiates the call through the OpenBTS software on A.

2. The OpenBTS component on A asks Asterisk to resolve the number.  The number
is not found in A's subscriber registry, so the Asterisk dial plan invokes the
[AGI][] script which in turn invokes [Serval DNA][] to broadcast a [DNA][]
request for the number.

3. The [Serval DNA][] daemons on all [Serval Mesh][] phones and all other
OpenBTS units receive the [DNA][] request.  On the OpenBTS units, the [Serval
DNA][] daemons invoke their [num2sip.py][] [DNA Helper][] scripts, which
consult the subscriber registries, but do not find any match, so send no reply.
However, a [Serval Mesh][] phone with the called number responds to A with a
VoMP URI containing its own [SID][] and [DID][].

4. The rest of the call establishment follows the same remaining steps as
described in the GSM-to-GSM case above, except that instead of a [Serval DNA][]
daemon on another OpenBTS unit, A is communicating with the [Serval DNA][]
daemon on the responding [Serval Mesh][] phone.  The VoMP protocol, codec
negotiation, ringing and call pick-up all work the same way.

5. Audio encoding and decoding on the on the [Serval Mesh][] phone is performed
by the [Batphone][] Java app, typically by invoking a native codec library
through a [JNI][] interface.

### Establishment of Serval-to-GSM call

This describes the steps involved when a [Serval Mesh][] smart-phone on the
[Serval mesh network][] calls a GSM phone in A's cell.

0. The channel driver on A has already announced its set of supported codecs to
the [Serval DNA][] daemon using the *monitor interface* MONITOR VOMP command,
sent at start up.

1. The [Serval Mesh][] phone broadcasts a [DNA][] request for the number.

2. The [Serval DNA][] daemon on B receives the [DNA][] request and invokes the
[num2sip.py][] [DNA Helper][] script, which consults B's subscriber registry.
It finds the number and responds to B with a VoMP URI containing B's [SID][]
and the requested number.

4. The rest of the call establishment follows the same remaining steps as
described in the GSM-to-GSM case above, except that instead of a [Serval DNA][]
daemon on an originating OpenBTS unit, B is communicating with the [Serval
DNA][] daemon on the originating [Serval Mesh][] phone.  The VoMP protocol,
codec negotiation, ringing and call pick-up all work the same way.

5. Audio encoding and decoding on the on the [Serval Mesh][] phone is performed
by the [Batphone][] Java app, typically by invoking a native codec library
through a [JNI][] interface.

Building and installing
-----------------------

The channel driver [README](./README.md) file has complete instructions for
downloading, building and configuring [Serval DNA][] and the channel driver.

Configuration
-------------

The sample configuration in the [conf\_adv](./conf_adv/) directory was used for
developing and testing the OpenBTS integration, so this is the best starting
point.

See [conf\_adv/README.md](./conf_adv/README.md) for more information, including
installation instructions.

Startup scripts
---------------

The OpenBTS unit must be set up to start the [Serval DNA][] daemon when the
OpenBTS daemon, smqueue and sipauthserve are started.  For testing, this was
done by adding a `/usr/sbin/servald start` command to the `/etc/rc.local`
script.

Testing
-------

Once the [Serval DNA][] daemon is running, use the following command to check
that it has created a single [Serval identity (SID)][SID].  It should just print
a single line of 64 hex digits, which is the SID:

    $ /usr/sbin/servald id self
    B435783C267A7262FDC0150F465746A66426305CE080B1683E0211C3582DBC49
    $

Next, check that GSM subscribers registered to the OpenBTS unit are detected by
the [DNA][] Lookup command.  Use a phone number of a GSM phone that is known to
be associated with the OpenBTS unit.  This should print a single line
containing a “sid:” URI with the [SID][] of the unit's identity, and the
requested number:

    $ /usr/sbin/servald dna lookup 12345678
    sid://B435783C267A7262FDC0150F465746A66426305CE080B1683E0211C3582DBC49/12345678:12345678:
    $

You can use the same command to look up the GSM numbers of phones associated
with other OpenBTS units to check that end-to-end lookup is working across the
mesh.

Known issues
------------

* During testing, OpenBTS seems quite unreliable.  Calls often failed to go
  through and there were problems registering. This may be addressed in later
  versions of OpenBTS.

* The AGI script is runs once per query and invokes [Serval DNA][] each time.
  This could be optimised if `app_serval.so` could do [DNA][] lookups directly
  over the *monitor interface*.

* At the time of testing, multi-hop [VoMP][] calls did not work due to immature
  [Serval mesh routing][] and a lack of compressed VoMP codecs.  This was
  mitigated by modifying `num2sip.py` to only return SIP URIs, causing
  inter-OpenBTS calls to bypass [VoMP][] in favour of [TCP/IP][] over
  [Wi-Fi][].  This would not be a production-ready solution in a typical
  wireless mesh, which generally has such a high packet loss rate end-to-end
  that [TCP/IP][] connections frequently fail.

* The [Serval DNA][] daemon does not currently cooperate with the [OLSR][]
  routing daemon, so each hop between OpenBTS units must be running a [Serval
  DNA][] daemon.

About the examples
------------------

The examples in this document are [Bourne shell][] commands, using standard
quoting and variable expansion.  Commands issued by the user are prefixed with
the shell prompt `$` or `#` to distinguish them from the output of the command.
A prompt of `#` indicates that the command must be executed as the super user.
Single and double quotes around arguments are part of the shell syntax, so are
not seen by the command.  Lines ending in backslash `\` continue the command on
the next line.

The directory paths used in the examples are for illustrative purposes only,
and are not the recommended values for a production system.  They coincide with
the paths used in [README.md](./README.md).


[Serval Project]: http://www.servalproject.org/
[Commotion OpenBTS]: https://commotionwireless.net/projects/openbts
[Serval DNA]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:servaldna:
[Serval mesh network]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:mesh_network
[Serval mesh routing]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:mesh_routing
[Serval Mesh]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:servalmesh:
[Batphone]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:servalmesh:
[integrated]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:commotion_openbts
[funding]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:activity:naf1
[OTI]: http://oti.newamerica.net/
[NAF]: http://www.newamerica.net/
[OpenBTS]: http://wush.net/trac/rangepublic/wiki
[Asterisk 1.8]: http://www.asterisk.org/downloads/asterisk-news/asterisk-180-released
[Serval DNA 48c899d]: https://github.com/servalproject/serval-dna/commit/48c899df39be2ab9fc2ec5e83cf61beaffcdccfe
[GSM]: http://en.wikipedia.org/wiki/GSM
[SIP]: http://en.wikipedia.org/wiki/Session_Initiation_Protocol
[VoMP]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:vomp
[MDP]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:mdp
[TCP/IP]: http://en.wikipedia.org/wiki/Transmission_Control_Protocol
[UDP/IP]: https://en.wikipedia.org/wiki/User_Datagram_Protocol
[Wi-Fi]: http://en.wikipedia.org/wiki/Wi-Fi
[DNA]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:dna
[DNA Helper]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:dna_helper
[DID]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:did
[SID]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:sid
[crypto]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:crypto
[AGI]: http://en.wikipedia.org/wiki/Asterisk_Gateway_Interface
[URI]: http://en.wikipedia.org/wiki/Uniform_resource_identifier
[JNI]: http://en.wikipedia.org/wiki/Java_Native_Interface
[OLSR]: http://www.olsr.org/
[num2sip.py]: ./conf_adv/num2sip.py
[servaldnaagi.py]: ./servaldnaagi.py
[serval-dna]: https://github.com/servalproject/serval-dna
[vomp.c]: https://github.com/servalproject/serval-dna/blob/development/vomp.c
[Bourne shell]: http://en.wikipedia.org/wiki/Bourne_shell
