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
 - Create a new packaging system where packages developed by it will be created according to the makefile at the root of it or by a special “package.json” instruction
   
**Promote cross-platform deployment with Docker and library abstractions**
 - Use abstractions more and internal specs with macro guards on platform
   
**Promote client and server use of C++ with WASM**
 - Use the header and interface based design to attempt to help “compile” api calls
 - Also use “DataTransferObjects” to help transfer data over the network and potentially databases

## Design

### Project Structure using this framework
#### File tree:
Root (project root)
src
main.c
services.c
server
(server files)
client
(client files)
(all other files)
headers
services.h
build (build files)
generated
binary
objects
lib (library files)
static (if static lib is requested)
dynamic (if DLL is requested)
project.json

#### Commands

**Installing packages**
```bash
webcraft
``` 

