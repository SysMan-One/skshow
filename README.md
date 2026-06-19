---

```markdown
# `sockshow` — A Linux utility to display TCP/UDP socket statistics

**Version:** X.00-03 (00.00.03)
**Author:** Ruslan R. (The BadAss Sysman) Laishev
**Creation Date:** 18-JUN-2026

---

## Description

`sockshow` is a console utility that displays the current state of network sockets (TCP and UDP) on a Linux system. It extracts information from the pseudo‑files `/proc/net/tcp{,6}` and `/proc/net/udp{,6}`. For TCP connections, it additionally retrieves extended counters (bytes and packets sent/received) via the **NETLINK_SOCK_DIAG** interface.

The tool allows you to:

- filter sockets by local/remote IP address and port;
- select only TCP or UDP sockets;
- limit the output to processes whose name matches a given substring;
- automatically refresh the data at a configurable interval (3 seconds by default), clearing the screen before each update;
- log diagnostic messages to a file.

---

## Syntax

```
sockshow [ /option=value ... ]
```

**Note:** Options follow the OpenVMS style – they begin with a leading slash `/`. Option names are case‑insensitive.

---

## Options

| Option       | Type             | Description                                                                                      |
|--------------|------------------|--------------------------------------------------------------------------------------------------|
| `/TRACE`     | flag             | Enable extended diagnostic output (debugging).                                                   |
| `/LOGFILE`   | string           | Path to a log file (instead of stdout).                                                          |
| `/LOGSIZE`   | number           | Maximum log file size in bytes; when reached, the log is rotated.                                |
| `/LIA`       | IPv4 address     | Local IP address (filter).                                                                      |
| `/RIA`       | IPv4 address     | Remote IP address (filter).                                                                     |
| `/LPN`       | number (1–65535) | Local port (filter).                                                                             |
| `/RPN`       | number (1–65535) | Remote port (filter).                                                                            |
| `/INTERVAL`  | number           | Refresh interval in seconds (default: 3).                                                        |
| `/PROTO`     | `TCP` or `UDP`   | Display only the given protocol. If omitted, both are shown.                                     |
| `/PMASK`     | string           | Filter sockets by process name (substring match).                                                |

---

## Examples

### 1. Show all active TCP and UDP connections (refresh every 3 seconds)

```bash
$ sockshow
```

### 2. Show only TCP connections, refresh every 5 seconds

```bash
$ sockshow /PROTO=TCP /INTERVAL=5
```

### 3. Show sockets with local port 80 (HTTP)

```bash
$ sockshow /LPN=80
```

### 4. Show connections to remote IP 192.168.1.1 and port 443

```bash
$ sockshow /RIA=192.168.1.1 /RPN=443
```

### 5. Filter by process name (e.g., only processes containing "nginx")

```bash
$ sockshow /PMASK=nginx
```

### 6. Enable debugging and log to a file

```bash
$ sockshow /TRACE /LOGFILE=debug.log /LOGSIZE=1048576
```

### 7. Show only UDP sockets opened by processes whose name contains "dhclient"

```bash
$ sockshow /PROTO=UDP /PMASK=dhclient
```

---

## Implementation Details

- For TCP sockets, counters (bytes/packets) are obtained via **NETLINK_SOCK_DIAG**. This requires kernel support (`CONFIG_INET_DIAG`, `CONFIG_TCP_DIAG`) and the `CAP_NET_ADMIN` capability (or running as root). If counters are unavailable, `N/A` is shown.
- The mapping from socket inode to process ID and name is built by scanning `/proc/*/fd/*` and reading the corresponding `comm` files.
- The utility intercepts `SIGTERM`, `SIGINT`, and `SIGQUIT` for graceful termination; `SIGUSR1` toggles the debug mode (`/TRACE`) at runtime.

---

## System Requirements

- **OS:** Linux (relies on `/proc/net/tcp*`, `/proc/net/udp*`, and NETLINK).
- **Kernel:** `CONFIG_INET_DIAG` and `CONFIG_TCP_DIAG` are recommended for TCP counters.
- **Permissions:** NETLINK access may require `CAP_NET_ADMIN` or root privileges. Reading `/proc/net/*` is allowed for normal users.
- **Compiler:** GCC (uses GNU extensions, e.g., `memmem`).

---

## Limitations

- IPv6 filtering by IP address is not supported (options `/LIA` and `/RIA` only work for IPv4).
- Process names are truncated to 10 characters in the output (fixed‑width column).
- With a large number of sockets, the refresh may take some time.

---

## Author

Ruslan R. (The BadAss Sysman) Laishev
For questions or suggestions, please contact the author (if an email address is provided in the source files).

---

## License

The code is distributed without an explicit license. Use at your own risk.
For commercial use, please contact the author.
```
