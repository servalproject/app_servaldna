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

Operational concept
-------------------

The [OpenBTS][] software has these three components: **OpenBTS**, **smqueue**
and **sipauthserve**. Taken as a whole these allow a GSM phone to register with
the OpenBTS unit and then make and receive SIP calls. 

The [Serval DNA][] daemon communicates with other daemons running on other
nodes on the [Serval mesh network][] using [MDP][] encapsulated within
[UDP/IP][] over [Wi-Fi][].

Every [Serval DNA][] daemon has its own [Serval Identity (SID)][SID], which is
a unique public key in an [elliptic curve cryptosystem][crypto].  The SID is
used as the mesh network address of the node.  Every node also has a [phone
number (DID)][DID] which may or may not be unique.

The [Serval DNA][] daemon uses the [DNA][] protocol to resolve a phone number
into a SID by broadcasting a DNA Lookup request and waiting for responses in
the form of a [URI][].  Every [Serval DNA][] daemon responds to DNA requests
for its own phone number by returning a VoMP URI containing its own [SID][] and
its own [DID][].  If a daemon is configured with a [DNA Helper][] script, then
it will invoke the script whenever a DNA Lookup request is received, and return
any URI that the script produces to the requestor.  This allows a daemon to act
as a proxy for more phone numbers than the one assigned to it.

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
connected via Wi-Fi using the Serval mesh.

### GSM-to-GSM call via Serval mesh

This describes the steps involved when a GSM phone in A's cell calls a GSM
phone in B's cell

1. The OpenBTS component on A asks Asterisk to resolve the number.  The number
is not found in A's subscriber registry, so the Asterisk dial plan invokes the
[AGI][] script which causes [Serval DNA][] to broadcast a [DNA][] request for
B's number.

2. The [Serval DNA][] daemon on B receives the [DNA][] request and invokes the
[num2sip.py][] script, which consults B's subscriber registry.  It finds the
number and responds to A with a URI containing B's [SID][] and the requested
number.

3. The [Serval DNA][] daemon on A receives the response and replies to the
waiting [AGI][] script, which returns the URI to Asterisk.  The Asterisk dial
plan handles the URI by using the VoMP channel driver to instruct its [Serval
DNA][] daemon to send a VoMP CALL request to B, listing a set of supported VoMP
codecs.

4. The VoMP CALL request is received by B's [Serval DNA][] daemon, which
notifies all its *monitor interface* clients, which in this case is only the
Asterisk channel driver.  Asterisk responds to the notification by initiating a
call to the GSM phone via the OpenBTS component, then replies to the VoMP CALL
request with VoMP PICKUP nominating the VoMP codec to use.

5. The [Serval DNA][] daemon on A receives the VoMP PICKUP reply and starts
streaming audio through the *monitor interface* to Asterisk.  The [Serval
DNA][] daemon on B does the same.  The OpenBTS software transcodes the GSM
audio to Asterisk's internal audio format, and the [Serval DNA][] daemon
transcodes Asterisk's audio format to the negotiated VoMP codec.

### GSM-to-Serval calls

This describes the steps involved when a GSM phone in A's cell calls any
[Serval Mesh][] smart-phone on the [Serval mesh network][].

TBC

### Serval-to-GSM calls

This describes the steps involved when a [Serval Mesh][] smart-phone on the
[Serval mesh network][] calls a GSM phone in A's cell.

TBC

### Serval-to-Serval calls

This describes the steps involved when a [Serval Mesh][] smart-phone on the
[Serval mesh network][] calls another [Serval Mesh][] smart-phone.

TBC



[Serval Project]: http://www.servalproject.org/
[Commotion OpenBTS]: https://commotionwireless.net/projects/openbts
[Serval DNA]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:servaldna:
[Serval mesh network]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:mesh_network
[Serval Mesh]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:servalmesh:
[integrated]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:commotion_openbts
[funding]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:activity:naf1
[OTI]: http://oti.newamerica.net/
[NAF]: http://www.newamerica.net/
[OpenBTS]: http://wush.net/trac/rangepublic/wiki
[Asterisk 1.8]: http://www.asterisk.org/downloads/asterisk-news/asterisk-180-released
[Serval DNA 48c899d]: https://github.com/servalproject/serval-dna/commit/48c899df39be2ab9fc2ec5e83cf61beaffcdccfe
[GSM]: http://en.wikipedia.org/wiki/GSM
[VoMP]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:vomp
[MDP]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:mdp
[UDP/IP]: https://en.wikipedia.org/wiki/User_Datagram_Protocol
[Wi-Fi]: http://en.wikipedia.org/wiki/Wi-Fi
[DNA]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:dna
[DNA Helper]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:dna_helper
[DID]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:did
[SID]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:sid
[crypto]: http://developer.servalproject.org/dokuwiki/doku.php?id=content:tech:crypto
[AGI]: http://en.wikipedia.org/wiki/Asterisk_Gateway_Interface
[URI]: http://en.wikipedia.org/wiki/Uniform_resource_identifier
[num2sip.py]: ./conf_adv/num2sip.py
[servaldnaagi.py]: ./servaldnaagi.py
