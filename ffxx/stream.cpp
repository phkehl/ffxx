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

/* LIBC/STL */
#include <array>
#include <cmath>
#include <cstdlib>

/* EXTERNAL */
#include <fpsdk_common/logging.hpp>

/* PACKAGE */
#include "stream.hpp"
#include "stream/base.hpp"
#include "stream/canstr.hpp"
#include "stream/exec.hpp"
#include "stream/files.hpp"
#include "stream/gga_sta.hpp"
#include "stream/ipc.hpp"
#include "stream/loop.hpp"
#include "stream/serial.hpp"
#include "stream/spidev.hpp"
#include "stream/tcpclients.hpp"
#include "stream/tcpservers.hpp"
#include "stream/udp.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

const char* StreamTypeStr(const StreamType type)
{
    switch (type) {  // clang-format off
        // @fp_codegen_begin{FFXX_STREAM_TYPE_CPP}
        case StreamType::UNSPECIFIED: break;
        case StreamType::SERIAL:              return "SERIAL";
        case StreamType::TCPCLI:              return "TCPCLI";
        case StreamType::TCPCLIS:             return "TCPCLIS";
        case StreamType::NTRIPCLI:            return "NTRIPCLI";
        case StreamType::NTRIPCLIS:           return "NTRIPCLIS";
        case StreamType::TELNET:              return "TELNET";
        case StreamType::TELNETS:             return "TELNETS";
        case StreamType::NTRIPSVR:            return "NTRIPSVR";
        case StreamType::NTRIPSVRS:           return "NTRIPSVRS";
        case StreamType::TCPSVR:              return "TCPSVR";
        case StreamType::UDPCLI:              return "UDPCLI";
        case StreamType::UDPSVR:              return "UDPSVR";
        case StreamType::SPIDEV:              return "SPIDEV";
        case StreamType::CANSTR:              return "CANSTR";
        case StreamType::GGA:                 return "GGA";
        case StreamType::STA:                 return "STA";
        case StreamType::LOOP:                return "LOOP";
        case StreamType::FILEOUT:             return "FILEOUT";
        case StreamType::FILEIN:              return "FILEIN";
        case StreamType::EXEC:                return "EXEC";
        case StreamType::IPCSVR:              return "IPCSVR";
        case StreamType::IPCCLI:              return "IPCCLI";
        // @fp_codegen_end{FFXX_STREAM_TYPE_CPP}
    }  // clang-format on
    return "?";
}

// ---------------------------------------------------------------------------------------------------------------------

const char* StreamModeStr(const StreamMode mode)
{
    switch (mode) {  // clang-format off
        case StreamMode::UNSPECIFIED: break;
        case StreamMode::RO:          return "RO";
        case StreamMode::WO:          return "WO";
        case StreamMode::RW:          return "RW";
    }  // clang-format on
    return "?";
}

// ---------------------------------------------------------------------------------------------------------------------

const char* StreamStateStr(const StreamState state)
{
    switch (state) {  // clang-format off
        case StreamState::CLOSED:     return "CLOSED";
        case StreamState::CONNECTING: return "CONNECTING";
        case StreamState::CONNECTED:  return "CONNECTED";
        case StreamState::ERROR:      return "ERROR";
    }  // clang-format on
    return "?";
}

// ---------------------------------------------------------------------------------------------------------------------

const char* StreamErrorStr(const StreamError error)
{
    switch (error) {  // clang-format off
        case StreamError::NONE:             return "NONE";
        case StreamError::RESOLVE_FAIL:     return "RESOLVE_FAIL";
        case StreamError::CONNECT_FAIL:     return "CONNECT_FAIL";
        case StreamError::CONNECT_TIMEOUT:  return "CONNECT_TIMEOUT";
        case StreamError::BAD_RESPONSE:     return "BAD_RESPONSE";
        case StreamError::AUTH_FAIL:        return "AUTH_FAIL";
        case StreamError::DEVICE_FAIL:      return "DEVICE_FAIL";
        case StreamError::NO_DATA_RECV:     return "NO_DATA_RECV";
        case StreamError::CONN_LOST:        return "CONN_LOST";
        case StreamError::BAD_MOUNTPOINT:   return "BAD_MOUNTPOINT";
        case StreamError::TLS_ERROR:        return "TLS_ERROR";
        case StreamError::TELNET_ERROR:     return "TELNET_ERROR";
    }  // clang-format on
    return "?";
}

// ---------------------------------------------------------------------------------------------------------------------

