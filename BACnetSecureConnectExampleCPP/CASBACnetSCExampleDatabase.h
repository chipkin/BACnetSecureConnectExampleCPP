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
};

class ExampleDatabaseDevice : public ExampleDatabaseBaseObject {
public:
    uint32_t systemStatus;
};

class ExampleDatabase {

public:
    ExampleDatabaseAnalogInput analogInput;
    ExampleDatabaseDevice device;
    
    // Constructor / Deconstructor
    ExampleDatabase();
    ~ExampleDatabase();

    // Set all the objects to have a default value.
    void Setup();

    // Update the values as needed
    void Loop();

private:
    const std::string GetColorName();
};

#endif // __CASBACnetStackExampleDatabase_h__
