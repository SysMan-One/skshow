#ifndef	_GNU_SOURCE
#define	_GNU_SOURCE			/* MUST be the very first line: enables memmem(), strnlen() etc.	*/
#endif

#define	__MODULE__	"SKSHOW"
/*
**  The identification of the product, and the ONLY place it is written down.
**  The form is X-aa-bb, optionally followed by ECOcc for a correction that
**  carries no new function - X-02-03, X-02-03ECO07.  __REV__ is the same value
**  in the UNIX style, aa.bb.cc, an absent ECO giving a cc of zero.
**
**  CMAKELISTS.TXT reads both out of this module, refuses a form other than the
**  above and refuses a __REV__ that does not follow from the __IDENT__, so the
**  two cannot drift apart and the package version cannot drift from either.
*/
#define	__IDENT__	"X-03-00"
#define	__REV__		"3.0.0"

/*
**++
**
**  FACILITY: A yet another utility to display statistic for TCP/IP sockets
**
**  ENVIRONMENT: Linux (kernel 3.x and above), glibc/musl, LE and BE targets
**
**  ABSTRACT: A nano-utility to show socket counters with ability to select records by a some criteria.
**
**	Socket inventory is taken from the /proc/net/{tcp,tcp6,udp,udp6} - it is always available.
**	Byte/packet counters are taken from the NETLINK_SOCK_DIAG (INET_DIAG_INFO) - it is optional,
**	a kernel without CONFIG_INET_DIAG just degrades to the "n/a" in the counter columns.
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  18-JUN-2026
**
**  USAGE:
**	$ skshow <verb> [<parameter>] [/qualifier[=value] ...]
**
**		SHOW		- display a single snapshot of the socket table and exit
**		MONITOR		- refresh the socket table until interrupted by a ^C
**		HELP [<topic>...]	- display the on-line help
**
**	A verb and a qualifier may be abbreviated as long as it stays unambiguous, a qualifier
**	is introduced by either a '/' or a '--'.
**
**	The description of the utility is a help library of the HELP subsystem, doc/skshow.md
**	(doc/skshow_ru.md in Russian), translated at build time and installed beside the image;
**	"skshow help <topic>..." answers out of it: a verb, a /qualifier, EXAMPLES, MESSAGES.
**	When no library is to be found - the HELP package was absent at build time, or the
**	image travels alone - the summary rendered out of the grammar tables answers instead,
**	so a help request never depends on anything being installed, see s_quals[] and s_help[].
**
**	Examples:
**		$ skshow show /state=listen /proto=tcp
**		$ skshow show /lia=127.0.0.1 /pmask=(nginx,redis)
**		$ skshow show /pmask=ssh*
**		$ skshow monitor /interval=2 /count=10 /nocounters
**		$ skshow monitor /nohighlight
**		$ skshow monitor /family=inet6 /state=(estab,time_wait)
**		$ skshow help pmask
**
**  MODIFICATION HISTORY:
**
**	18-JUN-2026	RRL	X.00-03 : Initial revision.
**
**	01-AUG-2026	RRL	X.01-00 : A general overhaul, see notes below.
**				- _GNU_SOURCE is moved to the first line, memmem() got a prototype;
**				- IPv6 octet order fixed: /proc prints s6_addr32[] in the host order,
**				  so the extra htonl() has corrupted every IPv6 address;
**				- inode is scanned by "%lu" in both IPv4 and IPv6 branches;
**				- "%s" in sscanf() is replaced by the suppressed "%*s";
**				- port options are accepted as OPTS$K_INT into an "int" - a direct
**				  OPTS$K_INT into an __be16 has overwritten an adjacent variable;
**				- /LIA, /RIA, /PROTO, /FAMILY, /STATE selections are implemented;
**				- record selection is moved out of the parser into s_match_filter();
**				- NETLINK is queried by a single dump per protocol/family per cycle,
**				  the answer is cached in a hash table - was: a full dump per socket,
**				  truncated by a single recv() of 8Kb;
**				- inode -> (pid, comm) map is a hash table now - was: a linear list;
**				- socket_info is cleaned up on every record - counters of a previous
**				  record have leaked into the TCP_LISTEN ones;
**				- counters are printed by PRIu64 - "%ld" is broken on 32-bit targets;
**				- the whole screen is composed in a single buffer and written by a
**				  single write() to a dup()-ed stdout: no flicker, and the table is
**				  not swallowed by __util$deflog()'s dup2();
**				- the signal handler is async-signal-safe (write() only);
**				- all the routines return a status, results go through the output
**				  parameters.
**
**	01-AUG-2026	RRL	X.02-00 : The command line is processed by the CLI routines now.
**				- __util$getparams() and its OPTS table are replaced by __cli$parse()
**				  and __cli$dispatch() over a DCL style grammar of the SHOW, MONITOR
**				  and HELP verbs; every verb carries an action routine;
**				- the value types (CLI$K_NUM, CLI$K_KWD, CLI$K_FILE) are validated by
**				  the CLI itself, so the hand written keyword lookups are gone;
**				- /NOCOUNTERS became a negated form of the /COUNTERS (CLI$M_NEGATABLE),
**				  /STATE and /PMASK accept a value list (CLI$M_LIST), /INTERVAL, /COUNT,
**				  /PROTO, /FAMILY and /LOGSIZE carry a declared default;
**				- added an on-line HELP which is rendered out of the grammar tables and
**				  therefore cannot drift away from the accepted syntax; the Unix style
**				  '-h', '--help', '/?' and a bare invocation are routed to it as well.
**
**	01-AUG-2026	RRL	X.02-01 : Adapted to the CLI_ROUTINES X.00-09.
**				- that revision recognizes a help token on its own and answers it from
**				  __cli$parse(), which pre-empts a caller supplied HELP verb: a bare
**				  "help" in the verb position never reaches the verb table. The whole
**				  command line is therefore pre-scanned for a help request BEFORE the
**				  parser is called, and answered by the local renderer - so a "skshow
**				  help show", a "skshow show /help" and a "skshow -h" all produce the
**				  very same, annotated text;
**				- __cli$parse() reports a help it has answered itself as STS$K_WARN,
**				  which is NOT a success under the '1 & status' convention; main() now
**				  tells that case from a real parse failure and exits quietly.
**
**	01-AUG-2026	RRL	X.02-02 : The CLI_ROUTINES X.00-11 requires a leading '-' or '/' on a
**				help token, so the HELP verb of this grammar is reachable again and its
**				action routine is restored. The pre-scan of main() is KEPT deliberately:
**				it routes "/help" and "-h" to this module's own annotated renderer rather
**				than to the library's terser built in one, so every spelling of the
**				question gets one and the same answer. The STS$K_WARN handling is kept
**				as well, for the case the pre-scan is ever bypassed.
**
**	01-AUG-2026	RRL	X.02-03 : The build consumes the StarLet set as an installed CMake
**				package - find_package(StarLet) and StarLet::starlet - so nothing of
**				the set is compiled beside this module any more; the command line, the
**				logging and the identification all come from the package. A Release
**				build is the default now, and under -O2 the three bare write(2) calls
**				tripped warn_unused_result: the result is consumed at each of them.
**				The identification is brought to the form X-aa-bb[ECOcc] and __REV__
**				to the aa.bb.cc that follows from it: the two used to disagree with
**				each other - X.02-03 against 02.00.03 - and with the package version
**				of CMAKELISTS.TXT, which said 1.0.0. This module is the head and the
**				only one, so it is the source both of the others are taken from.
**
**	28-AUG-2026	RRL	X-02-04 : A row that has moved since the previous refresh is shown in
**				reverse video, so that a busy socket stands out of a still table at a
**				glance. What counts as movement is anything the row displays: the
**				counters, the queue lengths, the state - and a socket that was not
**				there a cycle ago is movement too. The comparison is kept in a hash of
**				the socket identity, one entry per socket, reaped of what has gone.
**				See /HIGHLIGHT.
**
**	28-AUG-2026	RRL	X-02-05 : Brought up to CLI_ROUTINES X.00-16, which makes the Unix
**				spelling of a qualifier the GNU long form '--name' rather than a lone
**				'-name'. Only the wording changes here - the grammar tables are the
**				service's business - but the wording matters: '-lpn=80' is refused as
**				an unrecognized token now, and the help, the manual page and the
**				README have to say so.
**
**	11-SEP-2026	RRL	X-03-00 : Brought up to the StarLet 1.6.2 package and the HELP subsystem.
**				- the diagnostics a user is meant to read go through the message
**				  catalogue of the facility, $PUTMSG_FAO, in the OpenVMS envelope
**				  %SKSHOW-<S>-<IDENT>, <text>; $LOG stays the developer's channel and
**				  carries the errno of a failed C RTL call as it did;
**				- the log is pointed at the standard error by __util$setlogfd() at
**				  startup, so a diagnostic never lands in the table on the standard
**				  output; /LOGFILE still redirects it into a file as before;
**				- /PMASK ignores the case of the letters: /PMASK=SSHD and /PMASK=sshd
**				  select the same rows. A value that carries a '*', a '%' or a '?' is
**				  a wildcard pattern matched against the whole name by
**				  __util$pattern_match() ('*' a run, '%' and '?' one octet); one
**				  without stays a plain substring, as it always was. More than
**				  SKSHOW$K_MAXPMASK values are reported rather than dropped in silence;
**				- the on-line help is answered out of the help library of the product,
**				  doc/skshow.md translated by VHELPTRAN and installed under share/help,
**				  through the reading API of the HELP package - HLB$OPEN, HLB$LOOKUP_RUN,
**				  HLB$GETTEXT, HELP$PUT_TEXT - so "skshow help pmask" describes the
**				  qualifier at length and the topic looks exactly as it does under the
**				  browser, pager included. A Russian library is picked when the locale
**				  asks for it. The summary rendered out of the grammar tables is kept as
**				  the fallback for an image with no library at hand, and the built-in
**				  renderer is compiled whether the HELP package was found or not;
**				- a help request accepts /PAGE and /NOPAGE; the topic is paged at a
**				  terminal unless told otherwise;
**				- the author line is the house one, and every identifier of the build
**				  files is spelled without a '$'.
**
**--
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<stddef.h>
#include	<string.h>
#include	<strings.h>
#include	<unistd.h>
#include	<dirent.h>
#include	<ctype.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<inttypes.h>
#include	<signal.h>
#include	<time.h>

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/stat.h>
#include	<arpa/inet.h>
#include	<netinet/in.h>

#include	<linux/netlink.h>
#include	<linux/sock_diag.h>
#include	<linux/inet_diag.h>
#include	<linux/rtnetlink.h>						/* RTA_OK, RTA_DATA, RTA_NEXT		*/
#include	<linux/tcp.h>							/* struct tcp_info			*/

#define		__FAC__		"SKSHOW"
#define		__TFAC__	__FAC__ ": "				/* Special prefix for the $TRACE		*/
#include	"utility_routines.h"
#include	"starlet.h"						/* fao_prm_t for the $PUTMSG_FAO	*/
#include	"cli_routines.h"

/*
 * The reading side of the HELP subsystem - HLB$OPEN, HLB$LOOKUP_RUN, HLB$GETTEXT and the output
 * engine of the browser. HELPDEF.H defines __MODULE__ and the rest only when they are absent,
 * so the identification of this module stands. Compiled in when the HELP package was found at
 * configuration time, see SKSHOW_WITH_HLB in CMakeLists.txt.
 */
#ifdef	SKSHOW_WITH_HLB
#include	"helpdef.h"
#endif


/*
 * Some kernel headers of an elder generation do not export the INET_DIAG_NOCOOKIE,
 * a zeroed cookie is a legal value and must not be used as a wildcard.
 */
#ifndef	INET_DIAG_NOCOOKIE
#define	INET_DIAG_NOCOOKIE	(~0U)
#endif

/*
 * A field of the struct tcp_info may be absent in a running kernel even if it is present
 * in the compile-time header - the netlink attribute is just shorter. Check before use.
 */
#define	$TCPI_HAS(len, fld)	( (size_t) (len) >= (offsetof(struct tcp_info, fld)		\
					+ sizeof(((struct tcp_info *) 0)->fld)) )

#define	$MINSZ(a, b)		( ((size_t) (a) < (size_t) (b)) ? (size_t) (a) : (size_t) (b) )


/*
 * Module wide constants
 */
enum	{
	SKSHOW$K_HASHSZ		= 1021,					/* A prime, size of the both hash tables	*/
	SKSHOW$K_NLBUFSZ	= 128 * 1024,				/* A receive buffer for the NETLINK	*/
	SKSHOW$K_OBUFINI	= 64 * 1024,				/* An initial size of the screen buffer	*/
	SKSHOW$K_EPSZ		= 64,					/* "address:port" text buffer		*/
	SKSHOW$K_LINESZ		= 512,					/* A single table line			*/
	SKSHOW$K_EPCOL		= 32,					/* A width of the end-point column	*/
	SKSHOW$K_PROCCOL	= 14,					/* A width of the process name column	*/
	SKSHOW$K_MAXPMASK	= 8,					/* A maximum number of the /PMASK items	*/
	SKSHOW$K_MAXWORDS	= 16,					/* A maximum number of words of a help topic */
	SKSHOW$K_SZ_PATH	= 512					/* A file specification buffer		*/
	};

/*
 * The message catalogue of the facility. A diagnostic a user is meant to read is signalled by
 * $SKSHOWMSG - the $PUTMSG_FAO of the StarLet library under the name of this facility: the
 * catalogue registered by main() supplies the text, the library puts the envelope on out of the
 * facility name and the severity carried by the condition value - %SKSHOW-E-IVADDR, ...
 *
 * $LOG is the developer's channel and stays for the failures of the C RTL, where the errno has
 * to be seen; the two are not interchangeable.
 *
 * Neither the cast of a parameter nor the count of them is written by hand: FAO$_EACH() casts
 * every parameter to fao_prm_t and FAO$_NARG() counts them, inside the $PUTMSG_FAO macro.
 */
#define	SKSHOW$K_FACNO		0x0113					/* 0x0110 HELP, 0x0111 SEARCH, 0x0112 SDL */

#define	$SKSHOWSTS(msg, sev)	( $DEFFAC(SKSHOW$K_FACNO) | $DEFMSG(msg) | $DEFSEV(sev) )
#define	$SKSHOWMSG(a_sts, ...)	((int) $PUTMSG_FAO((a_sts), ##__VA_ARGS__))

enum	{
	SKSHOW$K_MSG_NORMAL	= 1,					/* Normal successful completion		*/
	SKSHOW$K_MSG_IVADDR,						/* Not an IP address			*/
	SKSHOW$K_MSG_IVKWD,						/* Not a keyword of the qualifier	*/
	SKSHOW$K_MSG_IVNUM,						/* Not a number				*/
	SKSHOW$K_MSG_RANGE,						/* A number is out of range		*/
	SKSHOW$K_MSG_CONFQUAL,						/* Conflicting qualifiers		*/
	SKSHOW$K_MSG_TOOMANY,						/* Too many values of a list qualifier	*/
	SKSHOW$K_MSG_NOPRIV,						/* Not the superuser			*/
	SKSHOW$K_MSG_NOTOPIC,						/* No documentation on a topic		*/
	SKSHOW$K_MSG_AMBIGVERB,						/* An ambiguous verb name		*/
	SKSHOW$K_MSG_SYNTAX,						/* The command line was rejected	*/

	SKSHOW$K_MSG_MAX						/* End-Of-List				*/
	};

#define	SKSHOW$_NORMAL		$SKSHOWSTS(SKSHOW$K_MSG_NORMAL,		STS$K_SUCCESS)
#define	SKSHOW$_IVADDR		$SKSHOWSTS(SKSHOW$K_MSG_IVADDR,		STS$K_ERROR)
#define	SKSHOW$_IVKWD		$SKSHOWSTS(SKSHOW$K_MSG_IVKWD,		STS$K_ERROR)
#define	SKSHOW$_IVNUM		$SKSHOWSTS(SKSHOW$K_MSG_IVNUM,		STS$K_ERROR)
#define	SKSHOW$_RANGE		$SKSHOWSTS(SKSHOW$K_MSG_RANGE,		STS$K_ERROR)
#define	SKSHOW$_CONFQUAL	$SKSHOWSTS(SKSHOW$K_MSG_CONFQUAL,	STS$K_ERROR)
#define	SKSHOW$_TOOMANY		$SKSHOWSTS(SKSHOW$K_MSG_TOOMANY,	STS$K_WARN)
#define	SKSHOW$_NOPRIV		$SKSHOWSTS(SKSHOW$K_MSG_NOPRIV,		STS$K_WARN)
#define	SKSHOW$_NOTOPIC		$SKSHOWSTS(SKSHOW$K_MSG_NOTOPIC,	STS$K_ERROR)
#define	SKSHOW$_AMBIGVERB	$SKSHOWSTS(SKSHOW$K_MSG_AMBIGVERB,	STS$K_ERROR)
#define	SKSHOW$_SYNTAX		$SKSHOWSTS(SKSHOW$K_MSG_SYNTAX,		STS$K_ERROR)

/*
 * The parameters are named a_ - and not sts, text - on purpose: a designated initializer is
 * macro-expanded like anything else, so a parameter spelled like a member of the EMSG_RECORD
 * would turn .sts into the argument itself.
 *
 * The array is NOT const: __util$inimsg() sorts it in place and stamps the facility name into
 * every record, so that the lookup of a message is a binary search with no list walk.
 */
#define	$SKSHOWREC(a_sts, a_txt) { .sts = (int) (a_sts), .textl = (unsigned char) (sizeof(a_txt) - 1), .text = a_txt }

