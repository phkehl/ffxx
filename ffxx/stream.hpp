/**
 * \verbatim
 * flipflip's c++ library (ffxx)
 *
 * Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
 * https://oinkzwurgl.org/projaeggd/ffxx/
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 * \endverbatim
 *
 * @file
 * @brief Streams
 */
// clang-format off
/**
 * @page FFXX_STREAM Streams
 *
 * @section FFXX_STREAM_INTRO Introduction
 *
 * This implements a input and output interface for streams of messages (see @ref FFXX_PARSER). Note that this
 * is not a general-purpose i/o library. Instead it focuses to support working with messages based devices, such as GNSS
 * receivers and other sensors that use a message based interface. Also, it does not aim to for high performance or
 * large throughput. Instead it focuses on convenience and flexibility in use with devices of low data rate (a few
 * kilobytes per second, but probably 100s of KiB works fine, too).
 *
 * A stream object is created from a @ref FFXX_STREAM_SPEC string. Once the stream is started we can read
 * (receive) messages:
 *
 * @code{cpp}
 * // Note: error handling omitted!
 * auto stream = Stream::FromSpec("tcpcli://localhost:12345");
 * stream->Start();
 * while (true) {
 *     ParserMsg msg;
 *     while (stream->Read(msg, 100)) {
 *          INFO("Received %s", msg.name_.c_str());
 *     }
 * }
 * stream->Stop();
 * stream.reset();
 * @endcode
 *
 * A fundamental concept, or behaviour, of most stream types is that they are always "working". Once a stream is started
 * it automatically "connects" and stays connected. It also automatically re-connectes should it ever loose connection.
 * That is, the stream never "stops" unless it is explicitly told to stop. Of course, read and write to the stream is
 * not possible unless it is successfully connected. Such streams always succeed to Start().
 *
 * Some stream types may fail to Start() if they fail to aquire a necessary resource, such as a network port, a CAN
 * interface or a SPI device. A few stream types can be configured for delayed initialisation in order to wait for a
 * resource to appear, for example to support hotpluggable serial ports (USB serial or ACM devices).
 *
 * Interfaces are provided to query or observe the stream state. In general the state diagram of a stream looks like
 * this:
 *
 *     --.[create]---> CLOSED ---[start]---> CONNECTING ------> CONNECTED
 *                       ^                     |   ^                |
 *                       |                (failed) |                |
 *                  (no retry)                 |  (retry)           |
 *                       |                     +   |                |
 *                       +-------------------- ERROR <--------------+
 *
 * That is, after the stream is created it is in state CLOSED. On start the state changes to CONNECTING. If that
 * succeeds, the stream is CONNECTED and can be used. Shout the attempt to connect fail, or should the connected stream
 * fail after being connected, the stream state goes to ERROR. Depending on the stream type and its configuration, this
 * may be the end and the stream state goes to CLOSED or the stream may try to connect again.
 *
 * The exact behaviour differs by @ref FFXX_STREAM_TYPES and @ref FFXX_STREAM_OPTS. For example, a
 * connection to a remote raw TCP/IP server (see @ref FFXX_STREAM_TYPES_TCPCLI_TCPCLIS) by default behaves as
 * follows:
 *
 *     ---[create]---> CLOSED ---[start]---> CONNECTING --> resolve --> connect --> CONNECTED
 *                                               ^             |          |            |
 *                                               |             |          |            |
 *                                           (retry_to)    (failed)   (failed)     (inact_to)
 *                                               |             |          |            |
 *                                               |             v          v            v
 *                                             ERROR <---------+----------+------------+
 *
 * In the state CONNECTING the given destination host name and port is resolved. For example, the hostname *localhost*
 * can resolve to multiple possible endpoints, typically *[::1]* (IPv6) and *127.0.0.1* (IPv4). The stream attempts to
 * connect to them in order. If an attempt succeeds, the stream is CONNECTED. If all attempts fail, the stream is in
 * ERROR. Of course, the resolve itself may fail, in which case the stream goes to ERROR as well.
 *
 * In CONNECTED state the stream is now ready to read and write to. An optional inactivity timeout kicks in when no data
 * is received from the remote end for some time. In this case the stream disconnects from the remote and and goes to
 * ERROR.
 *
 * If the ERROR state is reached a timer is set using a configurable timeout. Once the time is over the stream is back
 * in the CONNECTING state and attempts to resolve and connect again.
 *
 * And so on...
 *
 * @subsection FFXX_STREAM_SPEC Stream spec
 *
 * @fp_codegen_begin{FFXX_STREAM_SPEC}
 * The stream spec is in the form '<code>&lt;scheme&gt;</code>://<code>&lt;path&gt;</code>[,<code>&lt;option&gt;</code>][,<code>&lt;option&gt;</code>][...]'.
 * The <code>&lt;scheme&gt;</code> defines the structure of the <code>&lt;path&gt;</code> and which <code>&lt;option&gt;</code>s are applicable.
 * @fp_codegen_end{FFXX_STREAM_SPEC}
 *
 * @subsection FFXX_STREAM_TYPES Stream types
 *
 * @fp_codegen_begin{FFXX_STREAM_TYPES}
 *
 * @subsubsection FFXX_STREAM_TYPES_SUMMARY Summary
 *
 *     serial       <device>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]]       RW  -       A=0.0   R=5.0   H=off
 *     tcpcli(s)    <host>:<port>                                              RW  C=10.0  A=0.0   R=0.0   -
 *     ntripcli(s)  <host>:<port>                                              RW  C=10.0  A=10.0  R=5.0   -
 *     telnet(s)    <host>:<port>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]]  RW  C=10.0  A=10.0  R=5.0   -
 *     ntripsvr(s)  <credentials>@<host>:<port>/<mountpoint>[:<version>]       WO  C=10.0  -       R=5.0   -
 *     tcpsvr       [<host>]:<port>                                            RW  -       -       -       -
 *     udpcli       <host>:<port>                                              WO  -       -       -       -
 *     udpsvr       [<host>]:<port>                                            RO  -       -       -       -
 *     spidev       <device>[:<speed>[:<bpw>[:<xfersize>[:<mode>]]]]           RW  -       -       -       -
 *     canstr       <dev>:<canid_in>:<canid_out>[:<ff>[:<fd>[:<brs>]]]         RW  -       -       -       -
 *     gga          <lat>/<lon>/<height>[[:<interval>]:<talker>]               RO  -       -       -       -
 *     sta          <x>/<y>/<z>[[[:<interval>]:<sta>]:<type>]                  RO  -       -       -       -
 *     loop         [<delay>][:<rate>]                                         RW  -       -       -       -
 *     fileout      <file>[:<swap>[:<ts>]]                                     WO  -       -       -       -
 *     filein       <file>[:<speed>[:<offset>]]                                RO  -       -       -       -
 *     exec         <path>[[:<arg>]...]                                        RW  -       A=0.0   R=0.0   -
 *     ipcsvr       <name>                                                     RW  -       -       -       -
 *     ipccli       <name>                                                     RW  -       -       R=5.0   H=on
 *
 * @subsubsection FFXX_STREAM_TYPES_SERIAL Serial port or UART (RW)
 *
 * Spec: <em>serial://<code>&lt;device&gt;</code>[:<code>&lt;baudrate&gt;</code>[:<code>&lt;autobaud&gt;</code>[:<code>&lt;mode&gt;</code>[:<code>&lt;flow&gt;</code>]]]]</em>
 *
 * <code>&lt;device&gt;</code> device path (e.g. '/dev/ttyUSB0'), <code>&lt;autobaud&gt;</code> is one of 'none' (default),
 * 'passive', 'ubx', 'fp' or 'auto', <code>&lt;baudrate&gt;</code> in [bps] (default: 115200 resp. 921600 for ACM
 * devices, <code>&lt;mode&gt;</code> '8N1' (default, no other modes are currently supported), <code>&lt;flow&gt;</code> 'off'
 * (default), 'sw' or 'hw'
 *
 * <code>&lt;option&gt;</code>s (default): A=<code>&lt;timeout&gt;</code> (0.0), R=<code>&lt;timeout&gt;</code> (5.0), H=off|on (off)
 *
 * @subsubsection FFXX_STREAM_TYPES_TCPCLI_TCPCLIS TCP client (opt. TLS) (RW)
 *
 * Spec: <em>tcpcli(s)://<code>&lt;host&gt;</code>:<code>&lt;port&gt;</code></em>
 *
 * <code>&lt;host&gt;</code> address (<code>&lt;IPv4&gt;</code> or [<code>&lt;IPv6&gt;</code>]) or hostname, <code>&lt;port&gt;</code> port number
 *
 * <code>&lt;option&gt;</code>s (default): C=<code>&lt;timeout&gt;</code> (10.0), A=<code>&lt;timeout&gt;</code> (0.0), R=<code>&lt;timeout&gt;</code> (0.0)
 *
 * @subsubsection FFXX_STREAM_TYPES_NTRIPCLI_NTRIPCLIS NTRIP client (opt. TLS) (RW)
 *
 * Spec: <em>ntripcli(s)://<code>&lt;host&gt;</code>:<code>&lt;port&gt;</code></em>
 *
 * <code>&lt;credentials&gt;</code> is <code>&lt;username&gt;</code>:<code>&lt;password&gt;</code>, =<code>&lt;base64_encoded_credentials&gt;</code> or %<code>&lt;path&gt;</code> to read
 * either from a file, <code>&lt;host&gt;</code> address (<code>&lt;IPv4&gt;</code> or [<code>&lt;IPv6&gt;</code>]) or hostname, <code>&lt;port&gt;</code> port number
 * <code>&lt;mountpoint&gt;</code> name of the caster mountpoint, <code>&lt;version&gt;</code> NTRIP version 'auto' (default), 'v1'
 * or 'v2'
 *
 * <code>&lt;option&gt;</code>s (default): C=<code>&lt;timeout&gt;</code> (10.0), A=<code>&lt;timeout&gt;</code> (10.0), R=<code>&lt;timeout&gt;</code> (5.0)
 *
 * @subsubsection FFXX_STREAM_TYPES_TELNET_TELNETS Telnet/RFC2217 client (opt. TLS) (RW)
 *
 * Spec: <em>telnet(s)://<code>&lt;host&gt;</code>:<code>&lt;port&gt;</code>[:<code>&lt;baudrate&gt;</code>[:<code>&lt;autobaud&gt;</code>[:<code>&lt;mode&gt;</code>[:<code>&lt;flow&gt;</code>]]]]</em>
 *
 * <code>&lt;host&gt;</code> address (<code>&lt;IPv4&gt;</code> or [<code>&lt;IPv6&gt;</code>]) or hostname, <code>&lt;port&gt;</code> port number, <code>&lt;baudrate&gt;</code> in [bps]
 * (default: 115200), <code>&lt;autobaud&gt;</code> is one of 'none' (default), 'passive', 'ubx', 'fp' or 'auto',
 * <code>&lt;mode&gt;</code> '8N1' (default, no other modes are currently supported), <code>&lt;flow&gt;</code> is 'off' (default),
 * 'sw' or 'hw'
 *
 * <code>&lt;option&gt;</code>s (default): C=<code>&lt;timeout&gt;</code> (10.0), A=<code>&lt;timeout&gt;</code> (10.0), R=<code>&lt;timeout&gt;</code> (5.0)
 *
 * @subsubsection FFXX_STREAM_TYPES_NTRIPSVR_NTRIPSVRS NTRIP server (opt. TLS) (WO)
 *
 * Spec: <em>ntripsvr(s)://<code>&lt;credentials&gt;</code>@@<code>&lt;host&gt;</code>:<code>&lt;port&gt;</code>/<code>&lt;mountpoint&gt;</code>[:<code>&lt;version&gt;</code>]</em>
 *
 * <code>&lt;credentials&gt;</code> is <code>&lt;password&gt;</code> for v1, <code>&lt;username&gt;</code>:<code>&lt;password&gt;</code> for v2,
 * =<code>&lt;base64_encoded_credentials&gt;</code> or %<code>&lt;path&gt;</code> to read either from a file, <code>&lt;host&gt;</code> address (<code>&lt;IPv4&gt;</code>
 * or [<code>&lt;IPv6&gt;</code>]) or hostname, <code>&lt;port&gt;</code> port number <code>&lt;mountpoint&gt;</code> name of the caster mountpoint,
 * <code>&lt;version&gt;</code> NTRIP version 'v1' (default) or 'v2'
 *
 * <code>&lt;option&gt;</code>s (default): C=<code>&lt;timeout&gt;</code> (10.0), R=<code>&lt;timeout&gt;</code> (5.0)
 *
 * @subsubsection FFXX_STREAM_TYPES_TCPSVR TCP server (RW)
 *
 * Spec: <em>tcpsvr://[<code>&lt;host&gt;</code>]:<code>&lt;port&gt;</code></em>
 *
 * <code>&lt;host&gt;</code> address (<code>&lt;IPv4&gt;</code> or [<code>&lt;IPv6&gt;</code>]) or hostname (bind to all interfaces if empty),
 * <code>&lt;port&gt;</code> port number. This stream accepts a maximum of 20 clients.
 *
 * @subsubsection FFXX_STREAM_TYPES_UDPCLI UDP client (WO)
 *
 * Spec: <em>udpcli://<code>&lt;host&gt;</code>:<code>&lt;port&gt;</code></em>
 *
 * <code>&lt;host&gt;</code> address (<code>&lt;IPv4&gt;</code> or [<code>&lt;IPv6&gt;</code>]) or hostname, <code>&lt;port&gt;</code> port number. This stream is not
 * able to distinguish different clients (sources) and may mangle data if multiple clients
 * send data at the same time.
 *
 * @subsubsection FFXX_STREAM_TYPES_UDPSVR UDP server (RO)
 *
 * Spec: <em>udpsvr://[<code>&lt;host&gt;</code>]:<code>&lt;port&gt;</code></em>
 *
 * <code>&lt;host&gt;</code> address (<code>&lt;IPv4&gt;</code> or [<code>&lt;IPv6&gt;</code>]) or hostname (bind to all interfaces if empty),
 * <code>&lt;port&gt;</code> port number.
 *
 * @subsubsection FFXX_STREAM_TYPES_SPIDEV Linux spidev (master) (RW)
 *
 * Spec: <em>spidev://<code>&lt;device&gt;</code>[:<code>&lt;speed&gt;</code>[:<code>&lt;bpw&gt;</code>[:<code>&lt;xfersize&gt;</code>[:<code>&lt;mode&gt;</code>]]]]</em>
 *
 * <code>&lt;device&gt;</code> device path (e.g. '/dev/spidev0.3'), <code>&lt;speed&gt;</code> [Hz] (default: 1000000), <code>&lt;bpw&gt;</code> 8, 16
 * or 32 (default) bits per word, <code>&lt;xfersize&gt;</code> [bytes] (64-2048 and multiple of 4, default 64),
 * <code>&lt;mode&gt;</code> SPI mode (flags from linux/spi/spi.h, default 0x00000000)
 * This assumes that the device ignores all-0xff on input and sends all-0xff to indicate
 * no data.
 *
 * @subsubsection FFXX_STREAM_TYPES_CANSTR SocketCAN stream (RW)
 *
 * Spec: <em>canstr://<code>&lt;dev&gt;</code>:<code>&lt;canid_in&gt;</code>:<code>&lt;canid_out&gt;</code>[:<code>&lt;ff&gt;</code>[:<code>&lt;fd&gt;</code>[:<code>&lt;brs&gt;</code>]]]</em>
 *
 * <code>&lt;dev&gt;</code> interface device (e.g. 'can0'), <code>&lt;canid_out&gt;</code> / <code>&lt;canid_in&gt;</code>  CAN ID for outgoing
 * (write) / incoming (read) frames (0x001-0x7ff for SFF, 0x00000001-0x1fffffff for EFF),
 * <code>&lt;ff&gt;</code> frame format ('sff' or 'eff'), <code>&lt;fd&gt;</code> 'fd' for CAN FD or '' for classical CAN,
 * <code>&lt;brs&gt;</code> 'brs' or '' for CAN FD bitrate switch (only with <code>&lt;fd&gt;</code> = 'fd'). Note that CAN
 * interface (bitrates etc.) must be configured appropriately, e.g. using 'ip link'.
 *
 * @subsubsection FFXX_STREAM_TYPES_GGA NMEA GGA generator (RO)
 *
 * Spec: <em>gga://<code>&lt;lat&gt;</code>/<code>&lt;lon&gt;</code>/<code>&lt;height&gt;</code>[[:<code>&lt;interval&gt;</code>]:<code>&lt;talker&gt;</code>]</em>
 *
 * <code>&lt;lat&gt;</code> latitude [deg], <code>&lt;lon&gt;</code> longitude [deg], <code>&lt;height&gt;</code> height [m], <code>&lt;interval&gt;</code> output
 * interval in [s] (1.0 - 86400.0 s, default: 5.0), <code>&lt;talker&gt;</code> NMEA talker ID (default 'GN')
 *
 * @subsubsection FFXX_STREAM_TYPES_STA RTCM3 station message generator (RO)
 *
 * Spec: <em>sta://<code>&lt;x&gt;</code>/<code>&lt;y&gt;</code>/<code>&lt;z&gt;</code>[[[:<code>&lt;interval&gt;</code>]:<code>&lt;sta&gt;</code>]:<code>&lt;type&gt;</code>]</em>
 *
 * <code>&lt;x&gt;</code>/<code>&lt;y&gt;</code>/<code>&lt;z&gt;</code> ECEF coordinates [m], <code>&lt;interval&gt;</code> output interval in [s] (1.0 - 86400.0 s,
 * default: 5.0), <code>&lt;sta&gt;</code> station ID (default 0), <code>&lt;type&gt;</code> message type (default 1005, 1006 or
 * 1032).
 * DF022, DF023, DF024, DF142 are set to 1, DF021, DF141, DF364 and DF028 are set to 0
 *
 * @subsubsection FFXX_STREAM_TYPES_LOOP Loopback (echo) (RW)
 *
 * Spec: <em>loop://[<code>&lt;delay&gt;</code>][:<code>&lt;rate&gt;</code>]</em>
 *
 * Delay echoed data my <code>&lt;delay&gt;</code> [ms] (default 0) or limit rate of echoed data to
 * <code>&lt;bytes_per_sec&gt;</code> bytes per second (0 to disable rate limiting, default 0)
 *
 * @subsubsection FFXX_STREAM_TYPES_FILEOUT File writer (WO)
 *
 * Spec: <em>fileout://<code>&lt;file&gt;</code>[:<code>&lt;swap&gt;</code>[:<code>&lt;ts&gt;</code>]]</em>
 *
 * <code>&lt;file&gt;</code> file path with optional placeholders for UTC  '%Y' (year, e.g. 2024),
 * '%m' (month, 01-12) '%d' (day, 01-31), '%h' (hour, 00-23), '%M' (minute, 00-59),
 * '%S' (second, 00-60), '%j' (day of year, 001-366), '%W' (GPS week number, e.g. 1234),
 * '%w' (day of GPS week, 0-6), '%s' (GPS time of week [s], 0-604799), optional <code>&lt;swap&gt;</code> file
 * swap time [s] (60-86400, negative value for unaligned timestamps, default: '', that is,
 * no swap, <code>&lt;ts&gt;</code> store index sidecar file for replay ('ts') (default '', i.e. no sidecar file)
 *
 * @subsubsection FFXX_STREAM_TYPES_FILEIN File read (RO)
 *
 * Spec: <em>filein://<code>&lt;file&gt;</code>[:<code>&lt;speed&gt;</code>[:<code>&lt;offset&gt;</code>]]</em>
 *
 * <code>&lt;file&gt;</code> file path, <code>&lt;speed&gt;</code> replay speed (default 0.0, that is, ignore .ts file), <code>&lt;offset&gt;</code>
 * replay offset [s] (default: 0.0)
 *
 * @subsubsection FFXX_STREAM_TYPES_EXEC External program stdin/stdout (RW)
 *
 * Spec: <em>exec://<code>&lt;path&gt;</code>[[:<code>&lt;arg&gt;</code>]...]</em>
 *
 * <code>&lt;path&gt;</code> to executable, <code>&lt;arg&gt;</code> optional argument(s)
 *
 * <code>&lt;option&gt;</code>s (default): A=<code>&lt;timeout&gt;</code> (0.0), R=<code>&lt;timeout&gt;</code> (0.0)
 *
 * @subsubsection FFXX_STREAM_TYPES_IPCSVR Interprocess stream (server) (RW)
 *
 * Spec: <em>ipcsvr://<code>&lt;name&gt;</code></em>
 *
 * <code>&lt;name&gt;</code> unique name for the connection
 *
 * @subsubsection FFXX_STREAM_TYPES_IPCCLI Interprocess stream (client) (RW)
 *
 * Spec: <em>ipccli://<code>&lt;name&gt;</code></em>
 *
 * <code>&lt;name&gt;</code> unique name for the connection
 *
 * <code>&lt;option&gt;</code>s (default): R=<code>&lt;timeout&gt;</code> (5.0), H=off|on (on)
 * @fp_codegen_end{FFXX_STREAM_TYPES}
 *
 * @subsection FFXX_STREAM_OPTS Stream options
 *
 * @fp_codegen_begin{FFXX_STREAM_OPTS}
 * The <code>&lt;option&gt;</code>s are (not all streams support all options):
 *
 * - N=<code>&lt;name&gt;</code>     -- A short and concise name for the stream ([a-zA-Z0-9_)]
 * - RO           -- Make a RW stream read-only (input only), that is, ignore any writes (output)
 * - WO           -- Make a RW stream write-only (output only), that is, ignore any reads (input)
 * - C=<code>&lt;timeout&gt;</code>  -- Connect timeout [s] (1.0-3600.0, 0.0 to disable)
 * - A=<code>&lt;timeout&gt;</code>  -- Read (and only read!) inactivity timeout [s] (1.0-3600.0, 0.0 to disable)
 * - R=<code>&lt;timeout&gt;</code>  -- Retry timeout [s] (2.0-3600.0)
 * - H=off|on     -- Initialise on start ('off') or allow delayed initialisation ('on'). Useful for
 *                   hot-pluggable devices. Use with R=<code>&lt;timeout&gt;</code>.
 * @fp_codegen_end{FFXX_STREAM_OPTS}
 *
 *
 * @subsection FFXX_STREAM_TLS TLS
 *
 * @fp_codegen_begin{FFXX_STREAM_TLS}
 * Secure client streams (tcpclis://, etc.) can use TLS 1.2 or 1.3. To use server authentication the
 * corresponding certificate must be available. They are loaded from the path or file given by the
 * FFXX_STREAM_TLS_FILES_PATH environment variable. See the SSL_CTX_load_verify_locations(3ssl) man
 * page for details. The certificate must match the used hostname or address. See X509_check_host(3ssl)
 * man page for details.
 * @fp_codegen_end{FFXX_STREAM_TLS}
 *
 * @section FFXX_STREAM_NOTES Notes
 *
 * Some useful tools for testing:
 *
 * - netcat, ncat, wireshark
 *
 * TLS stuff:
 *
 * - Create server key and certificate
 *
 *       openssl req -nodes -new -subj "/C=CH/O=Happy Hacker Horror Club/CN=localhost" \
 *           -addext 'subjectAltName = DNS:localhost, IP:10.1.1.112, IP:127.0.0.1, IP:::1' \
 *           -x509 -keyout test-key.pem -out test-cert.pem
 *
 * - Create .0 files (symlinks)
 *
 *       c_rehash -v .
 *
 * - Server
 *
 *       openssl s_server -cert test-cert.pem -key test-key.pem -port 12345 -no_ssl3 -no_tls1 -no_tls1_1
 *       openssl s_server -cert test-cert.pem -key test-key.pem -port 12345 -tls1_2
 *       openssl s_server -cert test-cert.pem -key test-key.pem -port 12345 -tls1_3
 *       ncat --listen 12345 --ssl --ssl-cert test-cert.pem --ssl-key test-key.pem --verbose
 *
 * - Client
 *
 *       openssl s_client -connect localhost:12345 -tls1_2
 *       openssl s_client -connect localhost:12345 -tls1_3
 *       ncat --ssl --ssl-verify --ssl-trustfile test-cert.pem --verbose localhost 1234
 *
 * - Inspect cert
 *
 *       openssl x509 -noout -text -in test-cert.pem
 *       openssl x509 -noout -hash -in test-cert.pem
 *
 * - Get cert from a server
 *
 *       openssl s_client -showcerts -connect example.com:443 </dev/null | openssl x509 > example-cert.pem
 *
 * Missing features, ideas:
 *
 * - TLS clients with PSK? Useful? Possible?
 * - TLS server
 * - And also all the todos...
 *
 * TELNET/RFC2217 testing:
 *
 * - https://github.com/msantos/sredird
 *
 *     $ journalctl -f SYSLOG_IDENTIFIER=sredird &
 *     $ while true; do ncat --listen 12345 --exec "/usr/sbin/sredird 99 /dev/ttyUSB0 /tmp/lockilock"; sleep 0.1; done
 *
 * - https://sourceforge.net/projects/sercd/
 * - https://sourceforge.net/projects/ser2net/
 * - https://com0com.sourceforge.net/
 *
 */
