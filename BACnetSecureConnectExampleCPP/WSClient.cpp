#include "WSClient.h"
#include <boost/asio/ssl/host_name_verification.hpp> // Explicit include - don't know why Visual Studio does not detect this

//
// Uri
// ----------------------------------------------------------------------------
// https://stackoverflow.com/a/11044337
//

struct Uri {
public:
    std::string QueryString, Path, Protocol, Host, Port;

    static Uri Parse(const std::string &uri) {
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

        if (protocolEnd != uriEnd) {
            std::string prot = &*(protocolEnd);
            if ((prot.length() > 3) && (prot.substr(0, 3).compare("://") == 0)) {
                result.Protocol = std::string(protocolStart, protocolEnd);
                protocolEnd += 3; //      ://
            } else
                protocolEnd = uri.begin(); // no protocol
        } else
            protocolEnd = uri.begin(); // no protocol

        // host
        iterator_t hostStart = protocolEnd;
        iterator_t pathStart = std::find(hostStart, uriEnd, '/'); // get pathStart

        iterator_t hostEnd = std::find(protocolEnd,
                                       (pathStart != uriEnd) ? pathStart : queryStart,
                                       L':'); // check for port

        result.Host = std::string(hostStart, hostEnd);

        // port
        if ((hostEnd != uriEnd) && ((&*(hostEnd))[0] == ':')) // we have a port
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

    } // Parse
};    // uri

//
// WSClientUnsecure
// ----------------------------------------------------------------------------

WSClientUnsecure::WSClientUnsecure() {
    this->async_ws = NULL;
}

bool WSClientUnsecure::IsConnected() {
    if (this->async_ws != NULL) {

        return this->async_ws->IsConnected();
    }
    return false;
}

bool WSClientUnsecure::Connect(const WSURI uri, uint8_t *errorCode) {
    if (this->IsConnected()) {
        // We are connected, reconnect
        this->async_ws->doClose();
    }

    try {
        for (int offset = 0; offset < IOC_THREADS; offset++) {
            this->threads.emplace_back([&] {
                try {
                    // Start connection
                    this->async_ws = std::make_shared<WSClientUnsecureAsync>(this->ioc);
                    this->async_ws->run(uri, errorCode);
                    std::cout << "Error: this->async_ws->run() ENDED, not supposed to end\n";
                }
                catch (std::exception& e) {
                    std::cout << "DEBUG: this->async_ws->run() EXCEPTION - " << e.what() << std::endl;
                }});
        }

        // DEBUG - stall until connected
        // TODO - replace this with conditional variable
        while (this->async_ws == NULL) {
            continue;
        }
    }
    catch (std::exception const& e) {
        // NOTE: Error code set in async for now, may produce bad errors
        std::cout << e.what() << std::endl;
        return false;
    }
    return true;
}
void WSClientUnsecure::Disconnect() {
    this->async_ws->doClose();
}

size_t WSClientUnsecure::SendWSMessage(const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode) {
    if (this->async_ws == NULL) {
        return 0; // Not connected
    }
    
    // Stall for handshake
        // TODO - replace this with conditional variable
    while (!this->async_ws->IsConnected()) {
        continue;
    }

    try {
        this->async_ws->doWrite(message, messageLength);
        return this->async_ws->getBytesWritten();
    }
    catch (std::exception const& e) {
        // NOTE: Error code set in async for now, may produce bad errors
        std::cout << e.what() << std::endl;
        this->Disconnect();
        return 0;
    }
}

size_t WSClientUnsecure::RecvWSMessage(uint8_t *message, uint16_t maxMessageLength, uint8_t *errorCode) {
    if (this->async_ws == NULL) {
        return 0; // Not connected
    }

    return this->async_ws->pollQueue(message, maxMessageLength, errorCode);
}

//
// WSClientUnsecureAsync
// ----------------------------------------------------------------------------
// Start asynchronous connection
void WSClientUnsecureAsync::run(const WSURI uri, uint8_t *errorCode) {
    std::cout << "in WSClientUnsecureAsync::run()" << std::endl;

    Uri uriSplit = Uri::Parse(uri);
    if (uriSplit.Port.size() <= 0) {
        uriSplit.Port = WEB_SOCKET_DEFAULT_PORT_NOT_SECURE; // Default port 80
    }

    this->host = uriSplit.Host;
    this->port = uriSplit.Port;
    this->errorCode = errorCode;

    resolver.async_resolve(this->host, this->port, beast::bind_front_handler(&WSClientUnsecureAsync::onResolve, shared_from_this()));
   
    this->ioc->run();       // This should not return
}

