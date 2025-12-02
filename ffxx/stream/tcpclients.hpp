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
 * @brief Stream TCP clients (plain, ntrip or telnet, with or without TLS)
 */
#ifndef __FFXX_STREAM_TCPCLIENTS_HPP__
#define __FFXX_STREAM_TCPCLIENTS_HPP__

/* LIBC/STL */
#include <array>
#include <cstdint>
#include <memory>

/* EXTERNAL */
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <fpsdk_common/thread.hpp>

/* PACKAGE */
#include "autobauder.hpp"
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// Common stream options for all TCP clients
struct StreamOptsTcpClientsBase : public StreamOpts
{
    // clang-format off
    // Common options for all tcp clients
    std::string    host_;                            // Host
    bool           ipv6_      = false;               // host_ is IPv6
    uint16_t       port_      = 0;                   // Port
    bool           tls_       = false;               // Use TLS?

    // clang-format on
};

// TCP client stream implementation
class StreamTcpClientsBase : public StreamBase
{
   public:
    StreamTcpClientsBase(const StreamOptsTcpClientsBase& opts_init, const StreamOptsTcpClientsBase& opts_final);
    ~StreamTcpClientsBase();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   protected:
    const StreamOptsTcpClientsBase& opts_;

    // clang-format off

    // Common
    boost::asio::io_context                      ctx_;
    boost::asio::ip::tcp::resolver               resolver_;
    boost::asio::ip::tcp::resolver::results_type resolve_results_;
    std::string                                  connect_errors_;
    boost::asio::ip::tcp::endpoint               endpoint_;
    boost::asio::steady_timer                    timer_;
    bool                                         socket_conn_;

    // Raw socket
    boost::asio::ip::tcp::socket                 socket_raw_;

    // TLS socket
    using SslSocket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
    std::unique_ptr<SslSocket>                   socket_ssl_; // ptr because otherwise fail compile SetupTls() in some versions of boost
    boost::asio::ssl::context                    ssl_;
    bool                                         verify_peer_;
    std::string                                  cert_debug_;

    // Send / receive
    uint8_t                                      rx_buf_data_[parser::MAX_ADD_SIZE];
    boost::asio::mutable_buffer                  rx_buf_;
    uint8_t                                      tx_buf_data_[16 * 1024];
    std::size_t                                  tx_buf_size_;
    std::size_t                                  tx_buf_sent_;
    // clang-format on

    thread::Thread thread_;
    bool Worker();

    boost::asio::ip::tcp::socket& RawSocket();
    void ConfigSocket();
    void CloseSocket(const bool shutdown = true);
    bool SetupTls();

    // The general procedure is as follows:
    //
    // 1. resolve       StreamTcpClientsBase  Find endpoint(s)
    // 2. connect       StreamTcpClientsBase  Connect to endpoint (try all, until one succeeds)
    // 3. handshake     StreamTcpClientsBase  TLS handshake, no-op if no TLS is used
    // 4. request       StreamNtripCli        NTRIP "login", no-op for other streams
    //                  StreamTelnet          Telnet with RFC2217
    //    response      StreamNtripCli        Process response to request
    // 5. connected     StreamTcpClientsBase  Stream is up, start read/write
    //

    void DoRetry();
    void OnRetry(const boost::system::error_code& ec);

    void DoResolve();
    void OnResolve(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);

    void DoConnect(boost::asio::ip::tcp::resolver::iterator resolve_iter);
    void OnConnect(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator resolve_iter);

    void DoHandshake();
    void OnHandshake(const boost::system::error_code& ec);
    bool HandleVerifyCertificate(bool preverified, boost::asio::ssl::verify_context& verify);

    virtual void DoRequest();
    virtual void HandleRequest(const boost::system::error_code& ec, std::size_t bytes_transferred);

    virtual void DoResponse();
    virtual void OnResponse(const boost::system::error_code& ec, std::size_t bytes_transferred);

    void DoConnected(const std::string& add_info = "");
    void DoConnTimeout();
    void OnConnTimeout(const boost::system::error_code& ec);

    void DoInactTimeout();
    void OnInactTimeout(const boost::system::error_code& ec);

    virtual void DoRead();
    virtual void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred);
    virtual std::size_t FilterRead(const std::size_t size);

    void DoWrite();
    void OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred);
    virtual std::size_t FilterWrite();

    bool ProcessWrite(const std::size_t size) final;
};

// ---------------------------------------------------------------------------------------------------------------------

// TCPCLI(S) stream options
struct StreamOptsTcpcli : public StreamOptsTcpClientsBase
{
    static std::unique_ptr<StreamOptsTcpcli> FromPath(
        const std::string& path, std::vector<std::string>& errors, const StreamType type);
};

class StreamTcpcli : public StreamTcpClientsBase
{
   public:
    StreamTcpcli(const StreamOptsTcpcli& opts);
    ~StreamTcpcli();

   private:
    StreamOptsTcpcli opts_;
};

// ---------------------------------------------------------------------------------------------------------------------

