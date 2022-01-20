

#include "WSClient.h"
#include <string>
#include <boost/beast/core.hpp>


// Sends a WebSocket message and prints the response
int main(int argc, char** argv)
{
    try {
        WSClient wsc;
        if (wsc.Connect("ws://localhost:8080/")) {
            printf("Connected\n");

            std::string sentMessage = "testing";
            if (wsc.SendWSMessage((uint8_t*)sentMessage.c_str(), sentMessage.size()) > 0) {
                std::cout << "FYI: Message sent" << std::endl ;

                // This buffer will hold the incoming message
                uint8_t recvMessage[1024];                

                // Loop while connected 
                while ( wsc.IsConnected() ) {
                    size_t len = wsc.RecvWSMessage(recvMessage, 1024);
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
            wsc.Disconnect();
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
