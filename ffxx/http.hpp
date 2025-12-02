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
 *
 * @page FFXX_HTTP HTTP utilities
 *
 */
#ifndef __FFXX_HTTP_HPP__
#define __FFXX_HTTP_HPP__

/* LIBC/STL */
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/* EXTERNAL */
#include <nlohmann/json.hpp>

/* PACKAGE */

namespace ffxx {
/* ****************************************************************************************************************** */

/**
 * @brief Basic HTTP server for apps to provide simple APIs
 *
 * Request must be valid and non-empty JSON (while "{}" and "[]" are valid JSON, they are empty and therefore not
 * considered valid request data. Data can only be used with POST and WS requests, GET requests do not carry any data
 * (GET parameters are ignored). WS input messages must be of type text and contain valid and non-empty JSON.
 */
class HttpApiServer
{
   public:
    /**
     * @brief Constructor
     *
     * Do not use, use Create() to create a server instance
     */
    HttpApiServer() = default;

    /**
     * @brief Destructor
     */
    virtual ~HttpApiServer() = default;

    /**
     * @brief Create server
     *
     * The path is in the form <code>[&lt;host>]:&lt;port>[/&lt;prefix>]</code> where `<host>` is the address (`<IPv4>`
     * or `[<IPv6>]`) or the hostname, or empty to bind to all interfaces, `<port>` is the port number, and `<prefix>`
     * is an optional prefix to strip (ignore) from the path in requests.
     *
     * @param[in]  path     Path
     */
    static std::unique_ptr<HttpApiServer> Create(const std::string& path);

    /**
     * @brief Start the server
     *
     * @returns true if the server was started, false otherwise
     */
    virtual bool Start() = 0;

    /**
     * @brief Stop server
     */
    virtual void Stop() = 0;

    /**
     * @brief Request method
     */
    enum class Method
    {
        GET,   //!< GET request
        POST,  //!< POST request
        WS,    //!< Websocket message
    };

    /**
     * @brief A request
     */
    struct Request
    {
        Method method_;        //!< Request method
        std::string path_;     //!< The requested path (e.g. "/", "/api/foo", ...)
        nlohmann::json data_;  //!< POST data (empty if GET request)
        std::string session_;  //!< A string identifying the session (the remote, the client)
    };

    /**
     * @brief A HttpApiServer response
     */
    struct Response
    {
        std::string type_;           //!< The content type (e.g. "application/json")
        std::vector<uint8_t> body_;  //!< The data (e.g. "[ 1, 2, 3 ]")
        std::string error_;          //!< Error message (used when handler returns false and body_ is empty)
    };

    /**
     * @brief Request handler function signature
     *
     * @param[in]  req  The request
     * @param[out] res  The response
     *
     * @return true if the request was handled, false if not (invalid path, i.e. "404 not found")
     */
    using Handler = std::function<bool(const Request& req, Response& res)>;

    /**
     * @brief Set handler for method and path
     *
     * @param[in]  method   The HTTP method
     * @param[in]  path     The path to handle (e.g. "/foo")
     * @param[in]  handler  The handler function
     */
    virtual void SetHandler(const Method method, const std::string& path, Handler handler) = 0;

    /**
     * @brief Send to all websockets
     *
     * @param[in]  path  The path, as registered by SetHandler(Method::WS, ...)
     * @param[in]  res   The data to send
     */
    virtual void SendWs(const std::string& path, const Response& res) = 0;

    static constexpr std::size_t MAX_JSON_STR = 100000;                   //!< Maximum size of JSON data
    static constexpr const char* CONTENT_TYPE_JSON = "application/json";  //!< Content-type value
    static constexpr const char* CONTENT_TYPE_HTML = "text/html";         //!< Content-type value
    static constexpr const char* CONTENT_TYPE_CSS = "text/css";           //!< Content-type value
    static constexpr const char* CONTENT_TYPE_JS = "text/javascript";     //!< Content-type value
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_HTTP_HPP__
