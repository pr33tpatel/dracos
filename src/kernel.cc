#include <asciiart.h>
#include <ciu/ciu.h>
#include <cli/commandregistry.h>
#include <cli/shell.h>
#include <common/types.h>
#include <drivers/amd_am79c973.h>
#include <drivers/ata.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/terminal.h>
#include <drivers/timer.h>
#include <drivers/vga.h>
#include <gdt.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <hardwarecommunication/interrupts.h>
#include <hardwarecommunication/pci.h>
#include <memorymanagement.h>
#include <multitasking.h>
#include <net/arp.h>
#include <net/etherframe.h>
#include <net/icmp.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <syscalls.h>
#include <utils/ds/hashmap.h>
#include <utils/print.h>
#include <utils/string.h>

// NOTE: this turns GRAPHICSMODE on/off
// #define GRAPHICSMODE
#define NETWORK

using namespace os;
using namespace os::common;
using namespace os::utils;
using namespace os::utils::ds;
using namespace os::drivers;
using namespace os::hardwarecommunication;
using namespace os::gui;
using namespace os::net;
using namespace os::cli;
using namespace os::ciu;


// Console Event Handlers
class PrintfKeyboardEventHandler : public KeyboardEventHandler {
 public:
  void OnKeyDown(char c) {
    // char* foo = " ";
    // foo[0] = c;
    // printf(foo);
    putChar(c);
  }
};

class MouseToConsole : public MouseEventHandler {
  int8_t x, y;

 public:
  MouseToConsole() {}

  virtual void OnActivate() {
    static uint16_t* VideoMemory = (uint16_t*)0xb8000;
    x = 40;
    y = 12;
    VideoMemory[80 * y + x] = ((VideoMemory[80 * y + x] & 0xF000) >> 4) |
                              ((VideoMemory[80 * y + x] & 0x0F00) << 4) |
                              ((VideoMemory[80 * y + x] & 0x00FF));
  }

  void OnMouseMove(int8_t x_offset, int8_t y_offset) {
    static uint16_t* VideoMemory = (uint16_t*)0xb8000;

    VideoMemory[80 * y + x] = ((VideoMemory[80 * y + x] & 0xF000) >> 4) |
                              ((VideoMemory[80 * y + x] & 0x0F00) << 4) |
                              ((VideoMemory[80 * y + x] & 0x00FF));

    x += (int8_t)x_offset;
    if (x < 0) x = 0;  // prevent mouse overflow
    if (x >= 80) x = 79;

    y += (int8_t)y_offset;
    if (y < 0) y = 0;  // prevent mouse overflow
    if (y >= 25) y = 24;

    VideoMemory[80 * y + x] = ((VideoMemory[80 * y + x] & 0xF000) >> 4) |
                              ((VideoMemory[80 * y + x] & 0x0F00) << 4) |
                              ((VideoMemory[80 * y + x] & 0x00FF));
  }
};


// void sysprintf(char* str) {
//   asm("int $0x80" : : "a" (4), "b" (str));
// }
// multitasking test functions,
// TODO: move to test/system/multitasking.cc
void taskA() {
  /*
  while(true) {
    printf("A");
    for (int i =0; i < 100000000; i++);
    // asm("sti");
  }
  */
  while (true) {
    sysprintf("A");
  }
}
void taskB() {
  /*
  while(true) {
    printf("B");
    for (int i =0; i < 100000000; i++);
    // asm("sti");
  }
  */
  while (true) {
    sysprintf("B");
  }
}

