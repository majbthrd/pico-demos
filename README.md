## Install the Toolchain

```
$ sudo apt update
$ sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
```

## Build 'blinky'

```
$ cd ~/pico-demos/blinky/make
$ make
```

At this point, you have ~/pico-demos/blinky/make/blinky.uf2 as well as all the files to optionally debug with gdb and pico-debug.

## Build openocd

Until the CMSIS-DAP pull request is accepted by Raspberry Pi, we'll have to use a fork of the code.

```
$ sudo apt install automake autoconf build-essential texinfo libtool libhidapi-dev libusb-1.0-0-dev
$ git clone https://github.com/majbthrd/openocd.git --recursive --branch rp2040_cmsisdap_demo --depth=1
$ cd openocd
$ ./bootstrap
$ ./configure --enable-cmsis-dap
$ make -j4
$ sudo make install
```

## Install GDB

```
$ sudo apt install gdb-multiarch
```

## Use GDB, OpenOCD, and pico-debug to debug 'blinky'

Obtain "pico-debug-gimmecache.uf2" from the Releases at:

https://github.com/majbthrd/pico-debug

Boot the RP2040 with the BOOTSEL button pressed, copy over pico-debug-gimmecache.uf2 to the RP2040, and it immediately reboots as a CMSIS-DAP adapter (ready for OpenOCD to talk to it).

```
$ openocd -f interface/cmsis-dap.cfg -c "transport select swd" -c "adapter speed 4000" -f target/rp2040-core0.cfg
```

Your output should look something like this:

```
Info : Hardware thread awareness created
Info : RP2040 Flash Bank Command
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : CMSIS-DAP: SWD  Supported
Info : CMSIS-DAP: FW Version = 2.0.0
Info : CMSIS-DAP: Interface Initialised (SWD)
Info : SWCLK/TCK = 0 SWDIO/TMS = 0 TDI = 0 TDO = 0 nTRST = 0 nRESET = 0
Info : CMSIS-DAP: Interface ready
Info : clock speed 4000 kHz
Info : SWD DPIDR 0x0bc12477
Info : SWD DLPIDR 0x00000001
Info : rp2040.core0: hardware has 4 breakpoints, 2 watchpoints
Info : starting gdb server for rp2040.core0 on 3333
Info : Listening on port 3333 for gdb connections
```

This OpenOCD terminal needs to be left open. So go ahead and open another terminal, in this one we’ll attach a gdb instance to OpenOCD.

```
$ cd ~/pico-demos/blinky/make/build
$ gdb-multiarch blinky.elf
```

Connect GDB to OpenOCD,

```
(gdb) target remote localhost:3333
```

and load blinky.elf into flash,

```
(gdb) load
Loading section .text, size 0x24c lma 0x10000000
Loading section .init, size 0x4 lma 0x1000024c
Loading section .fini, size 0x4 lma 0x10000250
Start address 0x10000204, load size 596
Transfer rate: 422 bytes/sec, 198 bytes/write.
```

and then start it running.

```
(gdb) monitor reset init
(gdb) continue
```

Or, if you want to set a breakpoint at main() before running the executable,

```
(gdb) monitor reset init
(gdb) b main
(gdb) continue
Continuing.
Note: automatically using hardware breakpoints for read-only addresses.

Breakpoint 1, main () at ../main.c:48
48	  sys_init();
```

before continuing after you have hit the breakpoint,

```
(gdb) continue
```

To quit from gdb, type

```
(gdb) quit
```