const char* AutobaudModeStr(const AutobaudMode mode)
{
    switch (mode) {  // clang-format off
        case AutobaudMode::NONE:     return "NONE";
        case AutobaudMode::PASSIVE:  return "PASSIVE";
        case AutobaudMode::UBX:      return "UBX";
        case AutobaudMode::FP:       return "FP";
        case AutobaudMode::AUTO:     return "AUTO";
    }  // clang-format on
    return "?";
}

// ---------------------------------------------------------------------------------------------------------------------

const char* StreamHelpScreen()
{
    return  // clang-format off
    // @fp_codegen_begin{FFXX_STREAM_HELP}
    "The stream spec is in the form '<scheme>://<path>[,<option>][,<option>][...]'.\n"
    "The <scheme> defines the structure of the <path> and which <option>s are applicable.\n"
    "\n"
    "Summary:\n"
    "\n"
    "    serial       <device>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]]       RW  -       A=0.0   R=5.0   H=off\n"
    "    tcpcli(s)    <host>:<port>                                              RW  C=10.0  A=0.0   R=0.0   -\n"
    "    ntripcli(s)  <host>:<port>                                              RW  C=10.0  A=10.0  R=5.0   -\n"
    "    telnet(s)    <host>:<port>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]]  RW  C=10.0  A=10.0  R=5.0   -\n"
    "    ntripsvr(s)  <credentials>@<host>:<port>/<mountpoint>[:<version>]       WO  C=10.0  -       R=5.0   -\n"
    "    tcpsvr       [<host>]:<port>                                            RW  -       -       -       -\n"
    "    udpcli       <host>:<port>                                              WO  -       -       -       -\n"
    "    udpsvr       [<host>]:<port>                                            RO  -       -       -       -\n"
    "    spidev       <device>[:<speed>[:<bpw>[:<xfersize>[:<mode>]]]]           RW  -       -       -       -\n"
    "    canstr       <dev>:<canid_in>:<canid_out>[:<ff>[:<fd>[:<brs>]]]         RW  -       -       -       -\n"
    "    gga          <lat>/<lon>/<height>[[:<interval>]:<talker>]               RO  -       -       -       -\n"
    "    sta          <x>/<y>/<z>[[[:<interval>]:<sta>]:<type>]                  RO  -       -       -       -\n"
    "    loop         [<delay>][:<rate>]                                         RW  -       -       -       -\n"
    "    fileout      <file>[:<swap>[:<ts>]]                                     WO  -       -       -       -\n"
    "    filein       <file>[:<speed>[:<offset>]]                                RO  -       -       -       -\n"
    "    exec         <path>[[:<arg>]...]                                        RW  -       A=0.0   R=0.0   -\n"
    "    ipcsvr       <name>                                                     RW  -       -       -       -\n"
    "    ipccli       <name>                                                     RW  -       -       R=5.0   H=on\n"
    "\n"
    "Details:\n"
    "\n"
    "    serial://<device>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]] (RW) -- Serial port or UART\n"
    "\n"
    "        <device> device path (e.g. '/dev/ttyUSB0'), <autobaud> is one of 'none' (default),\n"
    "        'passive', 'ubx', 'fp' or 'auto', <baudrate> in [bps] (default: 115200 resp. 921600 for ACM\n"
    "        devices, <mode> '8N1' (default, no other modes are currently supported), <flow> 'off'\n"
    "        (default), 'sw' or 'hw'\n"
    "        <option>s (default): A=<timeout> (0.0), R=<timeout> (5.0), H=off|on (off)\n"
    "\n"
    "    tcpcli(s)://<host>:<port> (RW) -- TCP client (opt. TLS)\n"
    "\n"
    "        <host> address (<IPv4> or [<IPv6>]) or hostname, <port> port number\n"
    "        <option>s (default): C=<timeout> (10.0), A=<timeout> (0.0), R=<timeout> (0.0)\n"
    "\n"
    "    ntripcli(s)://<host>:<port> (RW) -- NTRIP client (opt. TLS)\n"
    "\n"
    "        <credentials> is <username>:<password>, =<base64_encoded_credentials> or %<path> to read\n"
    "        either from a file, <host> address (<IPv4> or [<IPv6>]) or hostname, <port> port number\n"
    "        <mountpoint> name of the caster mountpoint, <version> NTRIP version 'auto' (default), 'v1'\n"
    "        or 'v2'\n"
    "        <option>s (default): C=<timeout> (10.0), A=<timeout> (10.0), R=<timeout> (5.0)\n"
    "\n"
    "    telnet(s)://<host>:<port>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]] (RW) -- Telnet/RFC2217 client (opt. TLS)\n"
    "\n"
    "        <host> address (<IPv4> or [<IPv6>]) or hostname, <port> port number, <baudrate> in [bps]\n"
    "        (default: 115200), <autobaud> is one of 'none' (default), 'passive', 'ubx', 'fp' or 'auto',\n"
    "        <mode> '8N1' (default, no other modes are currently supported), <flow> is 'off' (default),\n"
    "        'sw' or 'hw'\n"
    "        <option>s (default): C=<timeout> (10.0), A=<timeout> (10.0), R=<timeout> (5.0)\n"
    "\n"
    "    ntripsvr(s)://<credentials>@<host>:<port>/<mountpoint>[:<version>] (WO) -- NTRIP server (opt. TLS)\n"
    "\n"
    "        <credentials> is <password> for v1, <username>:<password> for v2,\n"
    "        =<base64_encoded_credentials> or %<path> to read either from a file, <host> address (<IPv4>\n"
    "        or [<IPv6>]) or hostname, <port> port number <mountpoint> name of the caster mountpoint,\n"
    "        <version> NTRIP version 'v1' (default) or 'v2'\n"
    "        <option>s (default): C=<timeout> (10.0), R=<timeout> (5.0)\n"
    "\n"
    "    tcpsvr://[<host>]:<port> (RW) -- TCP server\n"
    "\n"
    "        <host> address (<IPv4> or [<IPv6>]) or hostname (bind to all interfaces if empty),\n"
    "        <port> port number. This stream accepts a maximum of 20 clients.\n"
    "\n"
    "    udpcli://<host>:<port> (WO) -- UDP client\n"
    "\n"
    "        <host> address (<IPv4> or [<IPv6>]) or hostname, <port> port number. This stream is not\n"
    "        able to distinguish different clients (sources) and may mangle data if multiple clients\n"
    "        send data at the same time.\n"
    "\n"
    "    udpsvr://[<host>]:<port> (RO) -- UDP server\n"
    "\n"
    "        <host> address (<IPv4> or [<IPv6>]) or hostname (bind to all interfaces if empty),\n"
    "        <port> port number.\n"
    "\n"
    "    spidev://<device>[:<speed>[:<bpw>[:<xfersize>[:<mode>]]]] (RW) -- Linux spidev (master)\n"
    "\n"
    "        <device> device path (e.g. '/dev/spidev0.3'), <speed> [Hz] (default: 1000000), <bpw> 8, 16\n"
    "        or 32 (default) bits per word, <xfersize> [bytes] (64-2048 and multiple of 4, default 64),\n"
    "        <mode> SPI mode (flags from linux/spi/spi.h, default 0x00000000)\n"
    "        This assumes that the device ignores all-0xff on input and sends all-0xff to indicate\n"
    "        no data.\n"
    "\n"
    "    canstr://<dev>:<canid_in>:<canid_out>[:<ff>[:<fd>[:<brs>]]] (RW) -- SocketCAN stream\n"
    "\n"
    "        <dev> interface device (e.g. 'can0'), <canid_out> / <canid_in>  CAN ID for outgoing\n"
    "        (write) / incoming (read) frames (0x001-0x7ff for SFF, 0x00000001-0x1fffffff for EFF),\n"
    "        <ff> frame format ('sff' or 'eff'), <fd> 'fd' for CAN FD or '' for classical CAN,\n"
    "        <brs> 'brs' or '' for CAN FD bitrate switch (only with <fd> = 'fd'). Note that CAN\n"
    "        interface (bitrates etc.) must be configured appropriately, e.g. using 'ip link'.\n"
    "\n"
    "    gga://<lat>/<lon>/<height>[[:<interval>]:<talker>] (RO) -- NMEA GGA generator\n"
    "\n"
    "        <lat> latitude [deg], <lon> longitude [deg], <height> height [m], <interval> output\n"
    "        interval in [s] (1.0 - 86400.0 s, default: 5.0), <talker> NMEA talker ID (default 'GN')\n"
    "\n"
    "    sta://<x>/<y>/<z>[[[:<interval>]:<sta>]:<type>] (RO) -- RTCM3 station message generator\n"
    "\n"
    "        <x>/<y>/<z> ECEF coordinates [m], <interval> output interval in [s] (1.0 - 86400.0 s,\n"
    "        default: 5.0), <sta> station ID (default 0), <type> message type (default 1005, 1006 or\n"
    "        1032).\n"
    "        DF022, DF023, DF024, DF142 are set to 1, DF021, DF141, DF364 and DF028 are set to 0\n"
    "\n"
    "    loop://[<delay>][:<rate>] (RW) -- Loopback (echo)\n"
    "\n"
    "        Delay echoed data my <delay> [ms] (default 0) or limit rate of echoed data to\n"
    "        <bytes_per_sec> bytes per second (0 to disable rate limiting, default 0)\n"
    "\n"
    "    fileout://<file>[:<swap>[:<ts>]] (WO) -- File writer\n"
    "\n"
    "        <file> file path with optional placeholders for UTC  '%Y' (year, e.g. 2024),\n"
    "        '%m' (month, 01-12) '%d' (day, 01-31), '%h' (hour, 00-23), '%M' (minute, 00-59),\n"
    "        '%S' (second, 00-60), '%j' (day of year, 001-366), '%W' (GPS week number, e.g. 1234),\n"
    "        '%w' (day of GPS week, 0-6), '%s' (GPS time of week [s], 0-604799), optional <swap> file\n"
    "        swap time [s] (60-86400, negative value for unaligned timestamps, default: '', that is,\n"
    "        no swap, <ts> store index sidecar file for replay ('ts') (default '', i.e. no sidecar file)\n"
    "\n"
    "    filein://<file>[:<speed>[:<offset>]] (RO) -- File read\n"
    "\n"
    "        <file> file path, <speed> replay speed (default 0.0, that is, ignore .ts file), <offset>\n"
    "        replay offset [s] (default: 0.0)\n"
    "\n"
    "    exec://<path>[[:<arg>]...] (RW) -- External program stdin/stdout\n"
    "\n"
    "        <path> to executable, <arg> optional argument(s)\n"
    "        <option>s (default): A=<timeout> (0.0), R=<timeout> (0.0)\n"
    "\n"
    "    ipcsvr://<name> (RW) -- Interprocess stream (server)\n"
    "\n"
    "        <name> unique name for the connection\n"
    "\n"
    "    ipccli://<name> (RW) -- Interprocess stream (client)\n"
    "\n"
    "        <name> unique name for the connection\n"
    "        <option>s (default): R=<timeout> (5.0), H=off|on (on)\n"
    "\n"
    "The <option>s are (not all streams support all options):\n"
    "\n"
    "- N=<name>     -- A short and concise name for the stream ([a-zA-Z0-9_)]\n"
    "- RO           -- Make a RW stream read-only (input only), that is, ignore any writes (output)\n"
    "- WO           -- Make a RW stream write-only (output only), that is, ignore any reads (input)\n"
    "- C=<timeout>  -- Connect timeout [s] (1.0-3600.0, 0.0 to disable)\n"
    "- A=<timeout>  -- Read (and only read!) inactivity timeout [s] (1.0-3600.0, 0.0 to disable)\n"
    "- R=<timeout>  -- Retry timeout [s] (2.0-3600.0)\n"
    "- H=off|on     -- Initialise on start ('off') or allow delayed initialisation ('on'). Useful for\n"
    "                  hot-pluggable devices. Use with R=<timeout>.\n"
    "\n"
    "Secure client streams (tcpclis://, etc.) can use TLS 1.2 or 1.3. To use server authentication the\n"
    "corresponding certificate must be available. They are loaded from the path or file given by the\n"
    "FFXX_STREAM_TLS_FILES_PATH environment variable. See the SSL_CTX_load_verify_locations(3ssl) man\n"
    "page for details. The certificate must match the used hostname or address. See X509_check_host(3ssl)\n"
    "man page for details.\n"
    // @fp_codegen_end{FFXX_STREAM_HELP}
    ;  // clang-format on
}