void TestHashTable() {
  printf(YELLOW_COLOR, BLACK_COLOR, "INITIALIZING HASH TABLE TEST...\n");

  // 1. Create the Table
  // Key: const char* (String), Value: int
  os::utils::ds::HashMap<const char*, int> map;

  // ---------------------------------------------------------
  // TEST 1: Basic Insertion & Lookup
  // ---------------------------------------------------------
  const char* key1 = "cpu_count";
  int val1 = 4;
  map.Insert(key1, val1);

  const char* key2 = "total_memory";
  int val2 = 1024;
  map.Insert(key2, val2);

  int result = 0;
  if (map.Get(key1, result) && result == 4) {
    printf(LIGHT_GREEN_COLOR, BLACK_COLOR, "[PASS] Key 'cpu_count' found with value: %d\n", result);
  } else {
    printf(RED_COLOR, BLACK_COLOR, "[FAIL] Key 'cpu_count' not found or wrong value.\n");
  }

  // ---------------------------------------------------------
  // TEST 2: Updating an Existing Key
  // ---------------------------------------------------------
  int newVal = 8;
  map.Insert(key1, newVal);  // Update "cpu_count" to 8

  if (map.Get(key1, result) && result == 8) {
    printf(LIGHT_GREEN_COLOR, BLACK_COLOR, "[PASS] Key 'cpu_count' updated correctly to: %d\n", result);
  } else {
    printf(RED_COLOR, BLACK_COLOR, "[FAIL] Key 'cpu_count' failed to update. Got: %d\n", result);
  }

  // ---------------------------------------------------------
  // TEST 3: Collision Handling (Harder Test)
  // ---------------------------------------------------------
  // We deliberately pick strings that might collide or just rely on the map logic
  const char* key3 = "collision_A";
  const char* key4 = "collision_B";
  int val3 = 100;
  int val4 = 200;

  map.Insert(key3, val3);
  map.Insert(key4, val4);
  int res3 = 0, res4 = 0;
  bool found3 = map.Get(key3, res3);
  bool found4 = map.Get(key4, res4);

  if (found3 && found4 && res3 == 100 && res4 == 200) {
    printf(LIGHT_GREEN_COLOR, BLACK_COLOR, "[PASS] Collision/Multiple keys retrieved successfully.\n");
    LinkedList<const char*> keyList;
    map.GetKeys(keyList);
    printf(BROWN_COLOR, BLACK_COLOR, "Keys in Hashmap: ");
    keyList.printList();

    LinkedList<int> valueList;
    map.GetValues(valueList);
    printf(BROWN_COLOR, BLACK_COLOR, "values in Hashmap: ");
    valueList.printList();

    LinkedList<Pair<const char*, int>> kvPairList;
    map.GetPairs(kvPairList);
    printf(BROWN_COLOR, BLACK_COLOR, "pairs in Hashmap: ");
    kvPairList.printPairList();


  } else {
    printf(RED_COLOR, BLACK_COLOR, "[FAIL] Collision test failed. A: %d, B: %d\n", res3, res4);
  }

  // ---------------------------------------------------------
  // TEST 4: Missing Key
  // ---------------------------------------------------------
  const char* missing = "non_existent";
  if (!map.Get(missing, result)) {
    printf(LIGHT_GREEN_COLOR, BLACK_COLOR, "[PASS] Missing key correctly returned false.\n");
  } else {
    printf(RED_COLOR, BLACK_COLOR, "[FAIL] Missing key was found? Value: %d\n", result);
  }
}

typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors() {
  for (constructor* i = &start_ctors; i != &end_ctors; i++) (*i)();
}


extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/) {
  Terminal terminal;
  // terminal->Clear();

  printf("Hello World! :)\n");

  GlobalDescriptorTable gdt;

  // clang-format off
  uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);  // HACK: find the upper memory bound in kilobytes, based on multiboot_info struct from GNU
  // clang-format on
  size_t heapStart = 10 * 1024 * 1024;  // NOTE: start of heap is at 10 MiB, address: 0x00A00000
  size_t padding = 10 * 1024;           // NOTE: padding is 10 KiB
  size_t heapSize = (*memupper) * 1024 - heapStart -
                    padding;  // NOTE: (*memupper) returns the total RAM above 1 MB, multiplying by
                              // 1024 converts total available RAM above 1 MB to bytes
  // NOTE: on a 512 MB RAM system, heapSize := 500 MB => (512 MB - ~1 MB) - 10 MiB - 10 KiB = 500 MB
  /* DIAGRAM: MEMORYDIMENSION at boot
   - MEMORYDIMENSION := { [ padding (10 KB)], [ ... ], [ heapStart    ...    heapSize ]}

  */
  MemoryManager heap(heapStart, heapSize);

  printf("Heap Start: 0x%08x = %d MiB\n", heapStart, (heapStart / (1024 * 1024)));

  // printf("\nheap start: 0x"); // 10 MiB heap start should be: 0x00A0000
  // printfHex((heapStart >> 3*8) & 0xFF); // byte 3 (MSB) => 0x00 & 0xFF = 0x00
  // printfHex((heapStart >> 2*8) & 0xFF); // byte 2       => 0xA0 & 0xFF = 0b(1010 0000) & 0b(1111 1111)
  // = 0b(1010 0000) = 0xA0 printfHex((heapStart >> 1*8 ) & 0xFF); // byte 1      => 0x00 & 0xFF = 0x00
  // printfHex((heapStart ) & 0xFF); // byte 0 (LSB)  => 0x00 & 0xFF = 0x00 void* allocated =
  // memoryManager.malloc(1024); // allocated printf("\nallocated: 0x"); printfHex(((size_t)allocated >>
  // 24) & 0xFF); printfHex(((size_t)allocated >> 16) & 0xFF); printfHex(((size_t)allocated >> 8 ) &
  // 0xFF); printfHex(((size_t)allocated      ) & 0xFF); printf("\n");


  // Multitasking/
  TaskManager taskManager;
  Task task1(&gdt, taskA);
  Task task2(&gdt, taskB);
  /*
  taskManager.AddTask(&task1);
  taskManager.AddTask(&task2);
  */

  InterruptManager interrupts(0x20, &gdt, &taskManager);
  SyscallHandler syscalls(&interrupts, 0x80);

  // printf("Initializing Hardware, Stage 1\n");