static	EMSG_RECORD	s_msgtab [] = {
	$SKSHOWREC(SKSHOW$_NORMAL,	"NORMAL, normal successful completion"),
	$SKSHOWREC(SKSHOW$_IVADDR,	"IVADDR, !AZ is not an IPv4 or an IPv6 address, qualifier /!AZ"),
	$SKSHOWREC(SKSHOW$_IVKWD,	"IVKWD, !AZ is not a keyword of the qualifier /!AZ"),
	$SKSHOWREC(SKSHOW$_IVNUM,	"IVNUM, !AZ is not a number, qualifier /!AZ"),
	$SKSHOWREC(SKSHOW$_RANGE,	"RANGE, /!AZ=!SQ is out of the !SQ ... !SQ range"),
	$SKSHOWREC(SKSHOW$_CONFQUAL,	"CONFQUAL, /!AZ and /!AZ cannot be given together as they stand"),
	$SKSHOWREC(SKSHOW$_TOOMANY,	"TOOMANY, too many !AZ, only the first !UL are used"),
	$SKSHOWREC(SKSHOW$_NOPRIV,	"NOPRIV, running as UID !UL, an owner of a foreign socket is shown as ???"),
	$SKSHOWREC(SKSHOW$_NOTOPIC,	"NOTOPIC, sorry, no documentation on !AZ"),
	$SKSHOWREC(SKSHOW$_AMBIGVERB,	"AMBIGVERB, !AZ is an ambiguous verb name"),
	$SKSHOWREC(SKSHOW$_SYNTAX,	"SYNTAX, type \"skshow help\" for a command summary")
	};

static	EMSG_RECORD_DESC	s_msgdsc = {
	.msgnr		= (unsigned) $ARRSZ(s_msgtab),
	.facno		= SKSHOW$K_FACNO,
	.msgrec		= s_msgtab,
	.facname	= __FAC__
	};

/*
 * TCP socket states, see the "st" field of the /proc/net/tcp.
 * A local copy is used to avoid a collision with the <netinet/tcp.h>.
 */
enum	{
	SKSHOW$K_TCP_ESTAB	= 1,
	SKSHOW$K_TCP_SYN_SENT	= 2,
	SKSHOW$K_TCP_SYN_RECV	= 3,
	SKSHOW$K_TCP_FIN_WAIT1	= 4,
	SKSHOW$K_TCP_FIN_WAIT2	= 5,
	SKSHOW$K_TCP_TIME_WAIT	= 6,
	SKSHOW$K_TCP_CLOSE	= 7,
	SKSHOW$K_TCP_CLOSE_WAIT	= 8,
	SKSHOW$K_TCP_LAST_ACK	= 9,
	SKSHOW$K_TCP_LISTEN	= 10,
	SKSHOW$K_TCP_CLOSING	= 11,

	SKSHOW$K_TCP_MAX	= 12					/* End-Of-List				*/
	};

/*
 * UDP reuses the TCP state numbering, only two values are ever seen.
 */
enum	{
	SKSHOW$K_UDP_ESTAB	= SKSHOW$K_TCP_ESTAB,
	SKSHOW$K_UDP_UNCONN	= SKSHOW$K_TCP_CLOSE
	};


#define	SKSHOW$K_PROC_TCP4	"/proc/net/tcp"
#define	SKSHOW$K_PROC_TCP6	"/proc/net/tcp6"
#define	SKSHOW$K_PROC_UDP4	"/proc/net/udp"
#define	SKSHOW$K_PROC_UDP6	"/proc/net/udp6"

#define	$ESC_CLS		"\033[2J\033[H"				/* Erase screen, cursor to the home	*/
#define	$ESC_HOME		"\033[H"				/* Cursor to the home			*/
#define	$ESC_EOL		"\033[K"				/* Erase to the end of line		*/
#define	$ESC_EOS		"\033[J"				/* Erase to the end of screen		*/
#define	$ESC_REV		"\033[7m"				/* Negative image, ECMA-48 SGR 7	*/
#define	$ESC_NORM		"\033[0m"				/* Back to the default rendition	*/


/*
 * An identity of a socket: is used as a key to bind a /proc/net record to a NETLINK answer.
 * All the address/port fields are kept in the network byte order, an IPv4 address occupies
 * the [0] element only, the rest is zeroed. The whole structure is memcmp()-ed, so it MUST
 * be memset()-ed before use to get the trailing padding zeroed.
 */
typedef	struct sk_key_t
{
	uint32_t	lia[4];						/* Local  IP address, NBO		*/
	uint32_t	ria[4];						/* Remote IP address, NBO		*/
	uint16_t	lpn, rpn;					/* Local, Remote port number, NBO	*/
	uint8_t		family;						/* AF_INET | AF_INET6			*/
	uint8_t		proto;						/* IPPROTO_TCP | IPPROTO_UDP		*/
} SKSHOW$SK_KEY;

/*
 * A cached NETLINK answer for a single socket.
 */
typedef	struct diag_ent_t
{
	struct diag_ent_t	*link;					/* A next entry in the hash chain	*/
	SKSHOW$SK_KEY		key;					/* A socket identity			*/
	uint64_t		rxbytes, txbytes,			/* INET_DIAG_INFO derived counters	*/
				rxpkts, txpkts;
	int			has_counters;				/* !0 - the counters above are valid	*/
} SKSHOW$DIAG_ENT;

/*
 * A socket as it stood at the previous refresh. Everything the row displays is kept,
 * so that "has this moved" is answered by one comparison rather than by a rule about
 * which field is interesting: a counter, a queue, a state - a change in any of them is
 * movement, and so is the socket not having been there at all.
 *
 * The cycle number tells a record touched during this refresh from one whose socket has
 * gone away; the latter are reaped at the end of the refresh, so the map holds one entry
 * per live socket and no more.
 */
typedef	struct prev_ent_t
{
	struct prev_ent_t	*link;				/* A next entry in the hash chain	*/
	SKSHOW$SK_KEY		key;				/* A socket identity			*/
	uint64_t		rxbytes, txbytes,		/* The counters as they stood		*/
				rxpkts, txpkts;
	unsigned		cycle,				/* The refresh that last touched it	*/
				rxq, txq;			/* The queue lengths as they stood	*/
	int			state;				/* The state as it stood		*/
} SKSHOW$PREV_ENT;

/*
 * An inode -> (pid, process name) binding, built by scanning of the /proc/<pid>/fd/.
 */
typedef	struct inode_ent_t
{
	struct inode_ent_t	*link;					/* A next entry in the hash chain	*/
	unsigned long		inode;					/* A socket's inode number		*/
	int			pid;					/* An owner's PID			*/
	ASC			procname;				/* An owner's /proc/<pid>/comm, ASCIC	*/
} SKSHOW$INODE_ENT;

/*
 * A single socket record as it has been extracted from the /proc/net/... and enriched by NETLINK.
 */
typedef	struct socket_info_t
{
	union	{
		struct sockaddr_in	sk4;
		struct sockaddr_in6	sk6;
		} lcl, rem;						/* Local and Remote end-points		*/

	uint64_t	rxbytes, txbytes,				/* Byte and packet counters		*/
			rxpkts, txpkts;
	unsigned long	inode;						/* A socket's inode number		*/
	unsigned	rxq, txq,					/* Receive/Transmit queue length	*/
			uid;						/* An owner's UID			*/
	int		family, proto, state,				/* AF_*, IPPROTO_*, SKSHOW$K_TCP_*	*/
			has_counters;					/* !0 - the counters above are valid	*/
} SKSHOW$SOCKET_INFO;

/*
 * A growable output accumulator: the whole screen is composed here and is written out
 * by a single write() - it kills the flicker and reduces a syscall storm.
 */
typedef	struct obuf_t
{
	char		*buf;						/* An allocated area, may be NULL	*/
	size_t		bufsz,						/* A size of the allocated area		*/
			buflen;						/* A number of octets in use		*/
} SKSHOW$OBUF;


/*
 * Command line driven global state
 */
ASC	g_logfspec = {0};						/* /LOGFILE				*/

int	g_trace = 0,							/* /TRACE				*/
	g_logsize = 0,							/* /LOGSIZE				*/
	g_interval = 3,							/* /INTERVAL, seconds			*/
	g_cycles = 0,							/* /COUNT, 0 - unlimited		*/
	g_counters = 1,							/* /COUNTERS, /NOCOUNTERS clears it	*/
	g_highlight = 1							/* /HIGHLIGHT, /NOHIGHLIGHT clears it	*/
	;

volatile sig_atomic_t	g_exit_flag = 0;				/* A global 'all must to be stop' flag	*/

/*
 * A resolved form of the selection criteria, is filled by s_setup_filters()
 */
static	ASC		s_pmask [SKSHOW$K_MAXPMASK];			/* /PMASK, a list of masks, lower-cased	*/
static	int		s_pmask_wild [SKSHOW$K_MAXPMASK];		/* !0 - the mask carries a wildcard	*/
static	uint32_t	s_lia[4], s_ria[4];				/* /LIA, /RIA in a binary form, NBO	*/
static	uint16_t	s_lpn, s_rpn;					/* /LPN, /RPN in the NBO		*/
static	unsigned	s_state_mask = 0;				/* /STATE, a bit per SKSHOW$K_TCP_*	*/
static	char		s_criteria [512] = {"none"};			/* The criteria echoed in the footer	*/
static	int		s_pmask_nr = 0,					/* A number of the /PMASK substrings	*/
			s_lia_set = 0, s_ria_set = 0,			/* !0 - the criteria is in effect	*/
			s_lia_family = 0, s_ria_family = 0,
			s_proto = 0,					/* 0 - any, or IPPROTO_TCP|IPPROTO_UDP	*/
			s_family = 0;					/* 0 - any, or AF_INET|AF_INET6		*/

/*
 * Module private run-time context
 */
static	SKSHOW$INODE_ENT	*s_inode_map[SKSHOW$K_HASHSZ];		/* inode -> (pid, comm)		*/
static	SKSHOW$DIAG_ENT		*s_diag_map[SKSHOW$K_HASHSZ];		/* socket id -> counters	*/
static	SKSHOW$PREV_ENT		*s_prev_map[SKSHOW$K_HASHSZ];		/* socket id -> previous cycle	*/
static	unsigned		s_cycle = 0;			/* A refresh serial number	*/
static	SKSHOW$OBUF		s_obuf = {0};				/* The screen accumulator	*/
static	unsigned		s_nlseq = 0;				/* A NETLINK sequence number	*/
static	int			s_ttyfd = STDOUT_FILENO,		/* A real stdout, see main()	*/
				s_istty = 0,				/* !0 - escape sequences are ok	*/
				s_eacces = 0;				/* A count of unreadable /proc/N/fd */


/*
 * A bare {0} terminator would leave the ASC's character array initialized by a scalar,
 * which is a -Wmissing-braces noise - a properly braced one is used instead.
 */
#define	$CLI_EOL	{ .name = {0, {0}} }

/*
 * Keyword tables of the value bearing qualifiers. A zero value means 'no filtering at all'.
 */
static	CLI_KEYWORD	s_kwd_proto [] = {
	{ .name = {$ASCINI("TCP")},		.val = IPPROTO_TCP },
	{ .name = {$ASCINI("UDP")},		.val = IPPROTO_UDP },
	{ .name = {$ASCINI("ALL")},		.val = 0 },
	$CLI_EOL
	};

static	CLI_KEYWORD	s_kwd_family [] = {
	{ .name = {$ASCINI("INET")},		.val = AF_INET },
	{ .name = {$ASCINI("INET6")},		.val = AF_INET6 },
	{ .name = {$ASCINI("IPV4")},		.val = AF_INET },
	{ .name = {$ASCINI("IPV6")},		.val = AF_INET6 },
	{ .name = {$ASCINI("ALL")},		.val = 0 },
	$CLI_EOL
	};

static	CLI_KEYWORD	s_kwd_state [] = {
	{ .name = {$ASCINI("ESTAB")},		.val = SKSHOW$K_TCP_ESTAB },
	{ .name = {$ASCINI("SYN_SENT")},	.val = SKSHOW$K_TCP_SYN_SENT },
	{ .name = {$ASCINI("SYN_RECV")},	.val = SKSHOW$K_TCP_SYN_RECV },
	{ .name = {$ASCINI("FIN_WAIT1")},	.val = SKSHOW$K_TCP_FIN_WAIT1 },
	{ .name = {$ASCINI("FIN_WAIT2")},	.val = SKSHOW$K_TCP_FIN_WAIT2 },
	{ .name = {$ASCINI("TIME_WAIT")},	.val = SKSHOW$K_TCP_TIME_WAIT },
	{ .name = {$ASCINI("CLOSE")},		.val = SKSHOW$K_TCP_CLOSE },
	{ .name = {$ASCINI("UNCONN")},		.val = SKSHOW$K_UDP_UNCONN },
	{ .name = {$ASCINI("CLOSE_WAIT")},	.val = SKSHOW$K_TCP_CLOSE_WAIT },
	{ .name = {$ASCINI("LAST_ACK")},	.val = SKSHOW$K_TCP_LAST_ACK },
	{ .name = {$ASCINI("LISTEN")},		.val = SKSHOW$K_TCP_LISTEN },
	{ .name = {$ASCINI("CLOSING")},		.val = SKSHOW$K_TCP_CLOSING },
	{ .name = {$ASCINI("ALL")},		.val = 0 },
	$CLI_EOL
	};

/*
 * An index of every qualifier in the s_quals[] below: the table is built by the designated
 * initializers, so the enumeration and the table cannot drift apart.
 */
enum	{
	SKSHOW$K_Q_LIA = 0,
	SKSHOW$K_Q_RIA,
	SKSHOW$K_Q_LPN,
	SKSHOW$K_Q_RPN,
	SKSHOW$K_Q_PROTO,
	SKSHOW$K_Q_FAMILY,
	SKSHOW$K_Q_STATE,
	SKSHOW$K_Q_PMASK,
	SKSHOW$K_Q_COUNTERS,
	SKSHOW$K_Q_HIGHLIGHT,
	SKSHOW$K_Q_INTERVAL,
	SKSHOW$K_Q_COUNT,
	SKSHOW$K_Q_TRACE,
	SKSHOW$K_Q_LOGFILE,
	SKSHOW$K_Q_LOGSIZE,

	SKSHOW$K_Q_MAX							/* End-Of-List				*/
	};

/*
 * A single qualifier table shared by the SHOW and the MONITOR verbs - the two differ by the
 * action argument only, see s_verbs[] below. /INTERVAL and /COUNT are accepted by the both
 * but are honoured in the MONITOR mode only.
 *
 * NOTE: /LIA and /RIA are declared as a string and not as a CLI$K_IPV4 / CLI$K_IPV6 on purpose -
 * the built in check accepts one family only, while both of them have to be taken here. The
 * conversion and the family detection are done by s_setup_filters().
 */
static	CLI_PQDESC	s_quals [SKSHOW$K_Q_MAX + 1] = {
	[SKSHOW$K_Q_LIA]	= { .name = {$ASCINI("LIA")},	  .type = CLI$K_QSTRING },
	[SKSHOW$K_Q_RIA]	= { .name = {$ASCINI("RIA")},	  .type = CLI$K_QSTRING },
	[SKSHOW$K_Q_LPN]	= { .name = {$ASCINI("LPN")},	  .type = CLI$K_NUM },
	[SKSHOW$K_Q_RPN]	= { .name = {$ASCINI("RPN")},	  .type = CLI$K_NUM },

	[SKSHOW$K_Q_PROTO]	= { .name = {$ASCINI("PROTO")},	  .type = CLI$K_KWD,
				    .kwd = s_kwd_proto,		  .defval = {$ASCINI("ALL")} },
	[SKSHOW$K_Q_FAMILY]	= { .name = {$ASCINI("FAMILY")},  .type = CLI$K_KWD,
				    .kwd = s_kwd_family,	  .defval = {$ASCINI("ALL")} },
	[SKSHOW$K_Q_STATE]	= { .name = {$ASCINI("STATE")},	  .type = CLI$K_KWD,
				    .kwd = s_kwd_state,		  .flag = CLI$M_LIST },
	[SKSHOW$K_Q_PMASK]	= { .name = {$ASCINI("PMASK")},	  .type = CLI$K_QSTRING,
				    .flag = CLI$M_LIST },

	[SKSHOW$K_Q_COUNTERS]	= { .name = {$ASCINI("COUNTERS")},.type = CLI$K_OPT,
				    .flag = CLI$M_NEGATABLE },
	[SKSHOW$K_Q_HIGHLIGHT]	= { .name = {$ASCINI("HIGHLIGHT")},.type = CLI$K_OPT,
				    .flag = CLI$M_NEGATABLE },

	[SKSHOW$K_Q_INTERVAL]	= { .name = {$ASCINI("INTERVAL")},.type = CLI$K_NUM,
				    .defval = {$ASCINI("3")} },
	[SKSHOW$K_Q_COUNT]	= { .name = {$ASCINI("COUNT")},	  .type = CLI$K_NUM,
				    .defval = {$ASCINI("0")} },

	[SKSHOW$K_Q_TRACE]	= { .name = {$ASCINI("TRACE")},	  .type = CLI$K_OPT,
				    .flag = CLI$M_NEGATABLE },
	[SKSHOW$K_Q_LOGFILE]	= { .name = {$ASCINI("LOGFILE")}, .type = CLI$K_FILE },
	[SKSHOW$K_Q_LOGSIZE]	= { .name = {$ASCINI("LOGSIZE")}, .type = CLI$K_NUM,
				    .defval = {$ASCINI("0")} }

	/* [SKSHOW$K_Q_MAX] is left zeroed - it is the end-of-table marker */
	};

_Static_assert($ARRSZ(s_quals) == (SKSHOW$K_Q_MAX + 1), "s_quals[] and the SKSHOW$K_Q_* enum have diverged");

/*
 * P1 of the HELP verb: an optional name of a verb to be detailed.
 */
static	CLI_PQDESC	s_parm_help [] = {
	{ .name = {$ASCINI("TOPIC")}, .type = CLI$K_QSTRING, .pn = CLI$K_P1 },
	$CLI_EOL
	};

/*
 * A mode of operation, is carried by the verb's action argument.
 */
enum	{
	SKSHOW$K_MODE_ONCE = 1,						/* A single snapshot			*/
	SKSHOW$K_MODE_LOOP						/* Refresh until interrupted		*/
	};

static	int	s_act_display	(CLI_CTX *a_clictx, void *a_arg);
static	int	s_act_help	(CLI_CTX *a_clictx, void *a_arg);

/*
 * The command grammar:
 *
 *	SKSHOW	SHOW	[/qualifiers ...]
 *		MONITOR	[/qualifiers ...]
 *		HELP	[<verb>]
 */
