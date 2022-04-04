/*
 * BACnet SC Example C++
 * ----------------------------------------------------------------------------
 * BACnetSCExampleCPP.cpp
 *
 * In this CAS BACnet SC example, we create a BACnet SC server with various
 * objects and properties from an example database.
 *
 * More information https://github.com/chipkin/BACnetServerExampleCPP
 *
 * This file contains the 'main' function. Program execution begins and ends there.
 *
 * Created by: Steven Smethurst
 * Modified by: Justin Wong
 */
// BACnet Stack
#include "CASBACnetStackAdapter.h"
// !!!!!! This file is part of the CAS BACnet Stack. Please contact Chipkin for more information.
// !!!!!! https://github.com/chipkin/BACnetServerExampleCPP/issues/8
#include "CASBACnetSCExampleConstants.h"
#include "CASBACnetSCExampleDatabase.h"

// Secure Connection libraries
#include "WSClient.h"
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>

// Helpers
#include "ChipkinConvert.h"
#include "ChipkinEndianness.h"
#include "ChipkinUtilities.h"

// System and OS
#include <iostream>
#include <string>
#ifndef __GNUC__   // Windows
#include <conio.h> // _kbhit
#else              // Linux
#include <sys/ioctl.h>
#include <termios.h>
bool _kbhit() {
    termios term;
    tcgetattr(0, &term);
    termios term2 = term;
    term2.c_lflag &= ~ICANON;
    tcsetattr(0, TCSANOW, &term2);
    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);
    tcsetattr(0, TCSANOW, &term);
    return byteswaiting > 0;
}
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
void Sleep(int milliseconds) {
    usleep(milliseconds * 1000);
}
#endif // __GNUC__

// Globals
// ===========================================================================
ExampleDatabase g_database; // The example database that stores current values.

// Constants
// ===========================================================================
const std::string APPLICATION_VERSION = "0.0.3"; // See CHANGELOG.md for a full list of changes.
const uint32_t MAX_RENDER_BUFFER_LENGTH = 1024 * 20;

// Network Settings and Globals
// ===========================================================================

WSNetworkLayer ws_network;
const bool IS_WSS = true;
const std::string WS_URI = "";
const uint16_t WS_PORT = 8443;
const std::string WS_ADDRESS = "192.168.1.159";

// Callback Functions to Register to the DLL
// ===========================================================================
// Message Functions
uint16_t CallbackReceiveMessage(uint8_t *message, const uint16_t maxMessageLength, uint8_t *receivedConnectionString, const uint8_t maxConnectionStringLength, uint8_t *receivedConnectionStringLength, uint8_t *networkType);
uint16_t CallbackSendMessage(const uint8_t *message, const uint16_t messageLength, const uint8_t *connectionString, const uint8_t connectionStringLength, const uint8_t networkType, bool broadcast);

// System Functions
time_t CallbackGetSystemTime();
void CallbackLogDebugMessage(const char *message, const uint16_t messageLength, const uint8_t messageType);

// Get Property Functions
bool CallbackGetPropertyCharString(const uint32_t deviceInstance, const uint16_t objectType, const uint32_t objectInstance, const uint32_t propertyIdentifier, char *value, uint32_t *valueElementCount, const uint32_t maxElementCount, uint8_t *encodingType, const bool useArrayIndex, const uint32_t propertyArrayIndex);
bool CallbackGetPropertyReal(const uint32_t deviceInstance, const uint16_t objectType, const uint32_t objectInstance, const uint32_t propertyIdentifier, float *value, const bool useArrayIndex, const uint32_t propertyArrayIndex);

