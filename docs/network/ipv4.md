# IPv4 — Internet Protocol

> Part of the [Network Stack](index.md).  
> Sits above [Ethernet](ethernet.md), uses [ARP](arp.md) for MAC resolution, and
> dispatches to [ICMP](icmp.md) and future transport protocols.  
> Header: <a href="https://github.com/pr33tpatel/dracos/blob/main/include/net/ipv4.h" target="_blank" rel="noopener noreferrer">`include/net/ipv4.h`</a> <br>
> Source: <a href="https://github.com/pr33tpatel/dracos/blob/main/src/net/ipv4.cc" target="_blank" rel="noopener noreferrer">`src/net/ipv4.cc`</a>

IPv4 is the network-layer protocol responsible for addressing, routing, and delivering
datagrams between hosts. In DracOS it forms the middle tier of the network stack:
it receives raw payloads from the Ethernet layer, decides where to send outgoing packets,
and dispatches incoming payloads to the correct upper-layer handler by IP protocol number.

---

## Role in the stack

`InternetProtocolProvider` is an `EtherFrameHandler` that registers with
`EtherFrameProvider` for EtherType `0x0800`. It mirrors the same provider/handler
pattern the Ethernet layer uses:

```
EtherFrameProvider  (EtherType dispatch)
  └── InternetProtocolProvider  (protocol number dispatch)
        ├── handlers   →  Internet Control Message Protocol (ICMP)
        ├── handlers   →  User Datagram Protocol (UDP)
        └── handlers   →  Transmission Control Protocol (TCP)
```

`InternetProtocolProvider` holds a pointer to the ARP instance and calls
`arp->Resolve(nextHopIP)` on every outbound packet to obtain the destination MAC before
handing the frame down to the Ethernet layer.

---

## IP header

The IPv4 header carries:

| Field | Description |
|---|---|
| Version / IHL | IPv4, header length in 32-bit words |
| ToS | Type of service (set to 0) |
| Total length | Header + payload, big-endian |
| Identification | Packet ID (currently static) |
| Flags / fragment offset | DF flag set; fragmentation not supported |
| TTL | Time to live (set to 64) |
| Protocol | Upper-layer protocol number (1 = ICMP, 17 = UDP, 6 = TCP) |
| Header checksum | 16-bit Internet checksum of the header |
| Source IP | Sender's IPv4 address |
| Destination IP | Target IPv4 address |

The standard 20-byte IPv4 header (no options). The `InternetProtocolMessage` struct maps
directly onto the first bytes of the Ethernet payload:

```cpp
struct InternetProtocolMessage {
    uint8_t  headerLength : 4;    // IHL: header length in 32-bit words (always 5)
    uint8_t  version      : 4;    // IP version (always 4)
    uint8_t  tos;                 // Type of Service — set to 0
    uint16_t totalLength;         // header + payload (big-endian after manual swap)
    uint16_t identification;      // packet ID — currently static (0x0100)
    uint16_t flagsAndOffset;      // DF=1 (0x0040), fragmentation not supported
    uint8_t  timeToLive;          // TTL — initialized to 0x40 (64)
    uint8_t  protocol;            // upper-layer protocol number
    uint16_t checksum;            // Internet checksum of header only
    uint32_t srcIP;               // sender's IPv4 address (big-endian)
    uint32_t dstIP;               // target's IPv4 address (big-endian)
} __attribute__((packed));
```

Wire layout:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |Type of Service|          Total Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identification        |Flags|     Fragment Offset     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Time to Live |    Protocol   |        Header Checksum        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Source Address                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Destination Address                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Payload (protocol data) ...                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Well-known protocol numbers used by DracOS:

| Protocol | Number |
|---|---|
| ICMP | `1` |
| TCP  | `6`  |
| UDP  | `17`  |

---

## InternetProtocolProvider class

```cpp
class InternetProtocolProvider : public EtherFrameHandler {
    InternetProtocolHandler* handlers;
    AddressResolutionProtocol* arp;
    uint32_t gatewayIP;
    uint32_t subnetMask;
public:
    InternetProtocolProvider(EtherFrameProvider* backend,
                             AddressResolutionProtocol* arp,
                             uint32_t gatewayIP,
                             uint32_t subnetMask);
    ~InternetProtocolProvider();

    bool OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size) override;
    void Send(uint32_t dstIP_BE, uint8_t protocol,
              uint8_t* data, uint32_t size);

    static uint16_t Checksum(void* data, uint32_t lengthInBytes);
};
```