// clang-format on
#ifndef __FFXX_STREAM_HPP__
#define __FFXX_STREAM_HPP__

/* LIBC/STL */
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

/* EXTERNAL */
#include <fpsdk_common/parser/types.hpp>

/* PACKAGE */

namespace ffxx {
/* ****************************************************************************************************************** */

/**
 * @brief Stream types
 *
 * See @ref FFXX_STREAM_TYPES
 */
enum class StreamType  // clang-format off
{
    // @fp_codegen_begin{FFXX_STREAM_TYPE_HPP}
    UNSPECIFIED,          //!< Unspecified
    SERIAL,               //!< Serial port or UART, see @ref FFXX_STREAM_TYPES_SERIAL
    TCPCLI,               //!< TCP client (without TLS), see @ref FFXX_STREAM_TYPES_TCPCLI_TCPCLIS
    TCPCLIS,              //!< TCP client (with TLS), see @ref FFXX_STREAM_TYPES_TCPCLI_TCPCLIS
    NTRIPCLI,             //!< NTRIP client (without TLS), see @ref FFXX_STREAM_TYPES_NTRIPCLI_NTRIPCLIS
    NTRIPCLIS,            //!< NTRIP client (with TLS), see @ref FFXX_STREAM_TYPES_NTRIPCLI_NTRIPCLIS
    TELNET,               //!< TCP client (without TLS), see @ref FFXX_STREAM_TYPES_TELNET_TELNETS
    TELNETS,              //!< TCP client (with TLS), see @ref FFXX_STREAM_TYPES_TELNET_TELNETS
    NTRIPSVR,             //!< NTRIP server (without TLS), see @ref FFXX_STREAM_TYPES_NTRIPSVR_NTRIPSVRS
    NTRIPSVRS,            //!< NTRIP server (with TLS), see @ref FFXX_STREAM_TYPES_NTRIPSVR_NTRIPSVRS
    TCPSVR,               //!< TCP server, see @ref FFXX_STREAM_TYPES_TCPSVR
    UDPCLI,               //!< UDP client, see @ref FFXX_STREAM_TYPES_UDPCLI
    UDPSVR,               //!< UDP server, see @ref FFXX_STREAM_TYPES_UDPSVR
    SPIDEV,               //!< Linux spidev (master), see @ref FFXX_STREAM_TYPES_SPIDEV
    CANSTR,               //!< SocketCAN stream, see @ref FFXX_STREAM_TYPES_CANSTR
    GGA,                  //!< NMEA GGA generator, see @ref FFXX_STREAM_TYPES_GGA
    STA,                  //!< RTCM3 station message generator, see @ref FFXX_STREAM_TYPES_STA
    LOOP,                 //!< Loopback (echo), see @ref FFXX_STREAM_TYPES_LOOP
    FILEOUT,              //!< File writer, see @ref FFXX_STREAM_TYPES_FILEOUT
    FILEIN,               //!< File read, see @ref FFXX_STREAM_TYPES_FILEIN
    EXEC,                 //!< External program stdin/stdout, see @ref FFXX_STREAM_TYPES_EXEC
    IPCSVR,               //!< Interprocess stream (server), see @ref FFXX_STREAM_TYPES_IPCSVR
    IPCCLI,               //!< Interprocess stream (client), see @ref FFXX_STREAM_TYPES_IPCCLI
    // @fp_codegen_end{FFXX_STREAM_TYPE_HPP}
};  // clang-format on

/**
 * @brief Stringify stream type
 *
 * @param[in]  type  The stream type
 *
 * @returns the stringification of the stream type
 */
const char* StreamTypeStr(const StreamType type);

// ---------------------------------------------------------------------------------------------------------------------

/**
 * @brief Stream mode
 */
enum class StreamMode
{
    UNSPECIFIED,  //!< Unspecified
    RW,           //!< Read-write
    RO,           //!< Read-only
    WO,           //!< Write-only
};

/**
 * @brief Stringify stream mode
 *
 * @param[in]  mode  The stream mode
 *
 * @returns the stringification of the stream type
 */
const char* StreamModeStr(const StreamMode mode);

// ---------------------------------------------------------------------------------------------------------------------

/**
 * @brief Stream state
 */
enum class StreamState
{
    CLOSED,      //!< Stream closed
    CONNECTING,  //!< Stream connecting (waiting for connection)
    CONNECTED,   //!< Stream connected
    ERROR,       //!< Stream error (connection failed, in reconnect timeout)
};

/**
 * @brief Stringify stream state
 *
 * @param[in]  state  The stream state
 *
 * @returns the stringification of the stream state
 */
const char* StreamStateStr(const StreamState state);

// ---------------------------------------------------------------------------------------------------------------------

/**
 * @brief Stream error
 */
enum class StreamError
{
    NONE,             //!< No error
    RESOLVE_FAIL,     //!< Resolve failed
    CONNECT_FAIL,     //!< Connection failed
    CONNECT_TIMEOUT,  //!< Timeout connecting
    BAD_RESPONSE,     //!< Bad Response
    AUTH_FAIL,        //!< Authentication failed
    DEVICE_FAIL,      //!< Device operation (open, read, write, ...) failed
    NO_DATA_RECV,     //!< No data received
    CONN_LOST,        //!< Connection lost
    BAD_MOUNTPOINT,   //!< Bad mountpoint requested
    TLS_ERROR,        //!< TLS error (verification failed, etc.)
    TELNET_ERROR,     //!< Telnet options negotiation failed
};

/**
 * @brief Stringify stream error
 *
 * @param[in]  error  The stream state
 *
 * @returns the stringification of the stream state
 */
const char* StreamErrorStr(const StreamError error);

// ---------------------------------------------------------------------------------------------------------------------

/**
 * @brief Autobauding mode
 */
enum class AutobaudMode
{
    NONE,     //!< No autobauding
    PASSIVE,  //!< Passive autobauding (accept any message other than Protocol::OTHER messages)
    UBX,      //!< Actively detect u-blox GNSS receivers (poll UBX-MON-VER)
    FP,       //!< Actively detect Fixposition sensors (poll FP_B-VERSION)
    AUTO,     //!< Automatic autobauding (send UBX and FP polls, but otherwise like PASSIVE)
};

/**
 * @brief Stringify autobaud mode
 *
 * @param[in]  mode  The autobaud mode
 *
 * @returns the stringification of the autobaud mode
 */
const char* AutobaudModeStr(const AutobaudMode mode);

// ---------------------------------------------------------------------------------------------------------------------

struct StreamOpts;                                  //!< Forward declaration
struct Stream;                                      //!< Forward declaration
using StreamOptsPtr = std::unique_ptr<StreamOpts>;  //!< Stream options pointer
using StreamPtr = std::unique_ptr<Stream>;          //!< Stream pointer

// ---------------------------------------------------------------------------------------------------------------------

/**
 * @brief Stream options
 *
 * See also @ref FFXX_STREAM_SPEC.
 */
struct StreamOpts
{
    /**
     * @brief Make options from stream spec
     *
     * @param[in]   spec     The stream spec, see @ref FFXX_STREAM_SPEC
     * @param[out]  e_msg    Error message in case of bad spec
     *
     * @returns the stream options (and an empty e_msg) if the spec is valid, or nullptr and a useful e_msg otherwise
     */
    static StreamOptsPtr FromSpec(const std::string& spec, std::string& e_msg);

