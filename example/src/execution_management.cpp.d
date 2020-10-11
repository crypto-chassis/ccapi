//#include <boost/beast/example/common/root_certificates.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include "websocketpp/config/asio_client.hpp"
#include "websocketpp/common/connection_hdl.hpp"
#include "websocketpp/client.hpp"
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace wspp = websocketpp;
//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Performs an HTTP GET and prints the response
class session : public std::enable_shared_from_this<session>
{
    tcp::resolver resolver_;
    beast::ssl_stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::empty_body> req_;
    http::response<http::string_body> res_;

public:
    explicit
    session(
        net::any_io_executor ex,
        ssl::context& ctx)
    : resolver_(ex)
    , stream_(ex, ctx)
    {
    }

    // Start the asynchronous operation
    void
    run(
        char const* host,
        char const* port,
        char const* target,
        int version)
    {
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if(! SSL_set_tlsext_host_name(stream_.native_handle(), host))
        {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            std::cerr << ec.message() << "\n";
            return;
        }

        // Set up an HTTP GET request message
        req_.version(version);
        req_.method(http::verb::get);
        req_.target(target);
        req_.set(http::field::host, host+":"+port);
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req_.set("Connection","keep-alive");
        req_.set("Keep-Alive","timeout=5, max=1000");

        // Look up the domain name
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &session::on_resolve,
                shared_from_this()));
    }

    void
    on_resolve(
        beast::error_code ec,
        tcp::resolver::results_type results)
    {
        if(ec)
            return fail(ec, "resolve");

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream_).async_connect(
            results,
            beast::bind_front_handler(
                &session::on_connect,
                shared_from_this()));
    }

    void
    on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if(ec)
            return fail(ec, "connect");

        // Perform the SSL handshake
        stream_.async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(
                &session::on_handshake,
                shared_from_this()));
    }

    void
    on_handshake(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "handshake");

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        http::async_write(stream_, req_,
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }

    void
    on_write(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        // Receive the HTTP response
        http::async_read(stream_, buffer_, res_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "read");

        // Write the message to standard out
        std::cout << res_ << std::endl;

        // Set a timeout on the operation
//        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
//
//        // Gracefully close the stream
//        stream_.async_shutdown(
//            beast::bind_front_handler(
//                &session::on_shutdown,
//                shared_from_this()));
    }

    void
    on_shutdown(beast::error_code ec)
    {
        if(ec == net::error::eof)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec = {};
        }
        if(ec)
            return fail(ec, "shutdown");

        // If we get here then the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 4 && argc != 5)
    {
        std::cerr <<
            "Usage: http-client-async-ssl <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
            "Example:\n" <<
            "    http-client-async-ssl www.example.com 443 /\n" <<
            "    http-client-async-ssl www.example.com 443 / 1.0\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const target = argv[3];
    int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;
        typedef wspp::lib::asio::io_service IoContext;
        struct CustomClientConfig : public wspp::config::asio_tls_client {
          #ifdef WEBSOCKETPP_ENABLE_SINGLE_THREADING
            typedef wspp::config::asio_tls_client base;
            static bool const enable_multithreading = false;
            struct transport_config : public base::transport_config {
              static bool const enable_multithreading = false;
            };
          #endif
        };
//         The io_context is required for all I/O
        net::io_context ioc;
        typedef wspp::client<CustomClientConfig> TlsClient;
        TlsClient tlsClient;
        typedef wspp::lib::error_code ErrorCode;
        ErrorCode ec;
        tlsClient.init_asio(&ioc, ec);
        if (ec) {
//          CCAPI_LOGGER_FATAL("asio initialization error: " + ec.message());
        }
//        CCAPI_LOGGER_DEBUG("asio initialization end");
        tlsClient.start_perpetual();

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::sslv23};
    ctx.set_options(
            ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::no_sslv3
                | ssl::context::single_dh_use);
        ctx.set_verify_mode(ssl::verify_none);
    // This holds the root certificate used for verification
//    load_root_certificates(ctx);

    // Verify the remote server's certificate
    ctx.set_verify_mode(ssl::verify_none);

    // Launch the asynchronous operation
    // The session is constructed with a strand to
    // ensure that handlers do not execute concurrently.
    std::make_shared<session>(
        net::make_strand(ioc),
        ctx
        )->run(host, port, target, version);

    // Run the I/O service. The call will return when
    // the get operation is complete.
//    ioc.run();
    tlsClient.run();

    return EXIT_SUCCESS;
}
