#pragma once

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/buffers_iterator.hpp>

// SSL
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/bind.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#include <map>
#include <string>

typedef std::string WSURI;

#define WEB_SOCKET_DEFAULT_PORT_NOT_SECURE "80"
#define WEB_SOCKET_DEFAULT_PORT_SECURE "443"
#define IOC_THREADS 1
#define READ_THREADS 1

//
// WSClientBase
// ----------------------------------------------------------------------------
//
class WSClientBase{
private:
public:
    virtual bool IsConnected() = 0;
    virtual bool Connect(const WSURI uri, uint8_t *errorCode) = 0;
    virtual void Disconnect() = 0;
    virtual size_t SendWSMessage(const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode) = 0;
    virtual size_t RecvWSMessage(uint8_t *message, const uint16_t maxMessageLength, uint8_t *errorCode) = 0;
};

//
// WSClientAsync
// ----------------------------------------------------------------------------
// Based off of https://www.boost.org/doc/libs/develop/libs/beast/example/websocket/client/async/websocket_client_async.cpp
class WSClientUnsecureAsync : public std::enable_shared_from_this<WSClientUnsecureAsync> {
private:
    tcp::resolver resolver;
    websocket::stream<beast::tcp_stream> ws;
    std::string host;
    std::string port;
    uint8_t errorCode;

    // NOTE: io_context will use one thread to handle the websocket, 24/7. ioc->run() will block until websocket is closed.
    // Use a separate thread for ioc->run().
    net::io_context* ioc;
    std::vector<std::thread> threads;   // Set IOC_THREADS to 1 for now

    beast::flat_buffer buffer;
    size_t bytesWritten;
    uint8_t bufArr[1024];
    bool readPending;

    // Locks for write operation
    std::mutex writeLenMtx;

    // Queue for messages
    std::queue<std::string> messageQueue;
    std::mutex messageQueueMtx;
    std::mutex notifyRead;

    // Async functions
    void onResolve(beast::error_code errorCode, tcp::resolver::results_type results);
    void onConnect(beast::error_code errorCode, tcp::resolver::results_type::endpoint_type endpoint);
    void onHandshake(beast::error_code errorCode);
    void onWrite(beast::error_code errorCode, std::size_t bytesWritten);
    void onRead(beast::error_code errorCode, std::size_t bytesRead);
    void onClose(beast::error_code errorCode);

public:
    // NOTE: beast does not allow multiple calls of the same async function at the same time:
    // soft_mutex.cpp:83:
    //      If this assert goes off it means you are attempting to
    //      simultaneously initiate more than one of same asynchronous
    //      operation, which is not allowed. For example, you must wait
    //      for an async_read to complete before performing another
    //      async_read.

    // Conditional variables and locks
    // Variables for Connect needs to be public, unless we make explicit functions for setup and wait
    // Leave in public space for now, all abstracted by WSNetworkLayer anyways
    std::condition_variable writeCv;
    std::condition_variable closeCv;
    std::condition_variable connectCv;
    std::mutex writeMtx;
    std::mutex closeMtx;
    std::mutex connectMtx;
    bool writeDone;
    bool closeDone;
    bool connectDone;

    // Constructor
    explicit WSClientUnsecureAsync(net::io_context& ioc)
        : resolver(net::make_strand(ioc))
        , ws(net::make_strand(ioc)) {
        this->errorCode = 0;
        this->ioc = &ioc;
        this->readPending = false;
    }

    // Functions
    void run(const WSURI uri);
    void doRead();
    void doWrite(const uint8_t* message, const uint16_t messageLength);       // NOTE: getReadMessage() must be called after onRead()
    void doClose();


    // Getters
    size_t getBytesWritten();
    size_t pollQueue(uint8_t* message, uint16_t maxMessageLength, uint8_t* errorCode);
    uint8_t getAndResetErrorCode();

    // Status
    bool IsConnected();
};

//
// WSUnsecureClient
// ----------------------------------------------------------------------------
// Wraps WSUnsecureClientAsync into a synchronous interface
class WSClientUnsecure : public WSClientBase {
private:
    std::shared_ptr<WSClientUnsecureAsync> async_ws;        // shared_ptr for threading
    net::io_context ioc;
    net::executor_work_guard<boost::asio::io_context::executor_type> iocWorkGuard = boost::asio::make_work_guard(ioc);

