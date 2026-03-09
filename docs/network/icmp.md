# ICMP — Internet Control Message Protocol

> Part of the [Network Stack](index.md).  
> Sits above [IPv4](ipv4.md).  
> Header: <a href="https://github.com/pr33tpatel/dracos/blob/main/include/net/icmp.h" target="_blank" rel="noopener noreferrer">`include/net/icmp.h`</a> <br>
> Source: <a href="https://github.com/pr33tpatel/dracos/blob/main/src/net/icmp.cc" target="_blank" rel="noopener noreferrer">`src/net/icmp.cc`</a>

ICMP is the simplest protocol in the DracOS network stack and the only one currently
exposed to the user through the shell. It handles echo requests (ping) and echo replies,
and serves as the primary end-to-end diagnostic for the full network path.

---

## Role in the stack

`InternetControlMessageProtocol` inherits from `InternetProtocolHandler` and registers
with `InternetProtocolProvider` for IP protocol number `0x01`:

```cpp
InternetControlMessageProtocol::InternetControlMessageProtocol(
    InternetProtocolProvider* backend)
    : InternetProtocolHandler(backend, 0x01) {}
```

When IPv4 receives a datagram with `protocol == 1`, it extracts the payload and calls
`OnInternetProtocolReceived` here. ICMP is the leaf of the receive chain — it never
calls further handlers.

```
NIC → EtherFrameProvider → InternetProtocolProvider → InternetControlMessageProtocol
                                                              ↑ protocol == 0x01
```

---

## Message format

An ICMP message header contains:

| Field | Description |
|---|---|
| Type | `8` = echo request, `0` = echo reply |
| Code | `0` for echo messages |
| Checksum | 16-bit Internet checksum of the ICMP message |
| Data | Arbitrary payload (identifier + sequence number in practice) |

The `InternetControlMessageProtocolMessage` struct maps directly onto the ICMP payload
delivered by IPv4:

```cpp
struct InternetControlMessageProtocolMessage {
    uint8_t  type;      // 8 = echo request, 0 = echo reply
    uint8_t  code;      // 0 for echo messages
    uint16_t checksum;  // Internet checksum of the entire ICMP message
    uint32_t data;      // echo payload — identifier + sequence (or arbitrary data)
} __attribute__((packed));
```

Wire layout:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      Type     |      Code     |           Checksum            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Data / Identifier+Seq                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

ICMP type values handled by DracOS:

| Type | Meaning |
|---|---|
| `0` | Echo reply (received from a remote host responding to our ping) |
| `8` | Echo request (received from a remote host pinging us) |

All other type values are silently dropped (fall through the `switch`, return `false`).

---

## Checksum