    // clang-format off

    // ----- configurable from spec string -----

    std::string                  name_;                              //!< A short and concise name for the stream
    StreamType                   type_   = StreamType::UNSPECIFIED;  //!< Stream type
    StreamMode                   mode_   = StreamMode::UNSPECIFIED;  //!< Stream mode. This may contain secrets!
    std::string                  path_;                              //!< Stream path. This may contain secrets!
    std::string                  opts_;                              //!< Common stream options
    std::string                  spec_;                              //!< Canonical spec
    std::string                  disp_;                              //!< Display string, path_ (with secrets stripped) and opts_
    std::chrono::milliseconds    conn_to_           { 0 };           //!< Connect timeout [s]
    std::chrono::milliseconds    inact_to_          { 0 };           //!< Inactivity timeout [s] (0 = no timeout)
    std::chrono::milliseconds    retry_to_          { 0 };           //!< Timeout for reconnect [s] (0 = no reconnect)
    bool                         hotplug_           = false;         //!< Used delayed initialisation / hotplug

    static constexpr double      CONN_TO_MIN        =     1.0;       //!< Minimal value [s] for conn_to_ [ms]
    static constexpr double      CONN_TO_MAX        =  3600.0;       //!< Maximal value [s] for conn_to_ [ms]
    static constexpr double      INACT_TO_MIN       =     1.0;       //!< Minimal value [s] for inact_to_ [ms]
    static constexpr double      INACT_TO_MAX       =  3600.0;       //!< Maximal value [s] for inact_to_ [ms]
    static constexpr double      RETRY_TO_MIN       =     2.0;       //!< Minimal value [s] for retry_to_ [ms]
    static constexpr double      RETRY_TO_MAX       =  3600.0;       //!< Maximal value [s] for retry_to_ [ms]

