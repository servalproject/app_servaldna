#!/usr/bin/env python

#
# Copyright 2012 Serval Project Inc
#
# Author: Daniel O'Connor <daniel@servalproject.org>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

# This is an AGI script to call servald to resolve a DID to a SID or SIP URI.
#
# An example dialplan is as follows
#
# [globals]
# SERVALD_AGI=/Users/darius/projects/serval/app_servaldna/servaldnaagi.py
# SERVALD_BIN=/Users/darius/projects/serval/batphone/jni/serval-dna/servald
# SERVALD_INSTANCE=/Users/darius/projects/serval/instance
#
# ; Try and resolve using script which asks servald
# exten => _X.,1,AGI(${SERVALD_AGI},${SERVALD_BIN},${SERVALD_INSTANCE},${EXTEN})
#    same => n,Verbose(AGI has run)
#    same => n,Goto(${AGISTATUS})
# ; Lookup failed (check servald is running etc)
#    same => n(FAILURE),Playback(tt-weasels)
#    same => n,Verbose(weasels)
#    same => n,Hangup()
# ; Lookup worked
#    same => n(SUCCESS),Verbose(lookup done)
#    same => n,Goto(${SDNAAGI_STATUS})
# ; Actually resolved something try and dial it
#    same => n(RESOLVED),Dial(${SDNAAGI_DEST},25)
#    same => n,Hangup()
# ; Couldn't find something for this DID
#    same => n(UNRESOLVED),Playback(ss-noservice)
#    same => n,Verbose(unresolved)
#    same => n,Hangup()
#
# This is slow because we are a Python script and we have to fork
# servald.  When the monitor interface grows a way to do queries then
# we can write a proper extension.
#
# Note that servald must be running as the same user as asterisk for this to work.
#
import os
import subprocess
import sys

def println(s):
    sys.stdout.write(s)
    sys.stdout.write('\n')
    sys.stdout.flush()

def debug(s):
    #sys.stderr.write(s)
    #sys.stderr.write('\n')
    #sys.stderr.flush()
    println("VERBOSE \"%s\"" % s)
    
def main():
    debug('started')
    
    # Consume all variables sent by Asterisk
    vars = {}
    while True:
        line = sys.stdin.readline().strip()
        if line == '':
            break
        (key, val) = line.split(':', 1)
        vars[key] = val.strip()

    if 'agi_arg_1' not in vars or 'agi_arg_2' not in vars or 'agi_arg_3' not in vars:
        debug('Not enough arguments, need servald binary path, instance directory and number to lookup')
        sys.exit(1)

    binpath = vars['agi_arg_1']
    instancedir = vars['agi_arg_2']
    did = vars['agi_arg_3']

    os.environ['SERVALINSTANCE_PATH'] = instancedir
    servaldproc = subprocess.Popen([binpath, 'dna', 'lookup', did], stdout = subprocess.PIPE)
    servaldres = servaldproc.wait()
    (servaldout, servalderr) = servaldproc.communicate()
    if servaldres != 0:
        debug("Servald returned %d" % (servaldres))
        sys.exit(1)
        
    lines = servaldout.split('\n')

    # Grab first URI
    line = lines[0]
    # It is the first field, second is DID, third is name
    # XXX: servald uses : for the field separator but does not escape it inside a field (this should be fixed).
    # For now kludge around it
    uri = line[0:6] + line[6:].split(':')[0]
    debug("Looking at " + uri)
    
    if uri == '':
        println('SET VARIABLE "SDNAAGI_STATUS" "UNRESOLVED"')
        sys.stdin.readline()
        sys.exit(0)
    # Mangle
    method = uri[0:6].lower()
    if method == 'sip://':
        varval = 'SIP/' + uri[6:]
    elif method == 'sid://':
        varval = 'VOMP/' + uri[6:]
    else:
        debug("Unknown method for URI " + uri)
        sys.exit(1)

    debug("Resolved it!")
    println('SET VARIABLE "SDNAAGI_STATUS" "RESOLVED"')
    sys.stdin.readline()

    println('SET VARIABLE "SDNAAGI_DEST" "%s"' % (varval))
    sys.stdin.readline()

    sys.exit(0)

if __name__ == "__main__":
    main()