/* ****************************************************************************************************************** */

// Info on a stream type
struct StreamTypeInfo
{  // clang-format off
    StreamType    type_;      // Stream type
    const char*   scheme_;    // Scheme name
    StreamMode    mode_;      // Default (and supported) modes
    double        conn_to_;   // Default connect timeout [s], < 0.0 if not supported
    double        inact_to_;  // Default inactivity timeout [s], < 0.0 if not supported
    double        retry_to_;  // Default timeout for retry [s], < 0.0 if not supported
    int           hotplug_;   // Hotplug default on (1) or off (0), resp. not supported (-1)
};

// clang-format off
// @fp_codegen_begin{FFXX_STREAM_TYPE_INFO}
static constexpr std::array<StreamTypeInfo, 22> kStreamTypeInfos = {{
    { StreamType::SERIAL,            "serial",       StreamMode::RW,      -1.0,   0.0,   5.0,   0, },
    { StreamType::TCPCLI,            "tcpcli",       StreamMode::RW,      10.0,   0.0,   0.0,  -1, },
    { StreamType::TCPCLIS,           "tcpclis",      StreamMode::RW,      10.0,   0.0,   0.0,  -1, },
    { StreamType::NTRIPCLI,          "ntripcli",     StreamMode::RW,      10.0,  10.0,   5.0,  -1, },
    { StreamType::NTRIPCLIS,         "ntripcliS",    StreamMode::RW,      10.0,  10.0,   5.0,  -1, },
    { StreamType::TELNET,            "telnet",       StreamMode::RW,      10.0,  10.0,   5.0,  -1, },
    { StreamType::TELNETS,           "telnets",      StreamMode::RW,      10.0,  10.0,   5.0,  -1, },
    { StreamType::NTRIPSVR,          "ntripsvr",     StreamMode::WO,      10.0,  -1.0,   5.0,  -1, },
    { StreamType::NTRIPSVRS,         "ntripsvrs",    StreamMode::WO,      10.0,  -1.0,   5.0,  -1, },
    { StreamType::TCPSVR,            "tcpsvr",       StreamMode::RW,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::UDPCLI,            "udpcli",       StreamMode::WO,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::UDPSVR,            "udpsvr",       StreamMode::RO,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::SPIDEV,            "spidev",       StreamMode::RW,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::CANSTR,            "canstr",       StreamMode::RW,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::GGA,               "gga",          StreamMode::RO,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::STA,               "sta",          StreamMode::RO,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::LOOP,              "loop",         StreamMode::RW,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::FILEOUT,           "fileout",      StreamMode::WO,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::FILEIN,            "filein",       StreamMode::RO,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::EXEC,              "exec",         StreamMode::RW,      -1.0,   0.0,   0.0,  -1, },
    { StreamType::IPCSVR,            "ipcsvr",       StreamMode::RW,      -1.0,  -1.0,  -1.0,  -1, },
    { StreamType::IPCCLI,            "ipccli",       StreamMode::RW,      -1.0,  -1.0,   5.0,   1, },
}};
// @fp_codegen_end{FFXX_STREAM_TYPE_INFO}
   // clang-format on

