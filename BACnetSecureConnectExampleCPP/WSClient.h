#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#include <map>
#include <string>

typedef std::string WSURI;


//
// WSClient
// ----------------------------------------------------------------------------
// 
class WSClient {
private:

    // The io_context is required for all I/O
    net::io_context ioc;

    websocket::stream<tcp::socket>* m_ws;

public:

    WSClient();
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
    std::map<WSURI, WSClient> clients;

    // Check to see if this connection exists
    WSClient* GetWSClient(WSURI uri);

public:

    bool AddConnection(const WSURI uri);
    void RemoveConnection(const WSURI uri);
    size_t SendWSMessage(const WSURI uri, const uint8_t* message, const uint16_t messageLength);
    size_t RecvWSMessage(const WSURI uri, uint8_t* message, const uint16_t maxMessageLength);
};

