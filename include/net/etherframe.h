#ifndef __OS__NET__ETHERFRAME_H
#define __OS__NET__ETHERFRAME_H

#include <common/types.h>
#include <drivers/amd_am79c973.h>
#include <memorymanagement.h>

namespace os {
namespace net {


class EtherFrameProvider;
class EtherFrameHandler;

struct EtherFrameHeader {
  common::uint64_t dstMac_BE : 48;
  common::uint64_t srcMac_BE : 48;
  common::uint16_t etherType_BE;

} __attribute__((packed));

typedef common::uint32_t EtherFrameFooter;


/* EtherFrameHandler: processes EtherFrames for network protocols, (EtherFrames provided from
 * EtherFrameProvider) */
class EtherFrameHandler {
 protected:
  EtherFrameProvider* backend;
  common::uint16_t etherType_BE;

 public:
  EtherFrameHandler(EtherFrameProvider* backend, common::uint16_t etherType);
  ~EtherFrameHandler();

  bool virtual OnEtherFrameReceived(common::uint8_t* etherframePayload, common::uint32_t size);
  void Send(common::uint64_t dstMAC_BE, common::uint8_t* data, common::uint32_t size);
};

/* EtherFrameProvider: turns raw data into EthernetFrames*/
class EtherFrameProvider : public drivers::RawDataHandler {
  friend class EtherFrameHandler;

 protected:
  EtherFrameHandler* handlers[65535];

 public:
  EtherFrameProvider(drivers::amd_am79c973* backend);
  ~EtherFrameProvider();

  bool virtual OnRawDataReceived(common::uint8_t* buffer, common::uint32_t size);
  void Send(
      common::uint64_t dstMAC_BE,
      common::uint16_t etherType_BE,
      common::uint8_t* data,
      common::uint32_t size
  );

  common::uint64_t GetMACAddress();
  common::uint32_t GetIPAddress();
};

}  // namespace net
}  // namespace os

#endif