// Host resolution done
void WSClientUnsecureAsync::onResolve(beast::error_code errorCode, tcp::resolver::results_type results) {
    std::cout << "in WSClientUnsecureAsync::onResolve()" << std::endl;

    if (errorCode) {
        *this->errorCode = ERROR_DNS_NAME_RESOLUTION_FAILED;
    }

    // Set timeout
    beast::get_lowest_layer(this->ws).expires_after(std::chrono::seconds(30));

    // Start async connection
    beast::get_lowest_layer(this->ws).async_connect(results, beast::bind_front_handler(&WSClientUnsecureAsync::onConnect, shared_from_this()));
}

// Connection operation done
void WSClientUnsecureAsync::onConnect(beast::error_code errorCode, tcp::resolver::results_type::endpoint_type endpoint) {
    std::cout << "in WSClientUnsecureAsync::onConnect()" << std::endl;

    if (errorCode) {
        *this->errorCode = ERROR_TCP_CONNECTION_REFUSED;
    }

    // Turn off timeout because websocket stream has it own timeout system
    beast::get_lowest_layer(this->ws).expires_never();

    // Set default timeout settings for websocket
    websocket::stream_base::timeout timeoutOptions{
        std::chrono::seconds(30),   // handshake timeout
        websocket::stream_base::none(),   // idle timeout
        false    // keep alive pings
    };

    // Set the timeout options on the stream.
    this->ws.set_option(timeoutOptions);

    // Set more options
    this->ws.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(http::field::sec_websocket_protocol,
                "hub.bsc.bacnet.org");
        }));

    // Update host string
    host += ":" + std::to_string(endpoint.port());

    // Start async handshake
    this->ws.async_handshake(host, "/", beast::bind_front_handler(&WSClientUnsecureAsync::onHandshake, shared_from_this()));
}

void WSClientUnsecureAsync::onHandshake(beast::error_code errorCode) {
    std::cout << "in WSClientUnsecureAsync::onHandShake()" << std::endl;

    if (errorCode) {
        *this->errorCode = ERROR_TCP_CONNECTION_REFUSED;    // Set to ERROR_TLS_SERVER_CERTIFICATE_ERROR for Secure Connect
    }

    // Add code here for post connection setup, if any
    this->doneHandshake = true;
    this->doRead();
}

// Write to server
void WSClientUnsecureAsync::doWrite(const uint8_t* message, const uint16_t messageLength) {
    std::cout << "in WSClientUnsecureAsync::doWrite()" << std::endl;

    this->writeDone = false;
    std::unique_lock<std::mutex> lck(this->writeMtx);

    // Write to socket
    this->ws.binary(true);
    this->ws.async_write(net::buffer(message, messageLength), beast::bind_front_handler(&WSClientUnsecureAsync::onWrite, shared_from_this()));

    while (!this->writeDone) {
        this->writeCv.wait(lck);
    }
}

// Write operation done
void WSClientUnsecureAsync::onWrite(beast::error_code errorCode, std::size_t bytesWritten) {
    std::cout << "in WSClientUnsecureAsync::onWrite()" << std::endl;

    if (errorCode) {
        *this->errorCode = ERROR_TCP_ERROR;
        return;
    }

    // Secure bytesWritten lock
    this->writeLenMtx.lock();

    // Write value
    this->bytesWritten = bytesWritten;

    // Free bytesWritten lock
    this->writeLenMtx.unlock();

    // Free write operation lock
    this->writeDone = true;
    this->writeCv.notify_all();

    this->doRead();
}

size_t WSClientUnsecureAsync::getBytesWritten() {
    std::cout << "in WSClientUnsecureAsync::getBytesWritten()" << std::endl;

    // Secure lock
    this->writeLenMtx.lock();

    if (this->bytesWritten <= 0) {
        // Free lock
        this->writeLenMtx.unlock();
        return 0;
    }
    size_t bytesWritten = this->bytesWritten;

    // Free lock
    this->writeLenMtx.unlock();

    return bytesWritten;
}

// Read into our buffer
void WSClientUnsecureAsync::doRead() {
    std::cout << "in WSClientUnsecureAsync::doRead()\n";

    // Check if read already pending
    if (!readPending) {
        // Read to buffer
        this->ws.async_read(this->buffer, beast::bind_front_handler(&WSClientUnsecureAsync::onRead, shared_from_this()));
        //this->ws.async_read_some(net::buffer(this->bufArr, 1024), beast::bind_front_handler(&WSClientUnsecureAsync::onRead, shared_from_this()));

        readPending = true;
    }
    
}

