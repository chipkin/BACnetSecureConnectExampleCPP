#include "WSClient.h"
#include <boost/asio/ssl/host_name_verification.hpp>    // Explicit include - don't know why Visual Studio does not detect this


// 
// Uri
// ----------------------------------------------------------------------------
// https://stackoverflow.com/a/11044337
// 

struct Uri
{
public:
    std::string QueryString, Path, Protocol, Host, Port;

    static Uri Parse(const std::string& uri)
    {
        Uri result;

        typedef std::string::const_iterator iterator_t;

        if (uri.length() == 0)
            return result;

        iterator_t uriEnd = uri.end();

        // get query start
        iterator_t queryStart = std::find(uri.begin(), uriEnd, '?');

        // protocol
        iterator_t protocolStart = uri.begin();
        iterator_t protocolEnd = std::find(protocolStart, uriEnd, ':'); //"://");

        if (protocolEnd != uriEnd)
        {
            std::string prot = &*(protocolEnd);
            if ((prot.length() > 3) && (prot.substr(0, 3).compare("://") == 0))
            {
                result.Protocol = std::string(protocolStart, protocolEnd);
                protocolEnd += 3;   //      ://
            }
            else
                protocolEnd = uri.begin();  // no protocol
        }
        else
            protocolEnd = uri.begin();  // no protocol

        // host
        iterator_t hostStart = protocolEnd;
        iterator_t pathStart = std::find(hostStart, uriEnd, '/');  // get pathStart

        iterator_t hostEnd = std::find(protocolEnd,
            (pathStart != uriEnd) ? pathStart : queryStart,
            L':');  // check for port

        result.Host = std::string(hostStart, hostEnd);

        // port
        if ((hostEnd != uriEnd) && ((&*(hostEnd))[0] == ':'))  // we have a port
        {
            hostEnd++;
            iterator_t portEnd = (pathStart != uriEnd) ? pathStart : queryStart;
            result.Port = std::string(hostEnd, portEnd);
        }

        // path
        if (pathStart != uriEnd)
            result.Path = std::string(pathStart, queryStart);

        // query
        if (queryStart != uriEnd)
            result.QueryString = std::string(queryStart, uri.end());

        return result;

    }   // Parse
};  // uri




// 
// WSClientUnsecure
// ----------------------------------------------------------------------------


WSClientUnsecure::WSClientUnsecure() {
    this->m_ws = NULL;
}

bool WSClientUnsecure::IsConnected() {
    if (this->m_ws != NULL) {
        return this->m_ws->is_open();
    }
    return false;
}

bool WSClientUnsecure::Connect(const WSURI uri) {

    if (this->m_ws != NULL) {
        // We are connected, reconnect 
        this->Disconnect();
    }

    // Extract the parts from the uri 
    Uri uriSplit = Uri::Parse(uri);
    if (uriSplit.Port.size() <= 0) {
        uriSplit.Port = WEB_SOCKET_DEFAULT_PORT_NOT_SECURE; // Default port 80
    }

    // These objects perform our I/O
    tcp::resolver resolver{ ioc };
    this->m_ws = new websocket::stream<tcp::socket>(ioc);

    // Look up the domain name
    auto const results = resolver.resolve(uriSplit.Host, uriSplit.Port);

    // Make the connection on the IP address we get from a lookup
    auto ep = net::connect(m_ws->next_layer(), results);

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    std::string hostStr = uriSplit.Host;
    hostStr += ':' + std::to_string(ep.port());
        
    // Set a decorator to change the User-Agent of the handshake
    m_ws->set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req)
        {
            req.set(http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) +
                " websocket-client-coro");
        }));

    // Perform the websocket handshake
    m_ws->handshake(hostStr, uriSplit.Path + uriSplit.QueryString);

    // Check to see if the web socket is connected.
    return m_ws->is_open();


}
void WSClientUnsecure::Disconnect() {
    if (this->m_ws == NULL) {
        return;
    }

    // Close the WebSocket connection
    this->m_ws->close(websocket::close_code::normal);

    // Remove the socket 
    delete this->m_ws;
    this->m_ws = NULL;
}

size_t WSClientUnsecure::SendWSMessage(const uint8_t* message, const uint16_t messageLength) {
    if (this->m_ws == NULL) {
        return 0; // Not connected 
    }

    try {
        // Send the message
        return this->m_ws->write(net::buffer(message, messageLength));
    }
    catch (std::exception const& e) {
        this->Disconnect();
        std::cout << e.what() << std::endl;
        return 0;
    }
}

size_t WSClientUnsecure::RecvWSMessage(uint8_t* message, uint16_t maxMessageLength) {
    if (this->m_ws == NULL) {
        return 0; // Not connected 
    }

    try {
        beast::flat_buffer buffer;
        // Read a message into our buffer
        size_t len = this->m_ws->read(buffer);
        if (len > 0 ) {
            std::string recivedMessage = beast::buffers_to_string(buffer.data());
            if (recivedMessage.size() < maxMessageLength) {
                // Fits in the buffer 
                memcpy(message, recivedMessage.c_str(), recivedMessage.size());
                return len; 
            }            
        }
    }
    catch (std::exception const& e) {
        this->Disconnect();
        std::cout << e.what() << std::endl;
    }
    return 0;
}



