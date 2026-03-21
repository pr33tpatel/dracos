#ifndef __OS__NET__IPV4_H
#define __OS__NET__IPV4_H

#include <common/types.h>
#include <net/arp.h>
#include <net/etherframe.h>
#include <utils/print.h>

namespace os {
namespace net {

struct InternetProtocolMessage {
  /* bits 0-31 */
  common::uint8_t headerLength : 4;
  common::uint8_t version : 4;
  common::uint8_t tos;
  common::uint16_t totalLength;

  /* bits 32-63 */
  common::uint16_t identification;
  common::uint16_t flagsAndOffset;

  /* bits 64-95 */
  common::uint8_t timeToLive;
  common::uint8_t protocol;
  common::uint16_t checksum;

  /* bits 96-127 */
  common::uint32_t srcIP;  // source address

  /* bits 128-159 */
  common::uint32_t dstIP;  // destination address

  /* total: 20 bytes */
} __attribute__((packed));

class InternetProtocolProvider;

class InternetProtocolHandler {
 protected:
  InternetProtocolProvider* backend;
  common::uint8_t ip_protocol;

 public:
  InternetProtocolHandler(InternetProtocolProvider* backend, common::uint8_t protocol);
  ~InternetProtocolHandler();

  bool virtual OnInternetProtocolReceived(
      common::uint32_t srcIP_BE,
      common::uint32_t dstIP_BE,
      common::uint8_t* internetprotocolPayload,
      common::uint32_t size
  );
  void Send(common::uint32_t dstIP_BE, common::uint8_t* internetprotocolPayload, common::uint32_t size);
};


class InternetProtocolProvider : public EtherFrameHandler {
  friend class InternetProtocolHandler;

 protected:
  InternetProtocolHandler* handlers[255];
  AddressResolutionProtocol* arp;
  common::uint32_t gatewayIP;
  common::uint32_t subnetMask;

 public:
  InternetProtocolProvider(
      EtherFrameProvider* backend,
      AddressResolutionProtocol* arp,
      common::uint32_t gatewayIP,
      common::uint32_t subnetMask
  );
  ~InternetProtocolProvider();

  bool virtual OnEtherFrameReceived(common::uint8_t* etherframePayload, common::uint32_t size);
  void Send(
      common::uint32_t dstIP_BE, common::uint8_t protocol, common::uint8_t* data, common::uint32_t size
  );
  static common::uint16_t Checksum(void* data_, common::uint32_t lengthInBytes);
  common::uint32_t GetIPAddress();
};

}  // namespace net
}  // namespace os

#endif