/* ****************************************************************************************************************** */

/*static*/ constexpr double StreamOpts::INACT_TO_MIN;
/*static*/ constexpr double StreamOpts::INACT_TO_MAX;
/*static*/ constexpr double StreamOpts::RETRY_TO_MIN;
/*static*/ constexpr double StreamOpts::RETRY_TO_MAX;
/*static*/ constexpr std::size_t StreamOpts::R_QUEUE_SIZE_MIN;
/*static*/ constexpr std::size_t StreamOpts::W_QUEUE_SIZE_MIN;
/*static*/ constexpr std::size_t StreamOpts::TX_BUF_SIZE_MIN;
/*static*/ constexpr std::size_t StreamOpts::RX_BUF_SIZE_MIN;
/*static*/ constexpr std::size_t StreamOpts::MAX_CLIENTS_MIN;
/*static*/ constexpr const char* StreamOpts::TLS_FILES_PATH_ENV;
/*static*/ constexpr std::array<uint32_t, 8> StreamOpts::BAUDRATES;

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ StreamOptsPtr StreamOpts::FromSpec(const std::string& spec, std::string& e_msg)
{
    // Spec is "<scheme>://<path>:param1:param2:...,option1=value1,option2=value2..."
    std::string scheme;
    std::string path;
    std::vector<std::string> options;
    while (true) {
        // Split scheme+path and options, we need at least the scheme+path
        const std::vector<std::string> parts = string::StrSplit(spec, ",");
        if (parts.size() < 1) {
            break;
        }

        // Split scheme and path
        const std::vector<std::string> scheme_path = string::StrSplit(parts[0], "://");
        if (scheme_path.size() != 2) {
            break;
        }

        scheme = scheme_path[0];
        path = scheme_path[1];
        if (parts.size() > 0) {
            options = { parts.begin() + 1, parts.end() };
        }
        break;
    }
    DEBUG("spec=[%s] -> scheme=[%s] path=[%s] options=[%s]", spec.c_str(), scheme.c_str(), path.c_str(),
        string::StrJoin(options, "][").c_str());
    if (scheme.empty() || (path.size() > StreamOpts::MAX_PATH_LEN)) {
        e_msg = "Bad stream spec";
        return nullptr;
    }

    // Lookup scheme info
    auto info = std::find_if(kStreamTypeInfos.cbegin(), kStreamTypeInfos.cend(),
        [&scheme](const StreamTypeInfo& cand) { return scheme == cand.scheme_; });
    if (info == kStreamTypeInfos.cend()) {
        e_msg = "Bad stream spec (scheme)";
        return nullptr;
    }

    StreamOptsPtr stream_opts;

    // Check path and set stream-specific options
    std::vector<std::string> path_errors;

    switch (info->type_) {  // clang-format off
        case StreamType::UNSPECIFIED: break;
        case StreamType::SERIAL:    stream_opts =     StreamOptsSerial::FromPath(path, path_errors);              break;
        case StreamType::TCPCLI :   /* FALLTHROUGH */
        case StreamType::TCPCLIS:   stream_opts =     StreamOptsTcpcli::FromPath(path, path_errors, info->type_); break;
        case StreamType::NTRIPCLI:  /* FALLTHROUGH */
        case StreamType::NTRIPSVR:  /* FALLTHROUGH */
        case StreamType::NTRIPCLIS: /* FALLTHROUGH */
        case StreamType::NTRIPSVRS: stream_opts =   StreamOptsNtripcli::FromPath(path, path_errors, info->type_); break;
        case StreamType::TELNET :   /* FALLTHROUGH */
        case StreamType::TELNETS:   stream_opts =     StreamOptsTelnet::FromPath(path, path_errors, info->type_); break;
        case StreamType::TCPSVR:    stream_opts =     StreamOptsTcpsvr::FromPath(path, path_errors);              break;
        case StreamType::UDPCLI:    stream_opts =     StreamOptsUdpcli::FromPath(path, path_errors);              break;
        case StreamType::UDPSVR:    stream_opts =     StreamOptsUdpsvr::FromPath(path, path_errors);              break;
        case StreamType::SPIDEV:    stream_opts =     StreamOptsSpidev::FromPath(path, path_errors);              break;
        case StreamType::CANSTR:    stream_opts =     StreamOptsCanstr::FromPath(path, path_errors);              break;
        case StreamType::GGA:       stream_opts =        StreamOptsGga::FromPath(path, path_errors);              break;
        case StreamType::STA:       stream_opts =        StreamOptsSta::FromPath(path, path_errors);              break;
        case StreamType::LOOP:      stream_opts =       StreamOptsLoop::FromPath(path, path_errors);              break;
        case StreamType::FILEOUT:   stream_opts =    StreamOptsFileout::FromPath(path, path_errors);              break;
        case StreamType::FILEIN:    stream_opts =     StreamOptsFilein::FromPath(path, path_errors);              break;
        case StreamType::EXEC:      stream_opts =       StreamOptsExec::FromPath(path, path_errors);              break;
        case StreamType::IPCCLI:    /* FALLTHROUGH */
        case StreamType::IPCSVR:    stream_opts =        StreamOptsIpc::FromPath(path, path_errors, info->type_); break;
    }  // clang-format on
    if (!stream_opts) {
        e_msg = string::Sprintf("Bad stream spec (%s)", string::StrJoin(path_errors, ", ").c_str());
        return nullptr;
    }

    // Check and set common options
    // - Scheme info
    stream_opts->type_ = info->type_;
    stream_opts->mode_ = info->mode_;
    // - Stream defaults
    if (info->conn_to_ > 0.0) {
        stream_opts->conn_to_ = std::chrono::milliseconds(static_cast<int64_t>(std::floor(info->conn_to_ * 1e3)));
    }
    if (info->inact_to_ > 0.0) {
        stream_opts->inact_to_ = std::chrono::milliseconds(static_cast<int64_t>(std::floor(info->inact_to_ * 1e3)));
    }
    if (info->retry_to_ > 0.0) {
        stream_opts->retry_to_ = std::chrono::milliseconds(static_cast<int64_t>(std::floor(info->retry_to_ * 1e3)));
    }
    if (info->hotplug_ >= 0) {
        stream_opts->hotplug_ = (info->hotplug_ == 1);
    }
    // - TLS stuff
    const char* tls_verify_path = std::getenv(StreamOpts::TLS_FILES_PATH_ENV);
    if ((tls_verify_path != NULL) && (tls_verify_path[0] != '\0')) {
        stream_opts->tls_files_path_ = tls_verify_path;
        TRACE("tls_verify_path_=%s", tls_verify_path);
    }

    // Check options
    std::vector<std::string> option_errors;
    for (auto& option : options) {
        const std::vector<std::string> key_val = string::StrSplit(option, "=");
        if (key_val.size() < 1) {
            option_errors.push_back("bad option " + option);
            continue;
        }
        const bool have_val = (key_val.size() == 2);
        bool option_ok = true;
        // Stream name, limit to 15 chars
        if (key_val[0] == "N") {
            if (have_val) {
                stream_opts->name_ = key_val[1].substr(0, std::max(key_val[1].size(), (std::size_t)15));
            } else {
                option_ok = false;
            }
        }
        // Timeout options are only available for some stream types
        else if (key_val[0] == "C") {
            double conn_to = 0.0;
            const bool supported = (info->conn_to_ > 0.0);
            const bool val_ok = (have_val && string::StrToValue(key_val[1], conn_to));
            const bool in_range = !((conn_to < CONN_TO_MIN) || (conn_to > CONN_TO_MAX));
            if (!supported || !val_ok || !in_range) {
                option_ok = false;
            } else {
                stream_opts->conn_to_ = std::chrono::milliseconds(static_cast<uint64_t>(std::floor(conn_to * 1e3)));
            }
        } else if (key_val[0] == "A") {
            double inact_to = 0.0;
            const bool supported = (info->inact_to_ >= 0.0);
            const bool val_ok = (have_val && string::StrToValue(key_val[1], inact_to));
            const bool is_zero = (inact_to == 0.0);
            const bool in_range = !((inact_to < INACT_TO_MIN) || (inact_to > INACT_TO_MAX));
            if (!supported || !val_ok || (!is_zero && !in_range)) {
                option_ok = false;
            } else {
                stream_opts->inact_to_ = std::chrono::milliseconds(static_cast<uint64_t>(std::floor(inact_to * 1e3)));
            }
        } else if (key_val[0] == "R") {
            double retry_to = 0.0;
            const bool supported = (info->retry_to_ >= 0.0);
            const bool val_ok = (have_val && string::StrToValue(key_val[1], retry_to));
            const bool is_zero = (retry_to == 0.0);
            const bool in_range = !((retry_to < RETRY_TO_MIN) || (retry_to > RETRY_TO_MAX));
            if (!supported || !val_ok || (!is_zero && !in_range)) {
                option_ok = false;
            } else {
                stream_opts->retry_to_ = std::chrono::milliseconds(static_cast<uint64_t>(std::floor(retry_to * 1e3)));
            }
        }
        // RO/WO, onkly for RW streams
        else if (key_val[0] == "RO") {
            if (!have_val && (info->mode_ == StreamMode::RW)) {
                stream_opts->mode_ = StreamMode::RO;
            } else {
                option_ok = false;
            }
        } else if (key_val[0] == "WO") {
            if (!have_val && (info->mode_ == StreamMode::RW)) {
                stream_opts->mode_ = StreamMode::WO;
            } else {
                option_ok = false;
            }
        }
        // Hotplug, delayed initialisation
        else if (key_val[0] == "H") {
            if (have_val && (info->hotplug_ >= 0) && (key_val[1] == "off")) {
                stream_opts->hotplug_ = false;
            } else if (have_val && (info->hotplug_ >= 0) && (key_val[1] == "on")) {
                stream_opts->hotplug_ = true;
            } else {
                option_ok = false;
            }
        }
        // Unknown option
        else {
            option_errors.push_back("unknown option " + key_val[0]);
            option_ok = true;  // not really..
        }
        if (!option_ok) {
            option_errors.push_back("bad value for option " + key_val[0]);
        }
    }
    if (!option_errors.empty()) {
        e_msg = string::Sprintf("Bad stream spec (%s)", string::StrJoin(option_errors, ", ").c_str());
        return nullptr;
    }

    // Make a stream name if none given
    static int str_nr = 0;
    str_nr++;  // count all streams
    if (stream_opts->name_.empty()) {
        stream_opts->name_ = "str" + std::to_string(str_nr);
    }

    // Full, canonical spec for this stream
    stream_opts->UpdateSpec();

    e_msg.clear();
    return stream_opts;
}

