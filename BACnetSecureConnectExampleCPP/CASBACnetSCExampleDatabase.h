/*
 * BACnet Server Example C++
 * ----------------------------------------------------------------------------
 * CASBACnetStackExampleDatabase.h
 *
 * The CASBACnetStackExampleDatabase is a data store that contains
 * some example data used in the BACnetStackDLLExample.
 * This data is represented by BACnet objects for this server example.
 * There will be one object of each type currently supported by the CASBACnetStack.
 *
 * The database will include the following:
 *	- present value
 *	- name
 *	- for outputs priority array (bool and value)
 *
 * Created by: Steven Smethurst
 */

#ifndef __CASBACnetStackExampleDatabase_h__
#define __CASBACnetStackExampleDatabase_h__

#include <map>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

// Base class for all object types.
class ExampleDatabaseBaseObject {
public:
    // Const
    static const uint8_t PRIORITY_ARRAY_LENGTH = 16;

    // All objects will have the following properties
    std::string objectName;
    uint32_t instance;
};

class ExampleDatabaseAnalogInput : public ExampleDatabaseBaseObject {
public:
    float presentValue;
    float covIncurment;
    uint32_t reliability;
    std::string description; // This is an optional property that has been enabled.

    // DateTime Proprietary Value
    uint8_t proprietaryYear;
    uint8_t proprietaryMonth;
    uint8_t proprietaryDay;
    uint8_t proprietaryWeekDay;
    uint8_t proprietaryHour;
    uint8_t proprietaryMinute;
    uint8_t proprietarySecond;
    uint8_t proprietaryHundredthSeconds;
};

class ExampleDatabaseDevice : public ExampleDatabaseBaseObject {
public:
    int UTCOffset;
    int64_t currentTimeOffset;
    std::string description;
    uint32_t systemStatus;
};

class ExampleDatabaseNetworkPort : public ExampleDatabaseBaseObject 
{
	public:
		// Network Port Properties
		uint16_t BACnetIPUDPPort;
		uint8_t IPAddress[4];
		uint8_t IPAddressLength;
		uint8_t IPDefaultGateway[4];
		uint8_t IPDefaultGatewayLength;
		uint8_t IPSubnetMask[4];
		uint8_t IPSubnetMaskLength;
		std::vector<uint8_t*> IPDNSServers;
		uint8_t IPDNSServerLength;

		uint8_t BroadcastIPAddress[4];

		bool ChangesPending;
		uint8_t FdBbmdAddressHostType;	// 0 = None, 1 = IpAddress, 2 = Name
		uint8_t FdBbmdAddressHostIp[4];
		uint16_t FdBbmdAddressPort;
		uint16_t FdSubscriptionLifetime;
};

struct CreatedAnalogValue {
	std::string name;
	float value;

	CreatedAnalogValue() {
		this->name = "";
		this->value = 0.0f;
	}
};

class ExampleDatabase {

public:
    ExampleDatabaseAnalogInput analogInput;
    ExampleDatabaseDevice device;
    ExampleDatabaseNetworkPort networkPort;

    // Constructor / Deconstructor
    ExampleDatabase();
    ~ExampleDatabase();

    // Set all the objects to have a default value.
    void Setup();

    // Update the values as needed
    void Loop();

    // Helper Functions
    void LoadNetworkPortProperties();

private:
    const std::string GetColorName();
};

#endif // __CASBACnetStackExampleDatabase_h__
