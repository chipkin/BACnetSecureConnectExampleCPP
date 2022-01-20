# BACnet Secure Connect Example CPP

A basic [BACnet Secure Connect](https://www.bacnetinternational.org/page/secureconnect) (BACnetCS) server example written in C++ using the [CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack).

## Releases

Build versions of this example can be downloaded from the releases page:
[https://github.com/chipkin/BACnetSecureConnectExampleCPP/releases](https://github.com/chipkin/BACnetSecureConnectExampleCPP/releases)

## Testing

A WebSockets test server node application is included in this repo. ```/NodeTestWSServer/``` run with ```npm run run```

## Build

A [Visual studio 2022](https://visualstudio.microsoft.com/downloads/) project is included with this project. This project is also auto built using [Gitlab CI](https://docs.gitlab.com/ee/ci/) on every commit.

The [CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack) submodule is required for compilation.

### Pre-Requisites

- [boost v1.78.0](https://www.boost.org/users/history/version_1_78_0.html) how to install on [Windows with Visual studios](https://www.boost.org/doc/libs/1_65_0/more/getting_started/windows.html)
