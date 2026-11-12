# SKSHOW

[![CI](https://github.com/SysMan-One/skshow/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SysMan-One/skshow/actions/workflows/ci.yml)

A nano-utility that shows the TCP/IP sockets of the system with their byte and
packet counters, and can select among them by a number of criteria.  It reads
the inventory from `/proc/net/{tcp,tcp6,udp,udp6}`, which is always there, and
asks `NETLINK_SOCK_DIAG` for the counters, which is optional -- a kernel built
without `CONFIG_INET_DIAG` merely shows `n/a` in the counter columns.

    Proto  Local end-point       Remote end-point      State      PID Process    ...
    TCPv4  127.0.0.1:18099       127.0.0.1:41614       ESTAB     1196 python3    ...

## Command

    skshow SHOW    [/qualifiers ...]     one snapshot, then exit
    skshow MONITOR [/qualifiers ...]     refresh until interrupted by a ^C
    skshow HELP    [<topic> ...]         the on-line help

A verb and a qualifier may be abbreviated as long as it stays unambiguous, and a
qualifier is introduced by either a `/` or a `--`.  A lone `-name` is not a
qualifier: it is rejected as an unrecognized token, so a selection written that
way is refused rather than quietly dropped.

Selection: `/LIA` and `/RIA` by local and remote address, IPv4 or IPv6; `/LPN`
and `/RPN` by port; `/PROTO`, `/FAMILY`; `/STATE` and `/PMASK`, each of which
takes a list -- a record matches when it matches any element.  `/PMASK` ignores
the case of the letters: `sshd`, `SSHD` and `Sshd` are one and the same mask.  A
mask without a wildcard is a substring and matches anywhere in the name of the
owning process; a mask carrying a `*`, a `%` or a `?` is a pattern that has to
match the whole name -- `*` a run of characters, `%` and `?` exactly one --
matched by the `__util$pattern_match()` of the StarLet set.  `/NOCOUNTERS`
leaves NETLINK alone.  `/NOHIGHLIGHT` turns off the reverse-video marking of the
rows that have moved since the previous refresh -- a counter, a queue or the
state has changed, or the socket has just appeared.  Nothing is marked on the
first refresh, and nothing at all is marked when the output is not a terminal.
`/INTERVAL` and `/COUNT` govern MONITOR.  `/LOGFILE`, `/LOGSIZE` and `/TRACE`
govern the diagnostics, which go to the log destination and never into the
table.

    skshow show /state=listen /proto=tcp
    skshow show /lia=127.0.0.1 /pmask=(nginx,redis)
    skshow show '/pmask=SSH*'
    skshow monitor /interval=2 /count=10 /nocounters
    skshow monitor /family=inet6 /state=(estab,time_wait)
    skshow monitor /interval=1 /nohighlight

Run as root to see the owner of every socket: a `/proc/<pid>/fd` of a foreign
process is not readable otherwise, and such sockets are shown as `???`.  The
utility says how many processes it could not read.

## On-line help

The description of the utility is a help library of the HELP subsystem, written
in Markdown -- `doc/skshow.md`, and `doc/skshow_ru.md` in Russian -- translated
by `vhelptran` at build time and installed under `share/help` beside the image.
`skshow help` answers out of it through the reading API of the HELP package,
`HELP::help`, so a topic looks exactly as it does under the browser, the pager
included:

    skshow help                     the description and the list of topics
    skshow help monitor             a verb
    skshow help pmask               a qualifier, with or without its slash
    skshow help messages ivaddr     a message: what it means, what to do
    skshow help /nopage | less      the pager off

`-h`, `--help`, `/HELP` and `/?` reach the same text wherever they stand on the
command line, and so does an invocation with no argument at all.  Under a
Russian locale -- the first of `LC_ALL`, `LC_MESSAGES`, `LANG` that is set
begins with `ru` -- the Russian library answers.  `SKSHOW_HELPLIB` names a
library to read instead of the installed one.  `vhelp skshow` reads the very
same libraries.

When no library is to be found -- the HELP package was absent at build time, or
the image travels alone -- a summary rendered out of the grammar tables of the
parser answers instead, so the help never depends on anything being installed.
That summary cannot drift away from the syntax that is actually accepted, since
it is built from the very tables the parser uses.

## Diagnostics

Anything a user is meant to read goes through the message catalogue of the
facility, in the OpenVMS form, to the standard error -- or to the log file when
`/LOGFILE` is given -- never into the table on the standard output:

    %SKSHOW-E-IVADDR, 10.0.0 is not an IPv4 or an IPv6 address, qualifier /LIA
    %SKSHOW-W-NOPRIV, running as UID 1000, an owner of a foreign socket is shown as ???

The letter is the severity, the word after it the name of the message, and
`skshow help messages <name>` says what to do about it.  The command line
service adds messages of its own about a verb, a qualifier or a value it does
not accept.  `$LOG` remains the developer's channel: a failed C RTL call is
reported there with its `errno`, and `/TRACE` opens the running commentary.

The image exits with a success status when the command was carried out or the
help was displayed, and with a failure status when the command line was
rejected or the run failed.

## Build

The command line is parsed by the CLI_ROUTINES of the StarLet package
(UTILITY$ROUTINES), the diagnostics go through its message catalogue, and the
help is read by the HELP package; both are found where they were installed
rather than compiled alongside these sources.  Build and install StarLet first,
then HELP if the on-line help is wanted out of a library:

    cmake -S <utility_routines> -B sl -DCMAKE_BUILD_TYPE=Release
    cmake --build sl
    cmake --install sl --prefix /usr/local

    cmake -S <help> -B hl -DCMAKE_BUILD_TYPE=Release
    cmake --build hl
    cmake --install hl --prefix /usr/local

then this utility:

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build
    cmake --install build --prefix /usr/local

or, with the Makefile generator, from the build directory:

    make
    make test
    make install
    make uninstall

`make install` puts down the image, the manual page, the help libraries and
this file:

    <prefix>/bin/skshow
    <prefix>/share/man/man8/skshow.8
    <prefix>/share/help/skshow.hlb
    <prefix>/share/help/ru/skshow_ru.hlb
    <prefix>/share/doc/skshow/README.md

The directories come from `GNUInstallDirs`, so a distribution that spells them
otherwise is honoured.  `DESTDIR` is understood by both targets, which is what a
packager wants:

    make install DESTDIR=/tmp/stage

`make uninstall` removes exactly the files named by `install_manifest.txt` and
leaves the directories alone -- they are shared with whatever else lives under
the same prefix.  Running it twice is harmless.

When a package lives somewhere CMake does not search by default, add
`-DCMAKE_PREFIX_PATH=<prefix>`.  StarLet **1.6 or later** is required: the
message catalogue is reached through `$PUTMSG_FAO` with the parameter count
derived by the macro, the log is pointed at the standard error by
`__util$setlogfd()`, the `?` of a `/PMASK` pattern is a wildcard of
`__util$pattern_match()` from the 1.6.0 delivery, and CLI_ROUTINES X.00-16 makes
the Unix spelling of a qualifier the GNU long form `--name`, which the help and
the manual say.  HELP is optional and is looked for *before* StarLet on purpose:
its config asks for the StarLet it was built against with `find_dependency`, so
the very copy HELP was built against is the one this product then links.

A build with no type named is a release build: without it the compiler is given
no `NDEBUG`, and the tracing of the UTILITY$ROUTINES -- which `NDEBUG` is what
silences -- would reach the terminal on every run.  The same holds for the HELP
package: a HELP installed from a Debug build traces every library it opens onto
the standard error of this image.

The identification of the product is written down once, at the head of
`skshow.c`, in the form `X-aa-bb` with an optional `ECOcc` suffix.  `__REV__` is
the same value in the UNIX style, `aa.bb.cc`, an absent ECO giving a `cc` of
zero.  CMake reads both back out of the module, refuses a form other than that
and refuses a `__REV__` that does not follow from the `__IDENT__`, so the two
cannot drift apart and the package version cannot drift from either.  The help
libraries are stamped with the same identification.

Options, none of them carrying a `$` -- the house rule for the build files:

| | |
|---|---|
| `SKSHOW_M_STRICT` | `-Werror`, default OFF |
| `SKSHOW_M_SANITIZE` | the Address and UB sanitizers, default OFF |
| `SKSHOW_M_HELPAPI` | answer the help out of the library when HELP is found, default ON |
| `SKSHOW_M_HELPLIB` | translate and install the help libraries, default ON |
| `SKSHOW_K_HLBDIR` | where the libraries go, relative to the prefix, `share/help` |
| `BUILD_TESTING` | the smoke tests under CTest, default ON |

`ctest` runs a dozen smoke checks, none of them needing the superuser: the help
in its spellings, a snapshot with `/NOCOUNTERS`, a `/PMASK` list, and the
refusals -- an unknown topic, a keyword that is not one, a port out of range, a
lone `-name`.

## Notes on the implementation

The socket table is composed in one growable buffer and written by a single
`write(2)`, which is what keeps MONITOR from flickering.  The inventory is
matched to its counters through a hash of the socket identity, and the owner of
a socket through a hash of its inode, so a host with thousands of sockets costs
one NETLINK dump per family and protocol per cycle rather than one per socket.

The `/proc` parser reads an address exactly as the kernel prints it -- a `__be32`
through a host-order `%08X` -- and stores it back without conversion, which is
correct on a little-endian and on a big-endian target alike.  A port, printed in
the host order, does need the `htons()`.

`struct tcp_info` is copied into a zeroed local and every field is length-checked
against what the attribute actually carried: a running kernel may deliver a
shorter structure than the header of the build declares.

The standard output carries the table, so the diagnostics go to the log
destination, which `__util$setlogfd()` points at the standard error at startup;
`main()` keeps a `dup()` of the real standard output taken before any
`__util$deflog()`, which redirects both standard streams into the log file.

The help library is looked for where `SKSHOW_HELPLIB` says, then beside the
image -- `<image>/../share/help/skshow.hlb` of an installed tree and
`<image>/skshow.hlb` of a build tree, so both are relocatable -- and last where
the build was configured to install it.  A word that names no topic is tried
once more with a slash in front of it, and both ways under the facility topic,
so `skshow help show pmask` finds the qualifier although it is not a subtopic
of SHOW.

## License

Apache License 2.0 - see [LICENSE](LICENSE) and [NOTICE](NOTICE).

## Author

StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
