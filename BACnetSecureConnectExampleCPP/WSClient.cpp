#include "WSClient.h"
#include <boost/asio/ssl/host_name_verification.hpp> // Explicit include - don't know why Visual Studio does not detect this
#include <iomanip>

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

bool WSClientUnsecure::Connect(const WSURI uri, uint8_t* errorCode) {
    if (this->IsConnected()) {
        // We are connected, reconnect
        this->async_ws->doClose();
    }

    // Wrap async WSClient
    this->async_ws = std::make_shared<WSClientUnsecureAsync>(this->ioc);


    // Condition variables to stall for connection to be established
    this->async_ws->connectDone = false;
    std::unique_lock<std::mutex> lck(this->async_ws->connectMtx);

    try {
        for (int offset = 0; offset < IOC_THREADS; offset++) {
            this->threads.emplace_back([&] {
                try {
                    //// Start connection
                    //this->async_ws = std::make_shared<WSClientUnsecureAsync>(this->ioc);
                    //

                    this->async_ws->run(uri);
                    std::cout << "Error: this->async_ws->run() ENDED, not supposed to end\n";
                }
                catch (std::exception& e) {
                    std::cout << "DEBUG: this->async_ws->run() EXCEPTION - " << e.what() << std::endl;
                }});
        }

        while (!this->async_ws->connectDone) {
            this->async_ws->connectCv.wait(lck);
        }
    }
    catch (std::exception const& e) {
        // NOTE: Error code set in async for now, may produce bad errors
        std::cout << e.what() << std::endl;
        return false;
    }

    // Check for errors
    *errorCode = this->async_ws->getAndResetErrorCode();

    return true;
}
void WSClientUnsecure::Disconnect() {
    this->async_ws->doClose();
}

size_t WSClientUnsecure::SendWSMessage(const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode) {
    if (this->async_ws == NULL) {
        return 0; // Not connected
    }

    try {
        this->async_ws->doWrite(message, messageLength);
        *errorCode = this->async_ws->getAndResetErrorCode();
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

    size_t bytesRead = this->async_ws->pollQueue(message, maxMessageLength, errorCode);
    *errorCode = this->async_ws->getAndResetErrorCode();
    return bytesRead;
}

//
// WSClientUnsecureAsync
// ----------------------------------------------------------------------------
// Start asynchronous connection
void WSClientUnsecureAsync::run(const WSURI uri) {
    std::cout << "in WSClientUnsecureAsync::run()" << std::endl;

    Uri uriSplit = Uri::Parse(uri);
    if (uriSplit.Port.size() <= 0) {
        uriSplit.Port = WEB_SOCKET_DEFAULT_PORT_NOT_SECURE; // Default port 80
    }

    this->host = uriSplit.Host;
    this->port = uriSplit.Port;

    resolver.async_resolve(this->host, this->port, beast::bind_front_handler(&WSClientUnsecureAsync::onResolve, shared_from_this()));
   
    this->ioc->run();       // This should not return
}

// Host resolution done
void WSClientUnsecureAsync::onResolve(beast::error_code errorCode, tcp::resolver::results_type results) {
    std::cout << "in WSClientUnsecureAsync::onResolve()" << std::endl;

    if (errorCode) {
        this->errorCode = ERROR_DNS_NAME_RESOLUTION_FAILED;
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
        this->errorCode = ERROR_TCP_CONNECTION_REFUSED;
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
        this->errorCode = ERROR_TCP_CONNECTION_REFUSED;    // Set to ERROR_TLS_SERVER_CERTIFICATE_ERROR for Secure Connect
    }

    // Notify websocket is connected
    this->connectDone = true;
    this->connectCv.notify_all();

    // Add code here for post connection setup, if any
    
}

// Write to server
void WSClientUnsecureAsync::doWrite(const uint8_t* message, const uint16_t messageLength) {
    std::cout << "in WSClientUnsecureAsync::doWrite()" << std::endl;

    this->writeDone = false;
    std::unique_lock<std::mutex> lck(this->writeMtx);

    // Write to socket
    this->ws.binary(true);
    this->ws.async_write(net::buffer(message, messageLength), beast::bind_front_handler(&WSClientUnsecureAsync::onWrite, shared_from_this()));

    std::cout << "INFO: Send message - " << WSCommon::HexStringToString(std::string((char*)message, messageLength)) << std::endl;

    while (!this->writeDone) {
        this->writeCv.wait(lck);
    }
}

// Write operation done
void WSClientUnsecureAsync::onWrite(beast::error_code errorCode, std::size_t bytesWritten) {
    std::cout << "in WSClientUnsecureAsync::onWrite()" << std::endl;
    
    // Secure bytesWritten lock
    this->writeLenMtx.lock();

    if (errorCode) {
        this->errorCode = ERROR_TCP_ERROR;
        std::cout << "OnWrite failed: ERROR_TCP_ERROR errorCode=" << errorCode << std::endl;
        this->bytesWritten = 0;
    }
    else {
        // Write value
        this->bytesWritten = bytesWritten;
    }

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
        readPending = true;
    }
    
}

