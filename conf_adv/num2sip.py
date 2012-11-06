#!/usr/bin/python -u

# The -u above is to make stdout & stderr unbuffered & binary
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

#
# This program is run by servald to look up DIDs in the OpenBTS SQLITE3 DB.
#
import ConfigParser
import os
import sqlite3
import sys

dbgfh = sys.stderr

def main():
    dbgfh.write('Started\n')
    if len(sys.argv) != 2:
        print >>dbgfh, "Bad usage"
        print >>dbgfh, "%s conffile" % (sys.argv[0])
        sys.exit(1)

    conf = ConfigParser.ConfigParser()
    if len(conf.read([sys.argv[1]])) == 0:
        print >>dbgfh, "Unable to read configuration"
        sys.exit(1)

    if not conf.has_option('general', 'dbpath'):
        print >>dbhfh, "Configuration file doesn't have dbpath"
        sys.exit(1)

    myip = None
    mysid = None
    
    if conf.has_option('general', 'ip'):
        myip = conf.get('general', 'ip')

    if 'MYSID' in os.environ:
        mysid = os.environ['MYSID']

    if mysid == None and myip == None:
        print >>sys.stderr, "MYSID must be set in the environment or the ip option must be set in the configuration"
        sys.exit(1)

    dbpath = conf.get('general', 'dbpath')

    dbgfh.write('Opening DB\n')
    try:
        db = sqlite3.connect(dbpath)
    except sqlite3.OperationalError, e:
        print >>sys.stderr, "Unable to open DB \'%s\'" % (dbpath)
        sys.exit(1)

    c = db.cursor()
    print "STARTED"
    dbgfh.write('Processing\n')
    while True:
        line = sys.stdin.readline().strip()
        if line == "":
            dbgfh.write('Empty line\n')
            # EOF detection is broken :(
            break
        s = line.split('|')
        if len(s) != 3:
            dbgfh.write('Had %d elements, expected %d\n' % (len(s), 3))
            print "ERROR"
            continue
        (token, number, xxx) = s
        dbgfh.write('Looking up %s\n' % (number))
        c.execute('SELECT dial FROM dialdata_table WHERE exten = ?', (number, ))
        for r in c:
            if myip != None:
                print "%s|sip://%s@%s|%s|%s|" % (token, r[0], myip, number, "")
            if mysid != None:
                print "%s|sid://%s/%s|%s|%s|" % (token, mysid, number, number, "")
        print "DONE"
        
if __name__ == "__main__":
    main()

