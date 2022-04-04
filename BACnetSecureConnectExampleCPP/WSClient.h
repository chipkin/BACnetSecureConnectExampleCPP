#pragma once

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

// SSL
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <boost/bind.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

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

//
// WSClientBase
// ----------------------------------------------------------------------------
//
class WSClientBase {
private:
public:
    virtual bool IsConnected() = 0;
    virtual bool Connect(const WSURI uri, uint8_t *errorCode) = 0;
    virtual void Disconnect() = 0;
    virtual size_t SendWSMessage(const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode) = 0;
    virtual size_t RecvWSMessage(uint8_t *message, const uint16_t maxMessageLength, uint8_t *errorCode) = 0;
};

//
// WSClient
// ----------------------------------------------------------------------------
// Based off of https://github.com/boostorg/beast/tree/develop/example/websocket/client/sync
class WSClientUnsecure : public WSClientBase {
private:
    // The io_context is required for all I/O
    net::io_context ioc;

    websocket::stream<tcp::socket> *m_ws;

public:
    WSClientUnsecure();
    bool IsConnected();
    bool Connect(const WSURI uri, uint8_t *errorCode);
    void Disconnect();
    size_t SendWSMessage(const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode);
    size_t RecvWSMessage(uint8_t *message, const uint16_t maxMessageLength, uint8_t *errorCode);
};

//
// WSClientSecure
// ----------------------------------------------------------------------------
// Based off of https://github.com/boostorg/beast/tree/develop/example/websocket/client/sync-ssl
class WSClientSecure : public WSClientBase {
private:
    // The io_context is required for all I/O
    net::io_context ioc;

    // The SSL Context for configuring SSL options
    ssl::context *ctx;

    ssl::stream<tcp::socket> *m_wss;

    // Uri info
    bool isRawIP;
    uint8_t ipOctetString[4];

public:
    WSClientSecure();
    bool IsConnected();
    bool Connect(const WSURI uri, uint8_t *errorCode);
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
    bool AddConnection(const WSURI uri, uint8_t *errorCode);
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