<!-- ident: X.04-00 -->

# SKSHOW -- display the TCP/IP sockets of the system

Shows the TCP and UDP sockets of the system in one table: who
talks to whom, in what state, which process holds the socket, and
how many octets and packets have gone through it.  The table can
be narrowed down by address, port, protocol, family, state and by
the name of the owning process.

Format:

```
SKSHOW [/qualifiers...]              one snapshot, then exit
SKSHOW /CONTINUOUS [/qualifiers...]  redraw until Ctrl-C
SKSHOW HELP [topic...]               this help
```

There is no verb: SKSHOW alone reads the sockets once, prints the
table and exits.  /CONTINUOUS makes it redraw the table until you
press Ctrl-C.

A qualifier may be shortened as long as the short form is not
ambiguous: /CONT is /CONTINUOUS, /INT is /INTERVAL.  A qualifier
starts with a slash or with two dashes, so /PMASK and --PMASK are
the same thing.  A single dash is not a qualifier: -lpn=80 is
refused as an unrecognized token rather than dropped in silence.

A qualifier that takes a list accepts one value or several in
parentheses, and a row is shown when it matches any one of them:

```
$ skshow /state=listen
$ skshow /state=(listen,estab)
```

One snapshot may be piped or written to a file: nothing but plain
text is emitted when the standard output is not a terminal.

```
$ skshow /proto=tcp /state=listen
$ skshow /pmask=sshd > sockets.txt
```

Where the data comes from: the list of sockets is read from
/proc/net/tcp, tcp6, udp and udp6, which every kernel has.  The
counters are asked of the kernel through NETLINK; a kernel built
without CONFIG_INET_DIAG has none, and the counter columns then
say n/a.  UDP sockets carry no octet counters and say n/a too.

Run it as root to see the owner of every socket.  Without that
privilege the file descriptors of a foreign process cannot be
read, its sockets are shown as ??? and the footer says how many
processes could not be looked into.

Ask for more with HELP and the name of a qualifier or of a
section:

```
$ skshow help continuous
$ skshow help pmask
$ skshow help examples
$ skshow help messages ivaddr
```

## HELP -- display the on-line help

```
SKSHOW HELP [topic [subtopic...]] [/PAGE] [/NOPAGE]
```

Displays this text.  Without a topic, the description of the
utility and the list of what may be asked for.  With a topic --
a qualifier with or without its slash, EXAMPLES, MESSAGES --
that topic alone.  A message of the utility is a subtopic of
MESSAGES:

```
$ skshow help
$ skshow help /continuous
$ skshow help pmask
$ skshow help messages noprIV
```

The case of the letters does not matter.

The same text is reached by -h, --help, /HELP and /? wherever
they stand on the command line.  A bare SKSHOW does not display
the help: it displays the table.

At a terminal the text stops at the end of every page and waits
for Return; /NOPAGE lets it run through, which is what a pipe
wants.  /PAGE is the default at a terminal and is ignored
elsewhere.

The text lives in a help library, skshow.hlb, built out of the
Markdown description of the product and installed under
share/help beside the image.  The HELP browser reads the same
library:

```
$ vhelp skshow pmask
```

The variable SKSHOW_HELPLIB names another library to read.  When
no library can be found at all, a summary built into the image
answers instead -- shorter, but never absent.

## /LIA -- select by the local IP address

```
/LIA=address
```

Shows the sockets bound to this local address.  An IPv4 literal
(192.168.1.10) or an IPv6 literal (fe80::1, ::1).  The family of
the address must agree with /FAMILY when both are given.

```
$ skshow /lia=127.0.0.1
$ skshow /lia=::1
```

## /RIA -- select by the remote IP address

```
/RIA=address
```

Shows the sockets connected to this remote address.  An IPv4 or
an IPv6 literal, as for /LIA.  A listening socket has no remote
address and never matches.

```
$ skshow /ria=10.0.0.5
```

## /LPN -- select by the local port number

```
/LPN=port
```

Shows the sockets bound to this local port, 1 to 65535.

```
$ skshow /lpn=22
$ skshow /cont /lpn=443 /interval=1
```

