#include <common/types.h>
#include <drivers/amd_am79c973.h>
#define PRINT_DATA

using namespace os;
using namespace os::common;
using namespace os::utils;
using namespace os::drivers;
using namespace os::hardwarecommunication;


RawDataHandler::RawDataHandler(amd_am79c973* backend) {
  this->backend = backend;
  backend->SetHandler(this);
};

RawDataHandler::~RawDataHandler() {
  backend->SetHandler(0);
}

bool RawDataHandler::OnRawDataReceived(uint8_t* buffer, uint32_t size) {
  return false;
}

void RawDataHandler::Send(uint8_t* buffer, uint32_t size) {
  backend->Send(buffer, size);
}


amd_am79c973::amd_am79c973(
    PeripheralComponentInterconnectDeviceDescriptor* dev, InterruptManager* interrupts
)
    : Driver(),
      InterruptHandler(interrupts, dev->interrupt + interrupts->HardwareInterruptOffset()),
      MACAddress0Port(dev->portBase),
      MACAddress2Port(dev->portBase + 0x02),
      MACAddress4Port(dev->portBase + 0x04),
      registerDataPort(dev->portBase + 0x10),
      registerAddressPort(dev->portBase + 0x12),
      resetPort(dev->portBase + 0x14),
      busControlRegisterDataPort(dev->portBase + 0x16) {
  this->handler = 0;

  currentSendBuffer = 0;
  currentRecvBuffer = 0;

  uint64_t MAC0 = MACAddress0Port.Read() % 256;
  uint64_t MAC1 = MACAddress0Port.Read() / 256;
  uint64_t MAC2 = MACAddress2Port.Read() % 256;
  uint64_t MAC3 = MACAddress2Port.Read() / 256;
  uint64_t MAC4 = MACAddress4Port.Read() % 256;
  uint64_t MAC5 = MACAddress4Port.Read() / 256;

  uint64_t MAC = MAC5 << 40 | MAC4 << 32 | MAC3 << 24 | MAC2 << 16 | MAC1 << 8 | MAC0;


  // set the device to 32 bit mode
  registerAddressPort.Write(20);
  busControlRegisterDataPort.Write(0x102);

  // STOP reset
  registerAddressPort.Write(0);
  registerDataPort.Write(0x04);

  // initBlock
  initBlock.mode = 0x0000;  // promiscuous mode = false
  // initBlock.mode = 0x8000; // promiscuous mode = true
  initBlock.reserved1 = 0;
  initBlock.numSendBuffers = 3;
  initBlock.reserved2 = 0;
  initBlock.numRecvBuffers = 3;
  initBlock.physicalAddress = MAC;

  /* TEST: split MAC address into multiple segments to fix compiler error, STATUS: not needed, compiler
  can handle uint64_t MAC:48
  // initBlock.physicalAddressLow      = MAC0 | (MAC1 << 8);
  // initBlock.physicalAddressMiddle   = MAC2 | (MAC3 << 8);
  // initBlock.physicalAddressHigh     = MAC4 | (MAC5 << 8);
  */

  initBlock.reserved3 = 0;
  initBlock.logicalAddress = 0;

  sendBufferDescr = (BufferDescriptor*)((((uint32_t)&sendBufferDescrMemory[0]) + 15) & ~((uint32_t)0xF));
  initBlock.sendBufferDescrAddress = (uint32_t)sendBufferDescr;
  recvBufferDescr = (BufferDescriptor*)((((uint32_t)&recvBufferDescrMemory[0]) + 15) & ~((uint32_t)0xF));
  initBlock.recvBufferDescrAddress = (uint32_t)recvBufferDescr;

  for (uint8_t i = 0; i < 8; i++) {
    sendBufferDescr[i].address = (((uint32_t)&sendBuffers[i]) + 15) & ~(uint32_t)0xF;
    sendBufferDescr[i].flags = 0x7FF | 0xF000;
    sendBufferDescr[i].flags2 = 0;
    sendBufferDescr[i].avail = 0;

    recvBufferDescr[i].address = (((uint32_t)&recvBuffers[i]) + 15) & ~(uint32_t)0xF;
    recvBufferDescr[i].flags = 0xF7FF | 0x80000000;
    recvBufferDescr[i].flags2 = 0;
    sendBufferDescr[i].avail = 0;
  }

  registerAddressPort.Write(1);
  registerDataPort.Write((uint32_t)(&initBlock) & 0xFFFF);
  registerAddressPort.Write(2);
  registerDataPort.Write(((uint32_t)(&initBlock) >> 16) & 0xFFFF);
}


amd_am79c973::~amd_am79c973() {}


void amd_am79c973::Activate() {
  printf("AMD am79c973 Activating...\n");

  // 1. Enable Initialization (INIT) and Interrupts (IENA)
  registerAddressPort.Write(0);
  registerDataPort.Write(0x41);

  // 2. Set Auto-Pad in CSR4 (Fixes short packets)
  registerAddressPort.Write(4);
  uint32_t temp = registerDataPort.Read();
  registerAddressPort.Write(4);
  registerDataPort.Write(temp | 0xC00);

  // 3. CRITICAL: Unmask Receive & Transmit Interrupts in CSR3
  // By default, these are 1 (Disabled). We must write 0 (Enabled).
  registerAddressPort.Write(3);
  uint32_t csr3 = registerDataPort.Read();

  // Clear Bit 10 (RINTM) - Receive Interrupt Mask
  // Clear Bit 9  (TINTM) - Transmit Interrupt Mask
  registerAddressPort.Write(3);
  registerDataPort.Write(csr3 & ~((1 << 10) | (1 << 9)));

  // 4. Start the Card (STRT) and keep Interrupts Enabled (IENA)
  registerAddressPort.Write(0);
  registerDataPort.Write(0x42);
}