// A simple BACnetServerExample in CPP that uses secure connection
// ===========================================================================
int main(int argc, char **argv) {
    // Print the application version information
    // ---------------------------------------------------------------------------
    std::cout << "CAS BACnet Stack Server Example v" << APPLICATION_VERSION << std::endl;
    std::cout << "https://github.com/chipkin/BACnetSecureConnectExampleCPP" << std::endl
              << std::endl;

    // Load CAS BACnet Stack functions
    // ---------------------------------------------------------------------------
    std::cout << "FYI: Loading CAS BACnet Stack functions...";
    if (!LoadBACnetFunctions()) {
        std::cerr << "Failed to load the functions from the DLL" << std::endl;
        return 0;
    }
    std::cout << "OK" << std::endl;
    std::cout << "FYI: CAS BACnet Stack version: " << fpGetAPIMajorVersion() << "." << fpGetAPIMinorVersion() << "." << fpGetAPIPatchVersion() << "." << fpGetAPIBuildVersion() << std::endl;

    // Setup connection
    // ---------------------------------------------------------------------------
    // Store network settings to DB
    // TODO - necessary? Come back later at the end, access network/connection string directly for now

    // Build URI
    WSURI uri = "";
    if (IS_WSS) {
        uri += "wss://";
    } else {
        uri += "ws://";
    }
    uri += WS_ADDRESS + ":" + std::to_string(WS_PORT) + "/";

    // Attempt WS/WSS connection
    if (!ws_network.AddConnection(uri)) {
        // Connection unsuccessful
        std::cerr << "Failed to connect to websocket" << std::endl;
        std::cerr << "Press any key to exit the application..." << std::endl;
        (void)getchar();
        return EXIT_FAILURE;
    }
    std::cout << "OK, Connected to uri=[" << uri << "]" << std::endl;

    // Setup callbacks
    // ---------------------------------------------------------------------------
    std::cout << "FYI: Registering the Callback Functions with the CAS BACnet Stack" << std::endl;
    // Message callback functions
    fpRegisterCallbackReceiveMessage(CallbackReceiveMessage);
    fpRegisterCallbackSendMessage(CallbackSendMessage);

    // System callback functions
    fpRegisterCallbackGetSystemTime(CallbackGetSystemTime);
    fpRegisterCallbackLogDebugMessage(CallbackLogDebugMessage);

    // Get Property callback functions
    fpRegisterCallbackGetPropertyCharacterString(CallbackGetPropertyCharString);
    fpRegisterCallbackGetPropertyReal(CallbackGetPropertyReal);

    // Setup the BACnet device
    // ---------------------------------------------------------------------------
    std::cout << "Setting up server device. device.instance=[" << g_database.device.instance << "]" << std::endl;

    // Create the Device
    if (!fpAddDevice(g_database.device.instance)) {
        std::cerr << "Failed to add Device." << std::endl;
        return false;
    }
    std::cout << "Created Device." << std::endl;

    // Add objects
    // ---------------------------------------------------------------------------
    // AnalogInput (AI)
    std::cout << "Adding AnalogInput. analogInput.instance=[" << g_database.analogInput.instance << "]... ";
    if (!fpAddObject(g_database.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT, g_database.analogInput.instance)) {
        std::cerr << "Failed to add AnalogInput" << std::endl;
        return -1;
    }
    // Enable Reliability property
    fpSetPropertyByObjectTypeEnabled(g_database.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_RELIABILITY, true);
    std::cout << "OK" << std::endl;

    // NetworkPort (NP)
    std::cout << "Added NetworkPort. networkPort.instance=[" << g_database.networkPort.instance << "]... ";
    if (!fpAddNetworkPortObject(g_database.device.instance, g_database.networkPort.instance, CASBACnetStackExampleConstants::NETWORK_TYPE_IPV4, CASBACnetStackExampleConstants::PROTOCOL_LEVEL_BACNET_APPLICATION, CASBACnetStackExampleConstants::NETWORK_PORT_LOWEST_PROTOCOL_LAYER)) {
        std::cerr << "Failed to add NetworkPort" << std::endl;
        return -1;
    }
    fpSetPropertyEnabled(g_database.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_NETWORK_PORT, g_database.networkPort.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_BBMD_ACCEPT_FD_REGISTRATIONS, true);
    fpSetPropertyEnabled(g_database.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_NETWORK_PORT, g_database.networkPort.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_BBMD_BROADCAST_DISTRIBUTION_TABLE, true);
    fpSetPropertyEnabled(g_database.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_NETWORK_PORT, g_database.networkPort.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_BBMD_FOREIGN_DEVICE_TABLE, true);

    uint8_t ipPortConcat[6];
    memcpy(ipPortConcat, g_database.networkPort.IPAddress, 4);
    ipPortConcat[4] = g_database.networkPort.BACnetIPUDPPort / 256;
    ipPortConcat[5] = g_database.networkPort.BACnetIPUDPPort % 256;
    fpAddBDTEntry(ipPortConcat, 6, g_database.networkPort.IPSubnetMask, 4); // First BDT Entry must be server device

    std::cout << "OK" << std::endl;

    // Start the main loop
    // ---------------------------------------------------------------------------
    std::cout << "FYI: Entering main loop..." << std::endl;
    for (;;) {
        fpLoop();

        g_database.Loop(); // Increment Analog Input object Present Value property

        // Call Sleep to give some time back to the system
        Sleep(0); // Windows
    }

    return EXIT_SUCCESS;
}