## /RPN -- select by the remote port number

```
/RPN=port
```

Shows the sockets connected to this remote port, 1 to 65535.

```
$ skshow /rpn=53
```

## /PROTO -- select by the transport protocol

```
/PROTO=TCP
/PROTO=UDP
/PROTO=ALL        (default)
```

Shows the sockets of one protocol only.  ALL, the default, shows
both.

```
$ skshow /proto=udp
```

## /FAMILY -- select by the address family

```
/FAMILY=INET      IPv4 only
/FAMILY=INET6     IPv6 only
/FAMILY=ALL       (default)
```

IPV4 and IPV6 are accepted as other names of INET and INET6.

```
$ skshow /family=inet6
```

## /STATE -- select by the socket state

```
/STATE=keyword
/STATE=(keyword,...)
```

Shows the sockets that are in any of the given states.  The
keywords are those of the kernel:

```
ESTAB       SYN_SENT    SYN_RECV    FIN_WAIT1   FIN_WAIT2
TIME_WAIT   CLOSE       CLOSE_WAIT  LAST_ACK    LISTEN
CLOSING     UNCONN      ALL
```

UNCONN is what an unconnected UDP socket reports; it is the same
value as CLOSE.  ALL anywhere in the list drops the criterion.

```
$ skshow /state=listen
$ skshow /cont /state=(estab,time_wait)
```

## /PMASK -- select by the name of the owning process

```
/PMASK=mask
/PMASK=(mask,...)
```

Shows the sockets whose owning process has a name matching any
of the masks.  The name is the one the kernel keeps for the
process -- what ps prints in its COMM column, at most fifteen
characters.

The case of the letters is ignored: /PMASK=SSHD, /PMASK=sshd and
/PMASK=Sshd select the same rows.

A mask without a wildcard is a substring: it matches when it is
found anywhere in the name, so ssh matches sshd and sshd-session
alike.

A mask with a wildcard is a pattern that has to match the whole
name:

```
*       any run of characters, empty included
%       exactly one character
?       exactly one character, the same as %
```

so ssh* matches sshd and sshd-session but not openssh-agent, and
ssh? matches sshd alone.

A shell may expand a * or a ? of its own before the utility sees
it; quote the mask when in doubt.  The parentheses of a list are
best quoted for the same reason.

At most eight masks are taken; the rest are ignored, and the
message TOOMANY says so.

```
$ skshow /pmask=nginx
$ skshow /pmask=(nginx,redis,postgres)
$ skshow '/pmask=ssh*'
$ skshow /cont /pmask=java /interval=2
```

## /COUNTERS -- ask the kernel for the counters

```
/COUNTERS         (default)
/NOCOUNTERS
```

Asks NETLINK for the octet and packet counters of every TCP
socket.  This is the default.  /NOCOUNTERS skips the question:
the counter columns say n/a, and the table costs less on a host
that carries many thousands of sockets.

When both are given, the last one wins.

```
$ skshow /cont /nocounters /interval=1
```

## /HIGHLIGHT -- mark the rows that have moved

```
/HIGHLIGHT        (default)
/NOHIGHLIGHT
```

Under /CONTINUOUS, shows in reverse video every row that differs
from what it was at the previous cycle: a counter, a queue
length or the state has changed, or the socket has just
appeared.  Nothing is marked at the first cycle, since there is
nothing to differ from, and nothing is marked when the output is
not a terminal.  /NOHIGHLIGHT turns the marking off.

```
$ skshow /cont /nohighlight
```

## /CONTINUOUS -- redraw the table until interrupted

```
/CONTINUOUS
/NOCONTINUOUS     (default)
```

Reads the sockets again and again, every /INTERVAL seconds, and
redraws the table in place.  Press Ctrl-C to stop; /COUNT stops
it after that many cycles.  /CONT is enough to type.

Without /CONTINUOUS the sockets are read once, the table is
printed, and SKSHOW exits; /INTERVAL and /COUNT then have no
effect.

A row that has changed since the previous cycle -- a counter, a
queue length, the state -- or a socket that was not there a cycle
ago is shown in reverse video, so that a busy socket stands out
of a still table.  See /HIGHLIGHT to turn that off.

