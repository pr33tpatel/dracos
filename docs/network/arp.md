# ARP — Address Resolution Protocol

> Part of the [Network Stack](index.md).  
> Sits above [Ethernet](ethernet.md) and is used by [IPv4](ipv4.md) to resolve next-hop MACs.  
> Header: <a href="https://github.com/pr33tpatel/dracos/blob/main/include/net/arp.h" target="_blank" rel="noopener noreferrer">`include/net/arp.h`</a> <br>
> Source: <a href="https://github.com/pr33tpatel/dracos/blob/main/src/net/arp.cc" target="_blank" rel="noopener noreferrer">`src/net/arp.cc`</a>

ARP solves a fundamental problem: given an IPv4 address, what is the hardware (MAC)
address of the machine that owns it? Without this mapping, IPv4 cannot send packets
because Ethernet frames require a destination MAC. ARP operates directly over Ethernet
(EtherType `0x0806`) and has no dependency on IPv4 or any higher layer.

---

## Role in the stack

ARP is a peer of IPv4 at the Ethernet layer. Both register as `EtherFrameHandler`
subclasses with the `EtherFrameProvider`, each claiming a different EtherType:

```
EtherFrameProvider
  ├── handlers[0x0608]  →  AddressResolutionProtocol   (ARP,  EtherType 0x0806)
  └── handlers[0x0008]  →  InternetProtocolProvider    (IPv4, EtherType 0x0800)
```

IPv4 holds a pointer to the ARP instance and calls `Resolve(dstIP)` before every
outbound packet. ARP never calls back into IPv4.

---

## Packet format

ARP packets for IPv4-over-Ethernet have a fixed 28-byte layout. The `ARPMessage` struct
maps directly onto the payload delivered by the Ethernet layer:

An ARP packet carries:

| Field | Description |
|---|---|
| Hardware type | `0x0001` = Ethernet |
| Protocol type | `0x0800` = IPv4 |
| Hardware address size | `6` (MAC) |
| Protocol address size | `4` (IPv4) |
| Command | `0x0001` = request, `0x0002` = reply |
| Source MAC | Sender's MAC |
| Source IP | Sender's IPv4 |
| Destination MAC | Target MAC (zeroed in a request) |
| Destination IP | Target IPv4 |

```cpp
struct ARPMessage {
    uint16_t hardwareType_BE;    // 0x0001 = Ethernet
    uint16_t protocolType_BE;    // 0x0800 = IPv4
    uint8_t  hardwareAddressSize; // 6 (MAC)
    uint8_t  protocolAddressSize; // 4 (IPv4)
    uint16_t command_BE;          // 0x0001 = request, 0x0002 = reply
    uint64_t srcMAC_BE;           // sender hardware address (48-bit in low bytes)
    uint32_t srcIP_BE;            // sender protocol address
    uint64_t dstMAC_BE;           // target hardware address (zeroed in request)
    uint32_t dstIP_BE;            // target protocol address
} __attribute__((packed));
```

All multi-byte fields are big-endian on the wire. The struct is 28 bytes; no padding is
inserted due to `__attribute__((packed))`.

Wire layout:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Hardware Type         |         Protocol Type         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  HW Addr Size |  PR Addr Size |           Command             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Sender MAC (bytes 0–3)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Sender MAC (bytes 4–5)       |    Sender IP (bytes 0–1)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Sender IP (bytes 2–3)        |    Target MAC (bytes 0–1)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Target MAC (bytes 2–5)                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Target IP                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

---

## AddressResolutionProtocol class

```cpp
class AddressResolutionProtocol : public EtherFrameHandler {
    EtherFrameProvider* etherframe;

    // flat cache: parallel arrays of IP → MAC mappings
    uint32_t IPcache; [blog.leonardotamiano](https://blog.leonardotamiano.xyz/tech/linux-kernel-qemu-setup/)
    uint64_t MACcache; [blog.leonardotamiano](https://blog.leonardotamiano.xyz/tech/linux-kernel-qemu-setup/)
    int numCacheEntries;

public:
    AddressResolutionProtocol(EtherFrameProvider* etherframe);
    ~AddressResolutionProtocol();

    bool OnEtherFrameReceived(uint8_t* etherFramePayload, uint32_t size) override;

    void RequestMACAddress(uint32_t IP_BE);
    void BroadcastMACAddress(uint32_t IP_BE);
    uint64_t Resolve(uint32_t IP_BE);

private:
    uint64_t GetMACFromCache(uint32_t IP_BE);
};
```

The constructor calls `EtherFrameHandler(etherframe, 0x0806)`, which registers `this`
in the Ethernet layer's dispatch table for EtherType `0x0806`.

---

## Receive path

`OnEtherFrameReceived` is called by `EtherFrameProvider` when an ARP frame arrives.

```
OnEtherFrameReceived(payload, size)
  │
  ├─ cast payload to ARPMessage*
  ├─ validate:
  │    hardwareType == 0x0100 (BE for 0x0001)
  │    protocolType == 0x0008 (BE for 0x0800)
  │    hardwareAddressSize == 6
  │    protocolAddressSize == 4
  │    drop if any check fails
  │
  ├─ check dstIP_BE == our IP; drop if not
  │
  ├─ if command == 0x0100  (request, BE):
  │    reply in-place:
  │      arp->dstMAC_BE = arp->srcMAC_BE
  │      arp->dstIP_BE  = arp->srcIP_BE
  │      arp->srcMAC_BE = backend->GetMACAddress()
  │      arp->srcIP_BE  = backend->GetIPAddress()
  │      arp->command   = 0x0200  (reply, BE)
  │    return true  →  Ethernet layer swaps MACs and retransmits
  │
  └─ if command == 0x0200  (reply, BE):
       store srcIP_BE → srcMAC_BE in cache
       return false  (no retransmit needed)
```