void StreamOpts::UpdateSpec()
{
    auto info = std::find_if(kStreamTypeInfos.cbegin(), kStreamTypeInfos.cend(),
        [&](const StreamTypeInfo& cand) { return type_ == cand.type_; });
    if (info != kStreamTypeInfos.cend()) {
        spec_ = std::string(info->scheme_) + "://" + path_;
        opts_ += ",N=" + name_;
        if (info->mode_ != mode_) {
            opts_ += ",";
            opts_ += StreamModeStr(mode_);
        }
        if (info->conn_to_ >= 0.0) {
            opts_ += string::Sprintf(",C=%.1f", (double)conn_to_.count() * 1e-3);
        }
        if (info->inact_to_ >= 0.0) {
            opts_ += string::Sprintf(",A=%.1f", (double)inact_to_.count() * 1e-3);
        }
        if (info->retry_to_ >= 0.0) {
            opts_ += string::Sprintf(",R=%.1f", (double)retry_to_.count() * 1e-3);
        }
        if (info->hotplug_ >= 0) {
            opts_ += (hotplug_ > 0 ? ",H=on" : ",H=off");
        }
        spec_ += opts_;
    }
    if (disp_.empty()) {
        disp_ = path_;
    }
}

/* ****************************************************************************************************************** */

/*static*/ StreamPtr Stream::FromSpec(const std::string& spec)
{
    std::string e_msg;
    auto stream_opts = StreamOpts::FromSpec(spec, e_msg);
    StreamPtr stream = Stream::FromOpts(stream_opts);
    if (!e_msg.empty()) {
        WARNING("%s: %s", e_msg.c_str(), spec.c_str());
    }
    return stream;
}