// Read operation done
void WSClientUnsecureAsync::onRead(beast::error_code errorCode, std::size_t bytesRead) {
    std::cout << "in WSClientUnsecureAsync::onRead()\n";
    if (errorCode) {
        *this->errorCode = ERROR_TCP_ERROR;
        std::cout << "Error: WSClientUnsecureAsync::onRead() - " << errorCode << std::endl;
        return;
    }

    if (!this->readPending) {
        std::cout << "PANIC: WSClientUnsecureAsync::onRead() - read should be pending\n";
        return;
    }

    // Secure queue lock
    this->messageQueueMtx.lock();

    auto bufferData = this->buffer.data();
    std::cout << "INFO: onRead(), got message - " << std::hex << std::string(net::buffers_begin(bufferData), net::buffers_end(bufferData)) << std::endl;
    this->messageQueue.push(std::string(net::buffers_begin(bufferData), net::buffers_end(bufferData)));
    
    //this->messageQueue.push(std::string((char*)this->bufArr, bytesRead));

    // Free queue lock
    this->messageQueueMtx.unlock();

    // Read done, no read pending
    this->readPending = false;
}

// Close connection
void WSClientUnsecureAsync::doClose() {
    std::cout << "in WSClientUnsecureAsync::doClose()" << std::endl;

    this->closeDone = false;
    std::unique_lock<std::mutex> lck(this->closeMtx);

    this->ws.async_close(websocket::close_code::normal, beast::bind_front_handler(&WSClientUnsecureAsync::onClose, shared_from_this()));

    while (!this->closeDone) {
        this->closeCv.wait(lck);
    }
}

// Connection closed
void WSClientUnsecureAsync::onClose(beast::error_code errorCode) {
    std::cout << "in WSClientUnsecureAsync::onClose()" << std::endl;

    if (errorCode) {
        *this->errorCode = ERROR_TCP_ERROR;
    }

    this->doneHandshake = false;        // Reset handshake state

    // Free close operation lock
    this->closeDone = true;
    this->closeCv.notify_all();
}

bool WSClientUnsecureAsync::IsConnected() {
    // TODO: This is a hack, check for both ws.is_open() and handshake done later on
    //return this->ws.is_open();
    return this->doneHandshake;

    //// TODO: Test this
    //if (this->doneHandshake) {
    //    return true;
    //}
    //else {
    //    return this->ws.is_open();
    //}
}

// Poll queue for messages
size_t WSClientUnsecureAsync::pollQueue(uint8_t* message, uint16_t maxMessageLength, uint8_t* errorCode) {
    std::string currentMessage;

    // Secure queue lock
    this->messageQueueMtx.lock();

    if (this->messageQueue.size() > 0) {
        // There is message in queue
        currentMessage = this->messageQueue.front();
        std::cout << "INFO: Got message from BACnet Hub - " << currentMessage << std::endl;
        this->messageQueue.pop();
    }
    else {
        // No messages in queue
        // Free queue lock
        this->messageQueueMtx.unlock();
        return 0;
    }

    // Free queue lock
    this->messageQueueMtx.unlock();

    // Copy to pointer
    if (currentMessage.size() < maxMessageLength) {
        memcpy(message, currentMessage.c_str(), currentMessage.size());
        return currentMessage.size();
    }
    else {
        return 0;
    }
}

//
// WSClientSecure
// ----------------------------------------------------------------------------

// Attempting this: https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/overview/ssl.html

WSClientSecure::WSClientSecure() {
    this->m_wss = NULL;
    this->ctx = NULL;
    this->isRawIP = true;
}

bool WSClientSecure::IsConnected() {
    // No is_open function for ssl::stream
    if (this->m_wss != NULL) {
        return true;
    }
    return false;
}

bool WSClientSecure::Connect(const WSURI uri, uint8_t *errorCode) {
    // Parse Uri
    Uri parsedUri = Uri::Parse(uri);

    std::string host = parsedUri.Host;
    for (uint8_t offset = 0; offset < 3; offset++) {
        uint8_t periodIndex = host.find(".");

        uint32_t atoiVal = std::atoi(host.substr(0, periodIndex).c_str());
        // Check if octets exist/bad
        if (periodIndex == std::string::npos || atoiVal > 255 ||
            // Make sure octets with 0 as value is actually 0
            // https://en.cppreference.com/w/cpp/string/byte/atoi - If no conversion can be performed, ​0​ is returned.
            (atoiVal == 0 && (host.substr(0, periodIndex).length() != 1 || host[0] != '0'))) {
            // Set to false if octet is bad
            this->isRawIP = false;
            break;
        }

        // NOTE: Probably dont need to store ip as octet string, just keeping this here in case
        this->ipOctetString[offset] = std::atoi(host.substr(0, periodIndex).c_str());
        host = host.substr(periodIndex + 1);
    }
    // isRawIP defaults to true, no need to set

    // Setup SSL context and load certificate
    this->ctx = new ssl::context(ssl::context::sslv23);
    this->ctx->load_verify_file("./cert.pem"); // NOTE: Use the same certificate as your server here (verify_mode: ssl::verify_peer)

    // Open socket and connect to remote host
    this->m_wss = new ssl::stream<tcp::socket>(this->ioc, *this->ctx);
    tcp::resolver resolver{this->ioc};
    auto result = resolver.resolve({parsedUri.Host, parsedUri.Port});
    try {
        net::connect(this->m_wss->next_layer(), result.begin(), result.end());
        this->m_wss->lowest_layer().set_option(tcp::no_delay(true));
    } catch (std::exception const &e) {
        if (this->isRawIP) {
            *errorCode = ERROR_TCP_CONNECTION_REFUSED;
        }
        else {
            *errorCode = ERROR_DNS_NAME_RESOLUTION_FAILED;
        }
        std::cout << e.what() << std::endl;
        return false;
    }

    // Perform SSL handshake and verify remote host's certificate
    this->m_wss->set_verify_mode(ssl::verify_peer);
    try {
        this->m_wss->handshake(ssl::stream<tcp::socket>::client);
    } catch (std::exception const &e) {
        *errorCode = ERROR_TLS_SERVER_CERTIFICATE_ERROR;
        std::cout << e.what() << std::endl;
        return false;
    }

    return true;
}
void WSClientSecure::Disconnect() {
    if (!this->IsConnected()) {
        return;
    }

    // Shutdown the WebSocket connection
    this->m_wss->shutdown();

    // Remove the socket
    delete this->m_wss;
    this->m_wss = NULL;
}