The in-place reply avoids any allocation: the same 28-byte buffer that was delivered by
the Ethernet layer is modified and sent back. The Ethernet layer handles the outer MAC
swap (see [Ethernet receive path](ethernet.md#receive-path)).

---

## Send path

### `RequestMACAddress(uint32_t IP_BE)`

Broadcasts a "who has `IP`?" request onto the local network:

```cpp
void AddressResolutionProtocol::RequestMACAddress(uint32_t IP_BE) {
    ARPMessage arp;
    arp.hardwareType_BE      = 0x0100;  // Ethernet
    arp.protocolType_BE      = 0x0008;  // IPv4
    arp.hardwareAddressSize  = 6;
    arp.protocolAddressSize  = 4;
    arp.command_BE           = 0x0100;  // request

    arp.srcMAC_BE = etherframe->GetMACAddress();
    arp.srcIP_BE  = etherframe->GetIPAddress();
    arp.dstMAC_BE = 0xFFFFFFFFFFFF;     // broadcast
    arp.dstIP_BE  = IP_BE;

    EtherFrameHandler::Send(0xFFFFFFFFFFFF, (uint8_t*)&arp, sizeof(ARPMessage));
}
```

The reply will arrive asynchronously via `OnEtherFrameReceived` and populate the cache.
There is no completion event; callers poll the cache after sending (see `Resolve`).

### `BroadcastMACAddress(uint32_t IP_BE)`

Sends a gratuitous ARP — a reply packet announcing our own IP → MAC mapping without
having been asked. This is used at startup to populate stale caches on the network and
to notify other hosts of a new IP assignment:

```cpp
void AddressResolutionProtocol::BroadcastMACAddress(uint32_t IP_BE) {
    ARPMessage arp;
    // ... same header fields as request ...
    arp.command_BE = 0x0200;            // reply

    arp.srcMAC_BE = etherframe->GetMACAddress();
    arp.srcIP_BE  = IP_BE;
    arp.dstMAC_BE = 0xFFFFFFFFFFFF;
    arp.dstIP_BE  = IP_BE;              // target == source (gratuitous)

    EtherFrameHandler::Send(0xFFFFFFFFFFFF, (uint8_t*)&arp, sizeof(ARPMessage));
}
```

---

## Cache and resolution

### Cache structure

The cache is two parallel fixed-size arrays:

```cpp
uint32_t IPcache; [blog.leonardotamiano](https://blog.leonardotamiano.xyz/tech/linux-kernel-qemu-setup/)
uint64_t MACcache; [blog.leonardotamiano](https://blog.leonardotamiano.xyz/tech/linux-kernel-qemu-setup/)
int      numCacheEntries = 0;
```

`GetMACFromCache(IP_BE)` does a linear scan over `IPcache[0..numCacheEntries-1]` and
returns the corresponding `MACcache` entry if found, or `0` if not. Lookup is O(n);
acceptable for the current scale of a single-NIC kernel with a small local network.

### `Resolve(uint32_t IP_BE)`

`Resolve` is the primary interface used by IPv4. It is synchronous and blocking:

```
Resolve(IP_BE)
  │
  ├─ check cache: if found, return MAC immediately
  │
  ├─ send RequestMACAddress(IP_BE)
  │
  ├─ poll loop (up to 128 iterations):
  │    check cache
  │    if found: return MAC
  │    (busy-wait — no sleep/yield)
  │
  └─ if still not found:
       log "ARP timeout for <IP>"
       return 0xFFFFFFFFFFFF  (broadcast MAC as fallback)
```

The busy-wait poll works under the current single-threaded, interrupt-driven model
because the ARP reply interrupt can fire between polling iterations. When a preemptive
scheduler is added, this will need to be replaced with a proper sleep/wake mechanism
keyed on the cache update.

The broadcast MAC fallback means that if resolution fails, the packet is sent to all
hosts on the segment. This is intentional: it avoids silently dropping packets during
development where ARP may not yet be fully functional, at the cost of broadcasting
undeliverable traffic.

---

## Invariants and assumptions

- **Cache never evicts.** Once `numCacheEntries == 128`, new replies are silently
  discarded. Entries are never removed or refreshed.
- **One entry per IP.** There is no duplicate-check on insertion; if a host changes its
  MAC the old entry will persist and shadow the new one until the cache is full and
  rebuilt (e.g. reboot).
- **IPv4 over Ethernet only.** `hardwareType != 0x0001` or `protocolType != 0x0800`
  causes the frame to be dropped immediately.
- **Big-endian throughout.** All IP and MAC values in `ARPMessage` are big-endian on the
  wire. The cache also stores big-endian values; callers must pass `IP_BE` in
  network byte order.
- **No locking.** The cache is accessed from the interrupt handler (`OnEtherFrameReceived`)
  and from kernel thread context (`Resolve`). This is safe only because DracOS is
  currently single-threaded and non-preemptive.
