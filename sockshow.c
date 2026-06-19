#define	__MODULE__	"SKSHOW"
#define	__IDENT__	"X.00-03"
#define	__REV__		"00.00.03"

/*
**++
**
**  FACILITY: A yet another utility to display statistic for TCP/IP sockets
**
**  ENVIRONMENT: Linux
**
**  ABSTRACT: A nano-utility to show sockets counters with ability to select with some critaria
**
**
**  AUTHORS: Ruslan R. (The BadAss Sysman) Laishev
**
**  CREATION DATE:  18-JUN-2026
**
**
**  USAGE:
**	$ sockshow [options]
**		options:
**			/TRACE			- enable extensible debug diagnostic output
**
**			/LIA=<ip4_address>	- Local, Remote IP address to select socket
**			/RIA=<ip4_address>
**
**			/LPN=<number>		- --//-- port number --//--
**			/RPN=<number>
**
**			/PMASK=<string>		- process name
**
**  MODIFICATION HISTORY:
**
**
**--
*/


#include	<stdio.h>
#include	<stdlib.h>

#define _GNU_SOURCE			/* for memmem() */
#include	<string.h>

#include	<unistd.h>
#include	<dirent.h>
#include	<ctype.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<arpa/inet.h>
#include	<sys/socket.h>
#include	<linux/netlink.h>
#include	<linux/sock_diag.h>
#include	<linux/inet_diag.h>
#include	<linux/rtnetlink.h>   // для RTA_OK, RTA_DATA, RTA_NEXT
#include	<linux/tcp.h>         // для struct tcp_info
#include	<endian.h>
#include	<signal.h>



#define		__FAC__	"WG"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/
#include	"utility_routines.h"



ASC	g_logfspec = {0},
	q_proto = {0},
	q_pmask = {0}
	;


int	g_exit_flag = 0,						/* Global flag 'all must to be stop'	*/
	g_trace = 1,							/* A flag to produce extensible logging	*/
	g_interval = 3,							/* Refresh interval, 3 seconds by default */
	g_logsize = 0
	;

static __be32	g_lia, g_ria;
static __be16	g_lpn, g_rpn;
static int	g_proto = 0;


static const OPTS g_optstbl [] =					/* General CLI options		*/
{
	{$ASCINI("trace"),	&g_trace, 0,			OPTS$K_OPT},
	{$ASCINI("logfile"),	&g_logfspec, ASC$K_SZ,		OPTS$K_STR},
	{$ASCINI("logsize"),	&g_logsize, 0,			OPTS$K_INT},

	{$ASCINI("lia"),	&g_lia, 0,			OPTS$K_IP4},
	{$ASCINI("ria"),	&g_ria, 0,			OPTS$K_IP4},
	{$ASCINI("lpn"),	&g_lpn, sizeof(uint16_t),	OPTS$K_INT},
	{$ASCINI("rpn"),	&g_rpn, sizeof(uint16_t),	OPTS$K_INT},

	{$ASCINI("interval"),	&g_interval, 0,			OPTS$K_INT},
	{$ASCINI("proto"),	&q_proto, ASC$K_SZ,		OPTS$K_STR},
	{$ASCINI("pmask"),	&q_pmask, ASC$K_SZ,		OPTS$K_STR},

	OPTS_NULL
};


static const KWDENT	s_proto_option [] = {
	{$ASCINI("TCP"), IPPROTO_TCP},
	{$ASCINI("UDP"), IPPROTO_UDP},
};
static const int s_proto_option_sz = $ARRSZ(s_proto_option);





#ifndef IPv4_BYTES
#define IPv4_BYTES_FMT "%u.%u.%u.%u"
#define IPv4_BYTES(addr) \
		(uint8_t) ((addr) & 0xFF), \
		(uint8_t) (((addr) >> 8) & 0xFF),\
		(uint8_t) (((addr) >> 16) & 0xFF),\
		(uint8_t) (((addr) >> 24) & 0xFF)
#endif


#define PROC_TCP4 "/proc/net/tcp"
#define PROC_TCP6 "/proc/net/tcp6"
#define PROC_UDP4 "/proc/net/udp"
#define PROC_UDP6 "/proc/net/udp6"

