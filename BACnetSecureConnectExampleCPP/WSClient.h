#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

/*
// SSL 
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
*/

#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
// namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>

#include <map>
#include <string>

typedef std::string WSURI;

#define WEB_SOCKET_DEFAULT_PORT_NOT_SECURE  "80"
#define WEB_SOCKET_DEFAULT_PORT_SECURE      "443"


//
// WSClientBase
// ----------------------------------------------------------------------------
// 
class WSClientBase {
private:

public:
    
    virtual bool IsConnected() = 0;
    virtual bool Connect(const WSURI uri) = 0;
    virtual void Disconnect() = 0;
    virtual size_t SendWSMessage(const uint8_t* message, const uint16_t messageLength) = 0;
    virtual size_t RecvWSMessage(uint8_t* message, const uint16_t maxMessageLength) = 0;
};

//
// WSClient
// ----------------------------------------------------------------------------
// Based off of https://github.com/boostorg/beast/tree/develop/example/websocket/client/sync 
class WSClientUnsecure : public WSClientBase {
private:

    // The io_context is required for all I/O
    net::io_context ioc;

    websocket::stream<tcp::socket>* m_ws;

public:

    WSClientUnsecure();
    bool IsConnected();
    bool Connect(const WSURI uri);
    void Disconnect();
    size_t SendWSMessage(const uint8_t* message, const uint16_t messageLength);
    size_t RecvWSMessage(uint8_t* message, const uint16_t maxMessageLength);
};

//
// WSClientSecure
// ----------------------------------------------------------------------------
// Based off of https://github.com/boostorg/beast/tree/develop/example/websocket/client/sync-ssl 
class WSClientSecure : public WSClientBase {
private:

    // The io_context is required for all I/O
    net::io_context ioc;

    // websocket::stream<tcp::socket>* m_wss;

public:

    WSClientSecure();
    bool IsConnected();
    bool Connect(const WSURI uri);
    void Disconnect();
    size_t SendWSMessage(const uint8_t* message, const uint16_t messageLength);
    size_t RecvWSMessage(uint8_t* message, const uint16_t maxMessageLength);
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
    WSClientBase* GetWSClient(WSURI uri);

public:

    bool AddConnection(const WSURI uri);
    void RemoveConnection(const WSURI uri);
    bool IsConnected(const WSURI uri);
    size_t SendWSMessage(const WSURI uri, const uint8_t* message, const uint16_t messageLength);
    size_t RecvWSMessage(const WSURI uri, uint8_t* message, const uint16_t maxMessageLength);
};