    std::vector<std::thread> threads;   // Set IOC_THREADS to 1 for now

public:
    WSClientUnsecure();
    bool IsConnected();
    bool Connect(const WSURI uri, uint8_t* errorCode);
    void Disconnect();
    size_t SendWSMessage(const uint8_t* message, const uint16_t messageLength, uint8_t* errorCode);
    size_t RecvWSMessage(uint8_t* message, const uint16_t maxMessageLength, uint8_t* errorCode);
};

//
// WSClientAsync
// ----------------------------------------------------------------------------
// Based off of https://www.boost.org/doc/libs/develop/libs/beast/example/websocket/client/async-ssl/websocket_client_async_ssl.cpp
class WSClientSecureAsync : public std::enable_shared_from_this<WSClientSecureAsync> {
private:
    tcp::resolver resolver;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws;
    std::string host;
    std::string port;
    uint8_t errorCode;

    // NOTE: io_context will use one thread to handle the websocket, 24/7. ioc->run() will block until websocket is closed.
    // Use a separate thread for ioc->run().
    net::io_context* ioc;
    ssl::context* ctx;
    std::vector<std::thread> threads;   // Set IOC_THREADS to 1 for now

    beast::flat_buffer buffer;
    size_t bytesWritten;
    uint8_t bufArr[1024];
    bool readPending;

    // Locks for write operation
    std::mutex writeLenMtx;

    // Queue for messages
    std::queue<std::string> messageQueue;
    std::mutex messageQueueMtx;
    std::mutex notifyRead;

    // Async functions
    void onResolve(beast::error_code errorCode, tcp::resolver::results_type results);
    void onConnect(beast::error_code errorCode, tcp::resolver::results_type::endpoint_type endpoint);
    void onSslHandshake(beast::error_code errorCode);
    void onHandshake(beast::error_code errorCode);
    void onWrite(beast::error_code errorCode, std::size_t bytesWritten);    // NOTE: getBytesWritten() must be called after onWrite()
    void onRead(beast::error_code errorCode, std::size_t bytesRead);        // NOTE: getReadMessage() must be called after onRead()
    void onClose(beast::error_code errorCode);

public:
    // NOTE: beast does not allow multiple calls of the same async function at the same time:
    // soft_mutex.cpp:83:
    //      If this assert goes off it means you are attempting to
    //      simultaneously initiate more than one of same asynchronous
    //      operation, which is not allowed. For example, you must wait
    //      for an async_read to complete before performing another
    //      async_read.

    // Conditional variables and locks
    // Conditional variables and locks
    // Variables for Connect needs to be public, unless we make explicit functions for setup and wait
    // Leave in public space for now, all abstracted by WSNetworkLayer anyways
    std::condition_variable writeCv;
    std::condition_variable closeCv;
    std::condition_variable connectCv;
    std::mutex writeMtx;
    std::mutex closeMtx;
    std::mutex connectMtx;
    bool writeDone;
    bool closeDone;
    bool connectDone;

    // Constructor
    explicit WSClientSecureAsync(net::io_context& ioc, ssl::context& ctx)
        : resolver(net::make_strand(ioc))
        , ws(net::make_strand(ioc), ctx) {
        this->errorCode = 0;
        this->ioc = &ioc;
        this->ctx = &ctx;
        this->readPending = false;
    }

    // Getters
    size_t getBytesWritten();
    size_t pollQueue(uint8_t* message, uint16_t maxMessageLength, uint8_t* errorCode);
    uint8_t getAndResetErrorCode();

    // Functions
    void run(const WSURI uri);
    void doWrite(const uint8_t* message, const uint16_t messageLength);
    void doRead();
    void doClose();

    // Status
    bool IsConnected();
};

//
// WSClientSecure
// ----------------------------------------------------------------------------
// Based off of https://github.com/boostorg/beast/tree/develop/example/websocket/client/sync-ssl
class WSClientSecure : public WSClientBase {
private:
    std::shared_ptr<WSClientSecureAsync> async_ws;        // shared_ptr for threading
    net::io_context ioc;
    ssl::context ctx{ssl::context::tlsv13_client};
    net::executor_work_guard<boost::asio::io_context::executor_type> iocWorkGuard = boost::asio::make_work_guard(ioc);

    std::vector<std::thread> threads;   // Set IOC_THREADS to 1 for now