/* TCP socket states */
enum tcp_state {
	TCP_ESTABLISHED = 1,
	TCP_SYN_SENT    = 2,
	TCP_SYN_RECV    = 3,
	TCP_FIN_WAIT1   = 4,
	TCP_FIN_WAIT2   = 5,
	TCP_TIME_WAIT   = 6,
	TCP_CLOSE       = 7,
	TCP_CLOSE_WAIT  = 8,
	TCP_LAST_ACK    = 9,
	TCP_LISTEN      = 10,
	TCP_CLOSING     = 11
};

/* TCP socket states */
enum udp_state {
	UDP_ESTABLISHED = 1,
	UDP_UNCONNECTED = 7
};

/* Area to keep socket information */
typedef struct {
	union {
		struct sockaddr_in	sk;
		struct sockaddr_in6	sk6;
	} lcl;

	union {
		struct sockaddr_in	sk;
		struct sockaddr_in6	sk6;
	} rem;

	int state;

	unsigned long inode;

	int	rxq, txq;
	long	bytes_received;
	long	bytes_sent;
	long	packets_received;
	long	packets_sent;
	int	has_counters;
} socket_info;

// Отображение inode -> pid + comm
typedef struct inode_map_entry
{
    unsigned long inode;
	int	pid;
	char	procname[ASC$K_SZ + 8];
    struct inode_map_entry *next;
} inode_map_entry;

static inode_map_entry *g_inode_map = NULL;

/*
 *   DESCRIPTION: Translate TCP-state from number to string
 *
 *   INPUTS:
 *
 *   OUTPUTS:
 *
 *   RETURNS:
 */
static const char *s_tcp_state2str(int l_state)
{
	switch(l_state)
		{
		case TCP_ESTABLISHED: return "ESTAB";
		case TCP_SYN_SENT:    return "SYN_SENT";
		case TCP_SYN_RECV:    return "SYN_RECV";
		case TCP_FIN_WAIT1:   return "FIN_WAIT1";
		case TCP_FIN_WAIT2:   return "FIN_WAIT2";
		case TCP_TIME_WAIT:   return "TIME_WAIT";
		case TCP_CLOSE:       return "CLOSE";
		case TCP_CLOSE_WAIT:  return "CLOSE_WAIT";
		case TCP_LAST_ACK:    return "LAST_ACK";
		case TCP_LISTEN:      return "LISTEN";
		case TCP_CLOSING:     return "CLOSING";

		default:              return "UNKNOWN";
		}
}

/*
 *   DESCRIPTION: Translate UDP-state from number to string
 *
 *   INPUTS:
 *
 *   OUTPUTS:
 *
 *   RETURNS:
 */
static const char *s_udp_state2str(int l_state)
{
	switch(l_state)
		{
		case UDP_ESTABLISHED: return "ESTAB";
		case UDP_UNCONNECTED: return "UNCONN";

		default:              return "UNKNOWN";
		}
}




/*
 *   DESCRIPTION: Add new entry for a_inode -> (a_pid, procname)
 *   INPUTS:
 *   OUTPUTS:
 *   RETURNS;
 */
static void s_add_inode_mapping(
		unsigned long	a_inode,
			int	a_pid,
		const char	*a_comm
		)
{
inode_map_entry *e;

	if (!(e = malloc(sizeof(inode_map_entry))) )
		return;

	e->inode = a_inode;
	e->pid = a_pid;
	__util$str2asc(a_comm, e->procname);

	e->next = g_inode_map;
	g_inode_map = e;
}

/*
 *   DESCRIPTION: Find a_pid & procname by using given a_inode, or return default string.
 *   INPUTS:
 *   OUTPUTS:
 *   RETURNS;
 */
static int s_find_process_by_inode (
		unsigned long	a_inode,
			int	*a_pid,
			ASC	**a_procname
		)
{
static ASC	l_defstr = {$ASCINI("???")};

	for (inode_map_entry *e = g_inode_map; e; e = e->next)
		{
		if (e->inode == a_inode)
			{
			*a_pid = e->pid;				/* Store PID and Process name */
			(*a_procname) = (ASC *) &e->procname;

			return 0;
			}
		}

	/* Did not find anything ? Return default string as a name of process */
	*a_pid = 0;
	(*a_procname) = &l_defstr;

	return 0;
}