// 
// WSClientSecure
// ----------------------------------------------------------------------------

// Attempting this: https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/overview/ssl.html

WSClientSecure::WSClientSecure() {
    // ToDo: 
    this->m_wss = NULL;
    this->ctx = NULL;
}

bool WSClientSecure::IsConnected() {
    // No is_open function for ssl::stream
    if (this->m_wss != NULL) {
        return true;
    }
    return false;
}

bool WSClientSecure::Connect(const WSURI uri) {
    // Parse Uri
    Uri parsedUri = Uri::Parse(uri);

    // Setup SSL context and load certificate
    this->ctx = new ssl::context(ssl::context::sslv23);
    this->ctx->load_verify_file("./cert.pem");  // NOTE: Use the same certificate as your server here (verify_mode: ssl::verify_peer)

    // Open socket and connect to remote host
    this->m_wss = new ssl::stream<tcp::socket>(this->ioc, *this->ctx);
    tcp::resolver resolver{ this->ioc };
    auto result = resolver.resolve({ parsedUri.Host, parsedUri.Port });
    try {
        net::connect(this->m_wss->next_layer(), result.begin(), result.end());
        this->m_wss->lowest_layer().set_option(tcp::no_delay(true));
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
        return false;
    }

    // Perform SSL handshake and verify remote host's certificate
    this->m_wss->set_verify_mode(ssl::verify_peer);
    this->m_wss->handshake(ssl::stream<tcp::socket>::client);

    return true; 
}
void WSClientSecure::Disconnect() {
    if (! this->IsConnected()) {
        return;
    }
    
    // Shutdown the WebSocket connection
    this->m_wss->shutdown();

    // Remove the socket
    delete this->m_wss;
    this->m_wss = NULL;
}

size_t WSClientSecure::SendWSMessage(const uint8_t* message, const uint16_t messageLength) {
    if (! this->IsConnected() ) {
        return 0; // Not connected 
    }

    try {
        // Send the message
        return this->m_wss->write_some(net::buffer(message, messageLength));
    }
    catch (std::exception const& e) {
        this->Disconnect();
        std::cout << e.what() << std::endl;
        return 0;
    }

    return 0; 
}

size_t WSClientSecure::RecvWSMessage(uint8_t* message, uint16_t maxMessageLength) {
    if (!this->IsConnected()) {
        return 0; // Not connected 
    }

    try {
        // Create buffer using message pointer directly
        net::mutable_buffer buffer(message, maxMessageLength);

        // Read a message into our message buffer
        return this->m_wss->read_some(buffer);
    }
    catch (std::exception const& e) {
        this->Disconnect();
        std::cout << e.what() << std::endl;
    }
    return 0; 
}



// 
// WSNetworkLayer
// ----------------------------------------------------------------------------

// Check to see if this connection exists
WSClientBase* WSNetworkLayer::GetWSClient(const WSURI uri) {
    if (clients.count(uri) <= 0) {
        return NULL;
    }
    else {
        return this->clients[uri];
    }
}

bool WSNetworkLayer::IsConnected(const WSURI uri) {
    // Check to see if this connection exists
    WSClientBase* ws = GetWSClient(uri);
    if (ws == NULL) {
        return false;
    }

    return ws->IsConnected();
}


bool WSNetworkLayer::AddConnection(const WSURI uri) {
    // Check to see if this connection exists
    WSClientBase* ws = GetWSClient(uri);
    if (ws != NULL) {
        return true;
    }

    // Add a new connection. 
    // -------------------------

    // Extract the parts from the uri 
    Uri uriSplit = Uri::Parse(uri);
    if (uriSplit.Protocol.compare("ws") == 0) {
        this->clients[uri] = new WSClientUnsecure(); 
        return this->clients[uri]->Connect(uri);
    }
    else if (uriSplit.Protocol.compare("wss") == 0) {
        this->clients[uri] = new WSClientSecure();
        return this->clients[uri]->Connect(uri);
    }

    // Unknown 
    std::cout << "Error: Unknown protocol. Protocol=[" << uriSplit.Protocol  << "]" << std::endl;
    return false;
}
void WSNetworkLayer::RemoveConnection(const WSURI uri) {
    // Check to see if this connection exists
    WSClientBase* ws = GetWSClient(uri);

    // Disconnect 
    if (ws != NULL) {
        ws->Disconnect();
    }

    // Safe Delete 
    delete this->clients[uri];
    this->clients[uri] = NULL;

    // Remove from client list. 
    this->clients.erase(uri);
}
size_t WSNetworkLayer::SendWSMessage(const WSURI uri, const uint8_t* message, const uint16_t messageLength) {
    // Check to see if this connection exists
    WSClientBase* ws = GetWSClient(uri);
    if (ws == NULL) {
        // Error this connection does not exist. Do not automaticly add it. 
        return 0;
    }

    // Send message
    return ws->SendWSMessage(message, messageLength);
}

size_t WSNetworkLayer::RecvWSMessage(const WSURI uri, uint8_t* message, const uint16_t maxMessageLength) {
    // Check to see if this connection exists
    WSClientBase* ws = GetWSClient(uri);
    if (ws == NULL) {
        // Error this connection does not exist. Do not automaticly add it. 
        return 0;
    }

    // Send message
    return ws->RecvWSMessage(message, maxMessageLength);
}
