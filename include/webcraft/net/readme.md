# Networking

## What is Networking?

Networking is the practice of connecting computers and other devices together to share resources. Networking is a broad field that encompasses many technologies, protocols, and standards. Networking can be divided into two main categories: wired and wireless.

## How Do Computers and Processes Communicate?

Computers communicate with each other using a variety of protocols. A protocol is a set of rules that govern how data is transmitted between devices. The most common protocol used on the internet is the Transmission Control Protocol/Internet Protocol (TCP/IP). TCP/IP is a suite of protocols that defines how data is transmitted over the internet. Another alternative to this protocol is User Datagram Protocol (UDP). UDP is a connectionless protocol that is faster than TCP but less reliable. All of these happen over Sockets. A socket is an endpoint for communication between two machines.

## How Do We Implement Sockets in Our Framework?

We implement sockets on top of the native sockets provided by Windows and Linux operating systems SDK. For TCP/IP communication we use the 'Socket' and 'ServerSocket' classes provided in 'webcraft/net/sockets.h'. For UDP communication we use the 'DatagramSocket' class provided in 'webcraft/net/sockets.h'. The implementation definition is as follows:

```
class SocketBase {
private:
	SOCKET handle; # SOCKET is a typedef for SOCKET on Windows and int on Linux

}

class Socket {
}
```