// Read operation done
void WSClientUnsecureAsync::onRead(beast::error_code errorCode, std::size_t bytesRead) {
    std::cout << "in WSClientUnsecureAsync::onRead()\n";
    if (errorCode) {
        this->errorCode = ERROR_TCP_ERROR;
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

    std::string bufferString = std::string(net::buffers_begin(bufferData), net::buffers_end(bufferData));

    std::cout << "INFO: onRead(), got message - " << WSCommon::HexStringToString(bufferString) << std::endl;
    this->messageQueue.push(bufferString);
    this->buffer.consume(bytesRead);

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
        this->errorCode = ERROR_TCP_ERROR;
    }

    // Free close operation lock
    this->closeDone = true;
    this->closeCv.notify_all();
}

bool WSClientUnsecureAsync::IsConnected() {
    if (this->connectDone && this->ws.is_open()) {
        return true;
    }

    return false;
}

// Poll queue for messages
size_t WSClientUnsecureAsync::pollQueue(uint8_t* message, uint16_t maxMessageLength, uint8_t* errorCode) {
    std::string currentMessage;

    // Secure queue lock
    this->messageQueueMtx.lock();

    if (this->messageQueue.size() > 0) {
        // There is message in queue
        currentMessage = this->messageQueue.front();
        std::cout << "INFO: Got message from BACnet Hub - " << WSCommon::HexStringToString(currentMessage) << std::endl;
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

uint8_t WSClientUnsecureAsync::getAndResetErrorCode() {
    uint8_t errorCode = this->errorCode;
    this->errorCode = 0;
    return errorCode;
}

//
// WSClientSecureAsync
// ----------------------------------------------------------------------------
// Start asynchronous connection
void WSClientSecureAsync::run(const WSURI uri) {
    std::cout << "in WSClientSecureAsync::run()" << std::endl;

    Uri uriSplit = Uri::Parse(uri);
    if (uriSplit.Port.size() <= 0) {
        uriSplit.Port = WEB_SOCKET_DEFAULT_PORT_NOT_SECURE; // Default port 80
    }

    this->host = uriSplit.Host;
    this->port = uriSplit.Port;

    resolver.async_resolve(this->host, this->port, beast::bind_front_handler(&WSClientSecureAsync::onResolve, shared_from_this()));
   
    this->ioc->run();       // This should not return
}

// Host resolution done
void WSClientSecureAsync::onResolve(beast::error_code errorCode, tcp::resolver::results_type results) {
    std::cout << "in WSClientSecureAsync::onResolve()" << std::endl;

    if (errorCode) {
        this->errorCode = ERROR_DNS_NAME_RESOLUTION_FAILED;
        std::cout << "OnResolve failed: ERROR_DNS_NAME_RESOLUTION_FAILED errorCode=" << errorCode << std::endl;
        return;
    }

    // Set timeout
    beast::get_lowest_layer(this->ws).expires_after(std::chrono::seconds(30));

    // Start async connection
    beast::get_lowest_layer(this->ws).async_connect(results, beast::bind_front_handler(&WSClientSecureAsync::onConnect, shared_from_this()));
}

// Connection operation done
void WSClientSecureAsync::onConnect(beast::error_code errorCode, tcp::resolver::results_type::endpoint_type endpoint) {
    std::cout << "in WSClientSecureAsync::onConnect()" << std::endl;

    if (errorCode) {
        this->errorCode = ERROR_TCP_CONNECTION_REFUSED;
        std::cout << "OnConnect failed: ERROR_TCP_CONNECTION_REFUSED errorCode=" << errorCode << std::endl;
        return;
    }

    // Set a timeout on the operation
    beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if(! SSL_set_tlsext_host_name(this->ws.next_layer().native_handle(), this->host.c_str()))
    {
        errorCode = beast::error_code(static_cast<int>(::ERR_get_error()),
            net::error::get_ssl_category());
        this->errorCode = ERROR_TLS_ERROR;
        std::cout << "OnConnect failed: SSL_set_tlsext_host_name errorCode=" << errorCode << std::endl;
        return;
    }

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    this->host += ':' + std::to_string(endpoint.port());
    
    // Perform the SSL handshake
    this->ws.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(
            &WSClientSecureAsync::onSslHandshake,
            shared_from_this()));
    
}

void WSClientSecureAsync::onSslHandshake(beast::error_code errorCode) {
std::cout << "in WSClientSecureAsync::onSslHandshake()" << std::endl;

    if (errorCode) {
        this->errorCode = ERROR_TLS_SERVER_CERTIFICATE_ERROR;
        std::cout << "OnSslHandshake failed: ERROR_TLS_SERVER_CERTIFICATE_ERROR errorCode=" << errorCode << std::endl;
        return;
    }

    // Turn off timeout because websocket stream has it own timeout system
    beast::get_lowest_layer(this->ws).expires_never();

    // Set suggested timeout settings for the websocket
    this->ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

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

    // Start async handshake
    this->ws.async_handshake(this->host, "/", beast::bind_front_handler(&WSClientSecureAsync::onHandshake, shared_from_this()));

}

void WSClientSecureAsync::onHandshake(beast::error_code errorCode) {
    std::cout << "in WSClientSecureAsync::onHandShake()" << std::endl;

    if (errorCode) {
        this->errorCode = ERROR_TCP_CONNECTION_REFUSED;    // Set to ERROR_TLS_SERVER_CERTIFICATE_ERROR for Secure Connect
        std::cout << "OnHandshake failed: ERROR_TCP_CONNECTION_REFUSED errorCode=" << errorCode << std::endl;
        return;
    }

    // Notify websocket is connected
    this->connectDone = true;
    this->connectCv.notify_all();


    // Add code here for post connection setup, if any
    this->doRead();
}

// Write to server
void WSClientSecureAsync::doWrite(const uint8_t* message, const uint16_t messageLength) {
    std::cout << "in WSClientSecureAsync::doWrite()" << std::endl;

    this->writeDone = false;
    std::unique_lock<std::mutex> lck(this->writeMtx);

    // Write to socket
    this->ws.binary(true);
    this->ws.async_write(net::buffer(message, messageLength), beast::bind_front_handler(&WSClientSecureAsync::onWrite, shared_from_this()));

    std::cout << "INFO: Send message - " << WSCommon::HexStringToString(std::string((char*)message, messageLength)) << std::endl;

    while (!this->writeDone) {
        this->writeCv.wait(lck);
    }
}

// Write operation done
void WSClientSecureAsync::onWrite(beast::error_code errorCode, std::size_t bytesWritten) {
    std::cout << "in WSClientSecureAsync::onWrite()" << std::endl;

    // Secure bytesWritten lock
    this->writeLenMtx.lock();

    if (errorCode) {
        this->errorCode = ERROR_TCP_ERROR;
        std::cout << "OnWrite failed: ERROR_TCP_ERROR errorCode=" << errorCode << std::endl;
        this->bytesWritten = 0;
    }
    else {
        // Write value
        this->bytesWritten = bytesWritten;
    }

    // Free bytesWritten lock
    this->writeLenMtx.unlock();

    // Free write operation lock
    this->writeDone = true;
    this->writeCv.notify_all();

    this->doRead();
}

size_t WSClientSecureAsync::getBytesWritten() {
    std::cout << "in WSClientSecureAsync::getBytesWritten()" << std::endl;

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
void WSClientSecureAsync::doRead() {
    std::cout << "in WSClientSecureAsync::doRead()\n";

    // Check if read already pending
    if (!readPending) {
        // Read to buffer
        this->ws.async_read(this->buffer, beast::bind_front_handler(&WSClientSecureAsync::onRead, shared_from_this()));
        readPending = true;
    }
}

// Read operation done
void WSClientSecureAsync::onRead(beast::error_code errorCode, std::size_t bytesRead) {
    std::cout << "in WSClientSecureAsync::onRead()\n";
    if (errorCode) {
        this->errorCode = ERROR_TCP_ERROR;
        std::cout << "onRead failed: ERROR_TCP_ERROR errorCode=" << errorCode << std::endl;
        return;
    }

    if (!this->readPending) {
        std::cout << "PANIC: WSClientSecureAsync::onRead() - read should be pending\n";
        return;
    }

    // Secure queue lock
    this->messageQueueMtx.lock();

    auto bufferData = this->buffer.data();

    std::string bufferString = std::string(net::buffers_begin(bufferData), net::buffers_end(bufferData));

    std::cout << "INFO: onRead(), got message - " << WSCommon::HexStringToString(bufferString) << std::endl;
    this->messageQueue.push(bufferString);
    this->buffer.consume(bytesRead);

    // Free queue lock
    this->messageQueueMtx.unlock();

    // Read done, no read pending
    this->readPending = false;
}

// Close connection
void WSClientSecureAsync::doClose() {
    std::cout << "in WSClientSecureAsync::doClose()" << std::endl;

    this->closeDone = false;
    std::unique_lock<std::mutex> lck(this->closeMtx);

    this->ws.async_close(websocket::close_code::normal, beast::bind_front_handler(&WSClientSecureAsync::onClose, shared_from_this()));

    while (!this->closeDone) {
        this->closeCv.wait(lck);
    }
}

// Connection closed
void WSClientSecureAsync::onClose(beast::error_code errorCode) {
    std::cout << "in WSClientSecureAsync::onClose()" << std::endl;

    if (errorCode) {
        this->errorCode = ERROR_TCP_ERROR;
        std::cout << "onClose failed: ERROR_TCP_ERROR errorCode=" << errorCode << std::endl;
        return;
    }

    // Free close operation lock
    this->closeDone = true;
    this->closeCv.notify_all();
}

bool WSClientSecureAsync::IsConnected() {
    if (this->connectDone && this->ws.is_open()) {
        return true;
    }

    return false;
}

// Poll queue for messages
size_t WSClientSecureAsync::pollQueue(uint8_t* message, uint16_t maxMessageLength, uint8_t* errorCode) {
    std::string currentMessage;

    // Secure queue lock
    this->messageQueueMtx.lock();

    if (this->messageQueue.size() > 0) {
        // There is message in queue
        currentMessage = this->messageQueue.front();
        std::cout << "INFO: Got message from BACnet Hub - " << WSCommon::HexStringToString(currentMessage) << std::endl;
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

uint8_t WSClientSecureAsync::getAndResetErrorCode() {
    uint8_t errorCode = this->errorCode;
    this->errorCode = 0;
    return errorCode;
}

//
// WSClientSecure
// ----------------------------------------------------------------------------

// Attempting this: https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/overview/ssl.html

WSClientSecure::WSClientSecure() {
    this->async_ws = NULL;
    this->m_cert = "";
    this->m_key = "";
}

WSClientSecure::WSClientSecure(const std::string& certFilename, const std::string& keyFilename) {
    this->async_ws = NULL;
    this->m_cert = certFilename;
    this->m_key = keyFilename;
}

bool WSClientSecure::IsConnected() {
    if (this->async_ws != NULL) {

        return this->async_ws->IsConnected();
    }
    return false;
}

bool WSClientSecure::Connect(const WSURI uri, uint8_t* errorCode) {
    // Check parameters,
    if (this->IsConnected()) {
        // We are connected, reconnect
        this->async_ws->doClose();
    }

    // Load ceritifcate and private key into context
    this->ctx.use_certificate_file(this->m_cert, ssl::context::pem);
    this->ctx.use_private_key_file(this->m_key, ssl::context::pem);

    // Set context settings so that our SSL connection works
    // https://stackoverflow.com/questions/43117638/boost-asio-get-with-client-certificate-sslv3-hand-shake-failed
    this->ctx.set_options(boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::no_sslv3);

    // Wrap async WSClient
    this->async_ws = std::make_shared<WSClientSecureAsync>(this->ioc, this->ctx);

    // Setup conditional variables to stall for connection
    this->async_ws->connectDone = false;
    std::unique_lock<std::mutex> lck(this->async_ws->connectMtx);

    try {
        for (int offset = 0; offset < IOC_THREADS; offset++) {
            this->threads.emplace_back([&] {
                try {
                    // Start connection
                    this->async_ws->run(uri);
                    std::cout << "Error: this->async_ws->run() ENDED, not supposed to end\n";
                }
                catch (std::exception & e) {
                    std::cout << "DEBUG: this->async_ws->run() EXCEPTION - " << e.what() << std::endl;
                }});
        }

        while (!this->async_ws->connectDone) {
            this->async_ws->connectCv.wait(lck);
        }
    }
    catch (std::exception const& e) {
        // NOTE: Error code set in async for now, may produce bad errors
        std::cout << e.what() << std::endl;
        return false;
    }
    *errorCode = this->async_ws->getAndResetErrorCode();
    return true;
}
void WSClientSecure::Disconnect() {
    this->async_ws->doClose();
}

size_t WSClientSecure::SendWSMessage(const uint8_t *message, const uint16_t messageLength, uint8_t *errorCode) {
    if (this->async_ws == NULL) {
        return 0; // Not connected
    }

    try {
        this->async_ws->doWrite(message, messageLength);
        *errorCode = this->async_ws->getAndResetErrorCode();
        return this->async_ws->getBytesWritten();
    }
    catch (std::exception const& e) {
        // NOTE: Error code set in async for now, may produce bad errors
        std::cout << e.what() << std::endl;
        this->Disconnect();
        return 0;
    }
}

size_t WSClientSecure::RecvWSMessage(uint8_t *message, uint16_t maxMessageLength, uint8_t *errorCode) {
    if (this->async_ws == NULL) {
        return 0; // Not connected
    }

    size_t bytesRead = this->async_ws->pollQueue(message, maxMessageLength, errorCode);
    *errorCode = this->async_ws->getAndResetErrorCode();
    return bytesRead;
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

bool WSNetworkLayer::AddConnection(const WSURI uri, uint8_t *errorCode, const std::string& certFilename, const std::string& keyFilename) {
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
        WSClientUnsecure* unsecureClient = new(std::nothrow) WSClientUnsecure();
        if (unsecureClient == NULL) {
            std::cout << "Error: out of memory when creating unsecureClient" << std::endl;
            return false;
        }
        this->clients[uri] = unsecureClient;
        return this->clients[uri]->Connect(uri, errorCode);
    } else if (uriSplit.Protocol.compare("wss") == 0) {
        WSClientSecure* secureClient = new (std::nothrow) WSClientSecure(certFilename, keyFilename);
        if (secureClient == NULL) {
            std::cout << "Error: out of memory when creating secureClient" << std::endl;
            return false;
        }
        this->clients[uri] = secureClient;
        return this->clients[uri]->Connect(uri, errorCode);
    }
    else {
        // Unknown
        std::cout << "Error: Unknown protocol. Protocol=[" << uriSplit.Protocol << "]" << std::endl;
        return false;
    }
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

std::string WSCommon::HexStringToString(std::string hexString) {
    std::string output = "";
    if (hexString.size() == 0) {
        return output;
    }

    char temp[10];
    memset(temp, 0, 10);
    for (size_t i = 0; i < hexString.size(); i++) {
        memset(temp, 0, 10);
        snprintf(temp, 10, "%02X", hexString[i]);
        output.append(temp);
    }

    return output;
}