// NTRIPCLI(S) and NTRIPSVR(S) stream options
struct StreamOptsNtripcli : public StreamOptsTcpClientsBase
{
    static std::unique_ptr<StreamOptsNtripcli> FromPath(
        const std::string& path, std::vector<std::string>& errors, const StreamType type);

    // clang-format off
    enum class NtripVersion { AUTO, V1, V2 };
    std::string    credentials_;                     // <credentials>
    std::string    auth_base64_;                     // Authentication base64 encoded
    std::string    auth_plain_;                      // Authentication plain
    std::string    mountpoint_;                      // Mountpoint
    NtripVersion   version_   = NtripVersion::AUTO;  // NTRIP version
    // clang-format on
};

class StreamNtripcli : public StreamTcpClientsBase
{
   public:
    StreamNtripcli(const StreamOptsNtripcli& opts);
    ~StreamNtripcli();

   private:
    StreamOptsNtripcli opts_;
    std::string response_;

    void DoRequest() override final;
    void HandleRequest(const boost::system::error_code& ec, std::size_t bytes_transferred) override final;
    void DoResponse() override final;
    void OnResponse(const boost::system::error_code& ec, std::size_t bytes_transferred) override final;
};

// ---------------------------------------------------------------------------------------------------------------------

// TELNET(S) stream options
struct StreamOptsTelnet : public StreamOptsTcpClientsBase
{
    static std::unique_ptr<StreamOptsTelnet> FromPath(
        const std::string& path, std::vector<std::string>& errors, const StreamType type);

    // clang-format off
    uint32_t    baudrate_       = 0;                           // Baudrate
    SerialMode  serial_mode_    = SerialMode::UNSPECIFIED;  // Serial port mode
    SerialFlow  serial_flow_    = SerialFlow::UNSPECIFIED;  // Serial port flow control
    AutobaudMode autobaud_  = AutobaudMode::NONE;
    // clang-format on

    void UpdatePath();
};

// Telnet protocol characters (RFC854 -- Telnet Protocol Specification)
enum class TelnetCode : uint8_t
{  // clang-format off
    IAC    = 255, // Interpret as command
    SB     = 250, // Indicates that sub-negotiation parameters follow
    WILL   = 251, // Option code to indicate the desire to start doing something or to confirm that we started doing something
    WONT   = 252, // Option code to indicate the desire to stop doing something or to confirm that we stopped doing something
    DO     = 253, // Option code to indicate the demand that the other party starts doing something
    DONT   = 254, // Option code to indicate the demand that the other party stops doing something

    SE     = 240, // End of sub-negotiation parameters
    NOP    = 241, // No operation
    DM     = 242, // Data mark
    BRK    = 243, // Break
    IP     = 244, // Interrupt process
    AO     = 245, // Abort output
    AYT    = 246, // Are you there
    EC     = 247, // Erase character
    EL     = 248, // Erase line
    GA     = 249, // Go ahead signal
};  // clang-format on

const char* TelnetCodeStr(const TelnetCode code);

// Options
enum class TelnetOption : uint8_t
{  // clang-format off
    TRANSMIT_BINARY     =  0, // RFC856  -- Telnet Binary Transmission
    ECHO_               =  1, // RFC857  -- Telnet Echo Option
    SUPPRESS_GO_AHEAD   =  3, // RFC858  -- Telnet Suppress Go Ahead Option
    COM_PORT_OPTION     = 44, // RFC2217 -- Telnet Com Port Control Option
    LINEMODE            = 34, // RFC1184 -- Telnet Linemode Option
};  // clang-format on

const char* TelnetOptionStr(const TelnetOption option);

// RFC2217 com port options
enum class CpoCommand : uint8_t
{  // clang-format off
    // Client to access server
    C2S_SIGNATURE             =   0, // + text
    C2S_SET_BAUDRATE          =   1, // + uint32_t in network order
    C2S_SET_DATASIZE          =   2, // + CpoDataSize
    C2S_SET_PARITY            =   3, // + CpoParity
    C2S_SET_STOPSIZE          =   4, // + CpoStopSize
    C2S_SET_CONTROL           =   5, // + CpoControl
    C2S_NOTIFY_LINESTATE      =   6, // + CpoLineState
    C2S_NOTIFY_MODEMSTATE     =   7, // + CpoModemState
    C2S_FLOWCONTROL_SUSPEND   =   8, // nothing
    C2S_FLOWCONTROL_RESUME    =   9, // nothing
    C2S_SET_LINESTATE_MASK    =  10, // + CpoLineState
    C2S_SET_MODEMSTATE_MASK   =  11, // + CpoModemState
    C2S_PURGE_DATA            =  12, // + CpoPurgeData
    // Access server to client
    S2C_SIGNATURE             = 100,
    S2C_SET_BAUDRATE          = 101,
    S2C_SET_DATASIZE          = 102,
    S2C_SET_PARITY            = 103,
    S2C_SET_STOPSIZE          = 104,
    S2C_SET_CONTROL           = 105,
    S2C_NOTIFY_LINESTATE      = 106,
    S2C_NOTIFY_MODEMSTATE     = 107,
    S2C_FLOWCONTROL_SUSPEND   = 108,
    S2C_FLOWCONTROL_RESUME    = 109,
    S2C_SET_LINESTATE_MASK    = 110,
    S2C_SET_MODEMSTATE_MASK   = 111,
    S2C_PURGE_DATA            = 112,
    // Dummy
    POLL_SIGNATURE = 200,
};  // clang-format on