static	CLI_VERB	s_verbs [] = {
	{ .name = {$ASCINI("SHOW")},	.quals = s_quals,	.act_rtn = s_act_display,
					.act_arg = (void *) (intptr_t) SKSHOW$K_MODE_ONCE },
	{ .name = {$ASCINI("MONITOR")},	.quals = s_quals,	.act_rtn = s_act_display,
					.act_arg = (void *) (intptr_t) SKSHOW$K_MODE_LOOP },
	/*
	 * NOTE: in the normal course main() answers a help request from its own pre-scan and
	 * this action routine is never reached - it is the fallback for a command line that
	 * the pre-scan lets through, and it keeps the verb a first class member of the
	 * grammar, which is what the renderer walks to build the verb listing.
	 */
	{ .name = {$ASCINI("HELP")},	.params = s_parm_help,	.act_rtn = s_act_help },
	$CLI_EOL
	};

/*
 * A one line description of a verb or of a qualifier, keyed by an address of its descriptor.
 * The help renderer walks the grammar tables above and pulls a text from here, so a table
 * and its help cannot describe a different set of qualifiers.
 */
typedef	struct help_ent_t
{
	const void	*item;						/* CLI_VERB * or CLI_PQDESC *		*/
	const char	*hint;						/* A value placeholder override, or NULL	*/
	const char	*text;						/* A one line description		*/
} SKSHOW$HELP_ENT;

static const SKSHOW$HELP_ENT	s_help [] = {
	{ &s_verbs[0],				NULL,		"Display a single snapshot of the socket table and exit" },
	{ &s_verbs[1],				NULL,		"Refresh the socket table until interrupted by a ^C" },
	{ &s_verbs[2],				NULL,		"Display the on-line help; HELP <topic> details a verb or a qualifier" },

	{ &s_parm_help[0],			NULL,		"A topic: a verb, a /qualifier, EXAMPLES or MESSAGES" },

	{ &s_quals[SKSHOW$K_Q_LIA],		"=<address>",	"Select by a local IP address, an IPv4 or an IPv6 literal" },
	{ &s_quals[SKSHOW$K_Q_RIA],		"=<address>",	"Select by a remote IP address, an IPv4 or an IPv6 literal" },
	{ &s_quals[SKSHOW$K_Q_LPN],		"=<port>",	"Select by a local port number, 1 ... 65535" },
	{ &s_quals[SKSHOW$K_Q_RPN],		"=<port>",	"Select by a remote port number, 1 ... 65535" },
	{ &s_quals[SKSHOW$K_Q_PROTO],		NULL,		"Select by a transport protocol" },
	{ &s_quals[SKSHOW$K_Q_FAMILY],		NULL,		"Select by an address family" },
	{ &s_quals[SKSHOW$K_Q_STATE],		NULL,		"Select by a socket state, a record matches any of the list" },
	{ &s_quals[SKSHOW$K_Q_PMASK],		"=<mask>",	"Select by the owner's name, case ignored: a substring, or a *,%,? pattern" },
	{ &s_quals[SKSHOW$K_Q_COUNTERS],	NULL,		"Query NETLINK for the byte/packet counters, on by default" },
	{ &s_quals[SKSHOW$K_Q_HIGHLIGHT],	NULL,		"Show a row that has moved since the last refresh in reverse video" },
	{ &s_quals[SKSHOW$K_Q_INTERVAL],	"=<seconds>",	"A refresh interval, MONITOR only" },
	{ &s_quals[SKSHOW$K_Q_COUNT],		NULL,		"A number of the refresh cycles, 0 - unlimited, MONITOR only" },
	{ &s_quals[SKSHOW$K_Q_TRACE],		NULL,		"Produce an extensible diagnostic output, off by default" },
	{ &s_quals[SKSHOW$K_Q_LOGFILE],		NULL,		"A file to accept the diagnostic output, not the table" },
	{ &s_quals[SKSHOW$K_Q_LOGSIZE],		"=<octets>",	"Rewind the log file once it grows past this size" },

	{ NULL, NULL, NULL }
	};