int amd_am79c973::Reset() {
  resetPort.Read();
  resetPort.Write(0);
  return 10;
}


uint32_t amd_am79c973::HandleInterrupt(common::uint32_t esp) {
  registerAddressPort.Write(0);
  uint32_t temp = registerDataPort.Read();

  if ((temp & 0x8000) == 0x8000) printf(RED_COLOR, BLACK_COLOR, "NETWORK ERROR: AMD am79c973 ERROR\n");
  if ((temp & 0x2000) == 0x2000)
    printf(RED_COLOR, BLACK_COLOR, "NETWORK ERROR: AMD am79c973 COLLISION ERROR\n");
  if ((temp & 0x1000) == 0x1000)
    printf(RED_COLOR, BLACK_COLOR, "NETWORK ERROR: AMD am79c973 MISSED FRAME\n");
  if ((temp & 0x0800) == 0x0800)
    printf(RED_COLOR, BLACK_COLOR, "NETWORK ERROR: AMD am79c973 MEMORY ERROR\n");
  if ((temp & 0x0400) == 0x0400)
    printf(LIGHT_BLUE_COLOR, BLACK_COLOR, "NETWORK INTERRUPT: DATA RECEIVED\n");
  Receive();
  if ((temp & 0x0200) == 0x0200) printf(LIGHT_BLUE_COLOR, BLACK_COLOR, "NETWORK INTERRUPT: DATA SENT\n");
  // acknowledge
  registerAddressPort.Write(0);
  registerDataPort.Write(temp);

  // NOTE: printing INIT DONE got annoying so commented out
  // if((temp & 0x0100) == 0x0100) printf("AMD am79c973 INIT DONE\n");

  return esp;
}


void amd_am79c973::Send(uint8_t* buffer, int size) {
  int sendDescriptor = currentSendBuffer;
  currentSendBuffer = (currentSendBuffer + 1) % 8;

  if (size > 1518) size = 1518;

  for (uint8_t *src = buffer + size - 1,
               *dst = (uint8_t*)(sendBufferDescr[sendDescriptor].address + size - 1);
       src >= buffer;
       src--, dst--)
    *dst = *src;

#ifdef PRINT_DATA
  printf("\nSending Packet: ");
  for (int i = 0; i < size; i++) {
    printByte(buffer[i]);
    printf(" ");
  }
  printf("| Packet END.\n");
#endif

  sendBufferDescr[sendDescriptor].avail = 0;
  sendBufferDescr[sendDescriptor].flags2 = 0;
  sendBufferDescr[sendDescriptor].flags = 0x8300F000 | ((uint16_t)((-size) & 0xFFF));
  registerAddressPort.Write(0);
  registerDataPort.Write(0x48);
}


void amd_am79c973::Receive() {
  for (; (recvBufferDescr[currentRecvBuffer].flags & 0x80000000) == 0;
       currentRecvBuffer = (currentRecvBuffer + 1) % 8) {
    if (!(recvBufferDescr[currentRecvBuffer].flags & 0x40000000) &&
        (recvBufferDescr[currentRecvBuffer].flags & 0x03000000) == 0x03000000) {
      uint32_t size = recvBufferDescr[currentRecvBuffer].flags & 0xFFF;
      if (size > 64)  // remove checksum
        size -= 4;

      uint8_t* buffer = (uint8_t*)(recvBufferDescr[currentRecvBuffer].address);


      if (handler != 0) {
        if (handler->OnRawDataReceived(buffer, size)) {
          // printf("\nsend in recieve func\n");
          Send(buffer, size);
        }
      }


#ifdef PRINT_DATA
      //   print data
      printf("\nReceving Packet: ");
      for (int i = 0; i < 64; i++) {
        printByte(buffer[i]);
        printf(" ");
      }

      printf("| Packet END.\n");
    }

#endif


    recvBufferDescr[currentRecvBuffer].flags2 = 0;
    recvBufferDescr[currentRecvBuffer].flags = 0x8000F7FF;
  }
}

void amd_am79c973::SetHandler(RawDataHandler* handler) {
  this->handler = handler;
}

uint64_t amd_am79c973::GetMACAddress() {
  return initBlock.physicalAddress;
  /* TEST: from splitting mac address into multiple segments
  return initBlock.physicalAddressLow |
       ((uint64_t)initBlock.physicalAddressMiddle << 16) |
       ((uint64_t)initBlock.physicalAddressHigh << 32);
  */
}


void amd_am79c973::SetIPAddress(uint32_t IP) {
  initBlock.logicalAddress = IP;
}

uint32_t amd_am79c973::GetIPAddress() {
  return initBlock.logicalAddress;
}
