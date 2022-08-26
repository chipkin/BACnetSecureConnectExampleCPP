/*
 * BACnet Server Example C++
 * ----------------------------------------------------------------------------
 * CASBACnetStackExampleDatabase.cpp
 *
 * Sets up object names and properties in the database.
 *
 * Created by: Steven Smethurst
 */

#include "CASBACnetSCExampleDatabase.h"

#include <time.h> // time()
#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
#endif // _WIN32

ExampleDatabase::ExampleDatabase() {
    this->Setup();
}

ExampleDatabase::~ExampleDatabase() {
    this->Setup();
}

const std::string ExampleDatabase::GetColorName() {
    static uint16_t offset = 0;
    static const std::vector<std::string> colors = {
        "Amber", "Bronze", "Chartreuse", "Diamond", "Emerald", "Fuchsia", "Gold", "Hot Pink", "Indigo",
        "Kiwi", "Lilac", "Magenta", "Nickel", "Onyx", "Purple", "Quartz", "Red", "Silver", "Turquoise",
        "Umber", "Vermilion", "White", "Xanadu", "Yellow", "Zebra White", "Apricot", "Blueberry"};

    ++offset;
    return colors.at(offset % colors.size());
}

void ExampleDatabase::Setup() {
    this->device.instance = 389999;
    this->device.objectName = "Device Rainbow";
    // BACnetDeviceStatus ::= ENUMERATED { operational (0), operational-read-only (1), download-required (2),
    // download-in-progress (3), non-operational (4), backup-in-progress (5) }
    this->device.systemStatus = 0; // operational (0), non-operational (4)

    // Set the object name properites.
    this->analogInput.instance = 0;
    this->analogInput.objectName = "AnalogInput " + ExampleDatabase::GetColorName();
    this->analogInput.presentValue = 1.001f;
    this->analogInput.covIncurment = 2.0f;
    this->analogInput.reliability = 0; // no-fault-detected (0), unreliable-other (7)
}

void ExampleDatabase::Loop() {

    time_t currentTime = time(0);

    static time_t updateOnceASecondTimer = 0;
    if (updateOnceASecondTimer + 1 <= currentTime) {
        updateOnceASecondTimer = currentTime;

        this->analogInput.presentValue += 1.001f;
    }
}