    // ----- not currently configurable from spec string -----

    bool                         quiet_             = false;         //!< Disable INFO() output from the stream code
    std::size_t                  r_queue_size_      =       10000;   //!< Stream receive queue size [number of messages]
    std::size_t                  w_queue_size_      =  512 * 1024;   //!< Stream write queue size [bytes]
    std::size_t                  rx_buf_size_       =  128 * 1024;   //!< Receive buffer size [bytes], e.g. TCP socket
    std::size_t                  tx_buf_size_       =  128 * 1024;   //!< Transmit buffer size [bytes], e.g. TCP socket
    std::size_t                  max_clients_       =         20;    //!< Maximum number of clients (for server streams)

    static constexpr std::size_t R_QUEUE_SIZE_MIN   =  100;          //!< Minimal value for read_queue_size_
    static constexpr std::size_t W_QUEUE_SIZE_MIN   =    1 * 1024;   //!< Minimal value for write_queue_size_
    static constexpr std::size_t RX_BUF_SIZE_MIN    =    8 * 1024;   //!< Minimal value for rx_buf_size_
    static constexpr std::size_t TX_BUF_SIZE_MIN    =    8 * 1024;   //!< Minimal value for tx_buf_size_
    static constexpr std::size_t MAX_CLIENTS_MIN    =    1;          //!< Minimal value for max_clients_
    static constexpr std::size_t MAX_PATH_LEN       = 2000;          //!< Maximum length for path_

