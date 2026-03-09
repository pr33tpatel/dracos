# Ethernet Layer (etherframe)

> Part of the [Network Stack](index.md).  
> Sits above the [NIC driver](amd79c973.md) and below [ARP](arp.md) and [IPv4](ipv4.md).  
> Header: <a href="https://github.com/pr33tpatel/dracos/blob/main/include/net/etherframe.h" target="_blank" rel="noopener noreferrer">`include/net/etherframe.h`</a> <br>
> Source: <a href="https://github.com/pr33tpatel/dracos/blob/main/src/net/etherframe.cc" target="_blank" rel="noopener noreferrer">`src/net/etherframe.cc`</a>

The Ethernet layer is the first software layer above the NIC driver. It is responsible
for two things: dispatching incoming raw frames to the correct upper-layer protocol by
EtherType, and wrapping outgoing protocol payloads in a valid Ethernet header before
handing them to the NIC. It has no knowledge of ARP, IPv4, or any other protocol — those
register themselves with it.

---

## Frame format

The IEEE 802.3 Ethernet II frame header that this layer operates on:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Destination MAC (bytes 0–3)                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Destination MAC (bytes 4–5)   |   Source MAC (bytes 0–1)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Source MAC (bytes 2–5)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           EtherType           |        Payload ...            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

The `EtherFrameHeader` struct maps directly onto the first 14 bytes of the raw buffer:

```cpp
struct EtherFrameHeader {
    uint64_t dstMac_BE;     // 48-bit MAC, stored in low 6 bytes, big-endian
    uint64_t srcMac_BE;     // 48-bit MAC, stored in low 6 bytes, big-endian
    uint16_t etherType_BE;  // protocol identifier, big-endian
} __attribute__((packed));
```

All fields are big-endian on the wire. The NIC driver strips the preamble and FCS before
delivering frames here; this layer only sees header + payload.

Common EtherType values used by DracOS:

| EtherType | Protocol |
|---|---|
| `0x0800` | IPv4 |
| `0x0806` | ARP |

---

## EtherFrameProvider

`EtherFrameProvider` is the central dispatch object of this layer. It inherits from
`RawDataHandler` and registers itself with the NIC at construction, becoming the sole
recipient of raw frames from the hardware.

```cpp
class EtherFrameProvider : public drivers::RawDataHandler {
    EtherFrameHandler* handlers;
    drivers::amd_am79c973* backend;
public:
    EtherFrameProvider(drivers::amd_am79c973* backend);
    ~EtherFrameProvider();

    bool OnRawDataReceived(uint8_t* buffer, uint32_t size) override;
    void Send(uint64_t dstMAC_BE, uint16_t etherType_BE,
              uint8_t* buffer, uint32_t size);

    uint64_t GetMACAddress();
    uint32_t GetIPAddress();
};
```

The `handlers` array is indexed by raw 16-bit big-endian EtherType. Only one handler per
EtherType is supported; a second registration for the same EtherType silently replaces
the first.

### Receive path

```
NIC → OnRawDataReceived(buffer, size)
        │
        ├─ size check: must be >= sizeof(EtherFrameHeader)
        ├─ cast buffer head to EtherFrameHeader*
        ├─ filter:
        │    accept if dstMac_BE == our MAC
        │    accept if dstMac_BE == 0xFFFFFFFFFFFF  (broadcast)
        │    drop otherwise
        ├─ look up handlers[frame->etherType_BE]
        ├─ if handler found:
        │    sendBack = handler->OnEtherFrameReceived(
        │                   buffer + sizeof(EtherFrameHeader),
        │                   size   - sizeof(EtherFrameHeader))
        └─ if sendBack == true:
             frame->dstMac_BE = frame->srcMac_BE   // reply to sender
             frame->srcMac_BE = backend->GetMACAddress()
             return true  →  NIC will call Send(buffer, size)
```

