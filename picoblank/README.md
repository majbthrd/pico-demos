## What is this?

picoblank.uf2 writes 0xFF to the first 256 bytes of flash.  On boot, the RP2040 Boot ROM sees the absence of a CRC32 signature and acts as if the BOOTSEL button were pressed.

This is particularly handy when the developer is only loading RAM only images.