    // Secure TCP/IP streams with TLS, see FFXX_STREAM_TLS docu
    std::string                  tls_files_path_ /* = "" */;         //!< Path to file or directory with TLS certs and keys
    static constexpr const char *TLS_FILES_PATH_ENV = "FFXX_STREAM_TLS_FILES_PATH"; //!< Name of environment variable to use if tls_files_path_ is empty


    // ----- other useful things -----

    static constexpr uint16_t PORT_MIN = 1;      //!< Minimal value for IP port number
    static constexpr uint16_t PORT_MAX = 65535;  //!< Maximal value for IP port number

    static constexpr std::array<uint32_t, 8> BAUDRATES = {{
        9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 }}; //!< Valid baudrates (ordered by increasing speed)
    static constexpr std::array<const char*, 8> BAUDRATE_STRS = {{
        "9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600" }}; //!< BAUDRATES as strings
    static constexpr std::array<const char*, 8> BAUDRATE_FANCY_STRS = {{
        "9'600", "19'200", "38'400", "57'600", "115'200", "230'400", "460'800", "921'600" }}; //!< BAUDRATES as fancy strings

    static constexpr uint32_t BAUDRATE_DEF = 115200;                  //!< Default baudrate
    //! Available serial port modes
    enum class SerialMode { UNSPECIFIED /** Unspecified */, _8N1 /** 8N1 */ };
    //! Available serial flow control modes
    enum class SerialFlow { UNSPECIFIED /** Unspecified */, OFF /** Off/none */, SW /** Software (XON/XOFF) */, HW /** Hardware (RTS/CTS) */ };
    static constexpr SerialMode SERIAL_MODE_DEF = SerialMode::_8N1;   //!< Default value for SerialMode
    static constexpr SerialFlow SERIAL_FLOW_DEF = SerialFlow::OFF;    //!< Default value for SerialFlow