/*
 *   DESCRIPTION: Compute a hash bucket index for a given inode number.
 *
 *   INPUTS:
 *	a_inode:	A socket's inode number
 *
 *   OUTPUTS:
 *	a_idx:		A bucket index in the range of 0 ... SKSHOW$K_HASHSZ - 1
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
inline	static	int	s_hash_inode	(
			unsigned long	a_inode,
			unsigned	*a_idx
				)
{
	/* Knuth's multiplicative constant, the low bits of an inode are not evenly distributed */
	*a_idx = (unsigned) ((a_inode * 2654435761UL) % SKSHOW$K_HASHSZ);

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Compute a hash bucket index for a given socket identity.
 *		A field-by-field mixing is used deliberately: the structure padding must not
 *		take a part in the computation.
 *
 *   INPUTS:
 *	a_key:		An address of the socket identity to be hashed
 *
 *   OUTPUTS:
 *	a_idx:		A bucket index in the range of 0 ... SKSHOW$K_HASHSZ - 1
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
inline	static	int	s_hash_skid	(
		const	SKSHOW$SK_KEY	*a_key,
			unsigned	*a_idx
				)
{
unsigned	l_hash = 2166136261U;					/* FNV-1a offset basis			*/

	for (int i = 0; i < 4; i++)
		{
		l_hash = (l_hash ^ a_key->lia[i]) * 16777619U;
		l_hash = (l_hash ^ a_key->ria[i]) * 16777619U;
		}

	l_hash = (l_hash ^ a_key->lpn) * 16777619U;
	l_hash = (l_hash ^ a_key->rpn) * 16777619U;
	l_hash = (l_hash ^ a_key->family) * 16777619U;
	l_hash = (l_hash ^ a_key->proto) * 16777619U;

	*a_idx = l_hash % SKSHOW$K_HASHSZ;

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Ensure that the output accumulator has a room for a_need additional octets.
 *
 *   INPUTS:
 *	a_obuf:		An address of the accumulator descriptor
 *	a_need:		A number of octets to be available past the current tail
 *
 *   OUTPUTS:
 *	a_obuf:		The buf/bufsz fields may be updated
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory, the accumulator is left intact
 */
static	int	s_obuf_grow	(
			SKSHOW$OBUF	*a_obuf,
			size_t		a_need
				)
{
char	*l_new;
size_t	l_newsz;

	l_newsz = a_obuf->bufsz ? a_obuf->bufsz : SKSHOW$K_OBUFINI;

	while ( (l_newsz - a_obuf->buflen) < a_need )
		l_newsz += l_newsz;

	if ( !(l_new = realloc(a_obuf->buf, l_newsz)) )
		return	$LOG(STS$K_ERROR, "realloc(%zu) failed, errno: %d", l_newsz, errno);

	a_obuf->buf = l_new;
	a_obuf->bufsz = l_newsz;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Discard a content of the output accumulator without releasing the memory.
 *
 *   INPUTS:
 *	a_obuf:		An address of the accumulator descriptor
 *
 *   OUTPUTS:
 *	a_obuf:		The buflen field is zeroed
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory at the very first allocation
 */
static	int	s_obuf_reset	(
			SKSHOW$OBUF	*a_obuf
				)
{
	a_obuf->buflen = 0;

	if ( !a_obuf->buf )
		return	s_obuf_grow(a_obuf, SKSHOW$K_OBUFINI);

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Append a formatted text to the output accumulator, growing it if needed.
 *
 *   INPUTS:
 *	a_obuf:		An address of the accumulator descriptor
 *	a_fmt:		A printf(3) style format string
 *	...:		Format arguments
 *
 *   OUTPUTS:
 *	a_obuf:		The buf/bufsz/buflen fields are updated
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- an encoding error or out of memory
 */
static	int	s_obuf_addf	(
			SKSHOW$OBUF	*a_obuf,
		const	char		*a_fmt,
				...
				)
{
va_list	l_ap;
size_t	l_avail;
int	l_len;

	/*
	 * A first pass writes into the tail of the buffer, if there is not enough room -
	 * vsnprintf() reports a required length, the buffer is grown and a second pass succeeds.
	 */
	for (int i = 0; i < 2; i++)
		{
		if ( !a_obuf->buf && !(1 & s_obuf_grow(a_obuf, SKSHOW$K_OBUFINI)) )
			return	STS$K_ERROR;

		l_avail = a_obuf->bufsz - a_obuf->buflen;

		va_start(l_ap, a_fmt);
		l_len = vsnprintf(a_obuf->buf + a_obuf->buflen, l_avail, a_fmt, l_ap);
		va_end(l_ap);

		if ( 0 > l_len )
			return	$LOG(STS$K_ERROR, "vsnprintf() failed, errno: %d", errno);

		if ( ((size_t) l_len) < l_avail )
			{
			a_obuf->buflen += (size_t) l_len;
			return	STS$K_SUCCESS;
			}

		if ( !(1 & s_obuf_grow(a_obuf, ((size_t) l_len) + 1)) )
			return	STS$K_ERROR;
		}

	return	$LOG(STS$K_ERROR, "Cannot fit a formatted record into the output buffer");
}


/*
 *   DESCRIPTION: Write out a content of the output accumulator by a single (possibly partial)
 *		write() sequence and reset the accumulator.
 *
 *   INPUTS:
 *	a_obuf:		An address of the accumulator descriptor
 *
 *   OUTPUTS:
 *	a_obuf:		The buflen field is zeroed
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- a write() error other than EINTR
 */
static	int	s_obuf_flush	(
			SKSHOW$OBUF	*a_obuf
				)
{
size_t	l_off = 0;
ssize_t	l_len;

	while ( l_off < a_obuf->buflen )
		{
		if ( 0 >= (l_len = write(s_ttyfd, a_obuf->buf + l_off, a_obuf->buflen - l_off)) )
			{
			if ( errno == EINTR )
				continue;

			a_obuf->buflen = 0;
			return	$LOG(STS$K_ERROR, "write(%d) failed, errno: %d", s_ttyfd, errno);
			}

		l_off += (size_t) l_len;
		}

	a_obuf->buflen = 0;

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Release all the entries of the inode -> (pid, comm) map.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_inodemap_purge	(void)
{
SKSHOW$INODE_ENT	*l_ent, *l_next;

	for (int i = 0; i < SKSHOW$K_HASHSZ; i++)
		{
		for (l_ent = s_inode_map[i]; l_ent; l_ent = l_next)
			{
			l_next = l_ent->link;
			free(l_ent);
			}

		s_inode_map[i] = NULL;
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Add a new inode -> (pid, process name) binding into the map.
 *
 *   INPUTS:
 *	a_inode:	A socket's inode number
 *	a_pid:		An owner's PID
 *	a_comm:		An owner's process name, ASCIZ
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory
 */
static	int	s_inodemap_add	(
			unsigned long	a_inode,
			int		a_pid,
		const	char		*a_comm
				)
{
SKSHOW$INODE_ENT	*l_ent;
unsigned		l_idx;

	if ( !(l_ent = calloc(1, sizeof(SKSHOW$INODE_ENT))) )
		return	$LOG(STS$K_ERROR, "calloc(%zu) failed, errno: %d", sizeof(SKSHOW$INODE_ENT), errno);

	l_ent->inode = a_inode;
	l_ent->pid = a_pid;
	__util$str2asc(a_comm, &l_ent->procname);

	s_hash_inode(a_inode, &l_idx);
	l_ent->link = s_inode_map[l_idx];
	s_inode_map[l_idx] = l_ent;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Find an owner of a socket by the socket's inode number.
 *
 *   INPUTS:
 *	a_inode:	A socket's inode number
 *
 *   OUTPUTS:
 *	a_pid:		An owner's PID, or 0 if the binding has not been found
 *	a_procname:	An address of the owner's name, ASCIC; a "???" placeholder if not found
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- a binding has been found
 *	STS$K_WARN	- nothing has been found, the placeholders are returned
 */
static	int	s_inodemap_find	(
			unsigned long	a_inode,
			int		*a_pid,
			ASC		**a_procname
				)
{
static	ASC	s_defname = {$ASCINI("???")};
SKSHOW$INODE_ENT	*l_ent;
unsigned		l_idx;

	s_hash_inode(a_inode, &l_idx);

	for (l_ent = s_inode_map[l_idx]; l_ent; l_ent = l_ent->link)
		{
		if ( l_ent->inode != a_inode )
			continue;

		*a_pid = l_ent->pid;
		*a_procname = &l_ent->procname;

		return	STS$K_SUCCESS;
		}

	*a_pid = 0;
	*a_procname = &s_defname;

	return	STS$K_WARN;
}


/*
 *   DESCRIPTION: Read a process name from the /proc/<pid>/comm.
 *
 *   INPUTS:
 *	a_pid:		A PID of a process of interest
 *	a_bufsz:	A size of the output buffer, octets
 *
 *   OUTPUTS:
 *	a_buf:		An ASCIZ process name, "?" if the file is not readable
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_WARN	- the /proc/<pid>/comm is not readable, a placeholder is returned
 */
static	int	s_read_comm	(
			int	a_pid,
			char	*a_buf,
			size_t	a_bufsz
				)
{
char	l_fspec[64];
char	*l_lf;
FILE	*l_fp;

	a_buf[0] = '?';
	a_buf[1] = '\0';

	snprintf(l_fspec, sizeof(l_fspec), "/proc/%d/comm", a_pid);

	if ( !(l_fp = fopen(l_fspec, "r")) )
		return	STS$K_WARN;

	if ( !fgets(a_buf, (int) a_bufsz, l_fp) )
		{
		fclose(l_fp);
		a_buf[0] = '?';
		a_buf[1] = '\0';
		return	STS$K_WARN;
		}

	fclose(l_fp);

	if ( (l_lf = strchr(a_buf, '\n')) )
		*l_lf = '\0';

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Rebuild the inode -> (pid, process name) map by walking the /proc/<pid>/fd/.
 *		Note: a file descriptor directory of a foreign process is readable by the root
 *		only, a non-privileged run will show the "???" as an owner of such sockets.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	a_retcount:	A number of the collected bindings, optional - may be NULL
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- the /proc is not accessible at all
 */
static	int	s_inodemap_build	(
			int	*a_retcount
				)
{
char		l_fdpath[64], l_lnkpath[384], l_target[128], *l_endp;
/*
 * The process name goes to __util$str2asc(), which reads up to ASC$K_SZ octets of it;
 * a buffer of that size satisfies the routine's contract outright. A /proc/<pid>/comm
 * is at most sixteen characters, so the room is nominal.
 */
char		l_comm[ASC$K_SZ + 1];
DIR		*l_procdir, *l_fddir;
struct dirent	*l_pident, *l_fdent;
unsigned long	l_inode;
ssize_t		l_len;
int		l_pid, l_count = 0;

	s_inodemap_purge();

	if ( a_retcount )
		*a_retcount = 0;

	if ( !(l_procdir = opendir("/proc")) )
		return	$LOG(STS$K_ERROR, "opendir(/proc) failed, errno: %d", errno);

	while ( (l_pident = readdir(l_procdir)) )
		{
		if ( !isdigit((unsigned char) l_pident->d_name[0]) )
			continue;

		l_pid = atoi(l_pident->d_name);

		snprintf(l_fdpath, sizeof(l_fdpath), "/proc/%d/fd", l_pid);

		if ( !(l_fddir = opendir(l_fdpath)) )
			{
			if ( errno == EACCES )
				s_eacces++;

			continue;
			}

		/*
		 * A process name is read once per process, not once per descriptor.
		 */
		s_read_comm(l_pid, l_comm, sizeof(l_comm));

		while ( (l_fdent = readdir(l_fddir)) )
			{
			if ( l_fdent->d_name[0] == '.' )
				continue;

			snprintf(l_lnkpath, sizeof(l_lnkpath), "%s/%s", l_fdpath, l_fdent->d_name);

			if ( 0 >= (l_len = readlink(l_lnkpath, l_target, sizeof(l_target) - 1)) )
				continue;

			l_target[l_len] = '\0';

			if ( strncmp(l_target, "socket:[", 8) )
				continue;

			l_inode = strtoul(l_target + 8, &l_endp, 10);

			if ( !l_endp || (*l_endp != ']') )
				continue;

			if ( 1 & s_inodemap_add(l_inode, l_pid, l_comm) )
				l_count++;
			}

		closedir(l_fddir);
		}

	closedir(l_procdir);

	if ( a_retcount )
		*a_retcount = l_count;

	$IFTRACE(g_trace, "Collected %d inode->(pid, comm) bindings", l_count);

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Release all the entries of the previous-cycle map.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_prevmap_purge	(void)
{
SKSHOW$PREV_ENT	*l_ent, *l_next;

	for (int i = 0; i < SKSHOW$K_HASHSZ; i++)
		{
		for (l_ent = s_prev_map[i]; l_ent; l_ent = l_next)
			{
			l_next = l_ent->link;
			free(l_ent);
			}

		s_prev_map[i] = NULL;
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Compare a socket with itself as it stood at the previous refresh, then
 *		record the current state for the next one.
 *
 *		Everything the row displays takes part: a counter, a queue length or a
 *		state that has moved is movement, and so is a socket that was not in the
 *		map at all - it has appeared during the interval. Nothing is reported on
 *		the very first refresh, when there is no previous state to differ from.
 *
 *   INPUTS:
 *	a_key:		The socket identity
 *	a_skinfo:	The socket as it stands now
 *
 *   OUTPUTS:
 *	a_moved:	!0 - the socket has moved since the previous refresh
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory; a_moved is cleared, the row is merely not marked
 */
static	int	s_prevmap_touch	(
		const	SKSHOW$SK_KEY		*a_key,
		const	SKSHOW$SOCKET_INFO	*a_skinfo,
			int			*a_moved
				)
{
SKSHOW$PREV_ENT	*l_ent;
unsigned	l_idx;

	*a_moved = 0;

	s_hash_skid(a_key, &l_idx);

	for (l_ent = s_prev_map[l_idx]; l_ent; l_ent = l_ent->link)
		if ( !memcmp(&l_ent->key, a_key, sizeof(SKSHOW$SK_KEY)) )
			break;

	if ( l_ent )
		{
		/* A record from an earlier refresh: has anything of it moved ? */
		*a_moved = (l_ent->rxbytes != a_skinfo->rxbytes)
			|| (l_ent->txbytes != a_skinfo->txbytes)
			|| (l_ent->rxpkts != a_skinfo->rxpkts)
			|| (l_ent->txpkts != a_skinfo->txpkts)
			|| (l_ent->rxq != a_skinfo->rxq)
			|| (l_ent->txq != a_skinfo->txq)
			|| (l_ent->state != a_skinfo->state);
		}
	else	{
		if ( !(l_ent = calloc(1, sizeof(SKSHOW$PREV_ENT))) )
			return	$LOG(STS$K_ERROR, "calloc(%zu) failed, errno: %d", sizeof(SKSHOW$PREV_ENT), errno);

		memcpy(&l_ent->key, a_key, sizeof(SKSHOW$SK_KEY));

		l_ent->link = s_prev_map[l_idx];
		s_prev_map[l_idx] = l_ent;

		/*
		 * A socket unknown to the map has appeared during the interval - which is
		 * movement, except on the first refresh, when every socket is new and a
		 * whole screen in reverse video would say nothing at all.
		 */
		*a_moved = (s_cycle > 1);
		}

	l_ent->rxbytes	= a_skinfo->rxbytes;
	l_ent->txbytes	= a_skinfo->txbytes;
	l_ent->rxpkts	= a_skinfo->rxpkts;
	l_ent->txpkts	= a_skinfo->txpkts;
	l_ent->rxq	= a_skinfo->rxq;
	l_ent->txq	= a_skinfo->txq;
	l_ent->state	= a_skinfo->state;
	l_ent->cycle	= s_cycle;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Drop from the previous-cycle map every socket that was not seen during
 *		the refresh just finished, so that the map holds the live sockets only
 *		and a long MONITOR run does not accumulate the closed ones.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	a_retcount:	A number of the entries dropped, optional - may be NULL
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_prevmap_reap	(
			int	*a_retcount
				)
{
SKSHOW$PREV_ENT	*l_ent, *l_next, **l_prev;
int		l_count = 0;

	for (int i = 0; i < SKSHOW$K_HASHSZ; i++)
		{
		l_prev = &s_prev_map[i];

		for (l_ent = s_prev_map[i]; l_ent; l_ent = l_next)
			{
			l_next = l_ent->link;

			if ( l_ent->cycle == s_cycle )
				{
				l_prev = &l_ent->link;
				continue;
				}

			*l_prev = l_next;
			free(l_ent);
			l_count++;
			}
		}

	if ( a_retcount )
		*a_retcount = l_count;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Release all the entries of the NETLINK counters cache.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_diagmap_purge	(void)
{
SKSHOW$DIAG_ENT	*l_ent, *l_next;

	for (int i = 0; i < SKSHOW$K_HASHSZ; i++)
		{
		for (l_ent = s_diag_map[i]; l_ent; l_ent = l_next)
			{
			l_next = l_ent->link;
			free(l_ent);
			}

		s_diag_map[i] = NULL;
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Store a set of counters of a single socket into the cache.
 *
 *   INPUTS:
 *	a_key:		A socket identity
 *	a_rxbytes:	A number of the received octets
 *	a_txbytes:	A number of the acknowledged (sent) octets
 *	a_rxpkts:	A number of the received segments
 *	a_txpkts:	A number of the sent segments
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory
 */
static	int	s_diagmap_add	(
		const	SKSHOW$SK_KEY	*a_key,
			uint64_t	a_rxbytes,
			uint64_t	a_txbytes,
			uint64_t	a_rxpkts,
			uint64_t	a_txpkts
				)
{
SKSHOW$DIAG_ENT	*l_ent;
unsigned	l_idx;

	if ( !(l_ent = calloc(1, sizeof(SKSHOW$DIAG_ENT))) )
		return	$LOG(STS$K_ERROR, "calloc(%zu) failed, errno: %d", sizeof(SKSHOW$DIAG_ENT), errno);

	memcpy(&l_ent->key, a_key, sizeof(SKSHOW$SK_KEY));

	l_ent->rxbytes = a_rxbytes;
	l_ent->txbytes = a_txbytes;
	l_ent->rxpkts = a_rxpkts;
	l_ent->txpkts = a_txpkts;
	l_ent->has_counters = 1;

	s_hash_skid(a_key, &l_idx);
	l_ent->link = s_diag_map[l_idx];
	s_diag_map[l_idx] = l_ent;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Look up the counters of a socket in the cache.
 *
 *   INPUTS:
 *	a_key:		A socket identity
 *
 *   OUTPUTS:
 *	a_ent:		An address of the found cache entry
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the entry has been found
 *	STS$K_WARN	- nothing has been found
 */
static	int	s_diagmap_find	(
		const	SKSHOW$SK_KEY	*a_key,
			SKSHOW$DIAG_ENT	**a_ent
				)
{
SKSHOW$DIAG_ENT	*l_ent;
unsigned	l_idx;

	s_hash_skid(a_key, &l_idx);

	for (l_ent = s_diag_map[l_idx]; l_ent; l_ent = l_ent->link)
		{
		if ( memcmp(&l_ent->key, a_key, sizeof(SKSHOW$SK_KEY)) )
			continue;

		*a_ent = l_ent;

		return	STS$K_SUCCESS;
		}

	*a_ent = NULL;

	return	STS$K_WARN;
}


/*
 *   DESCRIPTION: Extract the byte/packet counters from a single SOCK_DIAG_BY_FAMILY message
 *		and put them into the cache.
 *
 *   INPUTS:
 *	a_nlh:		An address of the NETLINK message header
 *	a_proto:	A protocol the dump has been requested for, IPPROTO_TCP|IPPROTO_UDP
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the counters have been stored
 *	STS$K_WARN	- the message carries no INET_DIAG_INFO attribute
 *	STS$K_ERROR	- a malformed message or out of memory
 */
static	int	s_diag_msg2cache	(
		const	struct nlmsghdr	*a_nlh,
			int		a_proto
				)
{
SKSHOW$SK_KEY		l_key;
struct inet_diag_msg	*l_diag;
struct rtattr		*l_rta;
struct tcp_info		l_ti;
size_t			l_tilen;
int			l_rtalen;

	if ( a_nlh->nlmsg_len < NLMSG_LENGTH(sizeof(struct inet_diag_msg)) )
		return	$LOG(STS$K_ERROR, "Truncated inet_diag_msg, nlmsg_len: %u", a_nlh->nlmsg_len);

	l_diag = (struct inet_diag_msg *) NLMSG_DATA(a_nlh);

	/*
	 * Build the very same key as the /proc/net parser does: the address/port fields
	 * of the inet_diag_msg are already in the network byte order.
	 */
	memset(&l_key, 0, sizeof(l_key));

	l_key.family = l_diag->idiag_family;
	l_key.proto = (uint8_t) a_proto;
	l_key.lpn = l_diag->id.idiag_sport;
	l_key.rpn = l_diag->id.idiag_dport;

	memcpy(l_key.lia, l_diag->id.idiag_src, (l_key.family == AF_INET) ? 4 : 16);
	memcpy(l_key.ria, l_diag->id.idiag_dst, (l_key.family == AF_INET) ? 4 : 16);

	/*
	 * Walk the attributes looking for the INET_DIAG_INFO (a struct tcp_info).
	 * A running kernel may deliver a shorter structure than the compile-time header
	 * declares, so a zeroed local copy is used and every field is length checked.
	 */
	l_rta = (struct rtattr *) (l_diag + 1);
	l_rtalen = (int) (a_nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*l_diag)));

	for ( ; RTA_OK(l_rta, l_rtalen); l_rta = RTA_NEXT(l_rta, l_rtalen) )
		{
		if ( l_rta->rta_type != INET_DIAG_INFO )
			continue;

		l_tilen = RTA_PAYLOAD(l_rta);

		memset(&l_ti, 0, sizeof(l_ti));
		memcpy(&l_ti, RTA_DATA(l_rta), $MINSZ(l_tilen, sizeof(l_ti)));

		return	s_diagmap_add(&l_key,
			$TCPI_HAS(l_tilen, tcpi_bytes_received) ? (uint64_t) l_ti.tcpi_bytes_received : 0,
			$TCPI_HAS(l_tilen, tcpi_bytes_acked)	? (uint64_t) l_ti.tcpi_bytes_acked : 0,
			$TCPI_HAS(l_tilen, tcpi_segs_in)	? (uint64_t) l_ti.tcpi_segs_in : 0,
			$TCPI_HAS(l_tilen, tcpi_segs_out)	? (uint64_t) l_ti.tcpi_segs_out : 0);
		}

	return	STS$K_WARN;
}


/*
 *   DESCRIPTION: Perform a single NETLINK_SOCK_DIAG dump for a given family/protocol pair
 *		and put every returned record into the counters cache.
 *
 *		One dump per family/protocol per refresh cycle is issued - the previous
 *		implementation has opened a socket and dumped the whole kernel table for
 *		every single line of the /proc/net/tcp, which is O(N^2), and then has read
 *		a single 8Kb datagram only, silently losing everything past the first chunk.
 *
 *   INPUTS:
 *	a_family:	AF_INET | AF_INET6
 *	a_proto:	IPPROTO_TCP | IPPROTO_UDP
 *
 *   OUTPUTS:
 *	a_retcount:	A number of the cached records, optional - may be NULL
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_WARN	- the sock_diag facility is not available in this kernel
 *	STS$K_ERROR	- out of memory or an unexpected socket error
 */
static	int	s_diag_dump	(
			int	a_family,
			int	a_proto,
			int	*a_retcount
				)
{
struct	{
	struct nlmsghdr		nlh;
	struct inet_diag_req_v2	req;
	} l_nlreq;

struct sockaddr_nl	l_sanl;
struct iovec		l_iov;
struct msghdr		l_msg;
struct nlmsghdr		*l_nlh;
struct nlmsgerr		*l_nlerr;
struct timeval		l_tmo;
char			*l_buf;
ssize_t			l_rcvlen;
int			l_sd, l_len, l_done = 0, l_count = 0, l_status = STS$K_SUCCESS;

	if ( a_retcount )
		*a_retcount = 0;

	if ( !(l_buf = malloc(SKSHOW$K_NLBUFSZ)) )
		return	$LOG(STS$K_ERROR, "malloc(%d) failed, errno: %d", SKSHOW$K_NLBUFSZ, errno);

	if ( 0 > (l_sd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_SOCK_DIAG)) )
		{
		free(l_buf);
		return	$LOG(STS$K_WARN, "socket(AF_NETLINK, NETLINK_SOCK_DIAG) failed, errno: %d", errno);
		}

	/*
	 * A dump must never hang the refresh cycle - a lost answer just costs us the counters.
	 */
	l_tmo.tv_sec = 1;
	l_tmo.tv_usec = 0;
	setsockopt(l_sd, SOL_SOCKET, SO_RCVTIMEO, &l_tmo, sizeof(l_tmo));

	memset(&l_nlreq, 0, sizeof(l_nlreq));

	l_nlreq.nlh.nlmsg_len = sizeof(l_nlreq);
	l_nlreq.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
	l_nlreq.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	l_nlreq.nlh.nlmsg_seq = ++s_nlseq;
	l_nlreq.nlh.nlmsg_pid = 0;					/* Let the kernel to fill it in		*/

	l_nlreq.req.sdiag_family = (uint8_t) a_family;
	l_nlreq.req.sdiag_protocol = (uint8_t) a_proto;
	l_nlreq.req.idiag_states = ~0U;					/* Every state is of interest		*/
	l_nlreq.req.idiag_ext = (a_proto == IPPROTO_TCP) ? (1 << (INET_DIAG_INFO - 1)) : 0;

	/* A zeroed cookie is a legal value, the wildcard is the INET_DIAG_NOCOOKIE */
	l_nlreq.req.id.idiag_cookie[0] = l_nlreq.req.id.idiag_cookie[1] = INET_DIAG_NOCOOKIE;

	memset(&l_sanl, 0, sizeof(l_sanl));
	l_sanl.nl_family = AF_NETLINK;

	l_iov.iov_base = &l_nlreq;
	l_iov.iov_len = sizeof(l_nlreq);

	memset(&l_msg, 0, sizeof(l_msg));
	l_msg.msg_name = &l_sanl;
	l_msg.msg_namelen = sizeof(l_sanl);
	l_msg.msg_iov = &l_iov;
	l_msg.msg_iovlen = 1;

	if ( 0 > sendmsg(l_sd, &l_msg, 0) )
		{
		l_status = $LOG(STS$K_WARN, "sendmsg(NETLINK, family: %d, proto: %d) failed, errno: %d",
				a_family, a_proto, errno);
		l_done = 1;
		}

	/*
	 * A dump is a multipart answer: keep reading until the NLMSG_DONE or an error.
	 */
	while ( !l_done && !g_exit_flag )
		{
		if ( 0 >= (l_rcvlen = recv(l_sd, l_buf, SKSHOW$K_NLBUFSZ, 0)) )
			{
			if ( (0 > l_rcvlen) && (errno == EINTR) )
				continue;

			if ( 0 > l_rcvlen )
				l_status = $LOG(STS$K_WARN, "recv(NETLINK) failed, errno: %d", errno);

			break;
			}

		l_len = (int) l_rcvlen;

		for (l_nlh = (struct nlmsghdr *) l_buf; NLMSG_OK(l_nlh, l_len); l_nlh = NLMSG_NEXT(l_nlh, l_len))
			{
			if ( l_nlh->nlmsg_seq != s_nlseq )		/* A stale answer, skip it		*/
				continue;

			if ( l_nlh->nlmsg_type == NLMSG_DONE )
				{
				l_done = 1;
				break;
				}

			if ( l_nlh->nlmsg_type == NLMSG_ERROR )
				{
				l_nlerr = (struct nlmsgerr *) NLMSG_DATA(l_nlh);

				if ( (l_nlh->nlmsg_len >= NLMSG_LENGTH(sizeof(*l_nlerr))) && l_nlerr->error )
					l_status = $LOG(STS$K_WARN, "NETLINK dump (family: %d, proto: %d) is "
							"rejected by kernel, error: %d - no counters will be shown",
							a_family, a_proto, -l_nlerr->error);
				l_done = 1;
				break;
				}

			if ( l_nlh->nlmsg_type != SOCK_DIAG_BY_FAMILY )
				continue;

			if ( 1 & s_diag_msg2cache(l_nlh, a_proto) )
				l_count++;
			}
		}

	close(l_sd);
	free(l_buf);

	if ( a_retcount )
		*a_retcount = l_count;

	$IFTRACE(g_trace, "NETLINK dump (family: %d, proto: %d): %d record(s) cached", a_family, a_proto, l_count);

	return	l_status;
}


/*
 *   DESCRIPTION: Rebuild the whole counters cache for the currently selected families/protocols.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	a_retcount:	A total number of the cached records, optional - may be NULL
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_diagmap_build	(
			int	*a_retcount
				)
{
int	l_count = 0, l_part;

	s_diagmap_purge();

	if ( a_retcount )
		*a_retcount = 0;

	if ( !g_counters )
		return	STS$K_SUCCESS;

	/* The UDP has no per socket byte counters, so the TCP only is dumped */
	if ( s_proto && (s_proto != IPPROTO_TCP) )
		return	STS$K_SUCCESS;

	if ( !s_family || (s_family == AF_INET) )
		if ( 1 & s_diag_dump(AF_INET, IPPROTO_TCP, &l_part) )
			l_count += l_part;

	if ( !s_family || (s_family == AF_INET6) )
		if ( 1 & s_diag_dump(AF_INET6, IPPROTO_TCP, &l_part) )
			l_count += l_part;

	if ( a_retcount )
		*a_retcount = l_count;

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Translate a socket state code to a printable text.
 *
 *   INPUTS:
 *	a_proto:	IPPROTO_TCP | IPPROTO_UDP
 *	a_state:	A state code as it has been taken from the /proc/net/...
 *
 *   OUTPUTS:
 *	a_str:		An address of a static, read-only text
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the state code is known
 *	STS$K_WARN	- an unknown state code, the "UNKNOWN" text is returned
 */
static	int	s_state2str	(
			int	a_proto,
			int	a_state,
		const	char	**a_str
				)
{
static const char *s_tcp_str [SKSHOW$K_TCP_MAX] = {
	"UNKNOWN",	"ESTAB",	"SYN_SENT",	"SYN_RECV",
	"FIN_WAIT1",	"FIN_WAIT2",	"TIME_WAIT",	"CLOSE",
	"CLOSE_WAIT",	"LAST_ACK",	"LISTEN",	"CLOSING"
	};

	if ( a_proto == IPPROTO_UDP )
		{
		switch ( a_state )
			{
			case	SKSHOW$K_UDP_ESTAB:
				*a_str = "ESTAB";
				return	STS$K_SUCCESS;

			case	SKSHOW$K_UDP_UNCONN:
				*a_str = "UNCONN";
				return	STS$K_SUCCESS;

			default:
				*a_str = "UNKNOWN";
				return	STS$K_WARN;
			}
		}

	if ( (0 >= a_state) || (a_state >= SKSHOW$K_TCP_MAX) )
		{
		*a_str = "UNKNOWN";
		return	STS$K_WARN;
		}

	*a_str = s_tcp_str[a_state];

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Format an end-point as a printable "address:port" text.
 *
 *   INPUTS:
 *	a_family:	AF_INET | AF_INET6
 *	a_addr:		An address of the struct in_addr or struct in6_addr
 *	a_port:		A port number, NBO
 *	a_bufsz:	A size of the output buffer, octets
 *
 *   OUTPUTS:
 *	a_buf:		An ASCIZ text, an IPv6 address is wrapped into the square brackets
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- the address cannot be converted
 */
static	int	s_fmt_endpoint	(
			int		a_family,
		const	void		*a_addr,
			uint16_t	a_port,
			char		*a_buf,
			size_t		a_bufsz
				)
{
char	l_ip[INET6_ADDRSTRLEN];

	if ( !inet_ntop(a_family, a_addr, l_ip, sizeof(l_ip)) )
		{
		snprintf(a_buf, a_bufsz, "<?>:%u", ntohs(a_port));
		return	$LOG(STS$K_ERROR, "inet_ntop(%d) failed, errno: %d", a_family, errno);
		}

	if ( a_family == AF_INET6 )
		snprintf(a_buf, a_bufsz, "[%s]:%u", l_ip, ntohs(a_port));
	else	snprintf(a_buf, a_bufsz, "%s:%u", l_ip, ntohs(a_port));

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Build a hash key out of a parsed socket record.
 *
 *   INPUTS:
 *	a_skinfo:	A parsed socket record
 *
 *   OUTPUTS:
 *	a_key:		A filled and padding-cleaned socket identity
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_skinfo2key	(
		const	SKSHOW$SOCKET_INFO	*a_skinfo,
			SKSHOW$SK_KEY		*a_key
				)
{
	memset(a_key, 0, sizeof(SKSHOW$SK_KEY));

	a_key->family = (uint8_t) a_skinfo->family;
	a_key->proto = (uint8_t) a_skinfo->proto;

	if ( a_skinfo->family == AF_INET )
		{
		a_key->lpn = a_skinfo->lcl.sk4.sin_port;
		a_key->rpn = a_skinfo->rem.sk4.sin_port;

		memcpy(a_key->lia, &a_skinfo->lcl.sk4.sin_addr, 4);
		memcpy(a_key->ria, &a_skinfo->rem.sk4.sin_addr, 4);
		}
	else	{
		a_key->lpn = a_skinfo->lcl.sk6.sin6_port;
		a_key->rpn = a_skinfo->rem.sk6.sin6_port;

		memcpy(a_key->lia, &a_skinfo->lcl.sk6.sin6_addr, 16);
		memcpy(a_key->ria, &a_skinfo->rem.sk6.sin6_addr, 16);
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Parse a single record of the /proc/net/{tcp,tcp6,udp,udp6}.
 *
 *		An important note on the octet order: the kernel prints an IPv4 address and
 *		every 32-bit word of an IPv6 address by "%08X" applied to a __be32 value, that
 *		is - through a host order interpretation of the network order octets. Reading
 *		it back by "%8x" and storing the result into the very same __be32 field restores
 *		the original octets on both the LE and the BE targets. Any htonl() applied here
 *		would corrupt the address (which is exactly what the previous revision did to
 *		every IPv6 record). A port, in contrast, is printed in the host order and does
 *		require the htons().
 *
 *   INPUTS:
 *	a_line:		A single ASCIZ record, the leading header line must be skipped by a caller
 *	a_family:	AF_INET | AF_INET6
 *	a_proto:	IPPROTO_TCP | IPPROTO_UDP
 *
 *   OUTPUTS:
 *	a_skinfo:	A filled socket record, is fully overwritten on success
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_WARN	- a malformed record, has to be skipped by a caller
 */
static	int	s_parse_proc_line	(
		const	char			*a_line,
			int			a_family,
			int			a_proto,
			SKSHOW$SOCKET_INFO	*a_skinfo
				)
{
unsigned	l_lia[4] = {0}, l_ria[4] = {0},
		l_lpn = 0, l_rpn = 0, l_state = 0,
		l_rxq = 0, l_txq = 0, l_uid = 0;
unsigned long	l_inode = 0;
int		l_rc;

	memset(a_skinfo, 0, sizeof(SKSHOW$SOCKET_INFO));

	if ( a_family == AF_INET )
		{
		l_rc = sscanf(a_line, "%*s %8x:%4x %8x:%4x %2x %8x:%8x %*2x:%*8x %*8x %10u %*d %lu",
			&l_lia[0], &l_lpn,
			&l_ria[0], &l_rpn,
			&l_state,
			&l_txq, &l_rxq,
			&l_uid,
			&l_inode);

		if ( l_rc != 9 )
			return	STS$K_WARN;

		a_skinfo->lcl.sk4.sin_family = AF_INET;
		a_skinfo->lcl.sk4.sin_port = htons((uint16_t) l_lpn);
		memcpy(&a_skinfo->lcl.sk4.sin_addr, &l_lia[0], 4);

		a_skinfo->rem.sk4.sin_family = AF_INET;
		a_skinfo->rem.sk4.sin_port = htons((uint16_t) l_rpn);
		memcpy(&a_skinfo->rem.sk4.sin_addr, &l_ria[0], 4);
		}
	else	{
		l_rc = sscanf(a_line, "%*s %8x%8x%8x%8x:%4x %8x%8x%8x%8x:%4x %2x %8x:%8x %*2x:%*8x %*8x %10u %*d %lu",
			&l_lia[0], &l_lia[1], &l_lia[2], &l_lia[3], &l_lpn,
			&l_ria[0], &l_ria[1], &l_ria[2], &l_ria[3], &l_rpn,
			&l_state,
			&l_txq, &l_rxq,
			&l_uid,
			&l_inode);

		if ( l_rc != 15 )
			return	STS$K_WARN;

		a_skinfo->lcl.sk6.sin6_family = AF_INET6;
		a_skinfo->lcl.sk6.sin6_port = htons((uint16_t) l_lpn);
		memcpy(&a_skinfo->lcl.sk6.sin6_addr, l_lia, 16);

		a_skinfo->rem.sk6.sin6_family = AF_INET6;
		a_skinfo->rem.sk6.sin6_port = htons((uint16_t) l_rpn);
		memcpy(&a_skinfo->rem.sk6.sin6_addr, l_ria, 16);
		}

	a_skinfo->family = a_family;
	a_skinfo->proto = a_proto;
	a_skinfo->state = (int) l_state;
	a_skinfo->inode = l_inode;
	a_skinfo->rxq = l_rxq;
	a_skinfo->txq = l_txq;
	a_skinfo->uid = l_uid;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Make a lower-cased copy of a counted string. Only the ASCII letters are folded:
 *		a process name is what the kernel keeps in the comm field, and a /PMASK is
 *		typed at a shell, neither carries anything a locale would have to say about.
 *
 *   INPUTS:
 *	a_src:		A string to be folded, ASCIC
 *
 *   OUTPUTS:
 *	a_dst:		The folded copy, ASCIC, NUL-terminated as well
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_asc_lower	(
		const	ASC	*a_src,
			ASC	*a_dst
				)
{
	$ASCLEN(a_dst) = $ASCLEN(a_src);

	for (unsigned i = 0; i < $ASCLEN(a_src); i++)
		$ASCPTR(a_dst)[i] = (unsigned char) tolower($ASCPTR(a_src)[i]);

	$ASCPTR(a_dst)[$ASCLEN(a_dst)] = '\0';

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Check a parsed record against the whole set of the selection criteria.
 *		A selection is deliberately kept out of the parser: the parser reports a
 *		malformed record, this routine reports a not interesting one, and a caller
 *		is able to tell one from another.
 *
 *   INPUTS:
 *	a_skinfo:	A parsed socket record
 *	a_procname:	An owner's process name, ASCIC
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the record has to be displayed
 *	STS$K_WARN	- the record is filtered out
 */
static	int	s_match_filter	(
		const	SKSHOW$SOCKET_INFO	*a_skinfo,
		const	ASC			*a_procname
				)
{
const void	*l_lia, *l_ria;
ASC		l_name;
uint16_t	l_lpn, l_rpn;
int		l_addrsz, l_hit;

	/* /STATE is a list folded into a bit mask, an empty mask means 'any state' */
	if ( s_state_mask )
		{
		if ( (0 > a_skinfo->state) || (31 < a_skinfo->state) )
			return	STS$K_WARN;

		if ( !(s_state_mask & (1U << a_skinfo->state)) )
			return	STS$K_WARN;
		}

	if ( a_skinfo->family == AF_INET )
		{
		l_lia = &a_skinfo->lcl.sk4.sin_addr;
		l_ria = &a_skinfo->rem.sk4.sin_addr;
		l_lpn = a_skinfo->lcl.sk4.sin_port;
		l_rpn = a_skinfo->rem.sk4.sin_port;
		l_addrsz = 4;
		}
	else	{
		l_lia = &a_skinfo->lcl.sk6.sin6_addr;
		l_ria = &a_skinfo->rem.sk6.sin6_addr;
		l_lpn = a_skinfo->lcl.sk6.sin6_port;
		l_rpn = a_skinfo->rem.sk6.sin6_port;
		l_addrsz = 16;
		}

	if ( s_lpn && (l_lpn != s_lpn) )
		return	STS$K_WARN;

	if ( s_rpn && (l_rpn != s_rpn) )
		return	STS$K_WARN;

	if ( s_lia_set )
		{
		if ( s_lia_family != a_skinfo->family )
			return	STS$K_WARN;

		if ( memcmp(l_lia, s_lia, l_addrsz) )
			return	STS$K_WARN;
		}

	if ( s_ria_set )
		{
		if ( s_ria_family != a_skinfo->family )
			return	STS$K_WARN;

		if ( memcmp(l_ria, s_ria, l_addrsz) )
			return	STS$K_WARN;
		}

	/*
	 * /PMASK is a list too: a record matches when ANY of the masks does. The masks have been
	 * lower-cased by s_setup_filters(), the name is lower-cased here, so the case of the letters
	 * plays no part. A mask with a wildcard is matched against the whole name, one without is
	 * looked for anywhere in it.
	 */
	if ( s_pmask_nr )
		{
		s_asc_lower(a_procname, &l_name);

		l_hit = 0;

		for (int i = 0; (i < s_pmask_nr) && !l_hit; i++)
			{
			if ( s_pmask_wild[i] )
				l_hit = __util$pattern_match((char *) $ASCPTR(&l_name), (char *) $ASCPTR(&s_pmask[i]));
			else	l_hit = (NULL != memmem($ASCPTR(&l_name), $ASCLEN(&l_name),
						$ASCPTR(&s_pmask[i]), $ASCLEN(&s_pmask[i])));
			}

		if ( !l_hit )
			return	STS$K_WARN;
		}

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Put the table header into the output accumulator.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory
 */
static	int	s_print_header	(void)
{
	return	s_obuf_addf(&s_obuf,
		"%-6s %-*s %-*s %-10s %7s %-*s %6s %6s %12s %12s %10s %10s%s\n"
		"%-6s %-*s %-*s %-10s %7s %-*s %6s %6s %12s %12s %10s %10s%s\n",

		"Proto",
		SKSHOW$K_EPCOL, "Local end-point",
		SKSHOW$K_EPCOL, "Remote end-point",
		"State", "PID",
		SKSHOW$K_PROCCOL, "Process",
		"Rx-Q", "Tx-Q", "RxBytes", "TxBytes", "RxPkts", "TxPkts",
		s_istty ? $ESC_EOL : "",

		"------",
		SKSHOW$K_EPCOL, "--------------------------------",
		SKSHOW$K_EPCOL, "--------------------------------",
		"----------", "-------",
		SKSHOW$K_PROCCOL, "--------------",
		"------", "------", "------------", "------------", "----------", "----------",
		s_istty ? $ESC_EOL : "");
}


/*
 *   DESCRIPTION: Put the table footer, carrying a record count and the selection criteria
 *		in effect, into the output accumulator.
 *
 *   INPUTS:
 *	a_count:	A number of the displayed records
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory
 */
static	int	s_print_footer	(
			int	a_count
				)
{
char	l_when[32];
time_t	l_now;
struct	tm l_tm;

	time(&l_now);
	localtime_r(&l_now, &l_tm);
	strftime(l_when, sizeof(l_when), "%d-%b-%Y %H:%M:%S", &l_tm);

	if ( !(1 & s_obuf_addf(&s_obuf, "%.*s%s\n",
			(int) (7 + 2 * (SKSHOW$K_EPCOL + 1) + 60),
			"--------------------------------------------------------------------------------"
			"--------------------------------------------------------------------------------"
			"--------------------------------------------------------------------------------",
			s_istty ? $ESC_EOL : "")) )
		return	STS$K_ERROR;

	if ( !(1 & s_obuf_addf(&s_obuf, " %s : %d record(s) displayed [%s]%s\n",
			l_when, a_count, s_criteria, s_istty ? $ESC_EOL : "")) )
		return	STS$K_ERROR;

	if ( s_eacces )
		{
		if ( !(1 & s_obuf_addf(&s_obuf, " NOTE: %d process(es) are not readable - run as root "
				"to resolve every socket owner%s\n", s_eacces, s_istty ? $ESC_EOL : "")) )
			return	STS$K_ERROR;
		}

	if ( s_istty )
		return	s_obuf_addf(&s_obuf, "%s", $ESC_EOS);

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Read a /proc/net/... file, select the interesting records and put them,
 *		enriched by the process name and the NETLINK counters, into the output
 *		accumulator.
 *
 *   INPUTS:
 *	a_fspec:	A file to be processed
 *	a_family:	AF_INET | AF_INET6
 *	a_proto:	IPPROTO_TCP | IPPROTO_UDP
 *
 *   OUTPUTS:
 *	a_count:	Is incremented by a number of the displayed records
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_WARN	- the file is not available (an IPv6 disabled kernel, for instance)
 *	STS$K_ERROR	- out of memory
 */
static	int	s_process_proc_file	(
		const	char	*a_fspec,
			int	a_family,
			int	a_proto,
			int	*a_count
				)
{
char			l_line[SKSHOW$K_LINESZ], l_lep[SKSHOW$K_EPSZ], l_rep[SKSHOW$K_EPSZ];
char			l_rxb[24], l_txb[24], l_rxp[24], l_txp[24];
const char		*l_state, *l_proto;
ASC			*l_procname;
FILE			*l_fp;
SKSHOW$SOCKET_INFO	l_skinfo;
SKSHOW$SK_KEY		l_key;
SKSHOW$DIAG_ENT		*l_diag;
const char		*l_rev, *l_norm;
int			l_pid = 0, l_moved = 0;

	if ( !(l_fp = fopen(a_fspec, "r")) )
		{
		$IFTRACE(g_trace, "fopen(%s) failed, errno: %d - skipped", a_fspec, errno);
		return	STS$K_WARN;
		}

	l_proto = (a_proto == IPPROTO_TCP)
			? ((a_family == AF_INET) ? "TCPv4" : "TCPv6")
			: ((a_family == AF_INET) ? "UDPv4" : "UDPv6");

	/* Drop the column header line */
	if ( !fgets(l_line, sizeof(l_line), l_fp) )
		{
		fclose(l_fp);
		return	STS$K_WARN;
		}

	while ( fgets(l_line, sizeof(l_line), l_fp) && !g_exit_flag )
		{
		if ( !(1 & s_parse_proc_line(l_line, a_family, a_proto, &l_skinfo)) )
			continue;

		s_inodemap_find(l_skinfo.inode, &l_pid, &l_procname);

		if ( !(1 & s_match_filter(&l_skinfo, l_procname)) )
			continue;

		/*
		 * Attach the counters, if the NETLINK has reported anything for this socket.
		 */
		s_skinfo2key(&l_skinfo, &l_key);

		if ( 1 & s_diagmap_find(&l_key, &l_diag) )
			{
			l_skinfo.rxbytes = l_diag->rxbytes;
			l_skinfo.txbytes = l_diag->txbytes;
			l_skinfo.rxpkts = l_diag->rxpkts;
			l_skinfo.txpkts = l_diag->txpkts;
			l_skinfo.has_counters = l_diag->has_counters;
			}

		if ( a_family == AF_INET )
			{
			s_fmt_endpoint(AF_INET, &l_skinfo.lcl.sk4.sin_addr,
					l_skinfo.lcl.sk4.sin_port, l_lep, sizeof(l_lep));
			s_fmt_endpoint(AF_INET, &l_skinfo.rem.sk4.sin_addr,
					l_skinfo.rem.sk4.sin_port, l_rep, sizeof(l_rep));
			}
		else	{
			s_fmt_endpoint(AF_INET6, &l_skinfo.lcl.sk6.sin6_addr,
					l_skinfo.lcl.sk6.sin6_port, l_lep, sizeof(l_lep));
			s_fmt_endpoint(AF_INET6, &l_skinfo.rem.sk6.sin6_addr,
					l_skinfo.rem.sk6.sin6_port, l_rep, sizeof(l_rep));
			}

		s_state2str(a_proto, l_skinfo.state, &l_state);

		/*
		 * Has anything of this socket moved during the interval ? The map is kept
		 * whether or not the marking is wanted, so that a /HIGHLIGHT switched on in
		 * mid-run has something to compare against.
		 */
		s_prevmap_touch(&l_key, &l_skinfo, &l_moved);

		/*
		 * The rendition is reset BEFORE the erase-to-end-of-line: a terminal fills
		 * the erased part with the current background, and a reversed one would
		 * paint the rest of the line.
		 */
		l_rev = (l_moved && g_highlight && s_istty) ? $ESC_REV : "";
		l_norm = *l_rev ? $ESC_NORM : "";

		if ( l_skinfo.has_counters )
			{
			snprintf(l_rxb, sizeof(l_rxb), "%" PRIu64, l_skinfo.rxbytes);
			snprintf(l_txb, sizeof(l_txb), "%" PRIu64, l_skinfo.txbytes);
			snprintf(l_rxp, sizeof(l_rxp), "%" PRIu64, l_skinfo.rxpkts);
			snprintf(l_txp, sizeof(l_txp), "%" PRIu64, l_skinfo.txpkts);
			}
		else	{
			strcpy(l_rxb, "n/a");
			strcpy(l_txb, "n/a");
			strcpy(l_rxp, "n/a");
			strcpy(l_txp, "n/a");
			}

		if ( !(1 & s_obuf_addf(&s_obuf,
				"%s%-6s %-*.*s %-*.*s %-10.10s %7d %-*.*s %6u %6u %12s %12s %10s %10s%s%s\n",
				l_rev, l_proto,
				SKSHOW$K_EPCOL, SKSHOW$K_EPCOL, l_lep,
				SKSHOW$K_EPCOL, SKSHOW$K_EPCOL, l_rep,
				l_state, l_pid,
				SKSHOW$K_PROCCOL, SKSHOW$K_PROCCOL, $ASCPTR(l_procname),
				l_skinfo.rxq, l_skinfo.txq,
				l_rxb, l_txb, l_rxp, l_txp,
				l_norm, s_istty ? $ESC_EOL : "")) )
			{
			fclose(l_fp);
			return	STS$K_ERROR;
			}

		(*a_count)++;
		}

	fclose(l_fp);

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Perform a single refresh cycle: rebuild the maps, compose the whole screen
 *		into the accumulator and write it out atomically.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	a_retcount:	A number of the displayed records, optional - may be NULL
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory or an output error
 */
static	int	s_refresh	(
			int	*a_retcount
				)
{
int	l_count = 0;

	if ( a_retcount )
		*a_retcount = 0;

	s_cycle++;

	s_inodemap_build(NULL);
	s_diagmap_build(NULL);

	if ( !(1 & s_obuf_reset(&s_obuf)) )
		return	STS$K_ERROR;

	if ( s_istty && !(1 & s_obuf_addf(&s_obuf, "%s", $ESC_HOME)) )
		return	STS$K_ERROR;

	if ( !(1 & s_print_header()) )
		return	STS$K_ERROR;

	if ( !s_proto || (s_proto == IPPROTO_TCP) )
		{
		if ( !s_family || (s_family == AF_INET) )
			s_process_proc_file(SKSHOW$K_PROC_TCP4, AF_INET, IPPROTO_TCP, &l_count);

		if ( !s_family || (s_family == AF_INET6) )
			s_process_proc_file(SKSHOW$K_PROC_TCP6, AF_INET6, IPPROTO_TCP, &l_count);
		}

	if ( !s_proto || (s_proto == IPPROTO_UDP) )
		{
		if ( !s_family || (s_family == AF_INET) )
			s_process_proc_file(SKSHOW$K_PROC_UDP4, AF_INET, IPPROTO_UDP, &l_count);

		if ( !s_family || (s_family == AF_INET6) )
			s_process_proc_file(SKSHOW$K_PROC_UDP6, AF_INET6, IPPROTO_UDP, &l_count);
		}

	/* Whatever was not seen this time round has gone; do not carry it forward */
	s_prevmap_reap(NULL);

	if ( !(1 & s_print_footer(l_count)) )
		return	STS$K_ERROR;

	if ( !(1 & s_obuf_flush(&s_obuf)) )
		return	STS$K_ERROR;

	if ( a_retcount )
		*a_retcount = l_count;

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: A signal handler. Only the async-signal-safe primitives are allowed here,
 *		so a plain write(2) of a preformatted text is used instead of the fprintf(3).
 *
 *   INPUTS:
 *	a_signo:	A signal number
 *
 *   OUTPUTS:
 *	g_exit_flag:	Is set on the SIGTERM/SIGINT/SIGQUIT
 *	g_trace:	Is toggled on the SIGUSR1
 *
 *   RETURNS:
 *	NONE
 */
static	void	s_sig_handler	(
			int	a_signo
				)
{
static const char	s_msg_second [] = "\nExit flag is already set, exiting ...\n";
static const char	s_msg_exit [] = "\nInterrupted, set exit flag ...\n";
ssize_t	l_wrt;
int	l_errno = errno;

	switch ( a_signo )
		{
		case	SIGTERM:
		case	SIGINT:
		case	SIGQUIT:
			/*
			 * The result is consumed rather than voided: a (void) cast does not
			 * silence warn_unused_result under -O2, and there is nothing sensible
			 * to do about a failed write from inside a handler anyway.
			 */
			if ( g_exit_flag )
				{
				l_wrt = write(STDERR_FILENO, s_msg_second, sizeof(s_msg_second) - 1);
				_exit(128 + a_signo);
				}

			l_wrt = write(STDERR_FILENO, s_msg_exit, sizeof(s_msg_exit) - 1);
			g_exit_flag = 1;
			break;

		case	SIGUSR1:
			g_trace = !g_trace;
			break;

		default:
			break;
		}

	(void) l_wrt;					/* consumed, see above			*/

	errno = l_errno;
}


/*
 *   DESCRIPTION: Establish the signal handlers.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- a sigaction() has failed for at least one signal
 */
static	int	s_init_sig_handler	(void)
{
static const int	s_siglist [] = {SIGTERM, SIGINT, SIGUSR1, SIGQUIT, 0};
struct sigaction	l_sa;
int			l_status = STS$K_SUCCESS;

	/* A broken pipe (a piped output has been closed) must not kill us abruptly */
	memset(&l_sa, 0, sizeof(l_sa));
	l_sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &l_sa, NULL);

	memset(&l_sa, 0, sizeof(l_sa));
	l_sa.sa_handler = s_sig_handler;
	sigemptyset(&l_sa.sa_mask);
	l_sa.sa_flags = 0;						/* No SA_RESTART: interrupt the sleep	*/

	for (int i = 0; s_siglist[i]; i++)
		{
		if ( 0 > sigaction(s_siglist[i], &l_sa, NULL) )
			l_status = $LOG(STS$K_ERROR, "sigaction(%d/%#x) failed, errno: %d",
					s_siglist[i], s_siglist[i], errno);

		$IFTRACE(g_trace, "A handler for the signal %d/%#x (%s) has been set",
			s_siglist[i], s_siglist[i], strsignal(s_siglist[i]));
		}

	return	l_status;
}




#ifdef	SKSHOW_WITH_HLB

/*
 * The help library of the product: where it is looked for and what it is called.
 *
 * SKSHOW_HELPLIB names a library outright - no '$' in the name of an environment variable on
 * this system. The top level key is the facility, everything the utility documents hangs below.
 */
#define	SKSHOW$K_HLBLOG		"SKSHOW_HELPLIB"
#define	SKSHOW$K_HLBTOP		"SKSHOW"
#define	SKSHOW$K_HLBNAME	"skshow.hlb"
#define	SKSHOW$K_HLBNAME_RU	"skshow_ru.hlb"


/*
 *   DESCRIPTION: Tell whether a file specification names a readable regular file.
 *
 *   INPUTS:
 *	a_fspec:	The specification, ASCIZ
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the file is there and can be read
 *	STS$K_WARN	- it is not
 */
static	int	s_hlb_readable	(
		const	char	*a_fspec
				)
{
struct stat	l_st = {0};

	if ( !a_fspec || !*a_fspec )
		return	STS$K_WARN;

	if ( stat(a_fspec, &l_st) || !S_ISREG(l_st.st_mode) )
		return	STS$K_WARN;

	return	access(a_fspec, R_OK) ? STS$K_WARN : STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Tell whether the locale of the process asks for Russian: LC_ALL, then
 *		LC_MESSAGES, then LANG, the first one set decides - the order the C RTL
 *		itself consults them in.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- a Russian locale
 *	STS$K_WARN	- anything else, the English library is the one wanted
 */
static	int	s_hlb_lang_ru	(void)
{
static const char	*s_vars [] = {"LC_ALL", "LC_MESSAGES", "LANG", NULL};
const char		*l_env;

	for (int i = 0; s_vars[i]; i++)
		{
		if ( !(l_env = getenv(s_vars[i])) || !*l_env )
			continue;

		return	strncasecmp(l_env, "ru", 2) ? STS$K_WARN : STS$K_SUCCESS;
		}

	return	STS$K_WARN;
}


/*
 *   DESCRIPTION: Locate the help library of the product. The first candidate that is a readable
 *		file wins:
 *
 *		  1. what SKSHOW_HELPLIB says, when it says anything;
 *		  2. beside the image - <image>/../share/help/skshow.hlb, which makes an installed
 *		     tree relocatable, and <image>/skshow.hlb, which makes a build tree work;
 *		  3. where the product was configured to install it, SKSHOW_K_HLBDIR.
 *
 *		Under a Russian locale the Russian library - ru/skshow_ru.hlb of a tree, or
 *		skshow_ru.hlb beside the image - is tried at each place first, and the English
 *		one answers when there is none.
 *
 *   INPUTS:
 *	a_bufsz:	A size of the buffer, octets
 *
 *   OUTPUTS:
 *	a_buf:		The specification of the library, ASCIZ
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- a_buf holds the specification of a library
 *	STS$K_WARN	- no library can be found
 */
static	int	s_hlb_findlib	(
			size_t	a_bufsz,
			char	*a_buf
				)
{
static const char	*s_tails_en [] = {"/../share/help/" SKSHOW$K_HLBNAME, "/" SKSHOW$K_HLBNAME},
			*s_tails_ru [] = {"/../share/help/ru/" SKSHOW$K_HLBNAME_RU, "/" SKSHOW$K_HLBNAME_RU};
const char		*l_env, **l_tails;
char			l_image [SKSHOW$K_SZ_PATH] = {0}, *l_slash;
ssize_t			l_len;
int			l_ru = (1 & s_hlb_lang_ru());

	/* 1. The environment variable: a library, taken as it stands */
	if ( (l_env = getenv(SKSHOW$K_HLBLOG)) && *l_env )
		{
		snprintf(a_buf, a_bufsz, "%s", l_env);

		if ( 1 & s_hlb_readable(a_buf) )
			return	STS$K_SUCCESS;
		}

	/*
	 * 2. Beside the image. The path of the running program is read from /proc/self/exe rather
	 * than taken from argv[0]: the latter is whatever the caller chose to pass, and a utility
	 * invoked through PATH carries no directory in it at all.
	 */
	if ( 0 < (l_len = readlink("/proc/self/exe", l_image, sizeof(l_image) - 1)) )
		{
		l_image[l_len] = '\0';

		if ( (l_slash = strrchr(l_image, '/')) )
			*l_slash = '\0';
		}
	else	l_image[0] = '\0';

	/* The Russian set first when the locale asks for it, then the English one */
	for (int pass = l_ru ? 0 : 1; pass < 2; pass++)
		{
		l_tails = pass ? s_tails_en : s_tails_ru;

		if ( l_image[0] )
			{
			for (int i = 0; i < 2; i++)
				{
				snprintf(a_buf, a_bufsz, "%s%s", l_image, l_tails[i]);

				if ( 1 & s_hlb_readable(a_buf) )
					return	STS$K_SUCCESS;
				}
			}

		/* 3. Where the product was configured to install the library */
#ifdef	SKSHOW_K_HLBDIR
		snprintf(a_buf, a_bufsz, "%s%s", SKSHOW_K_HLBDIR,
			pass ? "/" SKSHOW$K_HLBNAME : "/ru/" SKSHOW$K_HLBNAME_RU);

		if ( 1 & s_hlb_readable(a_buf) )
			return	STS$K_SUCCESS;
#endif
		}

	a_buf[0] = '\0';

	return	STS$K_WARN;
}


/*
 *   DESCRIPTION: Descend one level of the topic tree. The word - or the run of words a key
 *		carrying blanks is made of - is looked up under the current topic as it has been
 *		given; failing that, with a slash in front of it, so that a qualifier is asked
 *		for as "pmask" as well as "/pmask"; failing that, both ways under the root, so
 *		that "help show pmask" finds the qualifier although it is not a subtopic of SHOW.
 *		A word spelled in the GNU way, "--pmask", is the slashed one.
 *
 *   INPUTS:
 *	a_lib:		The library
 *	a_root:		An index of the facility topic
 *	a_parent:	An index of the topic to descend from
 *	a_wordcnt:	A number of the words left
 *	a_words:	The words themselves
 *
 *   OUTPUTS:
 *	a_used:		A number of the words consumed
 *	a_index:	An index of the topic found
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the topic has been found
 *	STS$K_WARN	- no topic of this name
 */
static	int	s_hlb_descend	(
			HLB$LIB	*a_lib,
			int	a_root,
			int	a_parent,
			int	a_wordcnt,
			char	**a_words,
			int	*a_used,
			int	*a_index
				)
{
char		l_slashed [HLB$K_SZ_KEY + 2] = {'/'}, *l_word [1] = {l_slashed};
const char	*l_plain = a_words[0];
int		l_parents [2] = {a_parent, a_root};

	if ( (l_plain[0] == '-') && (l_plain[1] == '-') )	/* --name is /name			*/
		l_plain += 2;

	snprintf(l_slashed + 1, sizeof(l_slashed) - 1, "%s", l_plain);

	for (int i = 0; i < 2; i++)
		{
		if ( i && (l_parents[1] == l_parents[0]) )	/* The root has been tried already	*/
			break;

		if ( l_plain == a_words[0] )
			{
			if ( 1 & hlb$lookup_run(a_lib, l_parents[i], a_wordcnt, a_words, a_used, a_index) )
				return	STS$K_SUCCESS;

			if ( l_plain[0] == '/' )		/* Nothing to add a slash to		*/
				continue;
			}

		if ( 1 & hlb$lookup_run(a_lib, l_parents[i], 1, l_word, a_used, a_index) )
			{
			*a_used = 1;

			return	STS$K_SUCCESS;
			}
		}

	return	STS$K_WARN;
}


/*
 *   DESCRIPTION: Put the names of the subtopics of a topic, if it has any, into the output the
 *		way the browser does it - "Additional information available:" and the keys in
 *		columns - so that a reader of "skshow help" sees what else may be asked for.
 *
 *   INPUTS:
 *	a_lib:		The library
 *	a_index:	An index of the topic
 *	a_out:		The output context
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_hlb_subtopics	(
			HLB$LIB		*a_lib,
			int		a_index,
			HELP$OUT	*a_out
				)
{
HLB$KEY		*l_key;
char		l_line [HELP$K_SZ_LINE];
unsigned	l_len = 0, l_percol, l_width = a_out->width ? a_out->width : HELP$K_DEFWIDTH;
int		l_child;

	if ( !(1 & hlb$firstchild(a_lib, a_index, &l_child)) || (0 > l_child) )
		return	STS$K_SUCCESS;

	help$put(a_out, "\n  Additional information available:\n\n", 0);

	/* A column is as wide as a key may be, or narrower on a narrow terminal, 4 columns at least */
	l_percol = (l_width - 4) / 4;
	l_percol = (l_percol > 20) ? 20 : ((l_percol < 8) ? 8 : l_percol);

	for ( ; 0 <= l_child; l_child = l_key->next)
		{
		if ( !(1 & hlb$getkey(a_lib, l_child, &l_key)) )
			break;

		if ( l_len && ((l_len + l_key->keylen + 1) > (l_width - 4)) )
			{
			while ( l_len && (l_line[l_len - 1] == ' ') )
				l_len--;

			help$put(a_out, "    !AD\n", 2, (fao_prm_t) l_len, (fao_prm_t) l_line);
			l_len = 0;
			}

		l_len += (unsigned) snprintf(l_line + l_len, sizeof(l_line) - l_len, "%-*.*s ",
				(int) l_percol, (int) l_key->keylen, l_key->key);
		}

	/* The last column of the last line carries no padding: a trailing blank is a blank wasted */
	while ( l_len && (l_line[l_len - 1] == ' ') )
		l_len--;

	if ( l_len )
		help$put(a_out, "    !AD\n", 2, (fao_prm_t) l_len, (fao_prm_t) l_line);

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Answer a help request out of the help library of the product: locate the
 *		library, descend to the topic that has been asked for and display its text with
 *		the path of the topic standing as the heading, and the subtopics listed after.
 *
 *   INPUTS:
 *	a_wordcnt:	A number of the words naming the topic, 0 - the description of the utility
 *	a_words:	The words themselves
 *	a_page:		!0 - stop at the end of a page (only ever at a terminal)
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the topic has been displayed
 *	STS$K_WARN	- no library can be found, the caller is to display the built-in summary
 *	STS$K_ERROR	- the library is there but names no such topic
 */
static	int	s_hlb_help	(
			int	a_wordcnt,
			char	**a_words,
			int	a_page
				)
{
HLB$LIB		*l_lib = NULL;
HELP$OUT	l_out;
const char	*l_txt = NULL;
char		l_fspec [SKSHOW$K_SZ_PATH] = {0}, l_path [SKSHOW$K_SZ_PATH] = {0};
unsigned	l_txtlen = 0, l_pathlen = 0;
int		l_root, l_parent, l_index, l_used, l_pos = 0;

	if ( !(1 & s_hlb_findlib(sizeof(l_fspec), l_fspec)) )
		{
		$IFTRACE(g_trace, "No help library at hand, the built-in summary answers");

		return	STS$K_WARN;
		}

	$IFTRACE(g_trace, "Help library: %s", l_fspec);

	if ( !(1 & hlb$open(l_fspec, &l_lib)) )
		return	STS$K_WARN;

	/*
	 * Everything this library documents hangs below the one key of level one, so the descent
	 * starts there rather than at the root: the reader asks for a verb, not for "skshow show".
	 */
	if ( !(1 & hlb$lookup(l_lib, -1, (unsigned) sizeof(SKSHOW$K_HLBTOP) - 1, SKSHOW$K_HLBTOP, &l_root)) )
		{
		hlb$close(l_lib);

		return	STS$K_WARN;
		}

	for (l_parent = l_root; l_pos < a_wordcnt; l_parent = l_index, l_pos += l_used)
		{
		if ( 1 & s_hlb_descend(l_lib, l_root, l_parent, a_wordcnt - l_pos, a_words + l_pos, &l_used, &l_index) )
			continue;

		$SKSHOWMSG(SKSHOW$_NOTOPIC, a_words[l_pos]);
		hlb$close(l_lib);

		return	STS$K_ERROR;
		}

	if ( !(1 & hlb$gettext(l_lib, l_parent, &l_txtlen, &l_txt)) )
		{
		hlb$close(l_lib);

		return	STS$K_WARN;
		}

	/*
	 * The display is the one of the HELP subsystem, so that a topic looks here exactly as it
	 * looks under the browser - the indent, the width of the terminal and the pager included.
	 */
	help$out_init(&l_out, s_ttyfd, STDIN_FILENO, a_page, 0);

	if ( 1 & hlb$path(l_lib, l_parent, (unsigned) sizeof(l_path), l_path, &l_pathlen) )
		help$put(&l_out, "\n  !AD\n\n", 2, (fao_prm_t) l_pathlen, (fao_prm_t) l_path);

	help$put_text(&l_out, 4, l_txtlen, l_txt);
	s_hlb_subtopics(l_lib, l_parent, &l_out);
	help$put(&l_out, "\n", 0);
	help$flush(&l_out);

	hlb$close(l_lib);

	return	STS$K_SUCCESS;
}

#else	/* SKSHOW_WITH_HLB */

/*
 *   DESCRIPTION: The stand-in of the routine above, compiled when the HELP package was not found:
 *		there is no library to read, so the caller displays the built-in summary.
 *
 *   INPUTS:
 *	a_wordcnt:	A number of the words naming the topic
 *	a_words:	The words themselves
 *	a_page:		Unused
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_WARN	- always
 */
static	int	s_hlb_help	(
			int	a_wordcnt,
			char	**a_words,
			int	a_page
				)
{
	(void) a_wordcnt;
	(void) a_words;
	(void) a_page;

	return	STS$K_WARN;
}

#endif	/* SKSHOW_WITH_HLB */




/*
 *   DESCRIPTION: Look up the on-line help text of a verb or of a qualifier descriptor.
 *
 *   INPUTS:
 *	a_item:		An address of a CLI_VERB or of a CLI_PQDESC to be described
 *
 *   OUTPUTS:
 *	a_text:		An address of a static, read-only text; an empty string if nothing is known
 *	a_hint:		An address of a value placeholder override, or NULL; optional - may be NULL
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- a description has been found
 *	STS$K_WARN	- the item carries no description
 */
static	int	s_help_text	(
		const	void	*a_item,
		const	char	**a_text,
		const	char	**a_hint
			)
{
	if ( a_hint )
		*a_hint = NULL;

	for (const SKSHOW$HELP_ENT *l_run = s_help; l_run->item; l_run++)
		{
		if ( l_run->item != a_item )
			continue;

		*a_text = l_run->text;

		if ( a_hint )
			*a_hint = l_run->hint;

		return	STS$K_SUCCESS;
		}

	*a_text = "";

	return	STS$K_WARN;
}


/*
 *   DESCRIPTION: Render a value placeholder of a qualifier according to its declared type.
 *
 *   INPUTS:
 *	a_pq:		A qualifier descriptor
 *	a_over:		A placeholder override taken from the help table, or NULL
 *	a_bufsz:	A size of the output buffer, octets
 *
 *   OUTPUTS:
 *	a_buf:		An ASCIZ placeholder, an empty string for a valueless qualifier
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 */
static	int	s_help_valhint	(
			CLI_PQDESC	*a_pq,
		const	char		*a_over,
			char		*a_buf,
			size_t		a_bufsz
			)
{
const char	*l_hint;

	if ( a_over )
		{
		snprintf(a_buf, a_bufsz, "%s%s", a_over, (a_pq->flag & CLI$M_LIST) ? "|=(v,...)" : "");

		return	STS$K_SUCCESS;
		}

	switch ( a_pq->type )
		{
		case	CLI$K_OPT:	l_hint = "";		break;
		case	CLI$K_NUM:	l_hint = "=<number>";	break;
		case	CLI$K_FILE:	l_hint = "=<file>";	break;
		case	CLI$K_KWD:	l_hint = "=<keyword>";	break;
		case	CLI$K_IPV4:	l_hint = "=<ip4>";	break;
		case	CLI$K_IPV6:	l_hint = "=<ip6>";	break;
		case	CLI$K_MAC:	l_hint = "=<mac>";	break;
		case	CLI$K_TIME:	l_hint = "=<time>";	break;
		default:		l_hint = "=<string>";	break;
		}

	/* A list qualifier accepts either a single value or a parenthesized list */
	snprintf(a_buf, a_bufsz, "%s%s", l_hint,
		((a_pq->flag & CLI$M_LIST) && *l_hint) ? "|=(v,...)" : "");

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Put a description of a single qualifier, and of its keywords if any,
 *		into the output accumulator.
 *
 *   INPUTS:
 *	a_pq:		A qualifier descriptor
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory
 */
static	int	s_help_qual	(
			CLI_PQDESC	*a_pq
			)
{
char		l_name[96], l_hint[48], l_kwds[64];
const char	*l_text, *l_over;
size_t		l_len = 0, l_add;
int		l_first = 1;

	s_help_text(a_pq, &l_text, &l_over);
	s_help_valhint(a_pq, l_over, l_hint, sizeof(l_hint));

	/* A negatable qualifier is spelled as /[NO]<name> */
	snprintf(l_name, sizeof(l_name), "/%s%.*s%s",
		(a_pq->flag & CLI$M_NEGATABLE) ? "[NO]" : "",
		$ASC(&a_pq->name), l_hint);

	if ( !(1 & s_obuf_addf(&s_obuf, "      %-34s %s%s\n", l_name, l_text,
			(a_pq->flag & CLI$M_REQUIRED) ? " [required]" : "")) )
		return	STS$K_ERROR;

	/*
	 * A keyword bearing qualifier gets an extra line with the accepted values.
	 */
	if ( a_pq->kwd )
		{
		for (CLI_KEYWORD *l_kwd = a_pq->kwd; $ASCLEN(&l_kwd->name); l_kwd++)
			{
			l_add = $ASCLEN(&l_kwd->name) + 2;

			/* Flush the accumulated names once the line is about to overflow */
			if ( l_len && ((l_len + l_add) >= (sizeof(l_kwds) - 1)) )
				{
				if ( !(1 & s_obuf_addf(&s_obuf, "      %-34s   %s %s,\n", "",
						l_first ? "values:" : "       ", l_kwds)) )
					return	STS$K_ERROR;

				l_first = 0;
				l_len = 0;
				}

			l_len += snprintf(l_kwds + l_len, sizeof(l_kwds) - l_len, "%s%.*s",
					l_len ? ", " : "", $ASC(&l_kwd->name));
			}

		if ( l_len && !(1 & s_obuf_addf(&s_obuf, "      %-34s   %s %s\n", "",
				l_first ? "values:" : "       ", l_kwds)) )
			return	STS$K_ERROR;
		}

	if ( $ASCLEN(&a_pq->defval) )
		{
		if ( !(1 & s_obuf_addf(&s_obuf, "      %-34s   default: %.*s\n", "", $ASC(&a_pq->defval))) )
			return	STS$K_ERROR;
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Put a full description of a single verb - its parameters and its qualifiers -
 *		into the output accumulator.
 *
 *   INPUTS:
 *	a_verb:		A verb descriptor
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- out of memory
 */
static	int	s_help_verb	(
			CLI_VERB	*a_verb
			)
{
const char	*l_text;

	s_help_text(a_verb, &l_text, NULL);

	if ( !(1 & s_obuf_addf(&s_obuf, "\n  %.*s - %s\n", $ASC(&a_verb->name), l_text)) )
		return	STS$K_ERROR;

	if ( a_verb->params && a_verb->params->pn )
		{
		if ( !(1 & s_obuf_addf(&s_obuf, "\n    Parameters:\n")) )
			return	STS$K_ERROR;

		for (CLI_PQDESC *l_pq = a_verb->params; l_pq->pn && $ASCLEN(&l_pq->name); l_pq++)
			{
			s_help_text(l_pq, &l_text, NULL);

			if ( !(1 & s_obuf_addf(&s_obuf, "      P%-33d %s%s\n", l_pq->pn, l_text,
					(l_pq->flag & CLI$M_REQUIRED) ? " [required]" : " [optional]")) )
				return	STS$K_ERROR;
			}
		}

	if ( a_verb->quals && $ASCLEN(&a_verb->quals->name) )
		{
		if ( !(1 & s_obuf_addf(&s_obuf, "\n    Qualifiers:\n")) )
			return	STS$K_ERROR;

		for (CLI_PQDESC *l_pq = a_verb->quals; $ASCLEN(&l_pq->name); l_pq++)
			if ( !(1 & s_help_qual(l_pq)) )
				return	STS$K_ERROR;
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Render the on-line help - either an overview of the whole grammar, or a
 *		detailed description of a single verb - and write it out.
 *
 *   INPUTS:
 *	a_topic:	A name of the verb of interest, ASCIC; NULL or empty for an overview
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- an unknown topic, or an output error
 */
static	int	s_help_show	(
			ASC	*a_topic
			)
{
CLI_VERB	*l_verb, *l_sel = NULL;
const char	*l_text;

	if ( !(1 & s_obuf_reset(&s_obuf)) )
		return	STS$K_ERROR;

	/*
	 * A topic given: find the verb by an exact name or by an unambiguous abbreviation.
	 */
	if ( a_topic && $ASCLEN(a_topic) )
		{
		for (l_verb = s_verbs; $ASCLEN(&l_verb->name); l_verb++)
			{
			if ( $ASCLEN(a_topic) > $ASCLEN(&l_verb->name) )
				continue;

			if ( strncasecmp($ASCPTR(a_topic), $ASCPTR(&l_verb->name), $ASCLEN(a_topic)) )
				continue;

			if ( $ASCLEN(a_topic) == $ASCLEN(&l_verb->name) )	/* an exact name wins	*/
				{
				l_sel = l_verb;
				break;
				}

			if ( l_sel )
				return	$SKSHOWMSG(SKSHOW$_AMBIGVERB, $ASCPTR(a_topic));

			l_sel = l_verb;
			}

		if ( !l_sel )
			return	$SKSHOWMSG(SKSHOW$_NOTOPIC, $ASCPTR(a_topic));
		}

	if ( !(1 & s_obuf_addf(&s_obuf,
			"\n  " __MODULE__ " " __IDENT__ " - a TCP/IP socket statistic viewer\n"
			"\n  Usage:  skshow <verb> [<parameter>] [/qualifier[=value] ...]\n"
			"\n          A verb and a qualifier may be abbreviated as long as it stays unambiguous;\n"
			"          a qualifier is introduced by either a '/' or a '--'.\n")) )
		return	STS$K_ERROR;

	if ( l_sel )
		{
		if ( !(1 & s_help_verb(l_sel)) )
			return	STS$K_ERROR;
		}
	else	{
		if ( !(1 & s_obuf_addf(&s_obuf, "\n  Verbs:\n")) )
			return	STS$K_ERROR;

		for (l_verb = s_verbs; $ASCLEN(&l_verb->name); l_verb++)
			{
			s_help_text(l_verb, &l_text, NULL);

			if ( !(1 & s_obuf_addf(&s_obuf, "      %-12.*s %s\n", $ASC(&l_verb->name), l_text)) )
				return	STS$K_ERROR;
			}

		if ( !(1 & s_obuf_addf(&s_obuf,
				"\n  Type \"skshow help <verb>\" for the parameters and the qualifiers of a verb.\n"
				"\n  Examples:\n"
				"      skshow show /state=listen /proto=tcp\n"
				"      skshow show /lia=127.0.0.1 /pmask=(nginx,redis)\n"
				"      skshow show /pmask=ssh*\n"
				"      skshow monitor /interval=2 /count=10 /nocounters\n"
				"      skshow monitor /family=inet6 /state=(estab,time_wait)\n")) )
			return	STS$K_ERROR;
		}

	if ( !(1 & s_obuf_addf(&s_obuf, "\n")) )
		return	STS$K_ERROR;

	return	s_obuf_flush(&s_obuf);
}




/*
 *   DESCRIPTION: Answer a help request whichever way is open: out of the help library of the
 *		product when one is at hand, out of the grammar tables otherwise - so a request
 *		never depends on anything being installed.
 *
 *   INPUTS:
 *	a_wordcnt:	A number of the words naming the topic, 0 - an overview
 *	a_words:	The words themselves
 *	a_page:		!0 - page at a terminal, 0 - do not, <0 - page when the output is a terminal
 *	a_topic:	The first word as an ASCIC, for the built-in renderer; NULL for an overview
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- an unknown topic, or an output error
 */
static	int	s_help_any	(
			int	a_wordcnt,
			char	**a_words,
			int	a_page,
			ASC	*a_topic
				)
{
int	l_status;

	l_status = s_hlb_help(a_wordcnt, a_words, (0 > a_page) ? s_istty : a_page);

	if ( l_status == STS$K_WARN )
		l_status = s_help_show(a_topic);

	return	l_status;
}


/*
 *   DESCRIPTION: Resolve a keyword string, possibly abbreviated, against a keyword table.
 *		An exact, full length name always wins over an abbreviation.
 *
 *   INPUTS:
 *	a_kwd:		A keyword table, terminated by a zero length name
 *	a_name:		A name to be resolved, ASCIC
 *
 *   OUTPUTS:
 *	a_val:		A value associated with the matched keyword
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the keyword has been resolved
 *	STS$K_WARN	- nothing has been matched, or the abbreviation is ambiguous
 */
static	int	s_kwd2val	(
			CLI_KEYWORD		*a_kwd,
			ASC			*a_name,
			unsigned long long	*a_val
			)
{
CLI_KEYWORD	*l_run, *l_sel = NULL, *l_exact = NULL;
int		l_nmatch = 0;

	if ( !$ASCLEN(a_name) )
		return	STS$K_WARN;

	for (l_run = a_kwd; $ASCLEN(&l_run->name); l_run++)
		{
		if ( $ASCLEN(a_name) > $ASCLEN(&l_run->name) )
			continue;

		if ( strncasecmp($ASCPTR(a_name), $ASCPTR(&l_run->name), $ASCLEN(a_name)) )
			continue;

		if ( $ASCLEN(a_name) == $ASCLEN(&l_run->name) )
			{
			l_exact = l_run;
			continue;
			}

		l_nmatch++;
		l_sel = l_run;
		}

	if ( l_exact )
		l_sel = l_exact;
	else if ( 1 < l_nmatch )
		return	STS$K_WARN;

	if ( !l_sel )
		return	STS$K_WARN;

	*a_val = l_sel->val;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Retrieve a string value of a qualifier, or its declared default.
 *
 *   INPUTS:
 *	a_clictx:	A parse context
 *	a_pq:		A qualifier descriptor
 *
 *   OUTPUTS:
 *	a_val:		A value, ASCIC; a zero length string when nothing is available
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- a value (or a default) has been returned
 *	STS$K_WARN	- the qualifier is absent and carries no default
 */
static	int	s_get_asc	(
			CLI_CTX		*a_clictx,
			CLI_PQDESC	*a_pq,
			ASC		*a_val
			)
{
	$ASCLEN(a_val) = 0;
	$ASCPTR(a_val)[0] = '\0';

	if ( !(1 & __cli$get_value(a_clictx, a_pq, a_val)) )
		{
		$ASCLEN(a_val) = 0;
		$ASCPTR(a_val)[0] = '\0';

		return	STS$K_WARN;
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Retrieve a numeric value of a qualifier and check it against a range.
 *
 *   INPUTS:
 *	a_clictx:	A parse context
 *	a_pq:		A qualifier descriptor
 *	a_lo, a_hi:	An inclusive range of the acceptable values
 *
 *   OUTPUTS:
 *	a_val:		The value; is left untouched when the qualifier is absent
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- a value has been returned
 *	STS$K_WARN	- the qualifier is absent and carries no default
 *	STS$K_ERROR	- the value is out of the given range
 */
static	int	s_get_num	(
			CLI_CTX		*a_clictx,
			CLI_PQDESC	*a_pq,
			long long	a_lo,
			long long	a_hi,
			long long	*a_val
			)
{
ASC		l_val;
char		*l_endp;
long long	l_num;

	if ( !(1 & s_get_asc(a_clictx, a_pq, &l_val)) )
		return	STS$K_WARN;

	/* The syntax has been validated by the CLI as a CLI$K_NUM, the range is ours */
	errno = 0;
	l_num = strtoll($ASCPTR(&l_val), &l_endp, 0);

	if ( errno || !l_endp || *l_endp )
		return	$SKSHOWMSG(SKSHOW$_IVNUM, $ASCPTR(&l_val), $ASCPTR(&a_pq->name));

	if ( (l_num < a_lo) || (l_num > a_hi) )
		return	$SKSHOWMSG(SKSHOW$_RANGE, $ASCPTR(&a_pq->name), l_num, a_lo, a_hi);

	*a_val = l_num;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Retrieve a state of a valueless, negatable qualifier.
 *
 *   INPUTS:
 *	a_clictx:	A parse context
 *	a_pq:		A qualifier descriptor
 *	a_default:	A value to be returned when the qualifier is absent
 *
 *   OUTPUTS:
 *	a_val:		0 - the qualifier is negated, !0 - it is asserted
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the qualifier is present in either form
 *	STS$K_WARN	- the qualifier is absent, the default has been returned
 */
static	int	s_get_flag	(
			CLI_CTX		*a_clictx,
			CLI_PQDESC	*a_pq,
			int		a_default,
			int		*a_val
			)
{
unsigned	l_state = 0;

	*a_val = a_default;

	if ( !(1 & __cli$present(a_clictx, a_pq, &l_state)) )
		return	STS$K_WARN;

	*a_val = (l_state & CLI$M_NEGATED) ? 0 : 1;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Extract every selection criterion out of a parse context, convert it into
 *		the binary form used by s_match_filter(), and compose a text echo of the
 *		whole set for the footer line.
 *
 *   INPUTS:
 *	a_clictx:	A parse context of the SHOW or of the MONITOR verb
 *
 *   OUTPUTS:
 *	The g_* and the s_* selection globals are filled in
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- an unparsable or an out of range value has been given
 */
static	int	s_setup_filters	(
			CLI_CTX		*a_clictx
			)
{
ASC			l_val;
unsigned long long	l_kwdval;
long long		l_num;
int			l_opts, l_len = 0;

	/*
	 * An absent qualifier is a normal situation here and must not be logged as an error,
	 * so the signalling is switched off for the whole extraction.
	 */
	l_opts = a_clictx->opts;
	a_clictx->opts &= ~CLI$M_OPSIGNAL;

	/* /TRACE, /COUNTERS - valueless, negatable */
	s_get_flag(a_clictx, &s_quals[SKSHOW$K_Q_TRACE], 0, &g_trace);
	s_get_flag(a_clictx, &s_quals[SKSHOW$K_Q_COUNTERS], 1, &g_counters);
	s_get_flag(a_clictx, &s_quals[SKSHOW$K_Q_HIGHLIGHT], 1, &g_highlight);

	/* /LOGFILE, /LOGSIZE */
	s_get_asc(a_clictx, &s_quals[SKSHOW$K_Q_LOGFILE], &g_logfspec);

	l_num = g_logsize;
	if ( 2 & s_get_num(a_clictx, &s_quals[SKSHOW$K_Q_LOGSIZE], 0, 0x7FFFFFFF, &l_num) )
		goto fail;
	g_logsize = (int) l_num;

	/* /INTERVAL, /COUNT */
	l_num = g_interval;
	if ( 2 & s_get_num(a_clictx, &s_quals[SKSHOW$K_Q_INTERVAL], 0, 86400, &l_num) )
		goto fail;
	g_interval = (int) l_num;

	l_num = g_cycles;
	if ( 2 & s_get_num(a_clictx, &s_quals[SKSHOW$K_Q_COUNT], 0, 0x7FFFFFFF, &l_num) )
		goto fail;
	g_cycles = (int) l_num;

	/*
	 * /LIA, /RIA - an IPv4 or an IPv6 literal. inet_pton() yields the network order octets,
	 * exactly as the /proc parser produces, so a plain memcmp() is enough later on.
	 */
	if ( 1 & s_get_asc(a_clictx, &s_quals[SKSHOW$K_Q_LIA], &l_val) )
		{
		if ( 0 < inet_pton(AF_INET, $ASCPTR(&l_val), s_lia) )
			s_lia_family = AF_INET;
		else if ( 0 < inet_pton(AF_INET6, $ASCPTR(&l_val), s_lia) )
			s_lia_family = AF_INET6;
		else	{
			$SKSHOWMSG(SKSHOW$_IVADDR, $ASCPTR(&l_val), "LIA");
			goto fail;
			}

		s_lia_set = 1;
		l_len += snprintf(s_criteria + l_len, sizeof(s_criteria) - l_len, "%sLIA: %s",
				l_len ? ", " : "", $ASCPTR(&l_val));
		}

	if ( 1 & s_get_asc(a_clictx, &s_quals[SKSHOW$K_Q_RIA], &l_val) )
		{
		if ( 0 < inet_pton(AF_INET, $ASCPTR(&l_val), s_ria) )
			s_ria_family = AF_INET;
		else if ( 0 < inet_pton(AF_INET6, $ASCPTR(&l_val), s_ria) )
			s_ria_family = AF_INET6;
		else	{
			$SKSHOWMSG(SKSHOW$_IVADDR, $ASCPTR(&l_val), "RIA");
			goto fail;
			}

		s_ria_set = 1;
		l_len += snprintf(s_criteria + l_len, sizeof(s_criteria) - l_len, "%sRIA: %s",
				l_len ? ", " : "", $ASCPTR(&l_val));
		}

	/* /LPN, /RPN */
	l_num = 0;
	if ( 2 & s_get_num(a_clictx, &s_quals[SKSHOW$K_Q_LPN], 1, 65535, &l_num) )
		goto fail;

	if ( l_num )
		{
		s_lpn = htons((uint16_t) l_num);
		l_len += snprintf(s_criteria + l_len, sizeof(s_criteria) - l_len, "%sLPN: %lld",
				l_len ? ", " : "", l_num);
		}

	l_num = 0;
	if ( 2 & s_get_num(a_clictx, &s_quals[SKSHOW$K_Q_RPN], 1, 65535, &l_num) )
		goto fail;

	if ( l_num )
		{
		s_rpn = htons((uint16_t) l_num);
		l_len += snprintf(s_criteria + l_len, sizeof(s_criteria) - l_len, "%sRPN: %lld",
				l_len ? ", " : "", l_num);
		}

	/* /PROTO, /FAMILY - a single keyword each, a zero value means 'no filtering' */
	if ( 1 & s_get_asc(a_clictx, &s_quals[SKSHOW$K_Q_PROTO], &l_val) )
		{
		if ( !(1 & s_kwd2val(s_kwd_proto, &l_val, &l_kwdval)) )
			{
			$SKSHOWMSG(SKSHOW$_IVKWD, $ASCPTR(&l_val), "PROTO");
			goto fail;
			}

		s_proto = (int) l_kwdval;
		l_len += snprintf(s_criteria + l_len, sizeof(s_criteria) - l_len, "%sProto: %s",
				l_len ? ", " : "", s_proto ? $ASCPTR(&l_val) : "ALL");
		}

	if ( 1 & s_get_asc(a_clictx, &s_quals[SKSHOW$K_Q_FAMILY], &l_val) )
		{
		if ( !(1 & s_kwd2val(s_kwd_family, &l_val, &l_kwdval)) )
			{
			$SKSHOWMSG(SKSHOW$_IVKWD, $ASCPTR(&l_val), "FAMILY");
			goto fail;
			}

		s_family = (int) l_kwdval;
		l_len += snprintf(s_criteria + l_len, sizeof(s_criteria) - l_len, "%sFamily: %s",
				l_len ? ", " : "", s_family ? $ASCPTR(&l_val) : "ALL");
		}

	/*
	 * /STATE - a list of keywords folded into a bit mask; an ALL anywhere in the list
	 * drops the whole criterion.
	 */
	s_state_mask = 0;

	while ( 1 & s_get_asc(a_clictx, &s_quals[SKSHOW$K_Q_STATE], &l_val) )
		{
		if ( !(1 & s_kwd2val(s_kwd_state, &l_val, &l_kwdval)) )
			{
			$SKSHOWMSG(SKSHOW$_IVKWD, $ASCPTR(&l_val), "STATE");
			goto fail;
			}

		if ( !l_kwdval )					/* ALL - no filtering	*/
			{
			s_state_mask = 0;
			break;
			}

		s_state_mask |= (1U << l_kwdval);

		l_len += snprintf(s_criteria + l_len, sizeof(s_criteria) - l_len, "%s%s%s",
				l_len ? ", " : "", s_state_mask & ~(1U << l_kwdval) ? "" : "State: ",
				$ASCPTR(&l_val));
		}

	/*
	 * /PMASK - a list of masks, a record matches when any of them matches the owner's process
	 * name. The mask is echoed as typed and then lower-cased for the matcher, which lower-cases
	 * the name too - so the case of the letters is ignored. A mask carrying a '*', a '%' or a '?'
	 * is a wildcard pattern for __util$pattern_match(), one without is a plain substring.
	 */
	s_pmask_nr = 0;

	while ( 1 & s_get_asc(a_clictx, &s_quals[SKSHOW$K_Q_PMASK], &l_val) )
		{
		if ( s_pmask_nr >= SKSHOW$K_MAXPMASK )
			{
			$SKSHOWMSG(SKSHOW$_TOOMANY, "/PMASK values", (unsigned) SKSHOW$K_MAXPMASK);
			break;
			}

		l_len += snprintf(s_criteria + l_len, sizeof(s_criteria) - l_len, "%s%s\"%s\"",
				l_len ? ", " : "", s_pmask_nr ? "" : "PMask: ", $ASCPTR(&l_val));

		s_asc_lower(&l_val, &s_pmask[s_pmask_nr]);
		s_pmask_wild[s_pmask_nr] = (NULL != strpbrk((char *) $ASCPTR(&s_pmask[s_pmask_nr]), "*%?"));

		s_pmask_nr++;
		}

	/* A cross check: an IPv6 criterion against an explicitly requested IPv4 family and vice versa */
	if ( s_family && s_lia_set && (s_lia_family != s_family) )
		{
		$SKSHOWMSG(SKSHOW$_CONFQUAL, "LIA", "FAMILY");
		goto fail;
		}

	if ( s_family && s_ria_set && (s_ria_family != s_family) )
		{
		$SKSHOWMSG(SKSHOW$_CONFQUAL, "RIA", "FAMILY");
		goto fail;
		}

	if ( !l_len )
		snprintf(s_criteria, sizeof(s_criteria), "no selection");

	a_clictx->opts = l_opts;

	return	STS$K_SUCCESS;

fail:
	a_clictx->opts = l_opts;

	return	STS$K_ERROR;
}




/*
 *   DESCRIPTION: An action routine of the SHOW and of the MONITOR verbs: set up the criteria,
 *		establish the signal handlers and run the refresh loop.
 *
 *   INPUTS:
 *	a_clictx:	A parse context
 *	a_arg:		A mode of operation, SKSHOW$K_MODE_ONCE or SKSHOW$K_MODE_LOOP
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- an illegal criterion or a run-time failure
 */
static	int	s_act_display	(
			CLI_CTX	*a_clictx,
			void	*a_arg
			)
{
int	l_mode = (int) (intptr_t) a_arg, l_cycle = 0, l_count = 0, l_rest;

	if ( !(1 & s_setup_filters(a_clictx)) )
		return	STS$K_ERROR;

	/* A single snapshot: neither an interval nor a cycle count applies */
	if ( l_mode == SKSHOW$K_MODE_ONCE )
		{
		g_interval = 0;
		g_cycles = 1;
		}

	if ( $ASCLEN(&g_logfspec) )
		{
		__util$deflog($ASCPTR(&g_logfspec), NULL);

		$LOG(STS$K_INFO, "Rev: " __IDENT__ "/" __ARCH__NAME__ ", (built at " __DATE__ " " __TIME__
			" with CC " __VERSION__ ")");
		}

	if ( geteuid() )
		$SKSHOWMSG(SKSHOW$_NOPRIV, (unsigned) geteuid());

	s_init_sig_handler();

	if ( s_istty && (0 < g_interval) && (0 > write(s_ttyfd, $ESC_CLS, sizeof($ESC_CLS) - 1)) )
		$IFTRACE(g_trace, "Cannot clear the screen, errno: %d", errno);

	while ( !g_exit_flag )
		{
		if ( !(1 & s_refresh(&l_count)) )
			return	STS$K_ERROR;

		l_cycle++;

		if ( g_cycles && (l_cycle >= g_cycles) )
			break;

		if ( !g_interval )
			break;

		/* sleep(3) returns a remainder when it has been interrupted by a signal */
		for (l_rest = g_interval; l_rest && !g_exit_flag; )
			l_rest = (int) sleep((unsigned) l_rest);

		if ( g_logsize )
			__util$rewindlogfile(g_logsize);
		}

	$IFTRACE(g_trace, "%d refresh cycle(s) has been performed, %d record(s) at the last one",
		l_cycle, l_count);

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: An action routine of the HELP verb.
 *
 *   INPUTS:
 *	a_clictx:	A parse context
 *	a_arg:		Unused
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS
 *	STS$K_ERROR	- an unknown topic
 */
static	int	s_act_help	(
			CLI_CTX	*a_clictx,
			void	*a_arg
			)
{
ASC	l_topic = {0};
char	*l_words [1] = {(char *) $ASCPTR(&l_topic)};
int	l_opts;

	(void) a_arg;

	/* P1 is optional, its absence must not be signalled as an error */
	l_opts = a_clictx->opts;
	a_clictx->opts &= ~CLI$M_OPSIGNAL;

	s_get_asc(a_clictx, &s_parm_help[0], &l_topic);

	a_clictx->opts = l_opts;

	return	s_help_any($ASCLEN(&l_topic) ? 1 : 0, l_words, -1, &l_topic);
}


/*
 *   DESCRIPTION: Test a single command line token for a help request. Every spelling the
 *		CLI_ROUTINES itself recognizes is covered here, plus the Unix style forms -
 *		the local renderer has to pre-empt the built in one for every one of them,
 *		otherwise the same question would be answered by two different texts.
 *
 *   INPUTS:
 *	a_arg:		A token to be tested, ASCIZ
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the token asks for the on-line help
 *	STS$K_WARN	- it does not
 */
static	int	s_is_help_token	(
			const char	*a_arg
			)
{
const char	*l_p = a_arg;

	if ( !a_arg )
		return	STS$K_WARN;

	if ( (*l_p == '-') || (*l_p == '/') )				/* Strip a leading '-' or '/'		*/
		l_p++;

	if ( *l_p == '-' )						/* ... and a second '-' of the "--help"	*/
		l_p++;

	if ( !strcasecmp(l_p, "h") || !strcasecmp(l_p, "help") || !strcmp(l_p, "?") )
		return	STS$K_SUCCESS;

	return	STS$K_WARN;
}


/*
 *   DESCRIPTION: Scan a whole command line for a help request and pick up the topic for it.
 *		Every token but the help spelling itself names the topic - a verb, a qualifier
 *		written with its slash, a word of a key that carries blanks - so a
 *		"skshow help show", a "skshow show /help", a "skshow help pmask" and a
 *		"skshow -h /pmask" are all understood. /PAGE and /NOPAGE govern the pager of
 *		the topic and are not words of it.
 *
 *   INPUTS:
 *	a_argc:		An argument count, the image name included
 *	a_argv:		An argument vector
 *	a_maxwords:	A capacity of a_words
 *
 *   OUTPUTS:
 *	a_wordcnt:	A number of the words naming the topic
 *	a_words:	The words themselves, pointers into a_argv
 *	a_page:		1 - /PAGE, 0 - /NOPAGE, -1 - neither has been given
 *	a_topic:	The first word, ASCIC, for the built-in renderer; a zero length string for
 *			an overview
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- the command line asks for the on-line help
 *	STS$K_WARN	- it does not, the outputs are left empty
 */
static	int	s_is_help_request	(
			int	a_argc,
			char	**a_argv,
			int	a_maxwords,
			int	*a_wordcnt,
			char	**a_words,
			int	*a_page,
			ASC	*a_topic
				)
{
const char	*l_p;
int		l_hit = 0;

	$ASCLEN(a_topic) = 0;
	$ASCPTR(a_topic)[0] = '\0';
	*a_wordcnt = 0;
	*a_page = -1;

	for (int i = 1; i < a_argc; i++)
		if ( 1 & s_is_help_token(a_argv[i]) )
			l_hit = 1;

	if ( !l_hit )
		return	STS$K_WARN;

	/*
	 * Only the first help spelling is consumed, so a "help help" describes the HELP verb rather
	 * than falling back to the overview.
	 */
	l_hit = 0;

	for (int i = 1; i < a_argc; i++)
		{
		if ( !l_hit && (1 & s_is_help_token(a_argv[i])) )
			{
			l_hit = 1;					/* Consume the request itself	*/
			continue;
			}

		l_p = a_argv[i];

		if ( (l_p[0] == '/') || ((l_p[0] == '-') && (l_p[1] == '-')) )
			{
			l_p += (l_p[0] == '/') ? 1 : 2;

			if ( !strcasecmp(l_p, "page") )
				{
				*a_page = 1;
				continue;
				}

			if ( !strcasecmp(l_p, "nopage") )
				{
				*a_page = 0;
				continue;
				}
			}

		if ( *a_wordcnt < a_maxwords )
			a_words[(*a_wordcnt)++] = a_argv[i];

		if ( !$ASCLEN(a_topic) )
			__util$str2asc(a_argv[i], a_topic);
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: An entry point.
 *
 *   INPUTS:
 *	argc, argv:	A command line, see the USAGE section of the module header
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	EXIT_SUCCESS
 *	EXIT_FAILURE
 */
int	main	(
		int	argc,
		char	**argv
		)
{
CLI_CTX	*l_clictx = NULL;
ASC	l_topic = {0};
char	*l_words [SKSHOW$K_MAXWORDS];
int	l_opts = CLI$M_OPSIGNAL, l_status, l_wordcnt = 0, l_page = -1;

	/*
	 * The standard output carries the table, so the log - every $LOG and every message of
	 * the catalogue - is pointed at the standard error before anything can be said. The
	 * catalogue is registered right after, so that a diagnostic of the command line itself
	 * is already framed %SKSHOW-E-IDENT, text.
	 */
	__util$setlogfd(STDERR_FILENO);
	__util$inimsg(&s_msgdsc);

	/*
	 * A real stdout is dup()-ed before any __util$deflog() call: the deflog() redirects
	 * both the STDOUT_FILENO and the STDERR_FILENO into the log file, and the table
	 * would silently disappear into it.
	 */
	if ( 0 > (s_ttyfd = dup(STDOUT_FILENO)) )
		s_ttyfd = STDOUT_FILENO;
	else	fcntl(s_ttyfd, F_SETFD, FD_CLOEXEC);

	s_istty = isatty(s_ttyfd);

	/*
	 * A bare invocation, or a help request in any of its spellings. The scan MUST happen
	 * before the __cli$parse(): the CLI_ROUTINES X.00-09 answers a help token on its own
	 * and would otherwise print its own, far terser text instead of this one.
	 *
	 * /TRACE is honoured here as well, so that the search for the help library can be watched.
	 */
	for (int i = 1; i < argc; i++)
		if ( !strcasecmp(argv[i], "/trace") || !strcasecmp(argv[i], "--trace") )
			g_trace = 1;

	if ( 2 > argc )
		{
		l_status = s_help_any(0, NULL, -1, NULL);

		free(s_obuf.buf);

		return	(1 & l_status) ? EXIT_SUCCESS : EXIT_FAILURE;
		}

	if ( 1 & s_is_help_request(argc, argv, SKSHOW$K_MAXWORDS, &l_wordcnt, l_words, &l_page, &l_topic) )
		{
		l_status = s_help_any(l_wordcnt, l_words, l_page, &l_topic);

		free(s_obuf.buf);

		return	(1 & l_status) ? EXIT_SUCCESS : EXIT_FAILURE;
		}

	/*
	 * /TRACE has to be known before the parsing to let the parser itself be traced,
	 * hence a pre-scan - the qualifier is processed in a regular way afterwards.
	 */
	if ( g_trace )
		l_opts |= CLI$M_OPTRACE;

	$IFTRACE(l_opts & CLI$M_OPTRACE, "Rev: " __IDENT__ "/" __ARCH__NAME__ ", (built at "
		__DATE__ " " __TIME__ " with CC " __VERSION__ ")");

	/* argv[0] is a name of the image, a verb is expected in the argv[1] */
	l_status = __cli$parse(s_verbs, l_opts, argc - 1, argv + 1, (void **) &l_clictx);

	if ( !(1 & l_status) )
		{
		/*
		 * A STS$K_WARN means the parser has recognized a help request and has answered
		 * it itself - a belt and braces path, the pre-scan above should have caught it.
		 * It is not a failure and must not be reported as one.
		 */
		if ( l_status != STS$K_WARN )
			$SKSHOWMSG(SKSHOW$_SYNTAX);

		free(s_obuf.buf);

		return	(l_status == STS$K_WARN) ? EXIT_SUCCESS : EXIT_FAILURE;
		}

	l_status = __cli$dispatch(l_clictx);

	__cli$cleanup(l_clictx);

	s_inodemap_purge();
	s_diagmap_purge();
	s_prevmap_purge();

	free(s_obuf.buf);
	s_obuf.buf = NULL;

	if ( s_ttyfd != STDOUT_FILENO )
		close(s_ttyfd);

	return	(1 & l_status) ? EXIT_SUCCESS : EXIT_FAILURE;
}
