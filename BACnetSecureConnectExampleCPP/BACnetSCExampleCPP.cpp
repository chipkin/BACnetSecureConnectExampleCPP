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

// Example constants and database
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

// Enums and Constants
#include "BACnetSCConstants.h"
#include "CIBuildSettings.h"

// System and OS
#include <iostream>
#include <string>
#include <iomanip>
#include <cstdio>
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

using namespace CASBACnetStack;

// Globals
// ===========================================================================
ExampleDatabase g_database; // The example database that stores current values.

// Constants
// ===========================================================================
const std::string APPLICATION_VERSION = "0.0.3"; // See CHANGELOG.md for a full list of changes.
const uint32_t MAX_RENDER_BUFFER_LENGTH = 1024 * 20;

// Network Settings and Globals
// ===========================================================================

WSNetworkLayer g_ws_network;

const std::string primaryHubUri = "wss://192.168.1.84:4443/";
const std::string failoverHubUri = "wss://192.168.1.84:4444/";

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

// Websocket Callbacks
bool CallbackInitiateWebsocket(const char* websocketUri, const uint32_t websocketUriLength);
void CallbackDisconnectWebsocket(const char* websocketUri, const uint32_t websocketUriLength);

// Helper Functions
bool DoUserInput();

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

    // Websocket callback functions
    fpRegisterCallbackInitiateWebsocket(CallbackInitiateWebsocket);
    fpRegisterCallbackDisconnectWebsocket(CallbackDisconnectWebsocket);

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

    // Setup BACnet SC
    // ---------------------------------------------------------------------------
    const uint8_t uuid[BACnetSCConstants::BACNET_SC_DEVICE_UUID_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    std::cout << "Setting BACnetSC UUID... ";
    if (!fpSetBACnetSCUuid(uuid, BACnetSCConstants::BACNET_SC_DEVICE_UUID_LENGTH)) {
        std::cerr << "Failed to set the uuid" << std::endl;
        return -1;
    }
    std::cout << "OK" << std::endl;

    std::cout << "Setting up HubConnector... " << std::endl;
    const uint8_t vmac[BACnetSCConstants::BACNET_SC_VMAC_LENGTH] = { 0x09, 0x09, 0x09, 0x09 , 0x09 , 0x09 };
    std::cout << "  Connecting To primaryUri: " << primaryHubUri << std::endl;
    std::cout << "  Connecting To failoverUri: " << failoverHubUri << std::endl;
    if (!fpSetBACnetSCHubConnector(vmac, BACnetSCConstants::BACNET_SC_VMAC_LENGTH, primaryHubUri.c_str(), primaryHubUri.size(), NULL, 0)) {
        std::cerr << "Failed to set the hub connector settings" << std::endl;
        return -1;
    }
    std::cout << "OK" << std::endl;

    // Start the main loop
    // ---------------------------------------------------------------------------
    std::cout << "FYI: Entering main loop..." << std::endl;
    for (;;) {
        fpLoop();

        // Handle User Input
        if (!DoUserInput()) {
            // User press 'q' to quit the example application.
            break;
        }

        g_database.Loop();   // Increment Analog Input object Present Value property

        // Call Sleep to give some time back to the system
        Sleep(0); // Windows
    }
    return EXIT_SUCCESS;
}

// Helper Functions
// ===========================================================================