const char* CpoCommandStr(const CpoCommand code);

enum class CpoDataSize : uint8_t
{  // clang-format off
    REQUEST   = 0,
    FIVE      = 5,
    SIX       = 6,
    SEVEN     = 7,
    EIGHT     = 8,
};  // clang-format on

enum class CpoParity : uint8_t
{  // clang-format off
    REQUEST   = 0,
    NONE      = 1,
    ODD       = 2,
    EVEN      = 3,
    MARK      = 4,
    SPACE     = 5,
};  // clang-format on

enum class CpoStopSize : uint8_t
{  // clang-format off
    REQUEST   = 0,
    ONE       = 1,
    TWO       = 2,
    ONEHALF   = 3,
};  // clang-format on

enum class CpoControl : uint8_t
{  // clang-format off
    REQUEST         =  0,
    NONE            =  1,
    XONOFF          =  2,
    HARDWARE        =  3,
    REQ_BREAK       =  4,
    BREAK_ON        =  5,
    BREAK_OFF       =  6,
    REQ_DTR         =  7,
    DTR_ON          =  8,
    DTR_OFF         =  9,
    REQ_RTS         = 10,
    RTS_ON          = 11,
    RTS_OFF         = 12,
    REQ_FLOW        = 13,
    FLOW_NONE       = 14,
    FLOW_XONOFF     = 15,
    FLOW_HARDWARE   = 16,
    FLOW_DCD        = 17,
    FLOW_DTR        = 18,
    FLOW_DSR        = 19,
};  // clang-format on

// LINESTATE bits
enum class CpoLineState : uint8_t
{  // clang-format off
    NONE             = 0x00,
    TIMEOUT_ERR      = 0x80,
    SHIFT_EMPTY      = 0x40,
    HOLDING_EMPTY    = 0x20,
    BREAK_ERROR      = 0x10,
    FRAMEING_ERROR   = 0x08,
    PARITY_ERROR     = 0x04,
    OVERRUN_ERROR    = 0x02,
    DTA_READY        = 0x01,
};  // clang-format on

// MODEMSTATE bits
enum class CpoModemState : uint8_t
{  // clang-format off
    NONE    = 0x00,
    RLSD    = 0x80,  // Receive Line Signal Detect (a.k.a. Carrier Detect)
    RING    = 0x40,  // Ring Indicator
    DSR     = 0x20,  // Data-Set-Ready Signal State
    CTS     = 0x10,  // Clear-To-Send Signal State
    DRLSD   = 0x08,  // Delta Receive Line Signal Detect
    TERI    = 0x04,  // Trailing-Edge Ring Detector
    DDSR    = 0x02,  // Delta Data-Set-Ready
    DCTS    = 0x01,  // Delta Clear-To-Send
};  // clang-format on

enum class CpoPurgeData : uint8_t
{  // clang-format off
    RX     = 1,
    TX     = 2,
    BOTH   = 3,
};  // clang-format on

class StreamTelnet : public StreamTcpClientsBase
{
   public:
    StreamTelnet(const StreamOptsTelnet& opts);
    ~StreamTelnet();

    uint32_t GetBaudrate() override final;
    bool SetBaudrate(const uint32_t baudrate) override final;
    bool Autobaud(const AutobaudMode mode) override final;

   private:
    StreamOptsTelnet opts_;

    // clang-format off
    enum class TelnetState { NORMAL, IACSEEN, NEGOTIATE, OPTION, OPTION_IACSEEN };
    enum OptionState { UNSPECIFIED, ACK, NAK };
    struct Option { TelnetCode code_; TelnetOption option_; OptionState state_; };
    std::vector<Option>       telnet_negotiate_;
    TelnetState               telnet_rx_state_ = TelnetState::NORMAL;
    TelnetState               telnet_tx_state_ = TelnetState::NORMAL;
    TelnetCode                telnet_code_;
    std::vector<uint8_t>      telnet_option_;
    // clang-format on

    friend Autobauder<StreamTelnet>;  // I have no better idea.. :-/
    Autobauder<StreamTelnet> autobauder_;

    void DoRequest() override final;
    void HandleRequest(const boost::system::error_code& ec, std::size_t bytes_transferred) override final;
    void DoResponse() override final;
    void OnResponse(const boost::system::error_code& ec, std::size_t bytes_transferred) override final;

    void ProcessOption(const TelnetCode code, const TelnetOption option);

    std::size_t FilterRead(const std::size_t size) override final;
    std::size_t FilterWrite() override final;

    bool SendComPortOption(const CpoCommand command, const uint32_t payload = 0);
};

/* ****************************************************************************************************************** */
}  // namespace stream
#endif  // __FFXX_STREAM_TCPCLIENTS_HPP__