The constructor calls `EtherFrameHandler(backend, 0x0800)`, which registers `this` in
the Ethernet layer's dispatch table. The `handlers` array is zero-initialized across all
255 entries in the constructor body. Only one handler per protocol number is supported;
a second registration silently replaces the first.

---

## Receive path

```
EtherFrameProvider → OnEtherFrameReceived(payload, size)
  │
  ├─ size check: must be >= sizeof(InternetProtocolMessage); return false if not
  ├─ cast payload to InternetProtocolMessage*
  │
  ├─ if ip->dstIP != backend->GetIPAddress():
  │    printf("IPV4 drop: DstIP: %08x != MyIP %08x\n", ...)
  │    return false
  │
  ├─ clamp length: if ip->totalLength > size, use size
  ├─ look up handlers[ip->protocol]
  │
  ├─ if handler != 0:
  │    sendBack = handler->OnInternetProtocolReceived(
  │                   ip->srcIP,
  │                   ip->dstIP,
  │                   payload + 4 * ip->headerLength,   // skip header (32-bit words)
  │                   size    - 4 * ip->headerLength)
  │
  ├─ else:
  │    printf("IPV4 error: no handler for protocol %d\n", ip->protocol)
  │
  └─ if sendBack == true:
       swap ip->srcIP ↔ ip->dstIP
       ip->timeToLive = 0x40              // reset TTL to 64
       ip->checksum   = 0                 // must be zeroed before recalculation
       ip->checksum   = Checksum(ip, 4 * ip->headerLength)
       return true  →  Ethernet layer handles MAC swap and retransmit
```

The payload offset `4 * ip->headerLength` correctly skips the 20-byte standard header
(`headerLength == 5`) and would also skip options if they were present in a received
packet, though DracOS never generates them.

Note that `srcIP` and `dstIP` are passed directly into
`OnInternetProtocolReceived` — upper-layer protocols receive the original source and
destination IPs and may use them to build their own reply payloads before returning
`true`.

---

## Send path

```
caller → Send(dstIP_BE, protocol, data, size)
  │
  ├─ allocate: new uint8_t[sizeof(InternetProtocolMessage) + size]
  ├─ fill header:
  │    version        = 4
  │    headerLength   = sizeof(InternetProtocolMessage) / 4   (= 5)
  │    tos            = 0
  │    totalLength    = size + sizeof(InternetProtocolMessage)
  │                     then byte-swapped manually to big-endian
  │    identification = 0x0100   (static)
  │    flagsAndOffset = 0x0040   (DF set)
  │    timeToLive     = 0x40     (64)
  │    protocol       = protocol arg
  │    dstIP          = dstIP_BE
  │    srcIP          = backend->GetIPAddress()
  │    checksum       = 0  →  Checksum(message, sizeof(InternetProtocolMessage))
  │
  ├─ copy data into buffer + sizeof(InternetProtocolMessage)
  │
  ├─ routing decision:
  │    if (dstIP_BE & subnetMask) != (srcIP & subnetMask):
  │        dstIP_BE = gatewayIP      // off-link: route via gateway
  │    (otherwise send directly to dstIP_BE)
  │
  ├─ dstMAC = arp->Resolve(dstIP_BE)
  ├─ backend->Send(dstMAC, this->etherType_BE,
  │                buffer, sizeof(InternetProtocolMessage) + size)
  └─ delete[] buffer
```

The `totalLength` field requires an explicit manual byte-swap because the field is stored
in big-endian on the wire but the struct is written in host byte order. This is the only
field that needs a manual swap; `srcIP` and `dstIP` are stored in big-endian throughout
the stack already.

---

## Internet checksum

Both IPv4 and ICMP use the same static helper. The implementation performs the
accumulation in big-endian and handles odd-length buffers:

```cpp
uint16_t InternetProtocolProvider::Checksum(void* data_, uint32_t lengthInBytes) {
    uint16_t* data = (uint16_t*)data_;
    uint32_t temp = 0;

    for (int i = 0; i < lengthInBytes / 2; i++)
        // byte-swap each word to accumulate in big-endian
        temp += ((data[i] & 0xFF00) >> 8) | ((data[i] & 0x00FF) << 8);

    // handle odd trailing byte
    if (lengthInBytes % 2 == 1)
        temp += ((uint16_t)((char*)data_)[lengthInBytes - 1]) << 8;

    // fold 32-bit carry back into 16 bits
    while (temp & 0xFFFF0000)
        temp = (temp & 0xFFFF) + (temp >> 16);

    // one's complement, then byte-swap result back
    return ((~temp & 0xFF00) >> 8) | ((~temp & 0x00FF) << 8);
}
```

