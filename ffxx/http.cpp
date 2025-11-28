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
 * @brief HTTP utilities
 */

/* LIBC/STL */
#include <memory>
#include <mutex>
#include <queue>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

/* EXTERNAL */
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/optional.hpp>
#include <boost/system/error_code.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/thread.hpp>
#include <fpsdk_common/types.hpp>
#include <fpsdk_common/utils.hpp>

/* PACKAGE */
#include "http.hpp"
#include "utils.hpp"
#include "stream/base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

#define HTTP_TRACE_ENABLED 0

// clang-format off
#if HTTP_TRACE_ENABLED
#  define HTTP_TRACE(...) HttpLoggingPrint(logging::LoggingLevel::TRACE, 0, __VA_ARGS__)
#  define HTTP_TRACE_WARNING() HTTP_WARNING("HTTP_TRACE_ENABLED")
#else
#  define HTTP_TRACE(...)      /* nothing */
#  define HTTP_TRACE_WARNING() /* nothing */
#endif
#define HTTP_DEBUG(...)   HttpLoggingPrint(logging::LoggingLevel::DEBUG,   0, __VA_ARGS__)
#define HTTP_INFO(...)    HttpLoggingPrint(logging::LoggingLevel::INFO,    0, __VA_ARGS__)
#define HTTP_WARNING(...) HttpLoggingPrint(logging::LoggingLevel::WARNING, 0, __VA_ARGS__)
#define HTTP_WARNING_THR(_millis_, ...) do { \
    static uint32_t __last = 0; const uint32_t __now = time::GetMillis(); \
    static uint32_t __repeat = 0; __repeat++; \
    if ((__now - __last) >= (_millis_)) { __last = __now; \
        HttpLoggingPrint(logging::LoggingLevel::WARNING, __repeat, __VA_ARGS__);\
        __repeat = 0; } } while (0)
// clang-format on

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;

/* ****************************************************************************************************************** */

struct HttpApiServerOpts
{
    std::string prefix_;
    std::string host_;
    uint16_t port_ = 0;
    bool ipv6_ = false;
};

// examples/libs/beast/example/advanced/server/advanced_server.cpp

class HttpApiServerImpl : public HttpApiServer, private types::NoCopyNoMove
{
   public:
    HttpApiServerImpl(const HttpApiServerOpts& opts) /* clang-format off */ :
        HttpApiServer(),
        opts_     { opts },
        name_     { HostPortStr(opts.host_, opts.port_, opts.ipv6_) },
        thread_   { "httpapi", std::bind(&HttpApiServerImpl::Worker, this) }  // clang-format on
    {
        HTTP_TRACE("ctor");
        HTTP_TRACE_WARNING();
    }

    ~HttpApiServerImpl()
    {
        HTTP_TRACE("dtor");
    }

    bool Start() final;
    void Stop() final;

    void SetHandler(const Method method, const std::string& path, Handler handler) final;
    void SendWs(const std::string& path, const Response& res) final;

   private:
    HttpApiServerOpts opts_;
    std::string name_;

    thread::Thread thread_;
    bool Worker();

    struct Server : private fpsdk::common::types::NoCopyNoMove
    {
        Server(const std::string& name, asio::io_context& ctx);
        std::string name_;
        asio::ip::tcp::acceptor acceptor_;
    };
    std::vector<std::unique_ptr<Server>> servers_;

    struct Session : private fpsdk::common::types::NoCopyNoMove
    {
        Session(const std::string& name, asio::ip::tcp::socket&& socket);
        std::string name_;
        std::unique_ptr<beast::tcp_stream> stream_;
        beast::flat_buffer rx_buffer_;
        boost::optional<http::request_parser<http::string_body>> parser_;
        std::shared_ptr<void> res_;
        std::unique_ptr<websocket::stream<beast::tcp_stream>> ws_;
        std::string ws_path_;
        std::mutex ws_mutex_;
        std::queue<std::vector<uint8_t>> ws_queue_;
    };
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Session>> sessions_;
    static constexpr std::size_t MAX_SESSIONS = 20;
    void CloseSession(Session* session, const bool remove = true);

    asio::io_context ctx_;

    std::unordered_map<std::string, Handler> handlers_post_;
    std::unordered_map<std::string, Handler> handlers_get_;
    std::unordered_map<std::string, Handler> handlers_ws_;

    void DoAccept(Server* server);
    void OnAccept(const boost::system::error_code& ec, asio::ip::tcp::socket peer, Server* server);