ICMP uses the same Internet checksum function as IPv4
([`InternetProtocolProvider::Checksum`](ipv4.md#internet-checksum)), called directly on
the ICMP message buffer:

```cpp
msg->checksum = 0;  // must be zeroed before calculation
msg->checksum = InternetProtocolProvider::Checksum(
    (uint16_t*)msg,
    sizeof(InternetControlMessageProtocolMessage));
```

The checksum covers the entire ICMP message including the `data` field, unlike the IPv4
checksum which covers only the IP header. The `checksum` field must be zeroed before the
call; any non-zero value is included in the accumulation and produces an incorrect result.
Both the send path (`Ping`) and the in-place reply path zero the field explicitly before
recalculating.

---

## Receive path

`OnInternetProtocolReceived` is called by IPv4 with the ICMP payload and the original
source and destination IPs:

```
OnInternetProtocolReceived(srcIP_BE, dstIP_BE, payload, size)
  │
  ├─ printf("ICMP RECV\n")
  ├─ size check: must be >= sizeof(InternetControlMessageProtocolMessage)
  │   return false if not
  ├─ cast payload to InternetControlMessageProtocolMessage*
  │
  ├─ switch (msg->type):
  │
  │   case 0  (echo reply):
  │     printf("ping response from: 0x%08x\n", srcIP_BE)
  │     return false   // no retransmit
  │
  │   case 8  (echo request):
  │     msg->type     = 0       // convert to reply in-place
  │     msg->checksum = 0       // zero before recalculation
  │     msg->checksum = Checksum(msg, sizeof(...))
  │     return true    // signal IPv4 to swap IPs and retransmit
  │
  └─ default: return false
```

The `data` field is left untouched in the reply path. RFC 792 requires that echo reply
data be identical to echo request data, and since the buffer is modified in-place this
is satisfied without any explicit copy.

Returning `true` propagates up through IPv4 (which swaps `srcIP`/`dstIP`, resets TTL,
and recomputes the IP checksum) and then through the Ethernet layer (which swaps MACs),
and finally back to the NIC driver which retransmits the buffer. The ICMP reply path
involves zero heap allocations.

---

## Send path — `Ping`

`Ping(uint32_t ip_BE)` constructs and sends an ICMP echo request to an arbitrary IPv4
address:

```cpp
void InternetControlMessageProtocol::Ping(uint32_t ip_BE) {
    InternetControlMessageProtocolMessage icmp;
    icmp.type     = 8;       // echo request
    icmp.code     = 0;
    icmp.data     = 0x3713;  // static payload ("leet" in BE — no seq tracking)
    icmp.checksum = 0;
    icmp.checksum = InternetProtocolProvider::Checksum(
                        (uint16_t*)&icmp,
                        sizeof(InternetControlMessageProtocolMessage));

    InternetProtocolHandler::Send(ip_BE, (uint8_t*)&icmp,
                                  sizeof(InternetControlMessageProtocolMessage));
}
```

The full outbound call chain from `Ping`:

```
Ping(ip_BE)
  └─ InternetProtocolHandler::Send(ip_BE, payload, size)
       └─ InternetProtocolProvider::Send(ip_BE, protocol=1, payload, size)
            ├─ build IP header, compute IP checksum
            ├─ routing decision (direct or via gateway)
            ├─ arp->Resolve(nextHop) → dstMAC
            └─ EtherFrameHandler::Send(dstMAC, buffer, size)
                 └─ amd_am79c973::Send(frame, size)
```

`icmp` is allocated on the stack inside `Ping`; `InternetProtocolProvider::Send`
allocates its own heap buffer that includes the IP header + a copy of the ICMP message,
so the local `icmp` struct may be safely destroyed when `Ping` returns.

---

## CLI integration

`Ping` is the only network function exposed in the DracOS shell. The `ping` command
parses a dotted-decimal IP string, packs it into a big-endian `uint32_t`, and calls
`Ping` on the global ICMP instance. The reply (if any) is printed by
`OnInternetProtocolReceived` when the response interrupt fires.

There is no timeout, no sequence tracking, and no retry. The send is fire-and-forget;
the reply arrives asynchronously through the interrupt path.

---

## End-to-end diagnostic value

A successful ping confirms the entire stack is functional:

| Layer | What a successful ping confirms |
|---|---|
| NIC driver | DMA rings are operational; interrupts are firing |
| Ethernet | Frame parsing and MAC dispatch are correct |
| ARP | Target MAC was resolved and cached |
| IPv4 | Header construction, checksum, and routing are correct |
| ICMP | Echo request was built correctly; reply was recognised |

A failed ping can be narrowed down by watching the `printf` output at each layer:
`IPV4 drop` indicates a routing or address mismatch; absence of `ICMP RECV` with a
present `IPV4` log indicates a protocol dispatch failure; absence of any log indicates
the NIC is not receiving.

---

## Invariants and assumptions

- **Types 0 and 8 only.** All other ICMP types fall through the `switch` and return
  `false`. Type 3 (destination unreachable), type 11 (time exceeded), and others are
  silently discarded.
- **Static data field.** `icmp.data = 0x3713` is a hardcoded constant. Identifier and
  sequence number fields (normally occupying the high and low 16 bits of `data`) are not
  tracked, so multiple concurrent pings cannot be distinguished.
- **Fire-and-forget.** `Ping` returns immediately after sending. There is no blocking
  wait, no timeout, and no retry. If the reply never arrives (unreachable host, ARP
  timeout, etc.), nothing is printed.
- **Stack-allocated message.** The `InternetControlMessageProtocolMessage` in `Ping` is
  on the stack. It is safe because `InternetProtocolProvider::Send` copies the payload
  into its own heap buffer before `Ping` returns.
- **Single global instance.** There is one `InternetControlMessageProtocol` object
  created in `kernelMain`. There is no per-socket or per-connection concept.

---

## Future extensions

- **Sequence numbers and identifiers.** Use the high 16 bits of `data` as an identifier
  and the low 16 bits as a sequence counter, incrementing on each `Ping` call, to match
  replies to requests and report round-trip statistics.
- **Timeout handling.** Integrate with the timer driver to print "request timed out"
  if no reply arrives within a configurable window.
- **Type 3 — Destination Unreachable.** Generate and handle type 3 messages so that
  IPv4 can report routing failures back to the caller rather than silently dropping
  undeliverable packets.
- **Type 11 — Time Exceeded.** Emit type 11 when TTL reaches zero on a forwarded
  packet, which is a prerequisite for implementing `traceroute`.