The whole screen is composed in memory and written in one go, so
the table does not flicker.

```
$ skshow /cont
$ skshow /continuous /interval=5 /count=12 /nocounters
```

## /INTERVAL -- the refresh interval of /CONTINUOUS

```
/INTERVAL=seconds     (default 3)
```

How long /CONTINUOUS waits between two cycles, 0 to 86400
seconds.  Zero makes it a single snapshot.  Without /CONTINUOUS
this qualifier has no effect.

```
$ skshow /cont /interval=1
```

## /COUNT -- the number of cycles of /CONTINUOUS

```
/COUNT=number         (default 0, unlimited)
```

How many cycles /CONTINUOUS performs before SKSHOW exits on its
own.  Zero, the default, means until Ctrl-C.  Without /CONTINUOUS
this qualifier has no effect.

```
$ skshow /cont /count=10 /interval=6
```

## /TRACE -- diagnostic output

```
/TRACE
/NOTRACE          (default)
```

Makes the utility tell what it is doing: which files are read,
what NETLINK answers, where the help library was found.  The
diagnostics go to the standard error, or to the log file when
/LOGFILE is given, never into the table.  SIGUSR1 toggles the
tracing of a running /CONTINUOUS.

```
$ skshow /trace /pmask=sshd 2> trace.txt
```

## /LOGFILE -- send the diagnostics to a file

```
/LOGFILE=file
```

Names a file to receive the diagnostics and the messages of the
utility instead of the standard error.  The table itself always
goes to the standard output.

```
$ skshow /cont /logfile=/tmp/skshow.log /trace
```

## /LOGSIZE -- limit the size of the log file

```
/LOGSIZE=octets       (default 0, no limit)
```

Rewinds the log file to its beginning once it has grown past
this many octets, so that a /CONTINUOUS left running for days does
not fill the disk.  Meaningful with /LOGFILE only.

```
$ skshow /cont /logfile=/tmp/skshow.log /logsize=1000000
```

## /PAGE -- page the help text

```
/PAGE             (default at a terminal)
/NOPAGE
```

Governs the help only.  At a terminal the text stops at the
end of every page and waits for Return.  /NOPAGE lets it run
through, which is what a pipe or a redirection wants; there the
text is never paged whatever is given.

```
$ skshow help /nopage | less
```

## Output -- how to read the table

One row per socket, in the order of the /proc files: TCP over
IPv4, TCP over IPv6, UDP over IPv4, UDP over IPv6.  The columns:

```
Proto         TCPv4, TCPv6, UDPv4, UDPv6
Local         the local address and port
Remote        the remote address and port; *:* when none
State         the socket state, as /STATE spells it
PID           the process holding the socket
Process       its name; ??? when it could not be read
RxQ, TxQ      the receive and the transmit queue, octets
RxBytes       octets received
TxBytes       octets sent
RxPkts        packets received
TxPkts        packets sent
```

The footer line names the time of the snapshot, the number of
rows displayed and the selection in effect.  A second footer
line, when needed, says how many processes could not be looked
into for want of privilege.

The counters are those the kernel keeps for a TCP socket; a
kernel without CONFIG_INET_DIAG has none, and UDP has none
either -- n/a stands there.

## Examples

Listening TCP sockets:

```
$ skshow /state=listen /proto=tcp
```

What a pair of servers is holding on the loopback, whatever the
case of their names:

```
$ skshow /lia=127.0.0.1 /pmask=(NGINX,redis)
```

Every SSH-related socket, by a pattern:

```
$ skshow '/pmask=ssh*'
```

Ten refreshes, two seconds apart, without asking NETLINK:

```
$ skshow /cont /interval=2 /count=10 /nocounters
```

Watch a busy host, the moving rows standing out:

```
$ skshow /cont /interval=1
```

IPv6 sockets that are established or waiting out their close:

```
$ skshow /cont /family=inet6 /state=(estab,time_wait)
```

The help on a qualifier, and the whole help without paging:

```
$ skshow help pmask
$ skshow help /nopage > skshow-help.txt
```

## Messages -- what the utility says, and what to do

