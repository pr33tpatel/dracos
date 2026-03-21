#include <net/udp.h>

using namespace os;
using namespace os::common;
using namespace os::net;
using namespace os::utils;


UserDatagramProtocolHandler::UserDatagramProtocolHandler() {}


UserDatagramProtocolHandler::~UserDatagramProtocolHandler() {}


void UserDatagramProtocolHandler::HandleUserDatagramProtocolMessage(
    UserDatagramProtocolSocket* socket, uint8_t* data, uint16_t size
) {}


UserDatagramProtocolSocket::UserDatagramProtocolSocket(UserDatagramProtocolProvider* backend) {
  this->backend = backend;
  handler = 0;
}


UserDatagramProtocolSocket::~UserDatagramProtocolSocket() {}


void UserDatagramProtocolSocket::HandleUserDatagramProtocolMessage(uint8_t* data, uint16_t size) {
  if (handler != 0) {
    handler->HandleUserDatagramProtocolMessage(this, data, size);
  }
}


void UserDatagramProtocolSocket::Send(uint8_t* data, uint16_t size) {
  backend->Send(this, data, size);
}


void UserDatagramProtocolSocket::Disconnect() {
  backend->Disconnect(this);
}


UserDatagramProtocolProvider::UserDatagramProtocolProvider(InternetProtocolProvider* backend)
    : InternetProtocolHandler(backend, 0x11) {
  for (int i = 0; i < 65535; i++) {
    sockets[i] = 0;
  }
  numSockets = 0;
  freePort = 1024;
}


UserDatagramProtocolProvider::~UserDatagramProtocolProvider() {}


bool UserDatagramProtocolProvider::OnInternetProtocolReceived(
    uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t* internetprotocolPayload, uint32_t size
) {
  if (size < sizeof(UserDatagramProtocolMessage)) {
    return false;
  }

  UserDatagramProtocolMessage* msg = (UserDatagramProtocolMessage*)internetprotocolPayload;
  uint16_t localPort = msg->dstPort;  // the destination of the message we recieved is our local port
  uint16_t remotePort = msg->srcPort;


  UserDatagramProtocolSocket* socket = 0;
  for (int i = 0; i < numSockets && socket == 0; i++) {
    bool validPorts = (sockets[i]->localPort == msg->dstPort && sockets[i]->remotePort == msg->srcPort);
    bool validIPs = (sockets[i]->localIP == dstIP_BE && sockets[i]->remoteIP == srcIP_BE);

    if (validPorts && validIPs) {
      socket = sockets[i];
    }

    if (socket != 0) {
      socket->HandleUserDatagramProtocolMessage(
          internetprotocolPayload + sizeof(UserDatagramProtocolMessage),
          size - sizeof(UserDatagramProtocolMessage)
      );
    }
  }

  return false;
}


UserDatagramProtocolSocket* UserDatagramProtocolProvider::Connect(uint32_t ip, uint16_t port) {
  UserDatagramProtocolSocket* socket = 0;
  socket = new UserDatagramProtocolSocket(this);
  if (socket != 0) {
    new (socket) UserDatagramProtocolSocket(this);

    socket->remotePort = port;
    socket->remoteIP = ip;
    socket->localPort = freePort++;
    socket->localIP = backend->GetIPAddress();

    // convert ports to big endian
    socket->remotePort = ((socket->remotePort & 0xFF00) >> 8) | ((socket->remotePort & 0x00FF) << 8);
    socket->localPort = ((socket->localPort & 0xFF00) >> 8) | ((socket->localPort & 0x00FF) << 8);

    sockets[numSockets++] = socket;
  }
  return socket;
}


void UserDatagramProtocolProvider::Disconnect(UserDatagramProtocolSocket* socket) {
  for (int i = 0; i < numSockets; i++) {
    if (sockets[i] == socket) {
      sockets[i] = sockets[--numSockets];
      delete socket;
      break;
    }
  }
}


void UserDatagramProtocolProvider::Send(
    UserDatagramProtocolSocket* socket, uint8_t* data, uint16_t size
) {
  uint16_t totalLength = size + sizeof(UserDatagramProtocolMessage);
  uint8_t* buffer = new uint8_t[totalLength];
  uint8_t* buffer2 = buffer + sizeof(UserDatagramProtocolMessage);
  UserDatagramProtocolMessage* msg = (UserDatagramProtocolMessage*)buffer;

  msg->srcPort = socket->localPort;
  msg->dstPort = socket->remotePort;
  msg->length = ((totalLength & 0x00FF) << 8) | ((totalLength & 0xFF00) >> 8);

  for (int i = 0; i < size; i++) {
    buffer2[i] = data[i];
  }

  msg->checksum = 0;  // deactivate the checksum lmao
  InternetProtocolHandler::Send(socket->remoteIP, buffer, totalLength);

  delete[] buffer;
}