    std::string m_cert;
    std::string m_key;

public:
    WSClientSecure();
    WSClientSecure(const std::string& certFilename, const std::string& keyFilename);
    bool IsConnected();
    bool Connect(const WSURI uri, uint8_t* errorCode);
    void Disconnect();
    size_t SendWSMessage(const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode);
    size_t RecvWSMessage(uint8_t *message, const uint16_t maxMessageLength, uint8_t *errorCode);
};

//
// WSNetworkLayer
// ----------------------------------------------------------------------------
// Allows for a collection of WSClients.
//
class WSNetworkLayer {
private:
    std::map<WSURI, WSClientBase *> clients;

    // Check to see if this connection exists
    WSClientBase *GetWSClient(WSURI uri);

public:
    bool AddConnection(const WSURI uri, uint8_t *errorCode, const std::string& certFilename = "", const std::string& keyFilename = "");
    void RemoveConnection(const WSURI uri);
    bool IsConnected(const WSURI uri);
    size_t SendWSMessage(const WSURI uri, const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode);
    size_t RecvWSMessage(const WSURI uri, uint8_t *message, const uint16_t maxMessageLength, uint8_t *errorCode);
};

// Error Codes
static const uint8_t ERROR_DNS_UNAVAILABLE = 189;
static const uint8_t ERROR_DNS_NAME_RESOLUTION_FAILED = 190;
static const uint8_t ERROR_DNS_RESOLVER_FAILURE = 191;
static const uint8_t ERROR_DNS_ERROR = 192;
static const uint8_t ERROR_TCP_CONNECT_TIMEOUT = 193;
static const uint8_t ERROR_TCP_CONNECTION_REFUSED = 194;
static const uint8_t ERROR_TCP_CLOSED_BY_LOCAL = 195;
static const uint8_t ERROR_TCP_CLOSED_OTHER = 196;
static const uint8_t ERROR_TCP_ERROR = 197;
static const uint8_t ERROR_IP_ADDRESS_NOT_REACHABLE = 198;
static const uint8_t ERROR_IP_ERROR = 199;
static const uint8_t ERROR_TLS_CLIENT_CERTIFICATE_ERROR = 180;
static const uint8_t ERROR_TLS_SERVER_CERTIFICATE_ERROR = 181;
static const uint8_t ERROR_TLS_CLIENT_AUTHENTICATION_FAILED = 182;
static const uint8_t ERROR_TLS_SERVER_AUTHENTICATION_FAILED = 183;
static const uint8_t ERROR_TLS_CLIENT_CERTIFICATE_EXPIRED = 184;
static const uint8_t ERROR_TLS_SERVER_CERTIFICATE_EXPIRED = 185;
static const uint8_t ERROR_TLS_CLIENT_CERTIFICATE_REVOKED = 186;
static const uint8_t ERROR_TLS_SERVER_CERTIFICATE_REVOKED = 187;
static const uint8_t ERROR_TLS_ERROR = 188;
static const uint8_t ERROR_HTTP_ERROR = 165;
static const uint8_t ERROR_HTTP_NO_UPGRADE = 153;
static const uint8_t ERROR_HTTP_NOT_A_SERVER = 164;
static const uint8_t ERROR_HTTP_RESOURCE_NOT_LOCAL = 154;
static const uint8_t ERROR_HTTP_PROXY_AUTHENTICATION_FAILED = 155;
static const uint8_t ERROR_HTTP_RESPONSE_TIMEOUT = 156;
static const uint8_t ERROR_HTTP_RESPONSE_SYNTAX_ERROR = 157;
static const uint8_t ERROR_HTTP_RESPONSE_VALUE_ERROR = 158;
static const uint8_t ERROR_HTTP_RESPONSE_MISSING_HEADER = 159;
static const uint8_t ERROR_HTTP_TEMPORARY_UNAVAILABLE = 163;
static const uint8_t ERROR_HTTP_UNEXPECTED_RESPONSE_CODE = 152;
static const uint8_t ERROR_HTTP_UPGRADE_REQUIRED = 161;
static const uint8_t ERROR_HTTP_UPGRADE_ERROR = 162;
static const uint8_t ERROR_HTTP_WEBSOCKET_HEADER_ERROR = 160;

// Helper functions
class WSCommon {
public:
    static std::string HexStringToString(std::string hexString);
};