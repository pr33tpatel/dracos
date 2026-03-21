#ifndef __OS__NET__UDP_H
#define __OS__NET__UDP_H

#include <common/types.h>
#include <memorymanagement.h>
#include <net/ipv4.h>
#include <utils/print.h>


namespace os {
namespace net {

struct UserDatagramProtocolMessage {
  common::uint16_t srcPort;
  common::uint16_t dstPort;
  common::uint16_t length;
  common::uint16_t checksum;
} __attribute__((packed));

class UserDatagramProtocolProvider;
class UserDatagramProtocolSocket;
class UserDatagramProtocolHandler;


class UserDatagramProtocolSocket {
  friend class UserDatagramProtocol;

 public:
  common::uint16_t remotePort;
  common::uint32_t remoteIP;
  common::uint16_t localPort;
  common::uint32_t localIP;
  UserDatagramProtocolProvider* backend;
  UserDatagramProtocolHandler* handler;

 public:
  UserDatagramProtocolSocket(UserDatagramProtocolProvider* backend);
  ~UserDatagramProtocolSocket();
  virtual void HandleUserDatagramProtocolMessage(common::uint8_t* data, common::uint16_t size);
  virtual void Send(common::uint8_t* data, common::uint16_t size);
  virtual void Disconnect();
};


class UserDatagramProtocolHandler {
 public:
  UserDatagramProtocolHandler();
  ~UserDatagramProtocolHandler();
  virtual void HandleUserDatagramProtocolMessage(
      UserDatagramProtocolSocket* socket, common::uint8_t* data, common::uint16_t size
  );
};


class UserDatagramProtocolProvider : InternetProtocolHandler {
 protected:
  UserDatagramProtocolSocket* sockets[65535];  // array of all sockets
  common::uint16_t numSockets;
  common::uint16_t freePort;

 public:
  UserDatagramProtocolProvider(InternetProtocolProvider* backend);
  ~UserDatagramProtocolProvider();
  bool OnInternetProtocolReceived(
      common::uint32_t srcIP_BE,
      common::uint32_t dstIP_BE,
      common::uint8_t* internetprotocolPayload,
      common::uint32_t size
  );

  virtual UserDatagramProtocolSocket* Connect(common::uint32_t ip, common::uint16_t port);
  virtual void Disconnect(UserDatagramProtocolSocket* socket);
  virtual void Send(UserDatagramProtocolSocket* socket, common::uint8_t* data, common::uint16_t size);
};

}  // namespace net
}  // namespace os

#endif
