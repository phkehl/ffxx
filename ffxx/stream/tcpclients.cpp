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

/* LIBC/STL */
#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <iterator>
#include <regex>
#include <string>
#include <vector>

/* EXTERNAL */
#include <boost/asio/post.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/beast/http.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/predef/version_number.h>
#include <boost/version.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/path.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/types.hpp>
#include <fpsdk_common/utils.hpp>

/* PACKAGE */
#include "tcpclients.hpp"
#include "../utils.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

StreamTcpClientsBase::StreamTcpClientsBase(
    const StreamOptsTcpClientsBase& opts_init, const StreamOptsTcpClientsBase& opts_final) /* clang-format off */ :
    StreamBase(opts_init, opts_final),
    opts_            { opts_final },
    resolver_        { ctx_ },
    timer_           { ctx_ },
    socket_conn_     { false },
    socket_raw_      { ctx_ },
    ssl_             { boost::asio::ssl::context::tls },
    verify_peer_     { false },
    rx_buf_          { boost::asio::buffer(rx_buf_data_) },
    tx_buf_size_     { 0 },
    tx_buf_sent_     { 0 },
    thread_          { opts_init.name_, std::bind(&StreamTcpClientsBase::Worker, this) }  // clang-format on
{
}

StreamTcpClientsBase::~StreamTcpClientsBase()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamTcpClientsBase::Start()
{
    STREAM_TRACE("Start tls=%s", string::ToStr(opts_.tls_));
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }

    // Setup TLS once to handle bad tls_files_path_ early
    if (opts_.tls_ && !SetupTls()) {
        return false;
    }

    if (!thread_.Start()) {
        return false;
    }

    DoResolve();
    return true;
}