// Callback Implementations
// ===========================================================================
// Message callback functions
// ---------------------------------------------------------------------------
// Callback used by the BACnet Stack to check if there is a message to process
uint16_t CallbackReceiveMessage(uint8_t *message, const uint16_t maxMessageLength, uint8_t *receivedConnectionString, const uint8_t maxConnectionStringLength, uint8_t *receivedConnectionStringLength, uint8_t *networkType) {
    // Check parameters
    if (message == NULL || maxMessageLength == 0) {
        std::cerr << "Invalid input buffer" << std::endl;
        return 0;
    }
    if (receivedConnectionString == NULL || maxConnectionStringLength == 0) {
        std::cerr << "Invalid connection string buffer" << std::endl;
        return 0;
    }
    if (maxConnectionStringLength < 6) {
        std::cerr << "Not enough space for a UDP connection string" << std::endl;
        return 0;
    }

    // TODO: Implement SC Receive Message

    // OLD:
    // char ipAddress[32];
    // uint16_t port = 0;

    // // Attempt to read bytes
    // int bytesRead = g_udp.GetMessage(message, maxMessageLength, ipAddress, &port);
    // if (bytesRead > 0) {
    //     ChipkinCommon::CEndianness::ToBigEndian(&port, sizeof(uint16_t));
    //     std::cout << std::endl
    //               << "FYI: Received message from [" << ipAddress << ":" << port << "], length [" << bytesRead << "]" << std::endl;

    //     // Convert the IP Address to the connection string
    //     if (!ChipkinCommon::ChipkinConvert::IPAddressToBytes(ipAddress, receivedConnectionString, maxConnectionStringLength)) {
    //         std::cerr << "Failed to convert the ip address into a connectionString" << std::endl;
    //         return 0;
    //     }
    //     receivedConnectionString[4] = port / 256;
    //     receivedConnectionString[5] = port % 256;

    //     *receivedConnectionStringLength = 6;
    //     *networkType = CASBACnetStackExampleConstants::NETWORK_TYPE_IP;

    //     /*
    //     // Process the message as XML
    //     static char xmlRenderBuffer[MAX_RENDER_BUFFER_LENGTH ];
    //     if (fpDecodeAsXML((char*)message, bytesRead, xmlRenderBuffer, MAX_RENDER_BUFFER_LENGTH ) > 0) {
    //         std::cout << "---------------------" << std::endl;
    //         std::cout << xmlRenderBuffer << std::endl;
    //         std::cout << "---------------------" << std::endl;
    //         memset(xmlRenderBuffer, 0, MAX_RENDER_BUFFER_LENGTH );
    //     }
    //     */

    //     // Process the message as JSON
    //     static char jsonRenderBuffer[MAX_RENDER_BUFFER_LENGTH];
    //     if (fpDecodeAsJSON((char *)message, bytesRead, jsonRenderBuffer, MAX_RENDER_BUFFER_LENGTH) > 0) {
    //         std::cout << "---------------------" << std::endl;
    //         std::cout << jsonRenderBuffer << std::endl;
    //         std::cout << "---------------------" << std::endl;
    //         memset(jsonRenderBuffer, 0, MAX_RENDER_BUFFER_LENGTH);
    //     }
    // }
    //
    // return bytesRead;
    return 0;
}