size_t WSClientSecure::SendWSMessage(const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode) {
    if (!this->IsConnected()) {
        *errorCode = ERROR_TCP_ERROR;
        return 0; // Not connected
    }

    try {
        // Send the message
        return this->m_wss->write_some(net::buffer(message, messageLength));
    } catch (std::exception const &e) {
        *errorCode = ERROR_TCP_ERROR;
        this->Disconnect();
        std::cout << e.what() << std::endl;
        return 0;
    }

    return 0;
}

size_t WSClientSecure::RecvWSMessage(uint8_t *message, uint16_t maxMessageLength, uint8_t *errorCode) {
    if (!this->IsConnected()) {
        *errorCode = ERROR_TCP_ERROR;
        return 0; // Not connected
    }

    try {
        // Create buffer using message pointer directly
        net::mutable_buffer buffer(message, maxMessageLength);

        // Read a message into our message buffer
        return this->m_wss->read_some(buffer);
    } catch (std::exception const &e) {
        *errorCode = ERROR_TCP_ERROR;
        this->Disconnect();
        std::cout << e.what() << std::endl;
    }
    return 0;
}

//
// WSNetworkLayer
// ----------------------------------------------------------------------------

// Check to see if this connection exists
WSClientBase *WSNetworkLayer::GetWSClient(const WSURI uri) {
    if (clients.count(uri) <= 0) {
        return NULL;
    } else {
        return this->clients[uri];
    }
}

bool WSNetworkLayer::IsConnected(const WSURI uri) {
    // Check to see if this connection exists
    WSClientBase *ws = GetWSClient(uri);
    if (ws == NULL) {
        return false;
    }

    return ws->IsConnected();
}

bool WSNetworkLayer::AddConnection(const WSURI uri, uint8_t *errorCode) {
    // Check to see if this connection exists
    WSClientBase *ws = GetWSClient(uri);
    if (ws != NULL) {
        return true;
    }

    // Add a new connection.
    // -------------------------

    // Extract the parts from the uri
    Uri uriSplit = Uri::Parse(uri);
    if (uriSplit.Protocol.compare("ws") == 0) {
        this->clients[uri] = new WSClientUnsecure();
        return this->clients[uri]->Connect(uri, errorCode);
    } else if (uriSplit.Protocol.compare("wss") == 0) {
        this->clients[uri] = new WSClientSecure();
        return this->clients[uri]->Connect(uri, errorCode);
    }

    // Unknown
    std::cout << "Error: Unknown protocol. Protocol=[" << uriSplit.Protocol << "]" << std::endl;
    return false;
}
void WSNetworkLayer::RemoveConnection(const WSURI uri) {
    // Check to see if this connection exists
    WSClientBase *ws = GetWSClient(uri);

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
size_t WSNetworkLayer::SendWSMessage(const WSURI uri, const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode) {
    // Check to see if this connection exists
    WSClientBase *ws = GetWSClient(uri);
    if (ws == NULL) {
        // Error this connection does not exist. Do not automaticly add it.
        return 0;
    }

    // Send message
    return ws->SendWSMessage(message, messageLength, errorCode);
}

size_t WSNetworkLayer::RecvWSMessage(const WSURI uri, uint8_t *message, const uint16_t maxMessageLength, uint8_t *errorCode) {
    // Check to see if this connection exists
    WSClientBase *ws = GetWSClient(uri);
    if (ws == NULL) {
        // Error this connection does not exist. Do not automaticly add it.
        return 0;
    }

    // Send message
    return ws->RecvWSMessage(message, maxMessageLength, errorCode);
}
