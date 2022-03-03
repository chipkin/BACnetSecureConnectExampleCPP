

#include "WSClient.h"
#include <string>
#include <boost/beast/core.hpp>
#include <boost/asio/ssl.hpp>


// Sends a WebSocket message and prints the response
int main(int argc, char** argv)
{
    try {
        WSURI uri = "wss://localhost:8443/"; 
        //WSURI uri = "ws://localhost:8080/";
        WSNetworkLayer network;
        if (network.AddConnection(uri)) {
            std::cout << "Connected to uri=[" << uri << "]" << std::endl ;

            std::string sentMessage = "testing";
            if (network.SendWSMessage(uri, (uint8_t*)sentMessage.c_str(), sentMessage.size()) > 0) {
                std::cout << "FYI: Message sent. Message=[" << sentMessage << "]" << std::endl;

                // This buffer will hold the incoming message
                uint8_t recvMessage[1024];                

                // Loop while connected 
                while (network.IsConnected(uri) ) {
                    size_t len = network.RecvWSMessage(uri, recvMessage, 1024);
                    if (len > 0) {
                        recvMessage[len] = 0;
                        std::cout << recvMessage; //  << std::endl;
                    }
                }
            } 
            else {
                std::cout << "Error: Could not send message" << std::endl;
            }

            std::cout << "FYI: Disconnect" << std::endl;
            network.RemoveConnection(uri);
        } 
        else {
            std::cout << "Error: Could not connect" << std::endl;
        }
    }
    catch (std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