The in-place MAC swap is the key design point of this layer. Because ARP and ICMP both
build their replies by modifying the received buffer in place and returning `true`, the
Ethernet layer (and through it the NIC) can retransmit the same buffer without any
additional allocation. The full round-trip for an ARP request or a ping involves zero
heap allocations in the reply path.

### Send path

```
caller → Send(dstMAC_BE, etherType_BE, data, size)
           │
           ├─ allocate buffer: sizeof(EtherFrameHeader) + size
           ├─ fill header:
           │    frame->dstMac_BE    = dstMAC_BE
           │    frame->srcMac_BE    = backend->GetMACAddress()
           │    frame->etherType_BE = etherType_BE
           ├─ memcpy(buffer + sizeof(EtherFrameHeader), data, size)
           ├─ backend->Send(buffer, sizeof(EtherFrameHeader) + size)
           └─ delete[] buffer
```

The caller owns `data`; this layer makes its own copy inside the allocated frame buffer
and frees it after the NIC call returns.

---

## EtherFrameHandler

Upper-layer protocols inherit from `EtherFrameHandler` to receive frames for a specific
EtherType:

```cpp
class EtherFrameHandler {
protected:
    EtherFrameProvider* backend;
    uint16_t etherType_BE;
public:
    EtherFrameHandler(EtherFrameProvider* backend, uint16_t etherType);
    virtual ~EtherFrameHandler();

    virtual bool OnEtherFrameReceived(uint8_t* etherFramePayload, uint32_t size);
    void Send(uint64_t dstMAC_BE, uint8_t* buffer, uint32_t size);
};
```

### Registration

The constructor converts `etherType` to big-endian and registers `this` in the provider's
handler table:

```cpp
EtherFrameHandler::EtherFrameHandler(EtherFrameProvider* backend, uint16_t etherType)
    : backend(backend) {
    // convert host etherType to big-endian for table indexing
    etherType_BE = ((etherType & 0x00FF) << 8) | ((etherType & 0xFF00) >> 8);
    backend->handlers[etherType_BE] = this;
}
```

The destructor nulls the entry to prevent the provider from calling into a destroyed
handler:

```cpp
EtherFrameHandler::~EtherFrameHandler() {
    if (backend->handlers[etherType_BE] == this)
        backend->handlers[etherType_BE] = 0;
}
```

### Send convenience wrapper

`EtherFrameHandler::Send` lets upper layers send without knowing their own EtherType:

```cpp
void EtherFrameHandler::Send(uint64_t dstMAC_BE, uint8_t* buffer, uint32_t size) {
    backend->Send(dstMAC_BE, etherType_BE, buffer, size);
}
```

ARP and IPv4 both use this to avoid carrying a redundant EtherType constant in their own
code.

---

## Interaction with ARP and IPv4

Both ARP and IPv4 are `EtherFrameHandler` subclasses that register themselves at
construction:

```
EtherFrameProvider
  ├── handlers[0x0608]  →  AddressResolutionProtocol   (ARP,  0x0806 big-endian)
  └── handlers[0x0008]  →  InternetProtocolProvider    (IPv4, 0x0800 big-endian)
```

Neither ARP nor IPv4 needs to hold a pointer to the NIC directly. All sends go through
`EtherFrameHandler::Send → EtherFrameProvider::Send → amd_am79c973::Send`, keeping the
hardware boundary entirely inside this layer and below.

---

## Invariants and assumptions

- **One handler per EtherType.** The table supports exactly one handler per 16-bit
  EtherType value. Registering a second handler for the same type silently replaces the
  first.
- **No FCS validation.** The NIC hardware validates and strips the FCS before delivery.
  This layer never sees or checks it.
- **No fragmentation.** Ethernet frames larger than the NIC's MTU (1518 bytes) are not
  fragmented here; the NIC driver clamps the send size.
- **Broadcast only.** Multicast filtering is not implemented. The only special MAC
  address recognized besides our own is the all-ones broadcast.
- **Allocation on send.** The outbound path allocates a heap buffer per frame. This is
  acceptable for the current throughput but would be a bottleneck under high packet
  rates.