void StreamTcpClientsBase::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    if (socket_conn_) {
        StopWaitTxDone(timeout);
    }
    CloseSocket(socket_conn_);
    ctx_.stop();
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamTcpClientsBase::Worker()
{
    STREAM_TRACE("Worker");
    // Prevent run_for() below from returning early if there's no pending thing to handle. Otherwise we would busy-loop
    // below in case some stream implementation has states where nothing happens.
    auto work = boost::asio::make_work_guard(ctx_);
    while (!thread_.ShouldAbort()) {
        // Run i/o for a bit. The exact value of the time doesn't matter. It just needs to be small enough to make
        // ShouldAbort() "responsive" (and large enough to not busy wait)
        ctx_.run_for(std::chrono::milliseconds(337));
    }
    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

boost::asio::ip::tcp::socket& StreamTcpClientsBase::RawSocket()
{
    return opts_.tls_ ? socket_ssl_->next_layer() : socket_raw_;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::ConfigSocket()
{
    STREAM_TRACE("ConfigSocket tx_buf_size=%" PRIuMAX " rx_buf_size=%" PRIuMAX, opts_.tx_buf_size_, opts_.rx_buf_size_);
    boost::system::error_code ec;

    RawSocket().set_option(boost::asio::ip::tcp::no_delay(true), ec);
    if (ec) {
        STREAM_WARNING("no_delay fail: %s", ec.message().c_str());
    }
    // @todo good? bad? s.a. Stream{Tcp,Udp,Ntrip}{cli,svr}
    RawSocket().set_option(
        boost::asio::socket_base::send_buffer_size(std::min(opts_.TX_BUF_SIZE_MIN, opts_.tx_buf_size_)), ec);
    if (ec) {
        STREAM_WARNING("tx_buf_size_ fail: %s", ec.message().c_str());
    }
    RawSocket().set_option(
        boost::asio::socket_base::receive_buffer_size(std::min(opts_.RX_BUF_SIZE_MIN, opts_.rx_buf_size_)), ec);
    if (ec) {
        STREAM_WARNING("rx_buf_size_ fail: %s", ec.message().c_str());
    }

    socket_conn_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::CloseSocket(const bool shutdown)
{
    STREAM_TRACE("CloseSocket shutdown=%s", string::ToStr(shutdown));
    boost::system::error_code ec;

    // Cancel pending async_read()s (and any other operation)
    if (RawSocket().is_open()) {
        RawSocket().cancel(ec);
        if (ec) {
            STREAM_WARNING("socket cancel: %s", ec.message().c_str());
        }
    }

    // Actively shutdown connection
    if (shutdown) {
        RawSocket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec) {
            STREAM_WARNING("socket shutdown: %s", ec.message().c_str());
        }
    }

    // Close handle
    if (RawSocket().is_open()) {
        RawSocket().close(ec);
        if (ec) {
            STREAM_WARNING("socket close: %s", ec.message().c_str());
        }
    }

    socket_conn_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamTcpClientsBase::SetupTls()
{
    STREAM_TRACE("SetupTls tls_files_path=%s", opts_.tls_files_path_.c_str());
    // Setup a new context and socket, so that the full verification etc. is done on every connection attempt
    ssl_ = boost::asio::ssl::context(boost::asio::ssl::context::tls);
    socket_ssl_ = std::make_unique<SslSocket>(ctx_, ssl_);

    bool ok = true;

    // Only allow TLS 1.2 and 1.3 (see https://en.wikipedia.org/wiki/Transport_Layer_Security)
    ssl_.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                     boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::no_tlsv1 |
                     boost::asio::ssl::context::no_tlsv1_1);

    socket_ssl_->set_verify_callback(
        std::bind(&StreamTcpClientsBase::HandleVerifyCertificate, this, std::placeholders::_1, std::placeholders::_2));

    // Use server certificate if path is set
    if (!opts_.tls_files_path_.empty()) {
        if (!path::PathExists(opts_.tls_files_path_) || !path::PathIsReadable(opts_.tls_files_path_)) {
            STREAM_WARNING("Bad %s=%s", StreamOpts::TLS_FILES_PATH_ENV, opts_.tls_files_path_.c_str());
            return false;
        } else if (path::PathIsFile(opts_.tls_files_path_)) {
            boost::system::error_code ec;
            ssl_.load_verify_file(opts_.tls_files_path_.c_str(), ec);
            if (ec) {
                STREAM_WARNING("Failed loading %s: %s", opts_.tls_files_path_.c_str(), ec.message().c_str());
                ok = false;
            }
        } else {
            ssl_.add_verify_path(opts_.tls_files_path_.c_str());
        }
        STREAM_DEBUG("Peer verification enabled (%s)", opts_.tls_files_path_.c_str());
        socket_ssl_->set_verify_mode(boost::asio::ssl::verify_peer);
        verify_peer_ = true;
    } else {
        STREAM_WARNING("Peer verification disabled");
        socket_ssl_->set_verify_mode(boost::asio::ssl::verify_none);
        verify_peer_ = false;
    }

    // TODO: can this be helpful anywhere? https://github.com/djarek/certify

    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoRetry()
{
    STREAM_TRACE("DoRetry");
    resolver_.cancel();  // @todo Does this actually work? How to test it?
    timer_.cancel();
    if (opts_.retry_to_.count() > 0) {
        timer_.expires_after(opts_.retry_to_);
        timer_.async_wait(std::bind(&StreamTcpClientsBase::OnRetry, this, std::placeholders::_1));
    } else {
        SetStateClosed();
    }
}

void StreamTcpClientsBase::OnRetry(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnRetry %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Initiate a new cycle (resolve, connect, ...)
    else {
        DoResolve();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoResolve()
{
    STREAM_TRACE("DoResolve");

    // Resolve endpoints for given host and port
    SetStateConnecting("resolve " + HostPortStr(opts_.host_, opts_.port_, opts_.ipv6_));
    resolver_.async_resolve(opts_.host_, std::to_string(opts_.port_),
        // boost::asio::ip::resolver_query_base::flags::all_matching,
        std::bind(&StreamTcpClientsBase::OnResolve, this, std::placeholders::_1, std::placeholders::_2));

    DoConnTimeout();
}

void StreamTcpClientsBase::OnResolve(
    const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results)
{
    STREAM_TRACE("OnResolve %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Resolve failed, try again later
    else if (ec) {
        SetStateError(StreamError::RESOLVE_FAIL, ec.message());
        DoRetry();
    }

    // Resolve successful, try to connect (attempt all endpoints we got). For exmaple for "localhost" we may get ::1
    // (IPv6) and 127.0.0.1 (IPv4)
    else {
        for (auto& iter : results) {
            STREAM_DEBUG("Resolve %s -> %s %" PRIu16, HostPortStr(opts_.host_, opts_.port_, opts_.ipv6_).c_str(),
                iter.endpoint().address().to_string().c_str(), iter.endpoint().port());
        }

        // Let's connect...
        resolve_results_ = results;
        connect_errors_.clear();
        boost::asio::post(ctx_, std::bind(&StreamTcpClientsBase::DoConnect, this, resolve_results_.begin()));
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoConnect(boost::asio::ip::tcp::resolver::iterator resolve_iter)
{
    STREAM_TRACE("DoConnect");

    // Connect to endpoint
    endpoint_ = resolve_iter->endpoint();
    SetStateConnecting("attempting " + HostPortStr(endpoint_));

    // @todo prevent connecting to multicast addresses?

    // (Re-)setup TLS stuff
    if (opts_.tls_ && !SetupTls()) {
        SetStateError(StreamError::TLS_ERROR, "TLS setup fail");
        DoRetry();
        return;
    }

    RawSocket().async_connect(
        endpoint_, std::bind(&StreamTcpClientsBase::OnConnect, this, std::placeholders::_1, resolve_iter));
}

void StreamTcpClientsBase::OnConnect(
    const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator resolve_iter)
{
    STREAM_TRACE("OnConnect %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Connect failed
    else if (ec) {
        // Must close socket, or it cannot be used for the next attempt
        CloseSocket(false);
        connect_errors_ += ", " + ec.message() + " (" + HostPortStr(endpoint_) + ")";
        SetStateConnecting(HostPortStr(endpoint_) + " failed: " + ec.message());

        // We have another endpoint to try
        resolve_iter++;
        if (resolve_iter != resolve_results_.end()) {
            boost::asio::post(ctx_, std::bind(&StreamTcpClientsBase::DoConnect, this, resolve_iter));
        }
        // No more endpoints to try, retry again later
        else {
            SetStateError(StreamError::CONNECT_FAIL, connect_errors_.substr(2));
            DoRetry();
        }
    }
    // Connection successful
    else {
        ConfigSocket();
        // TLS connections must do handshake first
        if (opts_.tls_) {
            DoHandshake();
        }
        // Raw streams
        else {
            DoRequest();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoHandshake()
{
    STREAM_TRACE("DoHandshake");
    SetStateConnecting("handshake");
    socket_ssl_->async_handshake(boost::asio::ssl::stream_base::client,
        std::bind(&StreamTcpClientsBase::OnHandshake, this, std::placeholders::_1));
}

void StreamTcpClientsBase::OnHandshake(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnHandshake %s", ec.message().c_str());
    STREAM_DEBUG("Host cert: %s", cert_debug_.c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Handhake failed
    else if (ec) {
        CloseSocket();
        SetStateError(StreamError::TLS_ERROR, ec.message());
        DoRetry();
    }
    // Handshake successful
    else {
        DoRequest();
    }
}

bool StreamTcpClientsBase::HandleVerifyCertificate(bool preverified, boost::asio::ssl::verify_context& verify)
{
    STREAM_TRACE("HandleVerifyCertificate preverified=%s verify_peer=%s", string::ToStr(preverified),
        string::ToStr(verify_peer_));

    // Debugging
    X509* cert = X509_STORE_CTX_get_current_cert(verify.native_handle());
    char subject[256];
    X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject));
    char issuer[256];
    X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer));
    std::vector<std::string> alt_names;
    GENERAL_NAMES* ext = (GENERAL_NAMES*)X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
    if (ext != NULL) {
        for (int ix = 0; ix < sk_GENERAL_NAME_num(ext); ix++) {
            const GENERAL_NAME* gen = sk_GENERAL_NAME_value(ext, ix);
            switch (gen->type) {
                case GEN_DNS: {
                    alt_names.push_back(
                        std::string(gen->d.dNSName->data, gen->d.dNSName->data + gen->d.dNSName->length));
                    break;
                }
                case GEN_IPADD: {
                    if (gen->d.iPAddress->length == sizeof(boost::asio::ip::address_v4::bytes_type)) {
                        alt_names.push_back(boost::asio::ip::make_address_v4(
                            *((const boost::asio::ip::address_v4::bytes_type*)gen->d.iPAddress->data))
                                .to_string());
                    } else if (gen->d.iPAddress->length == sizeof(boost::asio::ip::address_v6::bytes_type)) {
                        alt_names.push_back(boost::asio::ip::make_address_v6(
                            *((const boost::asio::ip::address_v6::bytes_type*)gen->d.iPAddress->data))
                                .to_string());
                    }
                    break;
                }
            }
        }
        GENERAL_NAMES_free(ext);
    }

    cert_debug_ = string::Sprintf(
        "subject: %s, alt_names: %s, issuer: %s", subject, string::StrJoin(alt_names, ", ").c_str(), issuer);

    // Do hostname verification, see X509_check_host()
#if BOOST_VERSION >= BOOST_VERSION_NUMBER(1, 73, 0)
    const bool verified = boost::asio::ssl::host_name_verification(opts_.host_)(preverified, verify);
#else
    const bool verified = boost::asio::ssl::rfc2818_verification(opts_.host_)(preverified, verify);
#endif
    STREAM_TRACE("Host name verification %s", verified ? "successful" : "failed");

    // Return value propagates as error_code to OnHandshake()
    if (!verified && verify_peer_) {
        STREAM_WARNING("Host name verification failed (%s)", cert_debug_.c_str());
        return false;
    } else {
        return preverified;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoRequest()
{
    STREAM_TRACE("DoRequest");
    boost::asio::post(
        ctx_, std::bind(&StreamTcpClientsBase::HandleRequest, this, boost::system::error_code(), (std::size_t)0));
}

void StreamTcpClientsBase::HandleRequest(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
{
    STREAM_TRACE("HandleRequest %s", ec.message().c_str());
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Receiving response failed
    else if (ec) {
        CloseSocket(false);
        SetStateError(StreamError::CONN_LOST, "request: " + ec.message());
        DoRetry();
    }
    // Receiving response success
    else {
        DoResponse();
    }
}

void StreamTcpClientsBase::DoResponse()
{
    STREAM_TRACE("DoResponse");
    boost::asio::post(
        ctx_, std::bind(&StreamTcpClientsBase::OnResponse, this, boost::system::error_code(), (std::size_t)0));
}

void StreamTcpClientsBase::OnResponse(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
{
    STREAM_TRACE("OnResponse %s", ec.message().c_str());
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Receiving response failed
    else if (ec) {
        CloseSocket(false);
        SetStateError(StreamError::CONN_LOST, "response: " + ec.message());
        DoRetry();
    }
    // Receiving response success
    else {
        DoConnected();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoConnected(const std::string& add_info)
{
    STREAM_TRACE("DoConnected");
    timer_.cancel();
    SetStateConnected(HostPortStr(endpoint_) + (add_info.empty() ? "" : " " + add_info));
    DoRead();
    DoInactTimeout();
}

void StreamTcpClientsBase::DoConnTimeout()
{
    STREAM_TRACE("DoConnTimeout %" PRIiMAX, opts_.conn_to_.count());
    if (opts_.conn_to_.count() > 0) {
        timer_.cancel();
        timer_.expires_after(opts_.conn_to_);
        timer_.async_wait(std::bind(&StreamTcpClientsBase::OnConnTimeout, this, std::placeholders::_1));
    }
}

void StreamTcpClientsBase::OnConnTimeout(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnConnTimeout %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Inactivity timeout expired
    else {
        SetStateError(StreamError::CONNECT_TIMEOUT);
        CloseSocket(socket_conn_);
        DoRetry();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoInactTimeout()
{
    STREAM_TRACE("DoInactTimeout %" PRIiMAX, opts_.inact_to_.count());
    if (opts_.inact_to_.count() > 0) {
        timer_.cancel();
        timer_.expires_after(opts_.inact_to_);
        timer_.async_wait(std::bind(&StreamTcpClientsBase::OnInactTimeout, this, std::placeholders::_1));
    }
}

void StreamTcpClientsBase::OnInactTimeout(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnInactTimeout %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Inactivity timeout expired
    else {
        SetStateError(StreamError::NO_DATA_RECV);
        CloseSocket();
        DoRetry();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoRead()
{
    STREAM_TRACE("DoRead");
    if (opts_.tls_) {
        socket_ssl_->async_read_some(
            rx_buf_, std::bind(&StreamTcpClientsBase::OnRead, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        socket_raw_.async_read_some(
            rx_buf_, std::bind(&StreamTcpClientsBase::OnRead, this, std::placeholders::_1, std::placeholders::_2));
    }
}

void StreamTcpClientsBase::OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnRead %" PRIuMAX " %s", bytes_transferred, ec.message().c_str());
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // E.g. boost::asio::error::eof (= remote closed connection)
    else if (ec) {
        SetStateError(StreamError::CONN_LOST, "read " + ec.message());
        CloseSocket(false);
        DoRetry();
    }
    // Process incoming data, and read again
    else {
        ProcessRead(rx_buf_data_, FilterRead(bytes_transferred));
        DoRead();
        // Only if connected, as it uses the timer_, which is also used for {Do,Handle}ConnTimeout()
        if (GetState() == StreamState::CONNECTED) {
            DoInactTimeout();
        }
    }
}

std::size_t StreamTcpClientsBase::FilterRead(const std::size_t size)
{
    return size;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpClientsBase::DoWrite()
{
    // Get data from the write queue_ into the tx buffer and fire the asio write
    std::unique_lock<std::mutex> lock(mutex_);
    tx_buf_size_ = FilterWrite();
    tx_buf_sent_ = 0;
    if (tx_buf_size_ > 0) {
        STREAM_TRACE("DoWrite %" PRIuMAX, tx_buf_size_);
        if (opts_.tls_) {
            socket_ssl_->async_write_some(boost::asio::buffer(tx_buf_data_, tx_buf_size_),
                std::bind(&StreamTcpClientsBase::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
        } else {
            socket_raw_.async_write_some(boost::asio::buffer(tx_buf_data_, tx_buf_size_),
                std::bind(&StreamTcpClientsBase::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
        }
    } else {
        STREAM_TRACE("DoWrite done");
        tx_ongoing_ = false;
        NotifyTxDone();
    }
}

void StreamTcpClientsBase::OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnWrite %" PRIuMAX "/%" PRIuMAX " %s", bytes_transferred, tx_buf_size_, ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Error writing
    else if (ec) {
        SetStateError(StreamError::CONN_LOST, "write " + ec.message());
        CloseSocket(false);
        DoRetry();
    }
    // Write successful, check if there's more to write
    else {
        // Data was not fully sent, some is left in the tx buffer
        tx_buf_sent_ += bytes_transferred;
        if (tx_buf_sent_ < tx_buf_size_) {
            if (opts_.tls_) {
                socket_ssl_->async_write_some(
                    boost::asio::buffer(&tx_buf_data_[tx_buf_sent_], tx_buf_size_ - tx_buf_sent_),
                    std::bind(&StreamTcpClientsBase::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
            } else {
                socket_raw_.async_write_some(
                    boost::asio::buffer(&tx_buf_data_[tx_buf_sent_], tx_buf_size_ - tx_buf_sent_),
                    std::bind(&StreamTcpClientsBase::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
            }
        }
        // Everything sent, but check if there's new data to write
        else {
            tx_buf_size_ = 0;
            tx_buf_sent_ = 0;
            DoWrite();
        }
    }
}

std::size_t StreamTcpClientsBase::FilterWrite()
{
    const std::size_t size = std::min(write_queue_.Used(), sizeof(tx_buf_data_));
    if (size > 0) {
        write_queue_.Read(tx_buf_data_, size);
    }
    return size;
}

bool StreamTcpClientsBase::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);

    // Initiate the write in the io context (thread), unless a write is already ongoing
    if (!tx_ongoing_) {
        tx_ongoing_ = true;
        boost::asio::post(ctx_, std::bind(&StreamTcpClientsBase::DoWrite, this));
    }

    return true;
}

/* ****************************************************************************************************************** */

/*static*/ std::unique_ptr<StreamOptsTcpcli> StreamOptsTcpcli::FromPath(
    const std::string& path, std::vector<std::string>& errors, const StreamType type)
{
    StreamOptsTcpcli opts;
    bool ok = true;

    // "<host>:<port>"
    if (!MatchHostPortPath(path, opts.host_, opts.port_, opts.ipv6_)) {
        ok = false;
        errors.push_back("bad <host> or <port>");
    }
    opts.tls_ = (type == StreamType::TCPCLIS);

    opts.path_ = HostPortStr(opts.host_, opts.port_, opts.ipv6_);

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsTcpcli>(opts);
}

// ---------------------------------------------------------------------------------------------------------------------

StreamTcpcli::StreamTcpcli(const StreamOptsTcpcli& opts) /* clang-format off */ :
    StreamTcpClientsBase(opts, opts_),
    opts_   { opts }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamTcpcli::~StreamTcpcli()
{
}

/* ****************************************************************************************************************** */

// NTRIPCLI and NTRIPSVR
/*static*/ std::unique_ptr<StreamOptsNtripcli> StreamOptsNtripcli::FromPath(
    const std::string& path, std::vector<std::string>& errors, const StreamType type)
{
    StreamOptsNtripcli opts;
    bool ok = true;

    // client: "[<credentials>@]<host>:<port>/<mountpoint>"
    // server: "<credentials>@<host>:<port>/<mountpoint>"
    const std::regex path_re("^(?:(.+)@|)([^/]+)/([^:]+)(?::(auto|v1|v2)|)$");
    std::smatch m;
    if (std::regex_match(path, m, path_re) && (m.size() == 5)) {
        TRACE("match [%s] [%s] [%s] [%s] [%s]", m[0].str().c_str(), m[1].str().c_str(), m[2].str().c_str(),
            m[3].str().c_str(), m[4].str().c_str());
        if (!MatchHostPortPath(m[2], opts.host_, opts.port_, opts.ipv6_)) {
            ok = false;
            errors.push_back("bad <host> or <port>");
        }
        opts.tls_ = ((type == StreamType::NTRIPCLIS) || (type == StreamType::NTRIPSVRS));
        opts.mountpoint_ = m[3].str();

        opts.credentials_ = m[1].str();
        if (!opts.credentials_.empty()) {
            if (!CredentialsToAuth(opts.credentials_, opts.auth_plain_, opts.auth_base64_)) {
                errors.push_back("bad <credentials>");
            }
        }
        // Server always needs credentials
        else if (type == StreamType::NTRIPSVR) {
            errors.push_back("missing <credentials>");
        }

        DEBUG("credentials [%s] [%s] [%s]", opts.credentials_.c_str(), opts.auth_base64_.c_str(),
            opts.auth_plain_.c_str());  // Leaks secrets!

        // <version>
        if (type == StreamType::NTRIPSVR) {
            if (!m[4].str().empty()) {
                if (m[4].str() == "v1") {
                    opts.version_ = NtripVersion::V1;
                } else if (m[4].str() == "v2") {
                    opts.version_ = NtripVersion::V2;
                } else {
                    ok = false;
                    errors.push_back("bad <version>");
                }
            } else {
                opts.version_ = NtripVersion::V1;
            }
        } else {
            if (!m[4].str().empty()) {
                if (m[4].str() == "auto") {
                    opts.version_ = NtripVersion::AUTO;
                } else if (m[4].str() == "v1") {
                    opts.version_ = NtripVersion::V1;
                } else if (m[4].str() == "v2") {
                    opts.version_ = NtripVersion::V2;
                }
            }
        }
    } else {
        errors.push_back("bad path");
        ok = false;
    }
    const auto host_port = HostPortStr(opts.host_, opts.port_, opts.ipv6_);
    opts.path_ = opts.credentials_ + (opts.credentials_.empty() ? "" : "@") + host_port + "/" + opts.mountpoint_ + ":" +
                 (opts.version_ == NtripVersion::V1 ? "v1" : (opts.version_ == NtripVersion::V2 ? "v2" : "auto"));
    opts.disp_ = (opts.credentials_.empty() ? "" : "*****@") + host_port + "/" + opts.mountpoint_;

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsNtripcli>(opts);
}

// ---------------------------------------------------------------------------------------------------------------------

StreamNtripcli::StreamNtripcli(const StreamOptsNtripcli& opts) /* clang-format off */ :
    StreamTcpClientsBase(opts, opts_),
    opts_   { opts }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamNtripcli::~StreamNtripcli()
{
}

// ---------------------------------------------------------------------------------------------------------------------

// clang-format off
// References:
// - https://www.use-snip.com/kb/knowledge-base/ntrip-rev1-versus-rev2-formats/
// - NTRIP Client Devices / Best Practices (RTCM Paper 2023-SC104-1344)
// - RTCM STANDARD 10410.1 NETWORKED TRANSPORT OF RTCM via INTERNET PROTOCOL (Ntrip) - Version 2.0
//
// Client -> caster      v1                                              v2
//
//   >      GET /mountpoint HTTP/1.0\r\n                      GET /mountpoint HTTP/1.1\r\n
//   >     (Host: thecaster.com:2101\r\n)                     Host: thecaster.com:2101\r\n
//   >     (Ntrip-Version: Ntrip/2.0\r\n)                     Ntrip-Version: Ntrip/1.0\r\n
//   >      User-Agent: NTRIP software/version\r\n            User-Agent: NTRIP software/version\r\n
//   >      Accept: */*\r\n"                                  Accept: */*\r\n"
//   >      Connection: close\r\n                             Connection: close\r\n
//   >      Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n     Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n
//   >                                                       (Ntrip-GGA: $GPGGA....*XX\r\n)
//   >                                                       (Transfer-Encoding: chunked\r\n)
//   >      \r\n                                              \r\n
//
//   <      ICY 200 OK\r\n  (and nothing more!)               HTTP/1.1 200 OK\r\n
//   <      HTTP/1.0 401 Unauthorized\r\n                     HTTP/1.1 401 Unauthorized\r\n        and others HTTP error codes...
//   <                                                        Server: NTRIP software/version\r\n   and other HTTP response headers...
//   >      \r\n       (but not with ICY 200 OK!)             \r\n
//
// Server -> caster      v1                                              v2
//
//   >      SOURCE password /mountpoint\r\n                   POST /mountpoint HTTP/1.1
//   >                                                        Host: thecaster.com\r\n
//   >                                                        Ntrip-Version: Ntrip/1.0\r\n
//   >                                                        Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n
//   >      Source-Agent: NTRIP software/version\r\n          User-Agent: NTRIP software/version\r\n
//   >                                                        Connection: close\r\n
//   >      \r\n                                              \r\n
//
//   <      OK\r\n        (and nothing more!)                 HTTP/1.1 200 OK<CR><LF>
//   <      ERROR - Bad Password\r\n                          HTTP/1.1 401 Unauthorized\r\n        and others HTTP error codes...
//   <      ERROR - Bad Mountpoint\r\n                        Server: NTRIP software/version\r\n   and other HTTP response headers...
//   <      ERROR - Mount Point Invalid\r\n                  (Transfer-Encoding: chunked\r\n)
//   <      ERROR - ...?\r\n
//   <      \r\n          (but not with OK!)                  \r\n
//
// clang-format on

void StreamNtripcli::DoRequest()
{
    SetStateConnecting("request");

    // Build request
    using namespace boost::beast;
    std::string req_str;

    // NTRIP server v1 is special...
    if ((opts_.type_ == StreamType::NTRIPSVR) && (opts_.version_ == StreamOptsNtripcli::NtripVersion::V1)) {
        req_str =  // clang-format off
            "SOURCE " + opts_.auth_plain_ + " /" + opts_.mountpoint_ + "\r\n" +
            "Source-Agent: NTRIP " + GetUserAgentStr() + "\r\n" +
            "\r\n";  // clang-format on
    }
    // Client any, server v2
    else {
        http::request<http::empty_body> req;

        // - GET /mountpoint HTTP/1.x\r\n     SOURCE password /mountpoint\r\n    POST /mountpoint HTTP 1.1\r\n
        req.method(opts_.type_ == StreamType::NTRIPCLI ? http::verb::get : http::verb::post);
        const std::string target = "/" + opts_.mountpoint_;
        req.target(target);
        req.version(opts_.version_ == StreamOptsNtripcli::NtripVersion::V2 ? 11 : 10);

        // - (auto, v2) Host, Ntrip-Version
        if (opts_.version_ != StreamOptsNtripcli::NtripVersion::V1) {
            req.set(http::field::host, opts_.host_ + ":" + std::to_string(opts_.port_));
            req.set("Ntrip-Version", opts_.version_ == StreamOptsNtripcli::NtripVersion::V2 ? "2.0" : "1.0");
        }
        // - Authorization. Deliberately put before the following headers as they confuse some s#!7y NTRIP casters
        if (!opts_.auth_base64_.empty()) {
            req.set(http::field::authorization, "Basic " + opts_.auth_base64_);
        }
        // - User-Agent, Accept, Connection
        req.set(http::field::user_agent, "NTRIP " + GetUserAgentStr());
        if (opts_.type_ == StreamType::NTRIPCLI) {
            req.set(http::field::accept, "*/*");
        }
        req.set(http::field::connection, "close");
        // - (v2) has a Ntrip-GGA header, but also allows sending the GGA later. And we do not know the GGA here
        // anyways. if (opts_.version_ == StreamOptsNtripcli::NtripVersion::V2) {
        //     req.set("Ntrip-GGA", "$GPGGA....*XX");
        // }

        std::stringstream ss;
        ss << req;
        req_str = ss.str();
    }

    // Put request data into tx buffer
    tx_buf_size_ = req_str.size();
    std::memcpy(tx_buf_data_, req_str.data(), tx_buf_size_);

    // Send request
    STREAM_TRACE("DoRequest %" PRIuMAX, tx_buf_size_);
    TRACE_HEXDUMP(tx_buf_data_, tx_buf_size_, "    ", "request");
    if (opts_.tls_) {
        boost::asio::async_write(*socket_ssl_, boost::asio::buffer(tx_buf_data_, tx_buf_size_),
            std::bind(&StreamNtripcli::HandleRequest, this, std::placeholders::_1, (std::size_t)0));
    } else {
        boost::asio::async_write(socket_raw_, boost::asio::buffer(tx_buf_data_, tx_buf_size_),
            std::bind(&StreamNtripcli::HandleRequest, this, std::placeholders::_1, (std::size_t)0));
    }
}

void StreamNtripcli::HandleRequest(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
{
    STREAM_TRACE("HandleRequest %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Sending request failed
    else if (ec) {
        CloseSocket(false);
        SetStateError(StreamError::CONN_LOST, "request: " + ec.message());
        DoRetry();
    }
    // Sending request successful
    else {
        DoResponse();
    }
}

// We wait for the response by waiting for the \r\n delimitor. Most responses from the caster terminated HTTP style with
// \r\n\r\n, but not the v1 client "ICY 200 OK" and server "OK" or "ERROR something". This is mostly fine, as it surely
// gives us the first line of the response, which is what we need (besides the check for transfer-encoding). It seems
// that in practice we always (s) get the full response. We should use a match that checks for HTTP style response
// ending in \r\n\r\n as well as checking for the v1 responses. Also, we may get more data beyond the delimiter, which
// would be part of the data, which we should output from the stream. In practice it seems that this never (?) happens.
// And maybe using DynamicBuffer of std::string isn't quite save, too.
//
// @todo use boost::asio::async_read_until() with match_condition instead of delim
// @todo process response data beyond the delimiter
// @todo use std::vector<uint8_t> buffer

void StreamNtripcli::DoResponse()
{
    STREAM_TRACE("DoResponse");
    if (opts_.tls_) {
        boost::asio::async_read_until(*socket_ssl_, boost::asio::dynamic_buffer(response_, 1000), "\r\n",
            std::bind(&StreamNtripcli::OnResponse, this, std::placeholders::_1, (std::size_t)0));
    } else {
        boost::asio::async_read_until(socket_raw_, boost::asio::dynamic_buffer(response_, 1000), "\r\n",
            std::bind(&StreamNtripcli::OnResponse, this, std::placeholders::_1, (std::size_t)0));
    }
}

void StreamNtripcli::OnResponse(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
{
    STREAM_TRACE("OnResponse %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Receiving response failed
    else if (ec) {
        CloseSocket(false);
        SetStateError(StreamError::CONN_LOST, "response: " + ec.message());
        DoRetry();
    }
    // Receiving response success
    else {
        TRACE_HEXDUMP((const uint8_t*)response_.data(), response_.size(), "    ", "response");
        bool resp_ok = false;
        std::string error;
        using namespace fpsdk::common::string;

        if (StrStartsWith(response_, "ICY 200 OK") || StrStartsWith(response_, "OK") ||
            StrStartsWith(response_, "HTTP/1.0 200 OK") || StrStartsWith(response_, "HTTP/1.1 200 OK")) {
            resp_ok = true;
            if (StrContains(StrToLower(response_), "transfer-encoding")) {
                SetStateError(StreamError::BAD_MOUNTPOINT, "unsupported transfer-encoding");
                resp_ok = false;
            }
        } else {
            resp_ok = false;
            // Auth
            if (StrContains(response_, " 401 ") ||
                (StrStartsWith(response_, "ERROR") && StrContains(response_, /*P|p*/ "assword"))) {
                SetStateError(StreamError::AUTH_FAIL);
            }
            // Mountpoint
            else if (StrContains(response_, "SOURCETABLE") || StrContains(response_, " 404 ")) {
                SetStateError(StreamError::BAD_MOUNTPOINT);
            } else if (StrStartsWith(response_, "ERROR") && StrContains(response_, /*M|o*/ "ount")) {
                const auto parts = StrSplit(response_, "\r\n");
                SetStateError(StreamError::BAD_MOUNTPOINT, parts.size() > 0 ? parts[0] : "");
            }
            // Something else
            else {
                const auto parts = StrSplit(response_, "\r\n");
                SetStateError(StreamError::BAD_RESPONSE, parts.size() > 0 ? parts[0] : "");
            }
        }

        response_.clear();
        if (resp_ok) {
            DoConnected(opts_.mountpoint_);
        } else {
            DoRetry();
        }
    }
}

/* ****************************************************************************************************************** */

/*static*/ std::unique_ptr<StreamOptsTelnet> StreamOptsTelnet::FromPath(
    const std::string& path, std::vector<std::string>& errors, const StreamType type)
{
    StreamOptsTelnet opts;

    bool ok = true;

    // "<host>:<port>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]]"
    const auto parts = string::StrSplit(path, ":");
    if ((parts.size() >= 2) && (parts.size() <= 6) && !parts[0].empty()) {
        if (!MatchHostPortPath(parts[0] + ":" + parts[1], opts.host_, opts.port_, opts.ipv6_)) {
            ok = false;
            errors.push_back("bad <host> or <port>");
        }
        opts.tls_ = (type == StreamType::TELNETS);
        if (parts.size() > 2) {
            uint32_t baudrate = 0;
            if (string::StrToValue(parts[2], baudrate) &&
                (std::find(BAUDRATES.cbegin(), BAUDRATES.cend(), baudrate) != BAUDRATES.end())) {
                opts.baudrate_ = baudrate;
            } else {
                errors.push_back("bad <baudrate>");
                ok = false;
            }
        } else {
            opts.baudrate_ = 115200;
        }
        if (parts.size() > 3) {
            if (!StrToAutobaudMode(string::StrToUpper(parts[3]), opts.autobaud_)) {
                ok = false;
                errors.push_back("bad <autobaud>");
            }
        }
        if (parts.size() > 4) {
            // clang-format off
            if      (parts[4] == "8N1") { opts.serial_mode_ = SerialMode::_8N1; }
            // @todo all options: [7|8][N|E|O][1|1.5|2]
            else {
                ok = false;
                errors.push_back("bad <mode>");
            }
            // clang-format on
        } else {
            opts.serial_mode_ = SerialMode::_8N1;
        }
        if (parts.size() > 5) {
            // clang-format off
            if      (parts[5] == "off") { opts.serial_flow_ = SerialFlow::OFF; }
            else if (parts[5] == "sw")  { opts.serial_flow_ = SerialFlow::SW; }
            else if (parts[5] == "hw")  { opts.serial_flow_ = SerialFlow::HW; }
            else {
                ok = false;
                errors.push_back("bad <flow>");
            }
            // clang-format on
        } else {
            opts.serial_flow_ = SerialFlow::OFF;
        }
    } else {
        ok = false;
        errors.push_back("bad <device> or <baudrate>");
    }

    opts.UpdatePath();

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsTelnet>(opts);
}

void StreamOptsTelnet::UpdatePath()
{
    path_ = HostPortStr(host_, port_, ipv6_) + ":" + std::to_string(baudrate_) + ":" +
            string::StrToLower(AutobaudModeStr(autobaud_));
    switch (serial_mode_) {  // clang-format off
        case SerialMode::UNSPECIFIED: path_ += ":?";   break;
        case SerialMode::_8N1:        path_ += ":8N1"; break;
    }  // clang-format on
    switch (serial_flow_) {  // clang-format off
        case SerialFlow::UNSPECIFIED: path_ += ":?";   break;
        case SerialFlow::OFF:         path_ += ":off"; break;
        case SerialFlow::SW:          path_ += ":sw";  break;
        case SerialFlow::HW:          path_ += ":hw";  break;
    }  // clang-format on

    opts_.clear();
    disp_.clear();
    UpdateSpec();
}

// ---------------------------------------------------------------------------------------------------------------------

// The relevant specs
//
// - RFC854  -- Telnet Protocol Specification   (https://tools.ietf.org/html/rfc854.html)
// - RFC855  -- Telnet Option Specifications    (https://tools.ietf.org/html/rfc855.html)
// - RFC856  -- Telnet Binary Transmission      (https://tools.ietf.org/html/rfc856.html)
// - RFC857  -- Telnet Echo Option              (https://tools.ietf.org/html/rfc857.html)
// - RFC858  -- Telnet Suppress Go Ahead Option (https://tools.ietf.org/html/rfc858.html)
// - RFC1184 -- Telnet Linemode Option          (https://tools.ietf.org/html/rfc1184.html)
// - RFC2217 -- Telnet Com Port Control Option  (https://tools.ietf.org/html/rfc2217.html)

const char* TelnetCodeStr(const TelnetCode code)
{
    switch (code) {  // clang-format off
        case TelnetCode::IAC:  return "IAC";
        case TelnetCode::SB:   return "SB";
        case TelnetCode::WILL: return "WILL";
        case TelnetCode::WONT: return "WONT";
        case TelnetCode::DO:   return "DO";
        case TelnetCode::DONT: return "DONT";
        case TelnetCode::SE:   return "SE";
        case TelnetCode::NOP:  return "NOP";
        case TelnetCode::DM:   return "DM";
        case TelnetCode::BRK:  return "BRK";
        case TelnetCode::IP:   return "IP";
        case TelnetCode::AO:   return "AO";
        case TelnetCode::AYT:  return "AYT";
        case TelnetCode::EC:   return "EC";
        case TelnetCode::EL:   return "EL";
        case TelnetCode::GA:   return "GA";
    }  // clang-format on
    return "?";
}

const char* TelnetOptionStr(const TelnetOption option)
{
    switch (option) {  // clang-format off
        case TelnetOption::TRANSMIT_BINARY:   return "TRANSMIT_BINARY";
        case TelnetOption::ECHO_:             return "ECHO";
        case TelnetOption::SUPPRESS_GO_AHEAD: return "SUPPRESS_GO_AHEAD";
        case TelnetOption::COM_PORT_OPTION:   return "COM_PORT_OPTION";
        case TelnetOption::LINEMODE:          return "LINEMODE";
    }  // clang-format on
    return "?";
}

const char* CpoCommandStr(const CpoCommand command)
{
    switch (command) {  // clang-format off
        case CpoCommand::C2S_SIGNATURE:           return "C2S_SIGNATURE";
        case CpoCommand::C2S_SET_BAUDRATE:        return "C2S_SET_BAUDRATE";
        case CpoCommand::C2S_SET_DATASIZE:        return "C2S_SET_DATASIZE";
        case CpoCommand::C2S_SET_PARITY:          return "C2S_SET_PARITY";
        case CpoCommand::C2S_SET_STOPSIZE:        return "C2S_SET_STOPSIZE";
        case CpoCommand::C2S_SET_CONTROL:         return "C2S_SET_CONTROL";
        case CpoCommand::C2S_NOTIFY_LINESTATE:    return "C2S_NOTIFY_LINESTATE";
        case CpoCommand::C2S_NOTIFY_MODEMSTATE:   return "C2S_NOTIFY_MODEMSTATE";
        case CpoCommand::C2S_FLOWCONTROL_SUSPEND: return "C2S_FLOWCONTROL_SUSPEND";
        case CpoCommand::C2S_FLOWCONTROL_RESUME:  return "C2S_FLOWCONTROL_RESUME";
        case CpoCommand::C2S_SET_LINESTATE_MASK:  return "C2S_SET_LINESTATE_MASK";
        case CpoCommand::C2S_SET_MODEMSTATE_MASK: return "C2S_SET_MODEMSTATE_MASK";
        case CpoCommand::C2S_PURGE_DATA:          return "C2S_PURGE_DATA";
        case CpoCommand::S2C_SIGNATURE:           return "S2C_SIGNATURE";
        case CpoCommand::S2C_SET_BAUDRATE:        return "S2C_SET_BAUDRATE";
        case CpoCommand::S2C_SET_DATASIZE:        return "S2C_SET_DATASIZE";
        case CpoCommand::S2C_SET_PARITY:          return "S2C_SET_PARITY";
        case CpoCommand::S2C_SET_STOPSIZE:        return "S2C_SET_STOPSIZE";
        case CpoCommand::S2C_SET_CONTROL:         return "S2C_SET_CONTROL";
        case CpoCommand::S2C_NOTIFY_LINESTATE:    return "S2C_NOTIFY_LINESTATE";
        case CpoCommand::S2C_NOTIFY_MODEMSTATE:   return "S2C_NOTIFY_MODEMSTATE";
        case CpoCommand::S2C_FLOWCONTROL_SUSPEND: return "S2C_FLOWCONTROL_SUSPEND";
        case CpoCommand::S2C_FLOWCONTROL_RESUME:  return "S2C_FLOWCONTROL_RESUME";
        case CpoCommand::S2C_SET_LINESTATE_MASK:  return "S2C_SET_LINESTATE_MASK";
        case CpoCommand::S2C_SET_MODEMSTATE_MASK: return "S2C_SET_MODEMSTATE_MASK";
        case CpoCommand::S2C_PURGE_DATA:          return "S2C_PURGE_DATA";
        case CpoCommand::POLL_SIGNATURE:          break;
    }  // clang-format on
    return "?";
}

#if STREAM_TRACE_ENABLED
static std::string StringifyCpoCommand(const CpoCommand command, const uint8_t* data, const std::size_t size)
{
    // <command> [<data>]
    std::string str = "?";
    switch (command) {
        case CpoCommand::C2S_SIGNATURE:
        case CpoCommand::S2C_SIGNATURE:
        case CpoCommand::POLL_SIGNATURE:
            if (size == 0) {
                str = string::Sprintf("%-25s (request)", CpoCommandStr(command));
            } else {
                str = string::Sprintf("%-25s %s", CpoCommandStr(command), std::string(data, data + size).c_str());
            }
            break;
        case CpoCommand::C2S_SET_BAUDRATE:
        case CpoCommand::S2C_SET_BAUDRATE:
            str = string::Sprintf("%-25s %" PRIu32, CpoCommandStr(command), boost::endian::load_big_u32(data));
            break;
        case CpoCommand::C2S_SET_DATASIZE:
        case CpoCommand::S2C_SET_DATASIZE:
        case CpoCommand::C2S_SET_PARITY:
        case CpoCommand::S2C_SET_PARITY:
        case CpoCommand::C2S_SET_STOPSIZE:
        case CpoCommand::S2C_SET_STOPSIZE:
        case CpoCommand::C2S_SET_CONTROL:
        case CpoCommand::S2C_SET_CONTROL:
        case CpoCommand::C2S_PURGE_DATA:
        case CpoCommand::S2C_PURGE_DATA:
            str = string::Sprintf("%-25s %" PRIu8, CpoCommandStr(command), data[0]);
            break;
        case CpoCommand::C2S_NOTIFY_LINESTATE:
        case CpoCommand::S2C_NOTIFY_LINESTATE:
        case CpoCommand::C2S_NOTIFY_MODEMSTATE:
        case CpoCommand::S2C_NOTIFY_MODEMSTATE:
        case CpoCommand::C2S_SET_MODEMSTATE_MASK:
        case CpoCommand::S2C_SET_LINESTATE_MASK:
        case CpoCommand::C2S_SET_LINESTATE_MASK:
        case CpoCommand::S2C_SET_MODEMSTATE_MASK:
            str = string::Sprintf("%-25s 0x%02" PRIx8, CpoCommandStr(command), data[0]);
            break;
        case CpoCommand::C2S_FLOWCONTROL_SUSPEND:
        case CpoCommand::S2C_FLOWCONTROL_SUSPEND:
        case CpoCommand::C2S_FLOWCONTROL_RESUME:
        case CpoCommand::S2C_FLOWCONTROL_RESUME:
            break;
    }
    return str;
}
#endif  // STREAM_TRACE_ENABLED

// ---------------------------------------------------------------------------------------------------------------------

StreamTelnet::StreamTelnet(const StreamOptsTelnet& opts) /* clang-format off */ :
    StreamTcpClientsBase(opts, opts_),
    opts_         { opts },
    autobauder_   { *this }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamTelnet::~StreamTelnet()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTelnet::DoRequest()
{
    SetStateConnecting("negotiate");

    // The options we want and offer
    telnet_negotiate_.clear();
    // clang-format off
    telnet_negotiate_.push_back({ TelnetCode::DO,   TelnetOption::TRANSMIT_BINARY,    OptionState::UNSPECIFIED });
    telnet_negotiate_.push_back({ TelnetCode::DO,   TelnetOption::SUPPRESS_GO_AHEAD,  OptionState::UNSPECIFIED }); // some servers are very chatty
    telnet_negotiate_.push_back({ TelnetCode::DO,   TelnetOption::COM_PORT_OPTION,    OptionState::UNSPECIFIED });
    // Not all servers handle this :-/ For example, sredird says "WILL ECHO" (which makes no sense by itself) even though we said DONT ECHO
  //telnet_negotiate_.push_back({ TelnetCode::DONT, TelnetOption::ECHO_,               OptionState::UNSPECIFIED });
  //telnet_negotiate_.push_back({ TelnetCode::DONT, TelnetOption::LINEMODE,           OptionState::UNSPECIFIED }); // not all servers handle this :-/
    telnet_negotiate_.push_back({ TelnetCode::WILL, TelnetOption::TRANSMIT_BINARY,    OptionState::UNSPECIFIED });
    telnet_negotiate_.push_back({ TelnetCode::WILL, TelnetOption::SUPPRESS_GO_AHEAD,  OptionState::UNSPECIFIED });
    telnet_negotiate_.push_back({ TelnetCode::WILL, TelnetOption::COM_PORT_OPTION,    OptionState::UNSPECIFIED });
  //telnet_negotiate_.push_back({ TelnetCode::WONT, TelnetOption::ECHO_,              OptionState::UNSPECIFIED }); // not all servers handle this :-/
    // clang-format on

    // Send request
    tx_buf_size_ = 0;
    for (auto& o : telnet_negotiate_) {
        STREAM_TRACE("telnet > %-4s %s", TelnetCodeStr(o.code_), TelnetOptionStr(o.option_));
        tx_buf_data_[tx_buf_size_++] = types::EnumToVal(TelnetCode::IAC);
        tx_buf_data_[tx_buf_size_++] = types::EnumToVal(o.code_);
        tx_buf_data_[tx_buf_size_++] = types::EnumToVal(o.option_);
    }
    telnet_rx_state_ = TelnetState::NORMAL;
    telnet_tx_state_ = TelnetState::NORMAL;
    telnet_code_ = TelnetCode::IAC;

    // Send request
    STREAM_TRACE("DoRequest %" PRIuMAX, tx_buf_size_);
    // TRACE_HEXDUMP(tx_buf_data_, tx_buf_size_, NULL, NULL);
    if (opts_.tls_) {
        boost::asio::async_write(*socket_ssl_, boost::asio::buffer(tx_buf_data_, tx_buf_size_),
            std::bind(&StreamTelnet::HandleRequest, this, std::placeholders::_1, (std::size_t)0));
    } else {
        boost::asio::async_write(socket_raw_, boost::asio::buffer(tx_buf_data_, tx_buf_size_),
            std::bind(&StreamTelnet::HandleRequest, this, std::placeholders::_1, (std::size_t)0));
    }
}

void StreamTelnet::HandleRequest(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
{
    STREAM_TRACE("HandleRequest %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Sending request failed
    else if (ec) {
        CloseSocket(false);
        SetStateError(StreamError::CONN_LOST, "request: " + ec.message());
        DoRetry();
    }
    // Sending request successful
    else {
        DoResponse();
    }
}

void StreamTelnet::DoResponse()
{
    STREAM_TRACE("DoResponse");
    if (opts_.tls_) {
        socket_ssl_->async_read_some(
            rx_buf_, std::bind(&StreamTelnet::OnResponse, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        socket_raw_.async_read_some(
            rx_buf_, std::bind(&StreamTelnet::OnResponse, this, std::placeholders::_1, std::placeholders::_2));
    }
}

void StreamTelnet::OnResponse(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
        STREAM_TRACE("OnResponse %s", ec.message().c_str());
    }
    // E.g. boost::asio::error::eof (= remote closed connection)
    else if (ec) {
        SetStateError(StreamError::CONN_LOST, "read " + ec.message());
        CloseSocket(false);
        DoRetry();
    }
    // Process in-band data and check telnet option state
    else {
        FilterRead(bytes_transferred);

        std::size_t ack = 0;
        std::size_t nak = 0;
        for (const auto& opt : telnet_negotiate_) {
            switch (opt.state_) {  // clang-format off
                case OptionState::ACK:         ack++;  break;
                case OptionState::NAK:         nak++;  break;
                case OptionState::UNSPECIFIED:         break;
            }  // clang-format on
        }
        const bool done = ((ack + nak) == telnet_negotiate_.size());
        STREAM_TRACE("OnResponse %s ack/nak/tot=%" PRIuMAX "/%" PRIuMAX "/%" PRIuMAX " %s", ec.message().c_str(), ack,
            nak, telnet_negotiate_.size(), done ? "done" : "wait");

        // We have an answer for all options
        if (done) {
            // All options are agreed, we now have to configure the port
            bool ok = true;
            if (nak == 0) {
                // Configure port
                if (!SendComPortOption(CpoCommand::C2S_SET_BAUDRATE, opts_.baudrate_)) {
                    ok = false;
                }
                switch (opts_.serial_mode_) {
                    case StreamOpts::SerialMode::UNSPECIFIED:
                        ok = false;
                        break;
                    case StreamOpts::SerialMode::_8N1:
                        if (!SendComPortOption(CpoCommand::C2S_SET_DATASIZE, types::EnumToVal(CpoDataSize::EIGHT)) ||
                            !SendComPortOption(CpoCommand::C2S_SET_PARITY, types::EnumToVal(CpoParity::NONE)) ||
                            !SendComPortOption(CpoCommand::C2S_SET_STOPSIZE, types::EnumToVal(CpoStopSize::ONE))) {
                            ok = false;
                        }
                        break;
                }
                switch (opts_.serial_flow_) {
                    case StreamOpts::SerialFlow::UNSPECIFIED:
                        ok = false;
                        break;
                    case StreamOpts::SerialFlow::OFF:
                        if (!SendComPortOption(CpoCommand::C2S_SET_CONTROL, types::EnumToVal(CpoControl::NONE))) {
                            ok = false;
                        }
                        break;
                    case StreamOpts::SerialFlow::SW:
                        if (!SendComPortOption(CpoCommand::C2S_SET_CONTROL, types::EnumToVal(CpoControl::XONOFF))) {
                            ok = false;
                        }
                        break;
                    case StreamOpts::SerialFlow::HW:
                        if (!SendComPortOption(CpoCommand::C2S_SET_CONTROL, types::EnumToVal(CpoControl::HARDWARE))) {
                            ok = false;
                        }
                        break;
                }
                // See serial.cpp
                if (!SendComPortOption(CpoCommand::C2S_SET_CONTROL, types::EnumToVal(CpoControl::DTR_OFF)) ||
                    !SendComPortOption(CpoCommand::C2S_SET_CONTROL, types::EnumToVal(CpoControl::RTS_OFF))) {
                    ok = false;
                }

                // Silencio!
                if (!SendComPortOption(CpoCommand::C2S_NOTIFY_LINESTATE, types::EnumToVal(CpoLineState::NONE)) ||
                    !SendComPortOption(CpoCommand::C2S_NOTIFY_MODEMSTATE, types::EnumToVal(CpoModemState::NONE))) {
                    ok = false;
                }
                // Send and request signature
                if (!SendComPortOption(CpoCommand::C2S_SIGNATURE) || !SendComPortOption(CpoCommand::POLL_SIGNATURE)) {
                    ok = false;
                }
                // A fresh start..
                if (!SendComPortOption(CpoCommand::C2S_PURGE_DATA, types::EnumToVal(CpoPurgeData::BOTH))) {
                    ok = false;
                }

                if (!ok) {
                    SetStateError(StreamError::TELNET_ERROR, "config port failed");
                } else {
                    if (opts_.autobaud_ != AutobaudMode::NONE) {
                        Autobaud(opts_.autobaud_);
                    } else {
                        DoConnected(std::to_string(opts_.baudrate_));
                    }
                }
            }
            // There's a disagreement for at least one option
            else {
                std::string str;
                for (const auto& o : telnet_negotiate_) {
                    if (o.state_ == OptionState::NAK) {
                        str += string::Sprintf(", %s %s", TelnetCodeStr(o.code_), TelnetOptionStr(o.option_));
                    }
                }
                ok = false;
                SetStateError(StreamError::TELNET_ERROR, "options failed: " + str.substr(2));
            }
            if (!ok) {
                CloseSocket();
                DoRetry();
            }
        }
        // Wait for more answers
        else {
            DoResponse();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

std::size_t StreamTelnet::FilterRead(const std::size_t size)
{
    STREAM_TRACE("FilterRead %" PRIuMAX " in", size);

    std::size_t rx_size = 0;
    std::size_t rx_offs = 0;

    // IAC IAC -> 0xff
    // IAC <option> [...]
    // IAC SB <suboption> [...] IAC SE -> (nothing)

    //  clang-formt off
    static constexpr uint8_t IAC = types::EnumToVal(TelnetCode::IAC);
    static constexpr uint8_t DO = types::EnumToVal(TelnetCode::DO);
    static constexpr uint8_t DONT = types::EnumToVal(TelnetCode::DONT);
    static constexpr uint8_t WILL = types::EnumToVal(TelnetCode::WILL);
    static constexpr uint8_t WONT = types::EnumToVal(TelnetCode::WONT);
    static constexpr uint8_t SB = types::EnumToVal(TelnetCode::SB);
    static constexpr uint8_t SE = types::EnumToVal(TelnetCode::SE);
    // clang-format on

    // Process in-band control data, and remove it from buf
    while (rx_offs < size) {
        const uint8_t data = rx_buf_data_[rx_offs];
        rx_offs++;
        switch (telnet_rx_state_) {
            // Previous byte was some normal data
            case TelnetState::NORMAL:
                // Command may follow
                if (data == IAC) {
                    telnet_rx_state_ = TelnetState::IACSEEN;
                }
                // User data
                else {
                    rx_buf_data_[rx_size] = data;
                    rx_size++;
                }
                break;
            // Previous byte was IAC
            case TelnetState::IACSEEN:
                switch (data) {
                    // Actual 0xff user data
                    case IAC:
                        rx_buf_data_[rx_size] = data;
                        rx_size++;
                        telnet_rx_state_ = TelnetState::NORMAL;
                        break;
                    // Option negotiation
                    case DO:
                    case DONT:
                    case WILL:
                    case WONT:
                        telnet_rx_state_ = TelnetState::NEGOTIATE;
                        telnet_code_ = (TelnetCode)data;
                        break;
                    // Suboption begin
                    case SB:
                        telnet_option_.clear();
                        telnet_rx_state_ = TelnetState::OPTION;
                        break;
                    // Otherwise it is some other option, all of which don't come with any more data
                    default:
                        telnet_rx_state_ = TelnetState::NORMAL;
                        break;
                }
                break;
            // Previous two bytes was IAC and DO|DONT|WILL|WONT. This byte is the agument to the latter.
            case TelnetState::NEGOTIATE:
                ProcessOption(telnet_code_, (TelnetOption)data);
                telnet_code_ = TelnetCode::IAC;
                telnet_rx_state_ = TelnetState::NORMAL;
                break;
            // We're in a suboption
            case TelnetState::OPTION:
                switch (data) {
                    // IAC, may be followed by SE or IAC
                    case IAC:
                        telnet_rx_state_ = TelnetState::OPTION_IACSEEN;
                        break;
                    // Suboption data
                    default:
                        telnet_option_.push_back(data);
                        break;
                }
                break;
            case TelnetState::OPTION_IACSEEN:
                switch (data) {
                    // Escaped 0xff
                    case IAC:
                        telnet_option_.push_back(data);
                        break;
                    // End of suboption
                    case SE: {
                        // @todo handle suboption
                        const auto option = (TelnetOption)telnet_option_[0];
                        if (option == TelnetOption::COM_PORT_OPTION) {
                            STREAM_TRACE(
                                "telnet < COM_PORT_OPTION %s", StringifyCpoCommand((CpoCommand)telnet_option_[1],
                                                                   &telnet_option_[2], telnet_option_.size() - 2)
                                                                   .c_str());
                        } else {
                            STREAM_TRACE("telnet < %s", TelnetOptionStr(option));
                        }
                        // TRACE_HEXDUMP(telnet_option_.data(), telnet_option_.size(), NULL, NULL);
                        telnet_rx_state_ = TelnetState::NORMAL;
                        break;
                    }
                    // Unexpected, we can only expect IAC SB ... IAC *IAC* ... or  IAC SB ... IAC *SE*
                    default:
                        STREAM_WARNING_THR(1000, "Unexpected IAC SB ... IAC 0x%02" PRIu8 " sequence", data);
                        telnet_rx_state_ = TelnetState::NORMAL;  // What else can we do...?
                        break;
                }
        }
    }

    STREAM_TRACE("FilterRead %" PRIuMAX " out", rx_size);

    // The amount of user data left
    return rx_size;
}

// ---------------------------------------------------------------------------------------------------------------------

std::size_t StreamTelnet::FilterWrite()
{
    // Some servers (e.g. some Moxa devices) don't like too large packets
    // @todo Some Moxa devices don't like more then eight (8) 0xff in one packet... :-/
    constexpr std::size_t max_packet_size = 512;
    uint8_t buf[max_packet_size / 2];
    static_assert(sizeof(buf) < sizeof(tx_buf_data_) / 2, "");
    const std::size_t in_size = std::min(sizeof(buf), write_queue_.Used());
    std::size_t out_size = 0;
    if (in_size > 0) {
        write_queue_.Read(buf, in_size);
        for (std::size_t ix = 0; ix < in_size; ix++) {
            const uint8_t data = buf[ix];
            if (data == types::EnumToVal(TelnetCode::IAC)) {
                tx_buf_data_[out_size++] = types::EnumToVal(TelnetCode::IAC);
                tx_buf_data_[out_size++] = types::EnumToVal(TelnetCode::IAC);
            } else {
                tx_buf_data_[out_size++] = data;
            }
        }
    }

    STREAM_TRACE("FilterWrite %" PRIuMAX " -> %" PRIuMAX, in_size, out_size);
    return out_size;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTelnet::ProcessOption(const TelnetCode code, const TelnetOption option)
{
    auto beg = telnet_negotiate_.begin();
    auto end = telnet_negotiate_.end();
    auto ack = end;  // positive match (for example, we say DO, they say WILL)
    auto nak = end;  // negative match (for example, we say DO, they say WONT)
    switch (code) {  // clang-format off
        case TelnetCode::DO:
            ack = std::find_if(beg, end, [&code, &option](const Option& opt) { return (opt.code_ == TelnetCode::WILL) && (opt.option_ == option); });
            nak = std::find_if(beg, end, [&code, &option](const Option& opt) { return (opt.code_ == TelnetCode::WONT) && (opt.option_ == option); });
            break;
        case TelnetCode::DONT:
            ack = std::find_if(beg, end, [&code, &option](const Option& opt) { return (opt.code_ == TelnetCode::WONT) && (opt.option_ == option); });
            nak = std::find_if(beg, end, [&code, &option](const Option& opt) { return (opt.code_ == TelnetCode::WILL) && (opt.option_ == option); });
            break;
        case TelnetCode::WILL:
            ack = std::find_if(beg, end, [&code, &option](const Option& opt) { return (opt.code_ == TelnetCode::DO)   && (opt.option_ == option); });
            nak = std::find_if(beg, end, [&code, &option](const Option& opt) { return (opt.code_ == TelnetCode::DONT) && (opt.option_ == option); });
            break;
        case TelnetCode::WONT:
            ack = std::find_if(beg, end, [&code, &option](const Option& opt) { return (opt.code_ == TelnetCode::DONT) && (opt.option_ == option); });
            nak = std::find_if(beg, end, [&code, &option](const Option& opt) { return (opt.code_ == TelnetCode::DO)   && (opt.option_ == option); });
            break;
        default:
            break;
    }                // clang-format on

    // We're good
    if (ack != end) {
        STREAM_TRACE("ProcessOption %-4s %-20s ack %-4s %s", TelnetCodeStr(code), TelnetOptionStr(option),
            TelnetCodeStr(ack->code_), TelnetOptionStr(ack->option_));
        ack->state_ = OptionState::ACK;
    }
    // No match
    else if (nak != end) {
        STREAM_TRACE("ProcessOption %-4s %-20s nak %-4s %s", TelnetCodeStr(code), TelnetOptionStr(option),
            TelnetCodeStr(nak->code_), TelnetOptionStr(nak->option_));
        nak->state_ = OptionState::NAK;
    }
    // Unhandled option, ignore @todo what to do?
    else {
        STREAM_TRACE("ProcessOption %-4s %-20s ignore", TelnetCodeStr(code), TelnetOptionStr(option));
    }
}

// ---------------------------------------------------------------------------------------------------------------------

uint32_t StreamTelnet::GetBaudrate()
{
    return opts_.baudrate_;
}

bool StreamTelnet::SetBaudrate(const uint32_t baudrate)
{
    STREAM_TRACE("SetBaudrate %" PRIu32, baudrate);
    bool ok = true;

    // Physical baudrate change only if connected (port is open and we can change its baudrate)
    const auto state = GetState();
    if (RawSocket().is_open()) {
        ok = SendComPortOption(CpoCommand::C2S_SET_BAUDRATE, baudrate);
    }

    // Always update options
    opts_.baudrate_ = baudrate;
    opts_.UpdatePath();

    // Notify, and also reset the read timeout
    if (ok && (state == StreamState::CONNECTED)) {
        SetStateConnected(HostPortStr(endpoint_) + " " + std::to_string(opts_.baudrate_));
        DoInactTimeout();
    }

    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamTelnet::Autobaud(const AutobaudMode mode)
{
    return autobauder_.Start(mode, [this](const bool success) {
        if (success) {
            DoConnected(std::to_string(opts_.baudrate_));
        } else {
            SetStateError(StreamError::CONNECT_FAIL, "autobaud fail");
            CloseSocket();
            DoRetry();
        }
    });
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamTelnet::SendComPortOption(const CpoCommand command, const uint32_t payload)
{
    bool ok = true;
    uint8_t data[64];
    std::size_t size = 0;
    data[size++] = types::EnumToVal(TelnetCode::IAC);
    data[size++] = types::EnumToVal(TelnetCode::SB);
    data[size++] = types::EnumToVal(TelnetOption::COM_PORT_OPTION);
    data[size++] = types::EnumToVal(command);
    switch (command) {  // clang-format off
        case CpoCommand::POLL_SIGNATURE:
            data[3] = types::EnumToVal(CpoCommand::C2S_SIGNATURE);
            break;
        case CpoCommand::C2S_SIGNATURE: {
            const auto sig_data = GetUserAgentStr();
            const std::size_t sig_size = std::min(sig_data.size(), (std::size_t)32);
            memcpy(&data[size], sig_data.data(), sig_size);
            size += sig_size;
            break;
        }
        case CpoCommand::C2S_SET_BAUDRATE:
            boost::endian::store_big_u32(&data[size], payload);
            size += sizeof(uint32_t);
            break;
        case CpoCommand::C2S_SET_DATASIZE:         data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::C2S_SET_PARITY:           data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::C2S_SET_STOPSIZE:         data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::C2S_SET_CONTROL:          data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::C2S_NOTIFY_LINESTATE:     data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::C2S_NOTIFY_MODEMSTATE:    data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::C2S_FLOWCONTROL_SUSPEND:  break;
        case CpoCommand::C2S_FLOWCONTROL_RESUME:   break;
        case CpoCommand::C2S_SET_LINESTATE_MASK:   data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::C2S_SET_MODEMSTATE_MASK:  data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::C2S_PURGE_DATA:           data[size++] = (uint8_t)(payload & 0xff); break;
        case CpoCommand::S2C_SIGNATURE:
        case CpoCommand::S2C_SET_BAUDRATE:
        case CpoCommand::S2C_SET_DATASIZE:
        case CpoCommand::S2C_SET_PARITY:
        case CpoCommand::S2C_SET_STOPSIZE:
        case CpoCommand::S2C_SET_CONTROL:
        case CpoCommand::S2C_NOTIFY_LINESTATE:
        case CpoCommand::S2C_NOTIFY_MODEMSTATE:
        case CpoCommand::S2C_FLOWCONTROL_SUSPEND:
        case CpoCommand::S2C_FLOWCONTROL_RESUME:
        case CpoCommand::S2C_SET_LINESTATE_MASK:
        case CpoCommand::S2C_SET_MODEMSTATE_MASK:
        case CpoCommand::S2C_PURGE_DATA:
            ok = false;
            break;
    }  // clang-format on
    data[size++] = types::EnumToVal(TelnetCode::IAC);
    data[size++] = types::EnumToVal(TelnetCode::SE);

    if (ok) {
        STREAM_TRACE("telnet > %s", StringifyCpoCommand(command, &data[4], size - 6).c_str());
        // TRACE_HEXDUMP(data, size, NULL, NULL);
        if (opts_.tls_) {
            ok = (boost::asio::write(*socket_ssl_, boost::asio::buffer(data, size)) == size);
        } else {
            ok = (boost::asio::write(socket_raw_, boost::asio::buffer(data, size)) == size);
        }
    }

    return ok;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