    void DoRead(Session* session);
    void OnRead(Session* session, const beast::error_code ec, const std::size_t bytes_transferred);

    template <class BodyT, class AllocatorT>
    void HandleRequest(Session* session, http::request<BodyT, http::basic_fields<AllocatorT>>&& req);

    template <class BodyT>
    void DoWrite(Session* session, const http::response<BodyT>& res);
    void OnWrite(Session* session, const bool close, const beast::error_code ec, const std::size_t bytes_transferred);

    template <class BodyT, class AllocatorT>
    void DoAcceptWs(Session* session, http::request<BodyT, http::basic_fields<AllocatorT>> req);
    void OnAcceptWs(Session* session, const beast::error_code ec);

    void DoReadWs(Session* session);
    void OnReadWs(Session* session, const beast::error_code ec, const std::size_t bytes_transferred);

    void HandleRequestWs(Session* session);

    void DoWriteWs(Session* session);
    void OnWriteWs(Session* session, const beast::error_code ec, const std::size_t bytes_transferred);

    void HttpLoggingPrint(const fpsdk::common::logging::LoggingLevel level, const std::size_t repeat, const char* fmt,
        ...) const PRINTF_ATTR(4);
};

// ---------------------------------------------------------------------------------------------------------------------

HttpApiServerImpl::Server::Server(const std::string& name, asio::io_context& ctx) : name_{ name }, acceptor_{ ctx }
{
}

// ---------------------------------------------------------------------------------------------------------------------

HttpApiServerImpl::Session::Session(const std::string& name, asio::ip::tcp::socket&& socket) /* clang-format off */ :
    name_     { name },
    stream_   { std::make_unique<beast::tcp_stream>(std::move(socket)) }  // clang-format on
{
}

/* ****************************************************************************************************************** */