#ifdef GRAPHICSMODE
  /* NOTE: dont comment out, THIS IS THE DESKTOP */
  Desktop desktop(320, 200, 0xA8, 0x00, 0x00);
#endif

  DriverManager drvManager;

  Shell shell;

  CommandRegistry commandRegistry;

  ProgrammableIntervalTimer timer(&interrupts, 100);  // [PIT timer, frequency := 1000Hz => tick = 1ms]
  uint64_t startTicks = timer.ticks;                  // [tracks ticks passed from boot]

#ifdef GRAPHICSMODE
  MouseDriver mouse(&interrupts, &desktop);  // NOTE: handler: &desktop attaches the mouse to the desktop
#else
  MouseToConsole mousehandler;
  MouseDriver mouse(&interrupts, &mousehandler);
#endif
  drvManager.AddDriver(&mouse);


#ifdef GRAPHICSMODE
  KeyboardDriver keyboard(&interrupts, &desktop);
#else
  // PrintfKeyboardEventHandler kbhandler;
  // KeyboardDriver keyboard(&interrupts, &kbhandler);
  KeyboardDriver keyboard(&interrupts, &shell);

#endif
  drvManager.AddDriver(&keyboard);

  PeripheralComponentInterconnectController PCIController;
  // shell.SetPCI(&PCIController);
  PCIController.SelectDrivers(&drvManager, &interrupts);
  // PCIController.PrintPCIDrivers();

  VideoGraphicsArray vga;


  // printf("Initializing Hardware, Stage 2\n");
  drvManager.ActivateAll();

  // printf("Initializing Hardware, Stage 3\n");

  hardwarecommunication::Port8Bit pic2Mask(0xA1);
  uint8_t mask = pic2Mask.Read();

  mask |= 0xC0;
  pic2Mask.Write(mask);

  // primary ATA, interrupt 14
  // AdvancedTechnologyAttachment ata0m(0x1F0, true);
  // if (ata0m.Identify())
  //   printf("ATA PRIMARY MASTER FOUND\n");


#ifdef NETWORK

  // identify eth0
  amd_am79c973* eth0 = 0;
  for (int i = 0; i < drvManager.numDrivers; i++) {
    if (drvManager.drivers[i] != 0) {
      // We assume the network card is the one that ISN'T the mouse or keyboard
      // (You can also check by casting if you have RTTI, but this is standard for osdev)
      if (drvManager.drivers[i] != &mouse && drvManager.drivers[i] != &keyboard) {
        eth0 = (amd_am79c973*)drvManager.drivers[i];
        printf("Network Driver Found at Index: %d\n", i);  // Debug print
        break;
      }
    }
  }

  // --- CRITICAL SAFETY CHECK ---
  if (eth0 == 0) {
    printf("ERROR: No Network Card found!\n");
    printf("Possible causes:\n");
    printf("1. PCI Driver did not detect device 0x1022:0x2000\n");
    printf("2. MemoryManager::malloc failed (Heap full?)\n");
    while (1);  // Freeze kernel here so you can see the error
  }
  for (int i = 0; i < drvManager.numDrivers; i++) {
    if (drvManager.drivers[i] != 0) {
      if (i == 2) {
        eth0 = (amd_am79c973*)drvManager.drivers[i];
        printf("Found eth0 ");
        break;
      }
    }
  }

  if (eth0 != 0) printf("Welcome to DracOS Network\n");


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

  eth0->SetIPAddress(ip_BE);  // tell network card that this is our IP

  EtherFrameProvider etherframe(eth0);  // communicates with network card

  AddressResolutionProtocol arp(&etherframe);  // communicates with etherframe provider middle-layer

  InternetProtocolProvider ipv4(&etherframe, &arp, gip_BE, subnet_BE);

  InternetControlMessageProtocol icmp(&ipv4);

  UserDatagramProtocolProvider udp(&ipv4);

