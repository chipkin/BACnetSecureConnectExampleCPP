# BACnet Secure Connect Example CPP

A basic [BACnet Secure Connect](https://www.bacnetinternational.org/page/secureconnect) (BACnetCS) server example written in C++ using the [CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack).

If you are looking for a free open source java implmeation of BACnet/SC I suggest looking at the [BACnet/SC Reference Stack](https://sourceforge.net/projects/bacnet-sc-reference-stack/)

## Releases

Build versions of this example can be downloaded from the releases page:
[https://github.com/chipkin/BACnetSecureConnectExampleCPP/releases](https://github.com/chipkin/BACnetSecureConnectExampleCPP/releases)

## Testing

A WebSockets test server node application is included in this repo.   
- `/NodeTestWSServer/` for unsecure websockets,
-  `/NodeTestWSSecureServer/` for websockets with SSL    

Install and run:  
```
npm install
npm run run
```

## Build

A [Visual studio 2022](https://visualstudio.microsoft.com/downloads/) project is included with this project. This project is also auto built using [Gitlab CI](https://docs.gitlab.com/ee/ci/) on every commit.

The [CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack) submodule is required for compilation.

### Pre-Requisites

- [Boost v1.78.0](https://www.boost.org/users/history/version_1_78_0.html)
  - Steps to set up Boost with [Visual Studios on Windows](https://stackoverflow.com/a/29567344) (Place boost library in `REPO/BACnetSecureConnectExampleCPP/submodules` instead)
  - Double check the following project properties:
    - Project Settings => C/C++ => General:  
   `\submodules\boost_1_78_0`
    - Project Settings => Linker => General: Include additional library directory   
  `\submodules\boost_1_78_0\stage\lib`, or wherever the built `libs` are located (i.e. `\submodules\boost_1_78_0\x64\lib`)
- [OpenSSL v1.1.1](https://www.openssl.org/)
  - Steps to [build OpenSSL on Windows](https://github.com/openssl/openssl/blob/master/NOTES-WINDOWS.md)
  - Double check the following project properties:
    - Project Settings => C/C++ => General:   
  `\YOUR_OPENSSL_INSTALLATION\include`
    - Project Settings => Linker => General: Include additional library directory  
   `\YOUR_OPENSSL_INSTALLATION\lib`
    - Project Settings => Linker => Input: Additional dependencies  
  `\YOUR_OPENSSL_INSTALLATION\lib\libcrypto.lib`
  `\YOUR_OPENSSL_INSTALLATION\lib\libssl.lib`