/*static*/ StreamPtr Stream::FromOpts(StreamOptsPtr& opts)
{
    if (!opts) {
        return nullptr;
    }
    StreamPtr stream;
    switch (opts->type_) {  // clang-format off
        case StreamType::UNSPECIFIED: break;
        case StreamType::SERIAL:    stream = std::make_unique<StreamSerial     >( dynamic_cast<const StreamOptsSerial&     >(*opts ) ); break;
        case StreamType::TCPCLI:    /* FALLTHROUGH */
        case StreamType::TCPCLIS:   stream = std::make_unique<StreamTcpcli     >( dynamic_cast<const StreamOptsTcpcli&     >(*opts ) ); break;
        case StreamType::NTRIPCLI:  /* FALLTHROUGH */
        case StreamType::NTRIPSVR:  /* FALLTHROUGH */
        case StreamType::NTRIPCLIS: /* FALLTHROUGH */
        case StreamType::NTRIPSVRS: stream = std::make_unique<StreamNtripcli   >( dynamic_cast<const StreamOptsNtripcli&   >(*opts ) ); break;
        case StreamType::TELNET:    /* FALLTHROUGH */
        case StreamType::TELNETS:   stream = std::make_unique<StreamTelnet     >( dynamic_cast<const StreamOptsTelnet&     >(*opts ) ); break;
        case StreamType::TCPSVR:    stream = std::make_unique<StreamTcpsvr     >( dynamic_cast<const StreamOptsTcpsvr&     >(*opts ) ); break;
        case StreamType::UDPCLI:    stream = std::make_unique<StreamUdpcli     >( dynamic_cast<const StreamOptsUdpcli&     >(*opts ) ); break;
        case StreamType::UDPSVR:    stream = std::make_unique<StreamUdpsvr     >( dynamic_cast<const StreamOptsUdpsvr&     >(*opts ) ); break;
        case StreamType::SPIDEV:    stream = std::make_unique<StreamSpidev     >( dynamic_cast<const StreamOptsSpidev&     >(*opts ) ); break;
        case StreamType::CANSTR:    stream = std::make_unique<StreamCanstr     >( dynamic_cast<const StreamOptsCanstr&     >(*opts ) ); break;
        case StreamType::GGA:       stream = std::make_unique<StreamGga        >( dynamic_cast<const StreamOptsGga&        >(*opts ) ); break;
        case StreamType::STA:       stream = std::make_unique<StreamSta        >( dynamic_cast<const StreamOptsSta&        >(*opts ) ); break;
        case StreamType::LOOP:      stream = std::make_unique<StreamLoop       >( dynamic_cast<const StreamOptsLoop&       >(*opts ) ); break;
        case StreamType::FILEIN:    stream = std::make_unique<StreamFilein     >( dynamic_cast<const StreamOptsFilein&     >(*opts ) ); break;
        case StreamType::FILEOUT:   stream = std::make_unique<StreamFileout    >( dynamic_cast<const StreamOptsFileout&    >(*opts ) ); break;
        case StreamType::EXEC:      stream = std::make_unique<StreamExec       >( dynamic_cast<const StreamOptsExec&       >(*opts ) ); break;
        case StreamType::IPCSVR:    /* FALLTHROUGH */
        case StreamType::IPCCLI:    stream = std::make_unique<StreamIpc        >( dynamic_cast<const StreamOptsIpc&        >(*opts ) ); break;

    }  // clang-format on

    return stream;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