// Callback used by the BACnet Stack to send a BACnet message
uint16_t CallbackSendMessage(const uint8_t *message, const uint16_t messageLength, const uint8_t *connectionString, const uint8_t connectionStringLength, const uint8_t networkType, bool broadcast) {
    // Check parameters
    if (message == NULL || messageLength == 0) {
        std::cout << "Nothing to send" << std::endl;
        return 0;
    }
    if (connectionString == NULL || connectionStringLength == 0) {
        std::cout << "No connection string" << std::endl;
        return 0;
    }

    // TODO: Implement SC Send Message

    // OLD:
    // // Verify Network Type
    // if (networkType != CASBACnetStackExampleConstants::NETWORK_TYPE_IP) {
    //     std::cout << "Message for different network" << std::endl;
    //     return 0;
    // }

    // // Prepare the IP Address
    // char ipAddress[32];
    // if (broadcast) {
    //     snprintf(ipAddress, 32, "%hhu.%hhu.%hhu.%hhu",
    //              connectionString[0] | ~g_database.networkPort.IPSubnetMask[0],
    //              connectionString[1] | ~g_database.networkPort.IPSubnetMask[1],
    //              connectionString[2] | ~g_database.networkPort.IPSubnetMask[2],
    //              connectionString[3] | ~g_database.networkPort.IPSubnetMask[3]);
    // } else {
    //     snprintf(ipAddress, 32, "%u.%u.%u.%u", connectionString[0], connectionString[1], connectionString[2], connectionString[3]);
    // }

    // // Get the port
    // uint16_t port = 0;
    // port += connectionString[4] * 256;
    // port += connectionString[5];

    // std::cout << std::endl
    //           << "FYI: Sending message to [" << ipAddress << ":" << port << "] length [" << messageLength << "]" << std::endl;

    // // Send the message
    // if (!g_udp.SendMessage(ipAddress, port, (unsigned char *)message, messageLength)) {
    //     std::cout << "Failed to send message" << std::endl;
    //     return 0;
    // }

    // /*
    // // Get the XML rendered version of the just sent message
    // static char xmlRenderBuffer[MAX_RENDER_BUFFER_LENGTH];
    // if (fpDecodeAsXML((char*)message, messageLength, xmlRenderBuffer, MAX_RENDER_BUFFER_LENGTH) > 0) {
    //     std::cout << xmlRenderBuffer << std::endl;
    //     memset(xmlRenderBuffer, 0, MAX_RENDER_BUFFER_LENGTH);
    // }
    // */

    // // Get the JSON rendered version of the just sent message
    // static char jsonRenderBuffer[MAX_RENDER_BUFFER_LENGTH];
    // if (fpDecodeAsJSON((char *)message, messageLength, jsonRenderBuffer, MAX_RENDER_BUFFER_LENGTH) > 0) {
    //     std::cout << "---------------------" << std::endl;
    //     std::cout << jsonRenderBuffer << std::endl;
    //     std::cout << "---------------------" << std::endl;
    //     memset(jsonRenderBuffer, 0, MAX_RENDER_BUFFER_LENGTH);
    // }

    // return messageLength;
    return 0;
}