// Построить отображение inode -> (pid, comm) сканированием /proc
static void s_build_inode_map(void)
{
inode_map_entry *e, *next;
DIR *l_dirproc, *l_fd_dir;
struct dirent *l_dirent, *l_fd_entry;
int l_pid;
char l_fd_path[512] = {0}, l_link_path[512], l_target[256];
FILE	*l_cf;
ssize_t l_len;

	for (e = g_inode_map; e; e = next)			/* Release a memory of existent map */
		{
		next = e->next;
		free(e);
		}

	g_inode_map = NULL;



	if ( !(l_dirproc = opendir("/proc")) )
		return	(void) $LOG(STS$K_ERROR, "opendir(/ptoc), errno: %d", errno);


	while ((l_dirent = readdir(l_dirproc)) != NULL)
		{
		if ( !isdigit(l_dirent->d_name[0]) )
			continue;

		l_pid = atoi(l_dirent->d_name);


		snprintf(l_fd_path, sizeof(l_fd_path) - 1, "/proc/%d/fd", l_pid);

		if ( !(l_fd_dir = opendir(l_fd_path)) )
			continue;


		while ((l_fd_entry = readdir(l_fd_dir)) != NULL)
			{
			if (l_fd_entry->d_name[0] == '.')
				continue;

			snprintf(l_link_path, sizeof(l_link_path), "%s/%s", l_fd_path, l_fd_entry->d_name);

			if ( 0 < (l_len = readlink(l_link_path, l_target, sizeof(l_target) - 1)) )
				{
				l_target[l_len] = '\0';

				if (strncmp(l_target, "socket:[", 8) == 0)
					{
					char *end;
					unsigned long inode = strtoul(l_target + 8, &end, 10);

					if (end && *end == ']')
						{
						char comm[64];
						snprintf(comm, sizeof(comm), "/proc/%d/comm", l_pid);

						if ( l_cf = fopen(comm, "r") )
							{
							if ( fgets(comm, sizeof(comm), l_cf) )
								{
								char *nl = strchr(comm, '\n');

								if (nl)
									*nl = 0;

								s_add_inode_mapping(inode, l_pid, comm);
								}

							fclose(l_cf);
							}
						else	s_add_inode_mapping(inode, l_pid, "?");
						}
					}
				}

			}
		closedir(l_fd_dir);
		}

	closedir(l_dirproc);
}


/*
 *   DESCRIPTION: Use NETLINK to retrieve TCP counters
 *
 *   INPUTS:
 *
 *   OUTPUTS:
 *
 *   RETURNS:
 */
static void s_get_tcp_info(
		socket_info *a_sk_info
		)
{
int	l_sd, l_rc, l_family;
struct {
    struct nlmsghdr nlh;
    struct inet_diag_req_v2 req;
} l_nlreq = {0};
struct iovec l_iov = { &l_nlreq, sizeof(l_nlreq) };
struct msghdr l_msg = { .msg_iov = &l_iov,.msg_iovlen = 1};
char	l_buf[8192];
struct	nlmsghdr *l_nlh;


	a_sk_info->has_counters = 0;
	a_sk_info->bytes_received = a_sk_info->bytes_sent = 0;
	a_sk_info->packets_received = a_sk_info->packets_sent = 0;

	if ( 0 > (l_sd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG)) )
		return;


	l_nlreq.nlh.nlmsg_len = sizeof(l_nlreq);
	l_nlreq.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
	l_nlreq.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	l_nlreq.nlh.nlmsg_seq = 1234;
	l_nlreq.nlh.nlmsg_pid = getpid();


	l_family = l_nlreq.req.sdiag_family = a_sk_info->lcl.sk.sin_family;

	l_nlreq.req.sdiag_protocol = IPPROTO_TCP;
	l_nlreq.req.idiag_states = (1<<TCP_ESTABLISHED) | (1<<TCP_SYN_SENT) | (1<<TCP_SYN_RECV) |
			   (1<<TCP_FIN_WAIT1) | (1<<TCP_FIN_WAIT2) | (1<<TCP_TIME_WAIT) |
			   (1<<TCP_CLOSE) | (1<<TCP_CLOSE_WAIT) | (1<<TCP_LAST_ACK) |
			   (1<<TCP_LISTEN) | (1<<TCP_CLOSING);
	l_nlreq.req.idiag_ext = 1 << (INET_DIAG_INFO - 1);


	l_nlreq.req.id.idiag_sport = a_sk_info->lcl.sk.sin_port;
	l_nlreq.req.id.idiag_dport = a_sk_info->rem.sk.sin_port;

	if ( l_family == AF_INET )
		{
		l_nlreq.req.id.idiag_src[0] = a_sk_info->lcl.sk.sin_addr.s_addr;
		l_nlreq.req.id.idiag_dst[0] = a_sk_info->rem.sk.sin_addr.s_addr;
		}
	else	{
		memcpy(l_nlreq.req.id.idiag_src, a_sk_info->lcl.sk6.sin6_addr.s6_addr32, sizeof(struct in6_addr)) ;
		memcpy(l_nlreq.req.id.idiag_dst, a_sk_info->rem.sk6.sin6_addr.s6_addr32, sizeof(struct in6_addr)) ;
		}

