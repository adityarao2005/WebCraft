# WebCraft
The C++ Web Framework

## Problem Statement

C++ 20 has so many cool features which many developers don’t use because everyone is stuck on the old image of C++ which is clunky. But if we build this properly it can be as effective and performant as any of these other high-level languages if not better.


## Ideology

**Promote interface/header-based design.**
 - Can have a “headers” folder under root where it contains all the interfaces and headers in it
 - Then there can be a cmd command bundled with the project where when run, itll create a “build/generated” folder with all the generated files from the files in the header folder
 - This you can copy paste this into your src folder
   
**Promote service-based architecture and good use of design patterns**
 - Create dependency injection related packages and files
   
**Promote extensibility and ability to bundle with packages**
 - Create a new packaging system where packages developed by it will be created according to the makefile at the root of it or by a special “project.json” instruction file
   
**Promote cross-platform deployment with Docker and library abstractions**
 - Use abstractions more and internal specs with macro guards on platform
   
**Promote client and server use of C++ with WASM**
 - Use the header and interface based design to attempt to help “compile” api calls
 - Also use “DataTransferObjects” to help transfer data over the network and potentially databases

## Design

### Requirements

1. Project file tree definitions
2. Package management capabilities (like npm or pip)
3. Utility commands for project management (like what npm and pip have)

### Project Structure using this framework
#### File tree:
```
root/ (project root)
 +-- src/
 |    +-- main.c
 |    +-- services.c
 |    +-- server/ (server files)
 |    +-- client/ (client files)
 |    +-- (all other files)
 +-- headers/
 |    +-- services.h
 +-- resources/ (any other useful files should be here)
 +-- build/ (build files)
 |    +-- generated/
 |    +-- binary/
 |    +-- objects/
 +-- lib/ (library files)
 |    +-- include/
 |    +-- static/ (if static lib is requested)
 |    +-- dynamic/ (if DLL is requested)
 +-- project.json
```

### Package management capabilities

1. Need to be able to fetch source code from GitHub or any other online repository and build it
2. Should also be able to fetch source code from zip and build it
3. Should be able to grab header files and binaries from remote repositories
4. Should be able to link system libraries and header files


### Utility commands for project management

1. Should be able to assist in fetching and installing packages
2. Should be able to clean up dependencies and build files
3. Should be able to build, link, and run the project in development and production mode