// System callback Functions
// ---------------------------------------------------------------------------
// Callback used by the BACnet Stack to get the current time
time_t CallbackGetSystemTime() {
    return time(0) - g_database.device.currentTimeOffset;
}

void CallbackLogDebugMessage(const char *message, const uint16_t messageLength, const uint8_t messageType) {
    // This callback is called when the CAS BACnet Stack logs an error or info message
    // In this callback, you will be able to access this debug message. This callback is optional.
    std::cout << std::string(message, messageLength) << std::endl;
    return;
}

// Get Property callback functions
// ---------------------------------------------------------------------------
// Callback used by the BACnet Stack to get Character String property values from the user
bool CallbackGetPropertyCharString(const uint32_t deviceInstance, const uint16_t objectType, const uint32_t objectInstance, const uint32_t propertyIdentifier, char *value, uint32_t *valueElementCount, const uint32_t maxElementCount, uint8_t *encodingType, const bool useArrayIndex, const uint32_t propertyArrayIndex) {
    // Example of Object Name property
    if (propertyIdentifier == CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_OBJECT_NAME) {
        size_t stringSize = 0;
        if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_DEVICE && objectInstance == g_database.device.instance) {
            stringSize = g_database.device.objectName.size();
            if (stringSize > maxElementCount) {
                std::cerr << "Error - not enough space to store full name of objectType=[" << objectType << "], objectInstance=[" << objectInstance << " ]" << std::endl;
                return false;
            }
            memcpy(value, g_database.device.objectName.c_str(), stringSize);
            *valueElementCount = (uint32_t)stringSize;
            return true;
        } else if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT && objectInstance == g_database.analogInput.instance) {
            stringSize = g_database.analogInput.objectName.size();
            if (stringSize > maxElementCount) {
                std::cerr << "Error - not enough space to store full name of objectType=[" << objectType << "], objectInstance=[" << objectInstance << " ]" << std::endl;
                return false;
            }
            memcpy(value, g_database.analogInput.objectName.c_str(), stringSize);
            *valueElementCount = (uint32_t)stringSize;
            return true;
        }
    }
    return false;
}

// Callback used by the BACnet Stack to get Real property values from the user
bool CallbackGetPropertyReal(uint32_t deviceInstance, uint16_t objectType, uint32_t objectInstance, uint32_t propertyIdentifier, float *value, bool useArrayIndex, uint32_t propertyArrayIndex) {

    // Example of Analog Input object Present Value property
    if (propertyIdentifier == CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT && objectInstance == g_database.analogInput.instance) {
            *value = g_database.analogInput.presentValue;
            return true;
        }
    }
    return false;
}

// EXAMPLE: Send and receive with websockets
// try {
//     WSURI uri = "wss://localhost:8443/";
//     // WSURI uri = "ws://localhost:8080/";
//     WSNetworkLayer network;
//     if (network.AddConnection(uri)) {
//         std::cout << "Connected to uri=[" << uri << "]" << std::endl;

//         std::string sentMessage = "testing";
//         if (network.SendWSMessage(uri, (uint8_t *)sentMessage.c_str(), sentMessage.size()) > 0) {
//             std::cout << "FYI: Message sent. Message=[" << sentMessage << "]" << std::endl;

//             // This buffer will hold the incoming message
//             uint8_t recvMessage[1024];

//             // Loop while connected
//             while (network.IsConnected(uri)) {
//                 size_t len = network.RecvWSMessage(uri, recvMessage, 1024);
//                 if (len > 0) {
//                     recvMessage[len] = 0;
//                     std::cout << recvMessage; //  << std::endl;
//                 }
//             }
//         } else {
//             std::cout << "Error: Could not send message" << std::endl;
//         }

//         std::cout << "FYI: Disconnect" << std::endl;
//         network.RemoveConnection(uri);
//     } else {
//         std::cout << "Error: Could not connect" << std::endl;
//     }
// } catch (std::exception const &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     return EXIT_FAILURE;
// }