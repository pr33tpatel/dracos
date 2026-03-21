#include <net/ipv4.h>

using namespace os;
using namespace os::common;
using namespace os::net;
using namespace os::utils;


InternetProtocolHandler::InternetProtocolHandler(InternetProtocolProvider* backend, uint8_t protocol) {
  this->backend = backend;
  this->ip_protocol = protocol;
  backend->handlers[protocol] = this;
}


InternetProtocolHandler::~InternetProtocolHandler() {
  if (backend->handlers[ip_protocol] == this) backend->handlers[ip_protocol] = 0;
}


bool InternetProtocolHandler::OnInternetProtocolReceived(
    uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t* internetprotocolPayload, uint32_t size
) {
  return false;  // default case
}


void InternetProtocolHandler::Send(uint32_t dstIP_BE, uint8_t* internetprotocolPayload, uint32_t size) {
  backend->Send(dstIP_BE, ip_protocol, internetprotocolPayload, size);
}


InternetProtocolProvider::InternetProtocolProvider(
    EtherFrameProvider* backend, AddressResolutionProtocol* arp, uint32_t gatewayIP, uint32_t subnetMask
)
    : EtherFrameHandler(backend, 0x0800) {
  // set all handlers to 0
  for (int i = 0; i < 255; i++) handlers[i] = 0;

  this->arp = arp;
  this->gatewayIP = gatewayIP;
  this->subnetMask = subnetMask;
}


InternetProtocolProvider::~InternetProtocolProvider() {}


bool InternetProtocolProvider::OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size) {
  if (size < sizeof(InternetProtocolMessage)) return false;

  InternetProtocolMessage* ip_message = (InternetProtocolMessage*)etherframePayload;
  bool sendBack = false;

  if (ip_message->dstIP == backend->GetIPAddress()) {  // check if the message is for us

    int length = ip_message->totalLength;
    if (length > size) length = size;

    // TEST:
    // printf("Packet for me, Protocol:%d, Handler:%x\n",ip_message->protocol,
    // (uint32_t)handlers[ip_message->protocol]);

    if (handlers[ip_message->protocol] != 0) {
      sendBack = handlers[ip_message->protocol]->OnInternetProtocolReceived(
          ip_message->srcIP,
          ip_message->dstIP,
          etherframePayload +
              4 * ip_message
                      ->headerLength, /* the ip message header is arranged into 4 byte (32 bit) chunks */
          size - 4 * ip_message->headerLength
      );
    } else {
      printf("IPV4 error: no handler for protocol %d\n", ip_message->protocol);
    }
  } else {
    printf("IPV4 drop: DstIP: %08x != MyIP %08x\n", ip_message->dstIP, ip_message->srcIP);
  }

  if (sendBack) {
    /* if the sendBack signal is true,
    the sendBack destination is the original source,
    and sendBack source is this computer */
    uint32_t temp = ip_message->dstIP;
    ip_message->dstIP = ip_message->srcIP;
    ip_message->srcIP = temp;

    ip_message->timeToLive = 0x40;  // reset time to live to 64 steps, this changes the header so we have
                                    // to update the checksum
    ip_message->checksum = 0;
    ip_message->checksum = Checksum(
        (uint16_t*)(void*)ip_message, 4 * ip_message->headerLength
    );  // recalculate the checksum
  }

  return sendBack;
}


void InternetProtocolProvider::Send(uint32_t dstIP_BE, uint8_t protocol, uint8_t* data, uint32_t size) {
  // uint8_t* buffer =
  // (uint8_t*)MemoryManager::activeMemoryManager->malloc(sizeof(InternetProtocolMessage) + size);
  // REFACTOR:
  uint8_t* buffer = new uint8_t[sizeof(InternetProtocolMessage) + size];
  InternetProtocolMessage* message = (InternetProtocolMessage*)buffer;

  message->version = 4;
  message->headerLength = sizeof(InternetProtocolMessage) / 4;
  message->tos = 0;
  message->totalLength = size + sizeof(InternetProtocolMessage);

  message->totalLength = ((message->totalLength & 0xFF00) >> 8) | ((message->totalLength & 0x00FF) << 8);
  message->identification = 0x0100;
  message->flagsAndOffset = 0x0040;
  message->timeToLive = 0x40;
  message->protocol = protocol;

  message->dstIP = dstIP_BE;
  message->srcIP = backend->GetIPAddress();

  message->checksum = 0;  // NOTE: initalize with 0 bc this value (0) will also be accounted for in the
                          // checksum, any non-zero value will lead to the wrong checksum calculation
  message->checksum = Checksum((uint16_t*)(void*)message, sizeof(InternetProtocolMessage));

  uint8_t* databuffer = buffer + sizeof(InternetProtocolMessage);
  for (int i = 0; i < size; i++) {
    databuffer[i] = data[i];
  }

  // uint32_t route = dstIP_BE;
  /* if the destination is not within our own subnet/LAN,
     then, we don't talk to the target directly,
     instead, we let the gateway take care of it
  */
  if ((dstIP_BE & subnetMask) != (message->srcIP & subnetMask)) dstIP_BE = gatewayIP;

  backend->Send(
      arp->Resolve(dstIP_BE), this->etherType_BE, buffer, sizeof(InternetProtocolMessage) + size
  );

  // MemoryManager::activeMemoryManager->free(buffer);
  // REFACTOR:
  delete[] buffer;
}


uint16_t InternetProtocolProvider::Checksum(void* data_, uint32_t lengthInBytes) {
  uint16_t* data = (uint16_t*)data_;
  uint32_t temp = 0;

  for (int i = 0; i < lengthInBytes / 2; i++)
    temp += ((data[i] & 0xFF00) >> 8) | ((data[i] & 0x00FF) << 8);  // do it in big endian

  if (lengthInBytes % 2 == 1)
    temp += ((uint16_t)((char*)data)[lengthInBytes - 1])
            << 8;  // cast to a 1-byte array (array of single values), and shift for big endian

  while (temp & 0xFFFF0000)  // weird overflow stuff, idk
    temp = (temp & 0xFFFF) + (temp >> 16);

  return ((~temp & 0xFF00) >> 8) | ((~temp & 0x00FF) << 8);
}

uint32_t InternetProtocolProvider::GetIPAddress() {
  return backend->GetIPAddress();
}