#if 0
	$IFTRACE(g_trace, "Request over Netlink [src: " IPv4_BYTES_FMT ":%d, dst: " IPv4_BYTES_FMT ":%d]",
		  IPv4_BYTES(l_nlreq.req.id.idiag_src[0]),
		  ntohs(l_nlreq.req.id.idiag_sport),
		  IPv4_BYTES(l_nlreq.req.id.idiag_dst[0]) ,
		  ntohs(l_nlreq.req.id.idiag_dport) );
#endif

	if ( 0 > (l_rc = sendmsg(l_sd, &l_msg, 0)) )
		{
		close(l_sd);
		return;
		}

	if ( 0 > (l_rc = recv(l_sd, l_buf, sizeof(l_buf), 0)) )
		{
		close(l_sd);
		return;
		}


	for (l_nlh = (struct nlmsghdr*)l_buf; NLMSG_OK(l_nlh, l_rc); l_nlh = NLMSG_NEXT(l_nlh, l_rc))
		{
		if (l_nlh->nlmsg_type == NLMSG_DONE)
			break;

		if (l_nlh->nlmsg_type == SOCK_DIAG_BY_FAMILY)
			{
			struct inet_diag_msg *l_diag = NLMSG_DATA(l_nlh);

#if 0
			$IFTRACE(g_trace, "Answer fro Netlink [src: " IPv4_BYTES_FMT ":%d, dst: " IPv4_BYTES_FMT ":%d]",
				  IPv4_BYTES(l_diag->id.idiag_src[0]),
				  ntohs(l_diag->id.idiag_sport),
				  IPv4_BYTES(l_diag->id.idiag_dst[0]) ,
				  ntohs(l_diag->id.idiag_dport) );
#endif

			if (l_diag->idiag_family != l_family)
				continue;

			if ( (l_diag->id.idiag_sport != l_nlreq.req.id.idiag_sport)
				|| (l_diag->id.idiag_dport != l_nlreq.req.id.idiag_dport) )
				continue;

			if ( (l_diag->id.idiag_src[0] != l_nlreq.req.id.idiag_src[0])
				|| (l_diag->id.idiag_dst[0] != l_nlreq.req.id.idiag_dst[0]) )
				continue;

			struct rtattr *l_rtattr = (struct rtattr*)(l_diag+1);
			int rta_len = l_nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*l_diag));

			while (RTA_OK(l_rtattr, rta_len))
				{
				if (l_rtattr->rta_type == INET_DIAG_INFO)
					{
					struct tcp_info *l_tcp_info = RTA_DATA(l_rtattr);

					a_sk_info->bytes_received = l_tcp_info->tcpi_bytes_received;
					a_sk_info->bytes_sent = l_tcp_info->tcpi_bytes_acked;
					a_sk_info->packets_received = l_tcp_info->tcpi_segs_in;
					a_sk_info->packets_sent = l_tcp_info->tcpi_segs_out;
					a_sk_info->has_counters = 1;
					break;
					}

				l_rtattr = RTA_NEXT(l_rtattr, rta_len);
				}
			break;
			}
		}

	close(l_sd);
}

/*
 *  DESCRIPTION: Parse line of  "/proc/net/tcp" or "/proc/net/udp"
 *
 *
 */