    static constexpr double GGA_LAT_MIN     =   -90.0;  //!< Min <lat> for #StreamType::GGA
    static constexpr double GGA_LAT_MAX     =    90.0;  //!< Max <lat> for #StreamType::GGA
    static constexpr double GGA_LON_MIN     =  -180.0;  //!< Min <lon> for #StreamType::GGA
    static constexpr double GGA_LON_MAX     =   180.0;  //!< Max <lon> for #StreamType::GGA
    static constexpr double GGA_HEIGHT_MIN  = -1000.0;  //!< Min <height> for #StreamType::GGA
    static constexpr double GGA_HEIGHT_MAX  = 10000.0;  //!< Max <height> for #StreamType::GGA
    static constexpr double GGA_PERIOD_MIN  =     1.0;  //!< Min <period> for #StreamType::GGA
    static constexpr double GGA_PERIOD_MAX  = 86400.0;  //!< Max <period> for #StreamType::GGA

    // clang-format on

    // ----- internal use -----

    virtual ~StreamOpts() = default;  //!< Make it polymorphic

    /**
     * @brief Update spec_
     */
    void UpdateSpec();
};

/**
 * @brief Stream
 *
 * See @ref FFXX_STREAM
 */
class Stream
{
   public:
    /**
     * @brief Constructor
     *
     * @note Use FromSpec() or FromOpts() to instantiate a Stream object.
     */
    Stream() = default;

