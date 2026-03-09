```text
  ▄▄▄▄▄▄                      ▄▄▄▄      ▄▄▄▄▄   
 █▀██▀▀██                   ▄█▀▀████▄  ██▀▀▀▀█▄ 
   ██   ██ ▄                ██    ██   ▀██▄  ▄▀ 
   ██   ██ ████▄▄▀▀█▄ ▄███▀ ██    ██     ▀██▄▄  
 ▄ ██   ██ ██   ▄█▀██ ██    ██    ██   ▄   ▀██▄ 
 ▀██▀███▀ ▄█▀  ▄▀█▄██▄▀███▄  ▀████▀    ▀██████▀ 
                                                
```

> hand-built, x86, ring 0, bare-metal operating system 

DracOS is a 32-bit operating system written from scratch in C++ and a small amount of assembly.  

---

## Purpose and Motivation:

DracOS started because I wanted to learn how operating systems work and I thought the best way to learn was by building my own.
So, I started researching how x86 machines actually work, how they behave at boot, what's in the kernel, drivers and subsystems, etc. rather than treating the OS as a black box.

This project is a record of my experimentation, exploration, and implementations compiled in one place so they can read, dissected, built upon, and surpassed.

\- Preet

---

## Machine Internals

- **Kernel core**
  - Multiboot-compatible entry via GRUB
  - 32-bit protected mode
  - GDT, IDT, PIC setup and hardware interrupt handling
  - Multitasking 

- **Memory**
  - Custom heap allocator (`MemoryManager`) with chunk splitting and coalescing
  - Global `new` / `delete` backed by the kernel heap
  - Low-level `memcpy` / `memmove` / `memset` / `memcmp` implementations

- **Wire and disk**
  - AMD PCnet (am79c973) network driver
  - ATA (IDE) driver with 28-bit LBA read/write and flush
  - Raw disk image attached as an IDE drive under QEMU

- **Console and input**
  - Text-mode VGA terminal with colored output
  - Keyboard and mouse drivers
  - Simple shell on top of the terminal

- **Network stack**
  - Ethernet frame layer
  - ARP
  - IPv4
  - ICMP with a `ping` command exposed through the shell

- **Utilities and data structures**
  - Custom string and memory helpers (no host libc)
  - `printf`-style formatting and small conversion helpers
  - `LinkedList`, `HashMap`, fixed-size `Map`, and `Pair` in `os::utils::ds`

---

## Documentation 

Documentation files use Markdown files in the `docs/` directory. 
Documentation website: [github.pr33tpatel.io/dracos](https://github.pr33tpatel.io/dracos)
Note: Documentation website uses MkDocs

---

## Building

A simple Makefile drives the build; no external build system is required.

> **Safety:** DracOS (this software) is intended to be run under emulation (e.g., `qmeu-system-i386`) only.
> Booting this kernel on real hardware is not supported or tested and may corrupt data or leave your machine in an undefined state.
> Use this software at your own risk. See [License, Disclaimer, Safety](#license-disclaimer-safety) for more information.
### Prerequisites

You’ll need:

- 32-bit capable `g++`, `as`, `ld`
- `qemu-system-i386` and `qemu-img`
- `grub-mkrescue` and its dependencies

On a typical Debian/Ubuntu system, that means installing:

- `g++`, `binutils`, `qemu-system-x86`, `qemu-utils`, `grub-pc-bin`, `xorriso`, and 32-bit dev libs.

### Build the kernel

```bash
make
```

This produces `mykernel.bin`.

### Create ISO and disk image

```bash
make mykernel.iso    # bootable ISO with GRUB
make Image.img       # 128 MiB raw disk image
```

### Run under QEMU

```bash
make run
```

This boots DracOS under `qemu-system-i386` with:

- 512 MiB RAM, 1 vCPU
- PCnet NIC and user-mode networking
- `Image.img` attached as an IDE disk
- Text-mode VGA terminal

For a faster loop during debugging:

```bash
make kernel-debug
```

This boots `mykernel.bin` directly as a kernel image and enables QEMU’s CPU/interrupt logging.

More details live in [`docs/development.md`](docs/development.md).

---

## What this is (and isn’t)

DracOS is not chasing feature parity with general-purpose operating systems.
Each component is built to expose how the machine actually behaves, not to hide it behind layers of abstraction.

The focus is on system architecuture, interrupts, framebuffers, packets on the wire, heap internals, etc. 

---

## License, Disclaimer, Safety

DracOS (this software) is provided for educational and research purposes only, without warranty of any kind.  
Running custom kernels, bootloaders, or disk images always carries risk; you are responsible for how you build, run, and distribute this code on your own machines and hardware.

IT IS STRONGLY ADVISED TO RUN THIS OPERATING SYSTEM ONLY UNDER EMULATION (E.G., QEMU).  
Booting this software on real hardware is not supported and has not been tested. Doing so may corrupt data or leave your machine in an undefined state, and you are solely responsible for any damage or loss resulting from any interaction with this software.

---

## Credits

Parts of the project are inspired by and derived from
 ["Write Your Own Operating System" (WYOOS)](https://wyoos.org/) guide and its accompanying source code.  
That source code remains under its original license; this repository does not claim ownership of it. Where code has been copied or closely adapted, it is kept under the original license terms.

This project also references [wiki.osdev.org](https://wiki.osdev.org/) for certain implementations and code examples.

[MkDocs](https://www.mkdocs.org) is used to generate static doucmentation webpages.
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/) is used for the [documentation website](https://pr33tpatel.github.io/dracos) (built on MkDocs)