/*
root@devuan4-sysman:~/Desktop# cat /proc/net/tcp
  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
   0: 00000000:0016 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 14401 1 00000000f3daed9f 100 0 0 10 0
   1: 0100007F:0019 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 14454 1 0000000028a2e8dd 100 0 0 10 20
   2: 0100007F:0035 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 10138 1 0000000094b50739 100 0 0 10 0
   3: 0100007F:0277 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 19112 1 00000000fcbde32b 100 0 0 10 0
   4: 8292A8C0:E71A 78FE6A57:0050 08 00000000:00000000 00:00000000 00000000     0        0 36726 1 00000000c8ba7f39 25 4 30 10 -1

root@devuan4-sysman:~/Desktop# cat /proc/net/tcp6
  sl  local_address                         remote_address                        st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
   0: 00000000000000000000000000000000:0016 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 14403 1 000000005d5e8fd6 100 0 0 10 0
   1: 00000000000000000000000001000000:0277 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 19111 1 000000009b8d1ca6 100 0 0 10 0
   2: 00000000000000000000000001000000:0019 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 14455 1 000000001940162b 100 0 0 10 20
   3: 00000000000000000000000001000000:0035 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 10142 1 0000000092bdc69d 100 0 0 10 0
   4: 00000000000000000000000000000000:19A6 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 14536 1 00000000791bfeff 100 0 0 10 0
   5: 00000000000000000000000000000000:1451 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000   116        0 11834 1 0000000097fe4835 100 0 0 10 0
*/

static int s_parse_tcp_n_udp_line (
    const char    *a_line,
    int            a_is_ipv6,
    socket_info   *a_sk_info
)
{
char    l_line[32];
int     l_state, l_rc, l_lcl_port, l_rem_port,
	l_rxq, l_txq, l_tr, l_tm, l_retr, l_uid, l_tmo;
unsigned long l_inode = 0;

	if ( !a_is_ipv6 )
		{
		uint32_t l_lcl_ip, l_rem_ip;

		l_rc = sscanf(a_line, "%s %08x:%04x %08x:%04x %x %08x:%08x %02x:%08x %08x %d %d %d",
			l_line,
			&l_lcl_ip, &l_lcl_port,
			&l_rem_ip, &l_rem_port,
			&l_state,
			&l_txq, &l_rxq,
			&l_tr, &l_tm,
			&l_retr,
			&l_uid, &l_tmo,
			&l_inode);

		if ( l_rc != 14 )
			return -EINVAL;

		a_sk_info->lcl.sk.sin_family = AF_INET;
		a_sk_info->lcl.sk.sin_port   = htons(l_lcl_port);
		a_sk_info->lcl.sk.sin_addr.s_addr = l_lcl_ip;

		a_sk_info->rem.sk.sin_family = AF_INET;
		a_sk_info->rem.sk.sin_port   = htons(l_rem_port);
		a_sk_info->rem.sk.sin_addr.s_addr = l_rem_ip;
		}
	else	{/* IPv6 – /proc/net/tcp6 or /proc/net/udp6 */
		uint32_t l_lcl_ip6[4], l_rem_ip6[4];

		l_rc = sscanf(a_line, "%s %08x%08x%08x%08x:%04x %08x%08x%08x%08x:%04x %x %08x:%08x %02x:%08x %08x %d %d %lu",
			l_line,
			&l_lcl_ip6[0], &l_lcl_ip6[1], &l_lcl_ip6[2], &l_lcl_ip6[3], &l_lcl_port,
			&l_rem_ip6[0], &l_rem_ip6[1], &l_rem_ip6[2], &l_rem_ip6[3], &l_rem_port,
			&l_state,
			&l_txq, &l_rxq,
			&l_tr, &l_tm,
			&l_retr,
			&l_uid, &l_tmo,
			&l_inode);

		if ( l_rc != 20 )
			return -EINVAL;

		a_sk_info->lcl.sk6.sin6_family = AF_INET6;
		a_sk_info->lcl.sk6.sin6_port   = htons(l_lcl_port);
		a_sk_info->lcl.sk6.sin6_addr.s6_addr32[0] = htonl(l_lcl_ip6[0]);
		a_sk_info->lcl.sk6.sin6_addr.s6_addr32[1] = htonl(l_lcl_ip6[1]);
		a_sk_info->lcl.sk6.sin6_addr.s6_addr32[2] = htonl(l_lcl_ip6[2]);
		a_sk_info->lcl.sk6.sin6_addr.s6_addr32[3] = htonl(l_lcl_ip6[3]);

		a_sk_info->rem.sk6.sin6_family = AF_INET6;
		a_sk_info->rem.sk6.sin6_port   = htons(l_rem_port);
		a_sk_info->rem.sk6.sin6_addr.s6_addr32[0] = htonl(l_rem_ip6[0]);
		a_sk_info->rem.sk6.sin6_addr.s6_addr32[1] = htonl(l_rem_ip6[1]);
		a_sk_info->rem.sk6.sin6_addr.s6_addr32[2] = htonl(l_rem_ip6[2]);
		a_sk_info->rem.sk6.sin6_addr.s6_addr32[3] = htonl(l_rem_ip6[3]);
		}

	a_sk_info->state = l_state;
	a_sk_info->inode = l_inode;
	a_sk_info->rxq   = l_rxq;
	a_sk_info->txq   = l_txq;

	/* Фильтрация по портам (предполагается, что поля port лежат по одному смещению) */
	if ( g_lpn && (a_sk_info->lcl.sk.sin_port != g_lpn) )
		return -EINVAL;

	if ( g_rpn && (a_sk_info->rem.sk.sin_port != g_rpn) )
		return -EINVAL;

    return 0;
}