    /**
     * @brief Destructor
     */
    virtual ~Stream() = default;

    /**
     * @brief Create a stream from spec
     *
     * @param[in]  spec  The stream spec, see @ref FFXX_STREAM_SPEC
     *
     * @returns a stream handle or nullptr on error (bad spec, stream failed to initialise, ...)
     */
    static StreamPtr FromSpec(const std::string& spec);

    /**
     * @brief Create a stream from opts
     *
     * @param[in]  opts  The stream options obtained from StreamOpts::FromSpec()
     *
     * @returns a stream handle or nullptr on error (bad opts, stream failed to initialise, ...)
     */
    static StreamPtr FromOpts(StreamOptsPtr& opts);

    /**
     * @brief Start stream
     *
     * @returns true if the stream could be started, false otherwise
     */
    virtual bool Start() = 0;

    /**
     * @brief Stop stream
     *
     * @param[in]  timeout  Timeout [ms] to wait for pending writes to complete, 0 = do not wait, cancel pending writes
     */
    virtual void Stop(const uint32_t timeout = 0) = 0;

    /**
     * @brief Read next received message from stream
     *
     * @param[out]  msg      The received message frame
     * @param[in]   timeout  Timeout [ms] to wait for data, see Wait(), 0 = return immediately
     *
     * @returns true if a message was available, false otherwise
     */
    virtual bool Read(fpsdk::common::parser::ParserMsg& msg, const uint32_t timeout = 0) = 0;