The checksum field **must be zeroed** before calling this function. Both the send path
and the in-place reply path explicitly set `checksum = 0` before the call. A non-zero
value in the checksum field will be included in the accumulation and produce an incorrect
result.

The carry-folding `while` loop handles the case where multiple additions overflow the
16-bit boundary more than once — this is mathematically guaranteed to terminate in at
most two iterations for a 20-byte header, but the loop form is correct in general.

ICMP calls this function directly on its own message buffer (see [ICMP](icmp.md#checksum)).

---

## InternetProtocolHandler

Upper-layer protocols inherit from `InternetProtocolHandler`:

```cpp
class InternetProtocolHandler {
protected:
    InternetProtocolProvider* backend;
    uint8_t ip_protocol;
public:
    InternetProtocolHandler(InternetProtocolProvider* backend, uint8_t protocol);
    virtual ~InternetProtocolHandler();

    virtual bool OnInternetProtocolReceived(
        uint32_t srcIP_BE, uint32_t dstIP_BE,
        uint8_t* payload, uint32_t size);

    void Send(uint32_t dstIP_BE, uint8_t* payload, uint32_t size);
};
```

Note that `OnInternetProtocolReceived` receives both `srcIP_BE` and `dstIP_BE` directly
from the IP header. This is important: ICMP uses `srcIP_BE` to log the reply source, and
a future TCP implementation would use both to identify the connection.

The destructor nulls the handler entry to prevent the provider calling into a destroyed
object:

```cpp
InternetProtocolHandler::~InternetProtocolHandler() {
    if (backend->handlers[ip_protocol] == this)
        backend->handlers[ip_protocol] = 0;
}
```

`Send` is a thin wrapper that forwards to the provider with the protocol number already
set, so upper layers do not need to track their own protocol number:

```cpp
void InternetProtocolHandler::Send(uint32_t dstIP_BE,
                                   uint8_t* payload, uint32_t size) {
    backend->Send(dstIP_BE, ip_protocol, payload, size);
}
```

---

## Static network configuration

There is no DHCP. Network parameters are hardcoded in `kernelMain` and passed to
`InternetProtocolProvider` at construction.
``` cpp
  /* IP address */
  uint8_t ip1 = 10, ip2 = 0, ip3 = 2, ip4 = 15;
  uint32_t ip_BE =
      ((uint32_t)ip4 << 24) | ((uint32_t)ip3 << 16) | ((uint32_t)ip2 << 8) | ((uint32_t)ip1);

  /* GatewayIP address  */
  uint8_t gip1 = 10, gip2 = 0, gip3 = 2, gip4 = 2;
  uint32_t gip_BE =
      ((uint32_t)gip4 << 24) | ((uint32_t)gip3 << 16) | ((uint32_t)gip2 << 8) | ((uint32_t)gip1);

  /* Subnet Mask */
  uint8_t subnet1 = 255, subnet2 = 255, subnet3 = 255, subnet4 = 0;
  uint32_t subnet_BE = ((uint32_t)subnet4 << 24) | ((uint32_t)subnet3 << 16) | ((uint32_t)subnet2 << 8) |
                       ((uint32_t)subnet1);
```
The IP address is stored inside the NIC driver's `initBlock.logicalAddress` and
retrieved by `EtherFrameProvider::GetIPAddress()` whenever `srcIP` is needed.

---

## Invariants and assumptions

- **No fragmentation.** `flagsAndOffset = 0x0040` sets the DF bit on every outbound
  packet. Packets exceeding the NIC's 1518-byte MTU are truncated by the NIC driver,
  not fragmented. Incoming fragmented packets are passed to the upper-layer handler
  as-is; reassembly is not implemented.
- **Static identification.** `identification` is always `0x0100`. This is harmless for
  ICMP (single datagram, no reassembly) but must be incremented per-packet before TCP
  or UDP are implemented.
- **Destination filtering.** Only packets addressed exactly to our IP are accepted.
  Broadcast (`255.255.255.255`), subnet broadcast, and multicast addresses are dropped.
- **Single default route.** The routing decision is a single subnet mask comparison.
  There is no routing table, no ECMP, and no dynamic routing protocol.
- **One handler per protocol.** A second `InternetProtocolHandler` registering for the
  same protocol number silently replaces the first.
- **Allocation on send.** Every outbound packet allocates a heap buffer with `new[]`
  and frees it with `delete[]` after the NIC call returns. This is safe under the
  current single-threaded model but would require attention under concurrent senders.