/*
 *   DESCRIPTION: Clear screen of VT by sending DEC VT Escape sequence
 *
 *   INPUTS:
 *	NONE
 *   OUTPUTS:
 *      NONE
 *   RETURNS:
 *      NONE
 */
static void s_print_cls(void)
{
const char	l_esc_cls [] = {"\033[2J\033[H"};

	__util$out(l_esc_cls);
}

/*
 *   DESCRIPTION: Print couumnt headers
 *
 *   INPUTS:
 *	NONE
 *   OUTPUTS:
 *      NONE
 *   RETURNS:
 *      NONE
 */
static void s_print_header(void)
{
char	l_buf[2048];
const char	l_esc_home[] = {"\033[H"};
int	l_buflen = 0;

	__util$out(l_esc_home);

	l_buflen += snprintf(l_buf + l_buflen, sizeof(l_buf) - l_buflen - 1, "%-6s %-40s %-40s %-12s %-6s %-10s %-10s %-10s %-10s %-10s\n",
		"Proto", "Lcl End-point", "Rem End-point", "State",
		"PID", "Proc", "RxBytes", "TxBytes", "RxPkts", "TxPkts");

	l_buflen += snprintf(l_buf + l_buflen, sizeof(l_buf) - l_buflen - 1, "%-6s %-40s %-40s %-12s %-6s %-10s %-10s %-10s %-10s %-10s\n",
		"-----", "----------------------------------------", "----------------------------------------",
		"------------", "-----", "----------", "----------", "----------", "----------", "----------");


	__util$out(l_buf);
}

// Вывод заголовка
static void s_print_footer(
		int	a_count
		)
{

const char	l_esc_truncate[] = {"\033[J"};

	__util$out("----------------------------------------------------------------------------------------"
		   "\033[K\n"
		   " %d records is displayed [Lcl EP: <%#x:%d>, Rem EP: <%#x:%d>, Proto: %s, PMask: \"%s\"%s]",
		a_count,
		g_lia, ntohs(g_lpn), g_ria, ntohs(g_rpn),
		$ASCLEN(&q_proto) ? $ASCPTR(&q_proto) : "All",
		$ASCPTR(&q_pmask),
		l_esc_truncate);
}