A message of the utility looks like this:

```
%SKSHOW-E-IVADDR, 10.0.0 is not an IPv4 or an IPv6 address, qualifier /LIA
```

SKSHOW is the facility, the letter is the severity -- S success,
I information, W warning, E error, F fatal -- and IVADDR is the
name of the message.  Every message goes to the standard error,
or to the log file when /LOGFILE is given.

The command line service adds messages of its own, about a
qualifier it does not know, an abbreviation that fits two of
them, a value of the wrong kind: read the line, fix the
spelling, and see HELP for the accepted names.

The utility exits with a success status when the command was
carried out or the help was displayed, and with a failure status
when the command line was rejected or the run failed.

### AMBIGQUAL -- an abbreviation fits several qualifiers

```
%SKSHOW-E-AMBIGQUAL, co is an ambiguous qualifier name
```

The topic given to HELP is an abbreviation that fits more than
one qualifier: CO is /CONTINUOUS, /COUNTERS and /COUNT.  Give more
letters.  You see it only when the help library is missing and
the summary built into the image answers.

### CONFQUAL -- two qualifiers contradict each other

```
%SKSHOW-E-CONFQUAL, /LIA and /FAMILY cannot be given together as they stand
```

An IPv6 address has been given with /FAMILY=INET, or an IPv4
address with /FAMILY=INET6.  Drop /FAMILY, or make it agree with
the address.

### IVADDR -- not an IP address

```
%SKSHOW-E-IVADDR, 10.0.0 is not an IPv4 or an IPv6 address, qualifier /LIA
```

The value of /LIA or /RIA is neither an IPv4 literal nor an IPv6
one.  A host name is not accepted: look it up first and give the
address.

### IVKWD -- not a keyword of the qualifier

```
%SKSHOW-E-IVKWD, tcp6 is not a keyword of the qualifier /PROTO
```

The value of /PROTO, /FAMILY or /STATE is none of the keywords
the qualifier accepts, or an abbreviation that fits two of them.
See the topic of the qualifier for the list.

### IVNUM -- not a number

```
%SKSHOW-E-IVNUM, 3s is not a number, qualifier /INTERVAL
```

The value of a numeric qualifier could not be read as a number.
Give a plain decimal; seconds and octets carry no unit.

### NOPRIV -- not the superuser

```
%SKSHOW-W-NOPRIV, running as UID 1000, an owner of a foreign socket is shown as ???
```

The utility is running without the privilege to read the file
descriptors of other processes, so it cannot tell who holds most
of the sockets.  The table is displayed all the same.  Run it
as root, through sudo, to see every owner.

### NOTOPIC -- no documentation on the topic

```
%SKSHOW-E-NOTOPIC, sorry, no documentation on foo
```

HELP was asked for a topic that is not in the library: not a
qualifier, not a section.  Ask for HELP without a
topic to see what there is.

### RANGE -- a number is out of range

```
%SKSHOW-E-RANGE, /LPN=70000 is out of the 1 ... 65535 range
```

The value of a numeric qualifier is outside what makes sense: a
port is 1 to 65535, an interval 0 to 86400 seconds, a count and
a log size 0 and up.

### SYNTAX -- the command line was rejected

```
%SKSHOW-E-SYNTAX, type "skshow help" for a command summary
```

The command line service has refused the command and has said
why on the line above: an unknown qualifier, a missing value, a
lone -name, a word that is not a qualifier.  Fix that line.

SKSHOW has no verbs any more: a command of the elder syntax,
"skshow ..." or "skshow /cont ...", is refused with
"Too many parameters, surplus token 'show'".  Drop SHOW; put
/CONTINUOUS in place of MONITOR.

### TOOMANY -- too many values of a list

```
%SKSHOW-W-TOOMANY, too many /PMASK values, only the first 8 are used
```

More masks were given to /PMASK than the utility keeps.  The
first eight are used, the rest are ignored.  A pattern with *
may stand for several of them.

## Author

StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)

SKSHOW is built on the StarLet package (UTILITY$ROUTINES) and on
the HELP subsystem of the same line.

Licensed under the Apache License, Version 2.0.