#endif


#ifdef GRAPHICSMODE
  vga.SetMode(320, 200, 8);  // 320x200x256
  Window win1(&desktop, 10, 10, 20, 20, 0x00, 0x00, 0xA8);
  desktop.AddChild(&win1);
  Window win2(&win1, 30, 40, 30, 30, 0x00, 0xA8, 0x00);
  desktop.AddChild(&win2);
#endif


  /* TEST: uncomment this to use the shell
  bool systemDepsOK = commandRegistry.setSystemCmdDependencies(&shell, &PCIController, &heap);
  bool networkDepsOK = commandRegistry.setNetworkCmdDependencies(&arp, &ipv4, &icmp);
  // commandRegistry.setProcessCmdDependencies(&taskManager);

  if (systemDepsOK) {
    commandRegistry.RegisterAllCommands();
  }
  */


  CIU::Init();
  // system dependencies
  commandRegistry.InjectDependency("SYS.SHELL", &shell);
  commandRegistry.InjectDependency("SYS.PCI", &PCIController);
  commandRegistry.InjectDependency("SYS.HEAP", &heap);
  commandRegistry.InjectDependency("SYS.TERMINAL", &terminal);
  commandRegistry.InjectDependency("SYS.TIMER", &timer);

  // network dependencies
  commandRegistry.InjectDependency("NET.ARP", &arp);
  commandRegistry.InjectDependency("NET.IPV4", &ipv4);
  commandRegistry.InjectDependency("NET.ICMP", &icmp);
  // commandRegistry.InjectDependency("NET.UDP", &udp);

  // process dependencies
  commandRegistry.InjectDependency("PROC.TASKMANAGER", &taskManager);

  // filesystem dependencies

  // commandRegistry.ValidateAllDependencies();
  // bool sysDeps = commandRegistry.ValidateSystemDependencies();
  // bool netDeps = commandRegistry.ValidateNetworkDependencies();
  // bool procDeps = commandRegistry.ValidateProcessDependencies();
  // bool fsDeps = commandRegistry.ValidateFileSystemDependencies();

  // bool sysCmds = commandRegistry.RegisterSystemCommands();
  // bool netCmds = commandRegistry.RegisterNetworkCommands();
  // bool procCmds = commandRegistry.RegisterProcessCommands();
  // bool fsCmds = commandRegistry.RegisterFileSystemCommands();

  bool allCmds = commandRegistry.RegisterAllCommands();

  // TestHashTable();

  // activate interupts last
  interrupts.Activate();


  char* send_data = "7777777";
  // arp.Resolve(gip_BE);
  // ipv4.Send(gip_BE, 0x01, (uint8_t*) send_data, strlen(send_data));
  // printf(YELLOW_COLOR, BLACK_COLOR,"Length of data sent: %d Chars\t\t", strlen(send_data));
  // printf(YELLOW_COLOR, BLACK_COLOR,"Size of data sent: %d Bytes\n", sizeof(send_data[0]) *
  // strlen(send_data)); printf(YELLOW_COLOR, BLACK_COLOR,"Data sent: %s\n", send_data);
  //
  // clearScreen();
  arp.BroadcastMACAddress(gip_BE);
  // icmp.Ping(gip_BE);

  UserDatagramProtocolSocket* udpSocket = udp.Connect(gip_BE, 1234);
  char* udpData = "DracOS UserDatagramProtocol Connected\nMessage Sent!\n";
  udpSocket->Send((uint8_t*)udpData, strlen(udpData));

  // shell.ExecuteCommand();

  // printf("DracOS MWHAHAHHAH !!");

  printf(WHITE_COLOR, BLACK_COLOR, "ALL SYSTEMS GO\n");
  shell.PrintPrompt();
  while (1) {
    asm volatile("hlt");  // halt cpu until next interrupt, saving power and does not max out cpu usage
// using "hlt" is better than an while(1) infinite loop because it does not waste CPU cycles, generate
// heat, drain battery/power, etc.
#ifdef GRAPHICSMODE
    desktop.Draw(&vga);
#endif
  }
}
