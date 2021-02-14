## Description

At the time of writing, Rowley Crossworks for ARM does not yet have a CPU Support Package for the Raspberry Pi RP2040.  Moreover, the RP2040 uses SWD multi-drop, which Crossworks does not (at the time of writing) yet support.

However, this is a sample RP2040 project that works handily with Rowley Crossworks for ARM and pico-debug, a CMSIS-DAP debugger that runs on very same RP2040 being debugged.

## Limitations

- Crossworks has a superb feature of being able to display and decode peripheral registers, but this depends on an elaborate XML generated from the device’s SVD file.  I think we’ll have to wait on a CPU Support Package from Rowley for this functionality.

- When debugging applications with a RAM Section Placement (as opposed to FLASH), I’ve noticed that the RP2040 sometimes acts flaky if there is also a valid application programmed into flash.  The suggested workaround is to erase at least the first 256 bytes of flash when debugging RAM applications.