// Обработка TCP-файла
static void s_process_tcp_file (
		const char *filename,
			int is_ipv6,
			int	*a_count
		)
{
FILE *l_fp;
char l_buf[2048], l_src_ep[INET6_ADDRSTRLEN + 8], l_dst_ep[INET6_ADDRSTRLEN + 8];
ASC	*l_procname;
socket_info l_sk_info = {0};
int l_pid = 0, l_buflen;


	if ( !(l_fp = fopen(filename, "r")) )
		return;

	if (!fgets(l_buf, sizeof(l_buf), l_fp))
		{
		fclose(l_fp);
		return;
		}

	for (;  fgets(l_buf, sizeof(l_buf), l_fp); )
		{
		if ( s_parse_tcp_n_udp_line(l_buf, is_ipv6, &l_sk_info) )
			continue;


		if ( 0 > (s_find_process_by_inode(l_sk_info.inode, &l_pid, &l_procname)) )
			continue;

		if ( $ASCLEN(&q_pmask)
				&& !memmem($ASCPTR(l_procname), $ASCLEN(l_procname), $ASCPTR(&q_pmask), $ASCLEN(&q_pmask)) )
			continue;


		if (l_sk_info.state != TCP_LISTEN)
			s_get_tcp_info(&l_sk_info);


		if ( !is_ipv6 )
			{
			snprintf(l_src_ep, sizeof(l_src_ep) - 1, IPv4_BYTES_FMT ":%d", IPv4_BYTES(l_sk_info.lcl.sk.sin_addr.s_addr),
				 ntohs(l_sk_info.lcl.sk.sin_port) );
			snprintf(l_dst_ep, sizeof(l_dst_ep) - 1, IPv4_BYTES_FMT ":%d", IPv4_BYTES(l_sk_info.rem.sk.sin_addr.s_addr),
				 ntohs(l_sk_info.rem.sk.sin_port) );
			}
		else	{/* IPv6 – для /proc/net/tcp6 или /proc/net/udp6 */
			inet_ntop(AF_INET6, &l_sk_info.lcl.sk6.sin6_addr, l_buf, INET6_ADDRSTRLEN);
			snprintf(l_src_ep, sizeof(l_src_ep) - 1, "%s:%d", l_buf, ntohs(l_sk_info.lcl.sk6.sin6_port) );

			inet_ntop(AF_INET6, &l_sk_info.rem.sk6.sin6_addr, l_buf, INET6_ADDRSTRLEN);
			snprintf(l_dst_ep, sizeof(l_dst_ep) - 1, "%s:%d", l_buf, ntohs(l_sk_info.rem.sk6.sin6_port) );
			}


		l_buflen = snprintf(l_buf, sizeof(l_buf), "%-6s %40s %40s %-12s %-6d %-10.10s ",
			is_ipv6 ? "TCPv6" : "TCPv4",
			l_src_ep, l_dst_ep,
			s_tcp_state2str(l_sk_info.state),
			l_pid, $ASCPTR(l_procname) );

		if (l_sk_info.has_counters)
			__util$out("%.*s %-10ld %-10ld %-10ld %-10ld \033[K",
				   l_buflen, l_buf,
				   l_sk_info.bytes_received, l_sk_info.bytes_sent, l_sk_info.packets_received, l_sk_info.packets_sent);
		else	__util$out("%.*s %-10s %-10s %-10s %-10s",
				   l_buflen, l_buf,
				   "N/A", "N/A", "N/A", "N/A \033[K");


		 (*a_count)++;
		}

	fclose(l_fp);
}


static void s_process_udp_file (
		const char *filename,
		int	is_ipv6,
		int	*a_count
		)
{
FILE *l_fp;
char l_buf[2048], l_src_ep[INET6_ADDRSTRLEN + 8], l_dst_ep[INET6_ADDRSTRLEN + 8];
ASC	*l_procname;
socket_info l_sk_info = {0};
int	l_pid = 0;


	if ( !(l_fp = fopen(filename, "r")) )
		return;

	if (!fgets(l_buf, sizeof(l_buf), l_fp))
		{
		fclose(l_fp);
		return;
		}

	for (;  fgets(l_buf, sizeof(l_buf), l_fp); )
		{
		/*
		 * Parse single line from the "/proc/net/udp"
		 */
		if ( s_parse_tcp_n_udp_line(l_buf, is_ipv6, &l_sk_info) )
			continue;

		/*
		 * Get PID & Process name for socket owner
		 * */
		if ( 0 > s_find_process_by_inode(l_sk_info.inode, &l_pid, &l_procname) )
			continue;


		/*
		 * Do we need filter record by process mask string ?
		 * */
		if ( $ASCLEN(&q_pmask)
				&& !memmem($ASCPTR(l_procname), $ASCLEN(l_procname), $ASCPTR(&q_pmask), $ASCLEN(&q_pmask)) )
			continue;



		/*
		 * Build final output
		 */
		if ( !is_ipv6 )
			{
			snprintf(l_src_ep, sizeof(l_src_ep) - 1, IPv4_BYTES_FMT ":%d", IPv4_BYTES(l_sk_info.lcl.sk.sin_addr.s_addr),
				 ntohs(l_sk_info.lcl.sk.sin_port) );
			snprintf(l_dst_ep, sizeof(l_dst_ep) - 1, IPv4_BYTES_FMT ":%d", IPv4_BYTES(l_sk_info.rem.sk.sin_addr.s_addr),
				 ntohs(l_sk_info.rem.sk.sin_port) );
			}
		else	{/* IPv6 – for /proc/net/tcp6 или /proc/net/udp6 */
			inet_ntop(AF_INET6, &l_sk_info.lcl.sk6.sin6_addr, l_buf, INET6_ADDRSTRLEN);
			snprintf(l_src_ep, sizeof(l_src_ep) - 1, "%s:%d", l_buf, ntohs(l_sk_info.lcl.sk6.sin6_port) );

			inet_ntop(AF_INET6, &l_sk_info.rem.sk6.sin6_addr, l_buf, INET6_ADDRSTRLEN);
			snprintf(l_dst_ep, sizeof(l_dst_ep) - 1, "%s:%d", l_buf, ntohs(l_sk_info.rem.sk6.sin6_port) );
			}

		__util$out("%-6s %40s %40s %-12s %-6d %-10.10s\033[K",
			is_ipv6 ? "UDPv6" : "UDPv4",
			l_src_ep, l_dst_ep,
			s_udp_state2str(l_sk_info.state),
			l_pid, $ASCPTR(l_procname) );

		/*
		 * Count displayed string
		 */
		(*a_count)++;
		}

	fclose(l_fp);
}