    /**
     * @brief Write data to stream
     *
     * @param[in]  data     The data
     * @param[in]  size     The size of the data
     * @param[in]  timeout  Timeout [ms] to wait for capacity in tx buffer, 0 = return immediately
     *
     * @returns true if data successfully written, false if data too big and/or timeout too short, stream is not
     *          #StreamState::CONNECTED or stream is #StreamMode::RO
     */
    virtual bool Write(const uint8_t* data, const std::size_t size, const uint32_t timeout = 0) = 0;

    /**
     * @brief Write data to stream
     *
     * @param[in]  data     The data
     * @param[in]  timeout  Timeout [ms] to wait for capacity in tx buffer, 0 = return immediately
     *
     * @returns true if data successfully written, false if data too big and/or timeout too short, stream is not
     *          #StreamState::CONNECTED or stream is #StreamMode::RO
     */
    virtual bool Write(const std::vector<uint8_t> data, const uint32_t timeout = 0) = 0;

    /**
     * @brief Wait for data (but not necessarily a full message) received
     *
     * @param[in]  millis  Number of [ms] to wait at most, must be > 0
     *
     * @returns true if data received (within time limit), false if the timeout has expired
     */
    virtual bool Wait(const uint32_t millis) = 0;

    /**
     * @brief Get baudrate
     *
     * @returns the current baudrate (for streams that can change the baudrate), 0 for streams that cannot change the
     *          baudrate
     */
    virtual uint32_t GetBaudrate() = 0;

    /**
     * @brief Set baudrate
     *
     * @param[in]  baudrate  Baudrate value (one of StreamOpts::BAUDRATE)
     *
     * @returns true if the baudrate was changed, false otherwise (bad value, stream cannot change baudrate)
     */
    virtual bool SetBaudrate(const uint32_t baudrate) = 0;

    /**
     * @brief Do autobauding
     *
     * @param[in]  mode  Autobauding mode
     *
     * @returns true if the autobauding was started, false otherwise (stream cannot change baudrate)
     */
    virtual bool Autobaud(const AutobaudMode mode = AutobaudMode::AUTO) = 0;

    /**
     * @brief Get stream options
     *
     * @returns a copy of the stream options
     */
    virtual const StreamOpts GetOpts() = 0;

    /**
     * @brief Get stream type
     *
     * @returns the stream type
     */
    virtual StreamType GetType() const = 0;

    /**
     * @brief Get stream mode
     *
     * @returns the stream mode
     */
    virtual StreamMode GetMode() const = 0;

    /**
     * @brief Get stream state
     *
     * @returns the stream state
     */
    virtual StreamState GetState() const = 0;

    /**
     * @brief Get stream error details
     *
     * This gives additional details for the StreamState::ERROR.
     *
     * @returns the stream error
     */
    virtual StreamError GetError() const = 0;

    /**
     * @brief Get stream state info
     *
     * @returns a copy of the stream state info
     */
    virtual std::string GetInfo() = 0;

    /**
     * @brief Get parser stats
     *
     * @returns a copy of the parser stats
     */
    virtual fpsdk::common::parser::ParserStats GetParserStats() = 0;

    /**
     * @brief State observer function type
     *
     * See AddStateObserver().
     *
     * @param[in] old_state  Old state
     * @param[in] new_state  New state, like GetState()
     * @param[in] error      Error, like GetError())
     * @param[in] info       Info, like GetInfo()
     */
    using StateObserver = std::function<void(
        const StreamState old_state, const StreamState new_state, const StreamError error, const std::string& info)>;

    /**
     * @brief Add state change observer
     *
     * The observer is called whenever the state changes or there is a new info (in which case old_state == new_state).
     *
     * @param[in]  observer  State change observer function
     */
    virtual void AddStateObserver(StateObserver observer) = 0;

    /**
     * @brief Receive data ready observer function
     *
     * See AddReadObserver().
     */
    using ReadObserver = std::function<void()>;

    /**
     * @brief Add a receive data ready observer
     *
     * The observer is called whenever there is new data to Read().
     */
    virtual void AddReadObserver(ReadObserver observer) = 0;
};

/* ****************************************************************************************************************** */

/**
 * @brief Get help screen for CLI apps
 *
 * @return a multi-line string suitable as help screen for CLI apps
 */
const char* StreamHelpScreen();

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_HPP__