bool HttpApiServerImpl::Start()
{
    HTTP_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }
    if (!thread_.Start()) {
        return false;
    }
    HTTP_INFO("Starting server");

    // Find server endpoint(s)
    std::string error;
    const auto endpoints = ResolveTcpEndpoints(opts_.host_, opts_.port_, opts_.ipv6_, error);
    if (endpoints.empty()) {
        HTTP_WARNING("Failed resolving: %s", error.c_str());
        Stop();
        return false;
    }

    bool ok = true;

    // Open server socket(s)
    for (auto& endpoint : endpoints) {
        const std::string name = HostPortStr(endpoint);
        auto server = std::make_unique<Server>(name, ctx_);

        boost::system::error_code ec;

        // Create socket
        server->acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            HTTP_WARNING("open: %s", ec.message().c_str());
            ok = false;
            break;
        }

        // Set socket options
        server->acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        if (ec) {
            HTTP_WARNING("reuse_address: %s", ec.message().c_str());
            ok = false;
            break;
        }
        if (endpoint.address().is_v6()) {
            server->acceptor_.set_option(asio::ip::v6_only(true), ec);
            if (ec) {
                HTTP_WARNING("v6_only: %s", ec.message().c_str());
                ok = false;
                break;
            }
        }

        // Bind socket to address:port
        server->acceptor_.bind(endpoint, ec);
        if (ec) {
            HTTP_WARNING("bind: %s", ec.message().c_str());
            ok = false;
            break;
        }

        // Start listening
        server->acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            HTTP_WARNING("listen: %s", ec.message().c_str());
            break;
            ok = false;
        }

        HTTP_INFO("Listening on %s", name.c_str());
        servers_.emplace_back(std::move(server));
    }

    if (!ok) {
        Stop();
        return false;
    }

    // Start accepting connections
    for (auto& server : servers_) {
        DoAccept(server.get());
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::Stop()
{
    HTTP_TRACE("Stop");

    for (auto& server : servers_) {
        boost::system::error_code ec;
        server->acceptor_.cancel(ec);
        if (ec) {
            HTTP_WARNING("%s acceptor cancel: %s", server->name_.c_str(), ec.message().c_str());
        }
        server->acceptor_.close(ec);
        if (ec) {
            HTTP_WARNING("%s acceptor close: %s", server->name_.c_str(), ec.message().c_str());
        }
    }
    servers_.clear();

    for (auto& entry : sessions_) {
        CloseSession(entry.second.get(), false);
    }
    sessions_.clear();

    HTTP_TRACE("Stop ctx stop");

    ctx_.stop();

    HTTP_TRACE("Stop thread stop");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::SetHandler(const Method method, const std::string& path, Handler handler)
{
    switch (method) {
            // clang-format off
        case Method::GET:  HTTP_TRACE("SetHandler GET %s",  path.c_str()); handlers_get_[path]  = handler; break;
        case Method::POST: HTTP_TRACE("SetHandler POST %s", path.c_str()); handlers_post_[path] = handler; break;
        case Method::WS:   HTTP_TRACE("SetHandler WS %s",   path.c_str()); handlers_ws_[path]   = handler; break;
            // clang-format on
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool HttpApiServerImpl::Worker()
{
    HTTP_TRACE("Worker");
    // Prevent run_for() below from returning early if there's no pending thing to handle. Otherwise we would busy-loop
    // below in case some stream implementation has states where nothing happens.
    auto work = asio::make_work_guard(ctx_);
    while (!thread_.ShouldAbort()) {
        // Run i/o for a bit. The exact value of the time doesn't matter. It just needs to be small enough to make
        // ShouldAbort() "responsive" (and large enough to not busy wait)
        ctx_.run_for(std::chrono::milliseconds(337));
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::DoAccept(Server* server)
{
    HTTP_TRACE("DoAccept %s", server->name_.c_str());
    server->acceptor_.async_accept(asio::make_strand(ctx_),
        std::bind(&HttpApiServerImpl::OnAccept, this, std::placeholders::_1, std::placeholders::_2, server));
}

void HttpApiServerImpl::OnAccept(const boost::system::error_code& ec, asio::ip::tcp::socket peer, Server* server)
{
    HTTP_TRACE("OnAccept %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == asio::error::operation_aborted) {
    }
    // Accept failed
    // @todo Should we do something? Maybe we should handle at least some the possible errors?
    else if (ec) {
        HTTP_WARNING("%s accept fail: %s", server->name_.c_str(), ec.message().c_str());
    }
    // Create client session
    else {
        const std::string name = HostPortStr(peer.remote_endpoint());
        auto session = std::make_unique<Session>(name, std::move(peer));

        // Limit maximum number of sessions, and prevent client with same name (you never know...)
        if ((sessions_.size() >= MAX_SESSIONS) || (sessions_.find(name) != sessions_.end())) {
            HTTP_WARNING("Deny %s", name.c_str());
            session.reset();
        }
        // Accept and start session
        else {
            HTTP_DEBUG("Connect %s", name.c_str());
            std::unique_lock<std::mutex> lock(mutex_);
            auto x = sessions_.emplace(name, std::move(session));
            DoRead(x.first->second.get());
            // @todo we should be doing this (cf. the boost example), but it crashes
            // asio::dispatch(*session->stream_.get_executor(),
            //     beast::bind_front_handler(&HttpApiServerImpl::DoRead, this, x.first->second.get()));
        }

        DoAccept(server);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::DoRead(Session* session)
{
    HTTP_TRACE("DoRead %s", session->name_.c_str());

    // session->req_ = {};  // Empty request before read
    session->parser_.emplace();
    session->parser_->body_limit(MAX_JSON_STR);

    // Set the timeout
    session->stream_->expires_after(std::chrono::seconds(30));

    // Read a request
    http::async_read(*session->stream_, session->rx_buffer_, *session->parser_,
        beast::bind_front_handler(&HttpApiServerImpl::OnRead, this, session));
}

void HttpApiServerImpl::OnRead(Session* session, const beast::error_code ec, const std::size_t bytes_transferred)
{
    HTTP_TRACE("OnRead %s %s %" PRIuMAX, session->name_.c_str(), ec.message().c_str(), bytes_transferred);
    UNUSED(bytes_transferred);

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Failed
    else if (ec) {
        // This means they closed the connection, or stopped using it
        if ((ec == http::error::end_of_stream) || (ec == websocket::error::closed) || (ec == beast::error::timeout)) {
            HTTP_DEBUG("%s disconnect: %s", session->name_.c_str(), ec.message().c_str());
        } else {
            HTTP_WARNING("%s read: %s", session->name_.c_str(), ec.message().c_str());
        }
        CloseSession(session);
    }

    // We received the request
    else {
        HandleRequest(session, session->parser_->release());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

template <class BodyT>
static void SetCommonResHeaders(http::response<BodyT>& res)
{
    res.set(http::field::server, GetUserAgentStr());
    res.set(http::field::cache_control, "no-store");
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_methods, "GET, POST, HEAD, OPTIONS");
    res.set(http::field::access_control_allow_headers,
        "Access-Control-Allow-Headers, Access-Control-Allow-Methods, Authorization, X-Requested-With, Content-Type");
    res.set(http::field::date, string::Strftime("%a, %d %b %Y %H:%M:%S %Z"));
}

template <class BodyT, class AllocatorT>
void HttpApiServerImpl::HandleRequest(Session* session, http::request<BodyT, http::basic_fields<AllocatorT>>&& req)
{
    // Split query string into path and arguments, discard arguments (we're not using any GET parameters)
    const std::string path = string::StrSplit(std::string(req.target()), "?")[0];
    const std::string method = std::string(http::to_string(req.method()));
    HTTP_TRACE("HandleRequest %s %s %s %" PRIuMAX, session->name_.c_str(), method.c_str(), path.c_str(),
        req.payload_size().value_or(0));
    // TRACE_S("HandleRequest req " << req);

    // HEAD or OPTIONS request (CORS preflight check) --> send empty response
    if ((req.method() == http::verb::head) || (req.method() == http::verb::options)) {
        http::response<http::empty_body> res{ http::status::ok, req.version() };
        res.keep_alive(req.keep_alive());
        DoWrite(session, res);
        return;
    }

    Request api_req;
    api_req.session_ = session->name_;

    Response api_res;
    http::status res_status = http::status::unknown;

    // Request path must start with prefix if configured
    if (!opts_.prefix_.empty()) {
        if (!string::StrStartsWith(path, opts_.prefix_)) {
            res_status = http::status::not_found;
            api_res.error_ = "bad path prefix";
        } else {
            api_req.path_ = path.substr(opts_.prefix_.size());
        }
    } else {
        api_req.path_ = path;
    }

    // Bad prefix
    if (res_status != http::status::unknown) {
    }
    // Websocket
    else if (websocket::is_upgrade(req)) {
        if (handlers_ws_.find(api_req.path_) != handlers_ws_.end()) {
            HTTP_TRACE("%s websocket upgrade", session->name_.c_str());
            session->ws_ = std::make_unique<websocket::stream<beast::tcp_stream>>(session->stream_->release_socket());
            session->stream_.reset();
            session->ws_path_ = api_req.path_;
            DoAcceptWs(session, req);
            return;
        } else {
            res_status = http::status::not_found;
            api_res.error_ = "no websocket here";
        }
    }
    // GET
    else if (req.method() == http::verb::get) {
        api_req.method_ = Method::GET;
        auto handler = handlers_get_.find(api_req.path_);
        if (handler != handlers_get_.end()) {
            try {
                if (handler->second(api_req, api_res)) {
                    res_status = http::status::ok;
                } else {
                    res_status = http::status::bad_request;
                    if (api_res.error_.empty()) {
                        api_res.error_ = "handler fail";
                    }
                }
            } catch (std::exception& ex) {
                HTTP_WARNING("handler crashed: %s", ex.what());
                api_res.error_ = "handler crash";
            }
        } else {
            res_status = http::status::not_found;
        }
    }
    // POST (must be valid JSON)
    else if (req.method() == http::verb::post) {
        api_req.method_ = Method::POST;
        const auto content_type = req.find(http::field::content_type);
        const std::string content_type_str = (content_type != req.end() ? std::string(content_type->value()) : "");
        if (string::StrStartsWith(content_type_str, CONTENT_TYPE_JSON)) {
            auto handler = handlers_post_.find(api_req.path_);
            if (handler != handlers_post_.end()) {
                bool json_ok = false;
                try {
                    api_req.data_ = nlohmann::json::parse(req.body());
                    json_ok = true;
                } catch (std::exception& ex) {
                    res_status = http::status::bad_request;
                    api_res.error_ = std::string("bad json data: ") + ex.what();
                }
                if (json_ok) {
                    try {
                        if (handler->second(api_req, api_res)) {
                            res_status = http::status::ok;
                        } else {
                            res_status = http::status::bad_request;
                            if (api_res.error_.empty()) {
                                api_res.error_ = "handler fail";
                            }
                        }
                    } catch (std::exception& ex) {
                        HTTP_WARNING("handler crashed: %s", ex.what());
                        api_res.body_ = string::StrToBuf("handler crash");
                    }
                }
            } else {
                res_status = http::status::not_found;
            }
        } else {
            res_status = http::status::bad_request;
            api_res.error_ = "bad content type";
        }
    }
    // Bad method
    else {
        res_status = http::status::method_not_allowed;
        api_res.error_ = "bad method";
    }

    if (res_status == http::status::unknown) {
        res_status = http::status::internal_server_error;
        if (api_res.error_.empty()) {
            api_res.error_ = "unknown error";
        }
    }

    if (api_res.body_.empty() && !api_res.error_.empty()) {
        api_res.type_ = CONTENT_TYPE_JSON;
        api_res.body_ = string::StrToBuf(nlohmann::json::object({ { "error", api_res.error_ } }).dump());
    }

    // Prepare response
    http::response<http::vector_body<uint8_t>> res{ res_status, req.version() };
    SetCommonResHeaders(res);
    res.keep_alive(req.keep_alive());

    res.body() = api_res.body_;
    if (!res.body().empty()) {
        res.set(http::field::content_type, api_res.type_.empty() ? "text/plain" : api_res.type_);
        res.prepare_payload();
    }

    if (res.result() == http::status::ok) {
        HTTP_DEBUG("%s %s %s %d %s", session->name_.c_str(), method.c_str(), path.c_str(), res.result_int(),
            std::string(res.reason()).c_str());
    } else {
        HTTP_WARNING("%s %s %s %d %s: %s", session->name_.c_str(), method.c_str(), path.c_str(), res.result_int(),
            std::string(res.reason()).c_str(), api_res.error_.c_str());
    }
    // TRACE_S("HandleRequest res " << res);

    DoWrite(session, res);
}

// ---------------------------------------------------------------------------------------------------------------------

template <class BodyT>
void HttpApiServerImpl::DoWrite(Session* session, const http::response<BodyT>& res)
{
    HTTP_TRACE("DoWrite %s", session->name_.c_str());

    // Store the response in an type erased container to keep it alive until the write has completed
    auto sp = std::make_shared<http::response<BodyT>>(res);
    session->res_ = sp;

    http::async_write(
        *session->stream_, *sp, beast::bind_front_handler(&HttpApiServerImpl::OnWrite, this, session, sp->need_eof()));
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::OnWrite(
    Session* session, const bool close, const beast::error_code ec, const std::size_t bytes_transferred)
{
    HTTP_TRACE("OnWrite %s %s %s %" PRIuMAX, session->name_.c_str(), string::ToStr(close), ec.message().c_str(),
        bytes_transferred);
    UNUSED(bytes_transferred);

    // Destroy response
    session->res_.reset();

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Failed
    else if (ec) {
        HTTP_WARNING("%s write: %s", session->name_.c_str(), ec.message().c_str());
        CloseSession(session);
    }
    // Success
    else {
        if (close) {
            CloseSession(session);
        } else {
            DoRead(session);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::CloseSession(Session* session, const bool remove)
{
    HTTP_TRACE("CloseSession %s", session->name_.c_str());
    boost::system::error_code ec;

    if (session->stream_) {
        auto& socket = session->stream_->socket();

        if (socket.is_open()) {
            // Cancel pending async_read()s (and any other operation)
            socket.cancel(ec);
            if (ec) {
                HTTP_WARNING("%s socket cancel: %s", session->name_.c_str(), ec.message().c_str());
            }

            // Actively shutdown connection
            socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            if (ec) {
                HTTP_WARNING("%s socket shutdown: %s", session->name_.c_str(), ec.message().c_str());
            }
        }
        session->stream_.reset();
    } else if (session->ws_ && session->ws_->is_open()) {
        session->ws_->close(boost::beast::websocket::close_code::going_away, ec);
        if (ec) {
            HTTP_WARNING("%s ws close: %s", session->name_.c_str(), ec.message().c_str());
        }
        session->ws_.reset();
    }

    HTTP_TRACE("CloseSession %s done", session->name_.c_str());

    if (remove) {
        std::unique_lock<std::mutex> lock(mutex_);
        sessions_.erase(session->name_);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

template <class BodyT, class AllocatorT>
void HttpApiServerImpl::DoAcceptWs(Session* session, http::request<BodyT, http::basic_fields<AllocatorT>> req)
{
    HTTP_TRACE("DoAcceptWs %s %s", session->name_.c_str(), session->ws_path_.c_str());

    session->ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    session->ws_->set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) { res.set(http::field::server, GetUserAgentStr()); }));
    session->ws_->read_message_max(MAX_JSON_STR);

    session->ws_->async_accept(req, beast::bind_front_handler(&HttpApiServerImpl::OnAcceptWs, this, session));
}

void HttpApiServerImpl::OnAcceptWs(Session* session, const beast::error_code ec)
{
    HTTP_TRACE("OnAcceptWs %s %s %s", session->name_.c_str(), session->ws_path_.c_str(), ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Failed
    else if (ec) {
        HTTP_WARNING("%s %s ws accept: %s", session->name_.c_str(), session->ws_path_.c_str(), ec.message().c_str());
        CloseSession(session);
    }
    // Success
    else {
        HTTP_DEBUG("%s websocket %s", session->name_.c_str(), session->ws_path_.c_str());
        DoReadWs(session);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::DoReadWs(Session* session)
{
    HTTP_TRACE("DoReadWs %s %s", session->name_.c_str(), session->ws_path_.c_str());

    session->ws_->async_read(
        session->rx_buffer_, beast::bind_front_handler(&HttpApiServerImpl::OnReadWs, this, session));
}

void HttpApiServerImpl::OnReadWs(Session* session, const beast::error_code ec, const std::size_t bytes_transferred)
{
    HTTP_TRACE("OnReadWs %s %s %s %" PRIuMAX, session->name_.c_str(), session->ws_path_.c_str(), ec.message().c_str(),
        bytes_transferred);
    UNUSED(bytes_transferred);

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Failed
    else if (ec) {
        // This means they closed the connection, or stopped using it
        if (ec == websocket::error::closed) {
            HTTP_DEBUG(
                "%s %s ws disconnect: %s", session->name_.c_str(), session->ws_path_.c_str(), ec.message().c_str());
        } else {
            HTTP_WARNING("%s %s ws read: %s", session->name_.c_str(), session->ws_path_.c_str(), ec.message().c_str());
        }
        CloseSession(session);
    }
    // We have received a message
    else {
        HandleRequestWs(session);
        DoReadWs(session);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::HandleRequestWs(Session* session)
{
    HTTP_TRACE("HandleRequestWs %s %s %s %" PRIuMAX " %s", session->name_.c_str(), session->ws_path_.c_str(),
        string::ToStr(session->ws_->got_text()), session->rx_buffer_.data().size(),
        string::ToStr(session->ws_->is_message_done()));
    // TRACE_HEXDUMP((const uint8_t*)session->rx_buffer_.data().data(), session->rx_buffer_.size(), NULL,
    //     "HandleRequestWs %s %s req", session->name_.c_str(), session->ws_path_.c_str());

    // Wait for full message
    // @todo is this how it works?
    if (!session->ws_->is_message_done()) {
        DoReadWs(session);
        return;
    }

    // @todo what about pings?

    Request api_req;
    api_req.method_ = Method::WS;
    api_req.path_ = session->ws_path_;
    api_req.session_ = session->name_;
    bool ok = false;

    Response api_res;

    // We only accept valid JSON
    if (session->ws_->got_text()) {
        auto handler = handlers_ws_.find(session->ws_path_);  // should always succeed..
        if (handler != handlers_ws_.end()) {
            bool json_ok = false;
            try {
                const char* str_data = (const char*)session->rx_buffer_.data().data();
                std::string json{ str_data, str_data + session->rx_buffer_.data().size() };
                session->rx_buffer_.consume(session->rx_buffer_.data().size());
                api_req.data_ = nlohmann::json::parse(json);
                json_ok = true;
            } catch (std::exception& ex) {
                api_res.error_ = std::string("bad json data: ") + ex.what();
            }
            if (json_ok) {
                try {
                    if (handler->second(api_req, api_res)) {
                        ok = true;
                    } else if (api_res.error_.empty()) {
                        api_res.error_ = "handler fail";
                    }
                } catch (std::exception& ex) {
                    HTTP_WARNING("handler crashed: %s", ex.what());
                    api_res.error_ = "handler crash";
                }
            }
        }
    } else if (session->ws_->got_binary()) {
        api_res.error_ = "binary messages not supported";
    } else {
        api_res.error_ = "unknown websocket event";
    }

    if (!ok && api_res.error_.empty()) {
        api_res.error_ = "internal server error";
    }

    // Use error unless handler not provided a response
    if (api_res.body_.empty() && !api_res.error_.empty()) {
        api_res.body_ = string::StrToBuf(nlohmann::json({ { "error", api_res.error_ } }).dump());
        api_res.type_ = CONTENT_TYPE_JSON;
    }

    // Prepare response and add it to the tx queue
    if (ok) {
        HTTP_DEBUG("%s %s", session->name_.c_str(), session->ws_path_.c_str());
    } else {
        HTTP_WARNING("%s %s: %s", session->name_.c_str(), session->ws_path_.c_str(), api_res.error_.c_str());
    }
    // TRACE_HEXDUMP(api_res.body_.data(), api_res.body_.size(), NULL, "HandleRequestWs %s %s res",
    // session->name_.c_str(),
    //     session->ws_path_.c_str());

    // We're not sending empty messages
    if (!api_res.body_.empty()) {
        std::unique_lock<std::mutex> lock(session->ws_mutex_);
        session->ws_queue_.push(api_res.body_);
        // Initiate write if this was the first
        if (session->ws_queue_.size() == 1) {
            DoWriteWs(session);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::DoWriteWs(Session* session)
{
    const auto& next = session->ws_queue_.front();
    HTTP_TRACE("DoWriteWs %s %s %s %" PRIuMAX " %" PRIuMAX, session->name_.c_str(), session->ws_path_.c_str(),
        string::ToStr(session->ws_->text()), next.size(), session->ws_queue_.size());

    session->ws_->async_write(boost::asio::buffer(next.data(), next.size()),
        beast::bind_front_handler(&HttpApiServerImpl::OnWriteWs, this, session));
}

void HttpApiServerImpl::OnWriteWs(Session* session, const beast::error_code ec, const std::size_t bytes_transferred)
{
    HTTP_TRACE("OnWriteWs %s %s %s %" PRIuMAX, session->name_.c_str(), session->ws_path_.c_str(), ec.message().c_str(),
        bytes_transferred);
    UNUSED(bytes_transferred);

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Failed
    else if (ec) {
        // This means they closed the connection, or stopped using it
        if (ec == websocket::error::closed) {
            HTTP_DEBUG(
                "%s %s ws disconnect: %s", session->name_.c_str(), session->ws_path_.c_str(), ec.message().c_str());
        } else {
            HTTP_WARNING("%s %s ws read: %s", session->name_.c_str(), session->ws_path_.c_str(), ec.message().c_str());
        }
        CloseSession(session);
    }
    // Write complete
    else {
        std::unique_lock<std::mutex> lock(session->ws_mutex_);
        session->ws_queue_.pop();
        if (!session->ws_queue_.empty()) {
            DoWriteWs(session);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::SendWs(const std::string& path, const Response& res)
{
    for (auto& entry : sessions_) {
        auto& session = entry.second;
        if (session->ws_path_ == path) {
            {
                std::unique_lock<std::mutex> lock(session->ws_mutex_);
                session->ws_queue_.push(res.body_);
            }
            if (session->ws_queue_.size() == 1) {
                DoWriteWs(session.get());
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void HttpApiServerImpl::HttpLoggingPrint(
    const fpsdk::common::logging::LoggingLevel level, const std::size_t repeat, const char* fmt, ...) const
{
    if (!fpsdk::common::logging::LoggingIsLevel(level)) {
        return;
    }

    char line[1000];
    const int len = std::snprintf(line, sizeof(line), "HttpApiServer(%s) ", name_.c_str());

    va_list args;
    va_start(args, fmt);
    std::vsnprintf(&line[len], sizeof(line) - len, fmt, args);
    va_end(args);

    fpsdk::common::logging::LoggingPrint(level, repeat, "%s", line);
}

/* ****************************************************************************************************************** */

/*static*/ std::unique_ptr<HttpApiServer> HttpApiServer::Create(const std::string& path)
{
    bool ok = true;
    HttpApiServerOpts opts;
    const auto parts = string::StrSplit(path, "/");
    if ((parts.size() == 1) || (parts.size() == 2)) {
        if (parts.size() == 2) {
            opts.prefix_ = std::string("/") + parts[1];
        }
        if (!MatchHostPortPath(parts[0], opts.host_, opts.port_, opts.ipv6_, false)) {
            ok = false;
        }
    }
    if (ok) {
        auto server = std::make_unique<HttpApiServerImpl>(opts);
        return server;
    } else {
        WARNING("HttpApiServer() bad path %s", path.c_str());
        return nullptr;
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