static	void	s_sig_handler (int signo)
{
	if ( g_exit_flag )
		{
		fprintf(stdout, "Exit flag has been set, exiting ...\n");
		fflush(stdout);

		_exit(signo);
		}


	if ( (signo == SIGTERM) || (signo == SIGINT) || (signo == SIGQUIT))
		{
		fprintf(stdout, "Get the %d/%#x, set exit_flag!\n", signo, signo);
		fflush(stdout);

		g_exit_flag = 1;
		return;
		}
	else if ( signo == SIGUSR1)
		{
		g_trace = !g_trace;
		fprintf(stdout, "Set /TRACE=%s\n", g_trace ? "ON" : "OFF");
		fflush(stdout);
		return;
		}
	else	{
		fprintf(stdout, "Get the %d/%#x signal\n", signo, signo);
		fflush(stdout);
		}

	_exit(signo);
}

static void	s_init_sig_handler(void)
{
const int siglist [] = {SIGTERM, SIGINT, SIGUSR1, SIGQUIT, 0 };
int	i;

	/*
	 * Establishing a signals handler
	 */
	signal(SIGPIPE, SIG_IGN);	/* We don't want to crash the server due fucking unix shit */

	for ( i = 0; siglist[i]; i++)
		{
		if ( (signal(siglist[i], s_sig_handler)) == SIG_ERR )
			$LOG(STS$K_ERROR, "Error establishing handler for signal %d/%#x, error=%d", siglist[i], siglist[i], errno);

		$IFTRACE(g_trace, "Set handler for signal %d/%#x (%s)", siglist[i], siglist[i], strsignal(siglist[i]));
		}
}



int	main(int argc, char **argv)
{

	$LOG(STS$K_INFO, "Rev: " __IDENT__ "/"  __ARCH__NAME__   ", (built  at "__DATE__ " " __TIME__ " with CC " __VERSION__ ")");

	/*
	 * Process command line arguments
	 */
	if ( !(1 & __util$getparams(argc, argv, g_optstbl)) )
		return	$LOG(STS$K_ERROR, "Error processing configuration");


	if ( $ASCLEN(&g_logfspec) )
		{
		__util$deflog($ASCPTR(&g_logfspec), NULL);

		$LOG(STS$K_INFO, "Rev: " __IDENT__ "/"  __ARCH__NAME__   ", (built  at "__DATE__ " " __TIME__ " with CC " __VERSION__ ")");
		}

	if ( g_trace )
		__util$showparams(g_optstbl);


	s_init_sig_handler ();



	g_lpn = htons(g_lpn);				/* To NBO */
	g_rpn = htons(g_rpn);



	s_print_cls();					/* Clear screen */

	for (int l_count = 0; !g_exit_flag; l_count = 0)
		{
		s_build_inode_map();			/* Build map for current set of process */
		s_print_header();			/* Out listing header */

		if ( !g_proto || (g_proto == IPPROTO_TCP) )
			{
			s_process_tcp_file(PROC_TCP4, 0, &l_count);
			s_process_tcp_file(PROC_TCP6, 1 /* TVPv6 */, &l_count );
			}

		if ( !g_proto || (g_proto == IPPROTO_UDP) )
			{
			s_process_udp_file(PROC_UDP4, 0, &l_count);
			s_process_udp_file(PROC_UDP6, 1 /* TVPv6 */, &l_count );
			}


		s_print_footer(l_count);		/* Print footer line */
		sleep(g_interval);			/* Sleep for a given interval ... */
		}

	return 0;
}