// Handle User Input
// Note: User input in this example is used for the following:
//		h - Display options
//      w - send Who-is broadcast
//		q - Quit
bool DoUserInput() {
    // Check to see if the user hit any key
    if (!_kbhit()) {
        // No keys have been hit
        return true;
    }

    // Extract the letter that the user hit and convert it to lower case
    char action = tolower(getchar());

    // Handle the action 
    switch (action) {
        // Quit
    case 'q': {
        return false;
    }
        // Send Who-is
    case 'w': {
        size_t uriLength = primaryHubUri.size();
        fpSendWhoIs((const uint8_t*)primaryHubUri.c_str(), primaryHubUri.size(), CASBACnetStackExampleConstants::NETWORK_TYPE_SC, true, 0, NULL, 0);
        break;
    }
    case 'h':
    default: {
        // Print the Help
        std::cout << std::endl << std::endl;
        // Print the application version information 
        std::cout << "CAS BACnet Stack Server Example v" << APPLICATION_VERSION << "." << CIBUILDNUMBER << std::endl;
        // Print the user actions
        std::cout << "=================================" << std::endl;
        std::cout << "User Actions:" << std::endl;
        std::cout << "\tw - Send Who-is" << std::endl;
        std::cout << "\tq - Exit Application" << std::endl;
        break;
    }
    }

    return true;
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

    // Check the primary
    uint8_t errorCode = 0;
    uint16_t bytesRead = g_ws_network.RecvWSMessage(primaryHubUri, message, maxMessageLength, &errorCode);
    if (bytesRead > 0) {
        *networkType = CASBACnetStackExampleConstants::NETWORK_TYPE_SC;
        memcpy(receivedConnectionString, primaryHubUri.c_str(), primaryHubUri.size());
        *receivedConnectionStringLength = primaryHubUri.size();


        // Get the XML rendered version of the just sent message
        static char xmlRenderBuffer[MAX_RENDER_BUFFER_LENGTH];
        if (fpDecodeAsXML((char*)message, bytesRead, xmlRenderBuffer, MAX_RENDER_BUFFER_LENGTH, CASBACnetStackExampleConstants::NETWORK_TYPE_SC) > 0) {
            std::cout << xmlRenderBuffer << std::endl;
            memset(xmlRenderBuffer, 0, MAX_RENDER_BUFFER_LENGTH);
        }


        return bytesRead;
    }

    return 0;
}

// Callback used by the BACnet Stack to send a BACnet message
uint16_t CallbackSendMessage(const uint8_t *message, const uint16_t messageLength, const uint8_t *connectionString, const uint8_t connectionStringLength, const uint8_t networkType, bool broadcast) {
    (void)broadcast;    // Not used by SC

    // Check parameters
    if (message == NULL || messageLength == 0) {
        std::cout << "Nothing to send" << std::endl;
        return 0;
    }
    if (connectionString == NULL || connectionStringLength == 0) {
        std::cout << "No connection string" << std::endl;
        return 0;
    }

    if (networkType == CASBACnetStackExampleConstants::NETWORK_TYPE_SC) {
        // Handle BACnet SC message
        uint8_t errorCode = 0;
        size_t sentBytes = g_ws_network.SendWSMessage(WSURI((char*)connectionString, connectionStringLength), message, messageLength, &errorCode);

        if (sentBytes == 0) {
            // ToDo: handle error
            fpSetBACnetSCWebSocketStatus((char*)connectionString, connectionStringLength, BACnetSCConstants::WebsocketStatus_Disconnected, 0);
            return 0;
        }

         // Get the XML rendered version of the just sent message
         static char xmlRenderBuffer[MAX_RENDER_BUFFER_LENGTH];
         if (fpDecodeAsXML((char*)message, messageLength, xmlRenderBuffer, MAX_RENDER_BUFFER_LENGTH, networkType) > 0) {
             std::cout << xmlRenderBuffer << std::endl;
             memset(xmlRenderBuffer, 0, MAX_RENDER_BUFFER_LENGTH);
         }

        return sentBytes;
    }

    // Otherwise, ignore
    return 0;
}

// System callback Functions
// ---------------------------------------------------------------------------
// Callback used by the BACnet Stack to get the current time
time_t CallbackGetSystemTime() {
    return time(0);
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

// Websocket Callbacks

bool CallbackInitiateWebsocket(const char* websocketUri, const uint32_t websocketUriLength) {
    WSURI uri = WSURI(websocketUri, websocketUriLength);

    // Add connection to the network

    uint8_t errorCode = 0;
    if (g_ws_network.AddConnection(uri, &errorCode, "./cert.pem", "./key.key")) {
        std::cout << "Connected to uri=[" << uri << "]" << std::endl;
        fpSetBACnetSCWebSocketStatus(websocketUri, websocketUriLength, BACnetSCConstants::WebsocketStatus_Connected, 0);
        return true;
    }
    else {
        std::cout << "Error: Could not connect: ErrorCode: " << errorCode << std::endl;
        fpSetBACnetSCWebSocketStatus(websocketUri, websocketUriLength, BACnetSCConstants::WebsocketStatus_Error, errorCode);
        return false;
    }
}

void CallbackDisconnectWebsocket(const char* websocketUri, const uint32_t websocketUriLength) {
    if (websocketUri == NULL || websocketUriLength == 0) {
        // Nothing to do, no websocketUri provided
        return;
    }

    WSURI uri = WSURI(websocketUri, websocketUriLength);

    // Attempt to disconnect the socket
    g_ws_network.RemoveConnection(uri);
    return;
}
