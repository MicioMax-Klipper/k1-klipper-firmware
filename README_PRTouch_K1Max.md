# K1 Max Creality PRTouch v2 experimental bed firmware

Experimental bed MCU firmware for a rooted Creality K1 Max using the original
Creality PRTouch v2 probing logic.

## Status

Alpha / experimental.

Tested on one modified K1 Max only.

This is not a plug-and-play upgrade and it is not a full installer. You are
expected to understand Klipper, the K1 Max MCU layout, firmware flashing, and
how to recover your printer if something goes wrong.

## Architecture

This setup uses:

- Cartographer for bed mesh / bed mapping.
- Creality PRTouch v2 for the final Z touch / Z home.
- A patched Klipper host wrapper.
- Dedicated START_PRINT / nozzle preparation macros.

The old MDF / Pellcorp load-cell probe path is disabled in this branch.

## Target

Known tested target:

- Printer: Creality K1 Max
- Bed MCU family: bed0_110_G21
- Firmware version string: bed0_117_001

Do not flash this firmware if your bed MCU target does not match.

## Build

From this repository:

    ./_build.sh bed

Expected output:

    outfw/bed0_110_G21-bed0_117_001.bin

Known SHA256 for the tested binary:

    019927067a20d1706c27456fca16dadb2c887b83f894f0cfc71fbe8bd70b572f  outfw/bed0_110_G21-bed0_117_001.bin

The binary contains:

    bed0_117_001
    Request Serial Bootloader!! ~

## Relevant firmware config

Current bed firmware configuration uses:

    CONFIG_WANT_LOAD_CELL_PROBE=n
    CONFIG_HAVE_PRTOUCH=y
    CONFIG_WANT_PRTOUCH_V2=y

The build includes the Creality-style helpers:

    src/hx711s.c
    src/dirzctl.c
    src/prtouch_v2.c

## Important warning

The firmware is only one part of the system. You also need the matching Klipper
host wrapper and printer configuration/macros.

The known-good order is:

1. heat bed / start hotend heating
2. nozzle prep and wipe
3. Cartographer bed mesh
4. Creality PRTouch final Z home
5. purge
6. print

In particular, the final PRTouch Z home must happen after the Cartographer mesh
is created/loaded.

## Flashing

Copy the generated binary to the K1 Max, for example:

    scp outfw/bed0_110_G21-bed0_117_001.bin root@k1max:/usr/data/klipper/fw/K1/bed0_110_G21-bed0_117_001.bin

Then flash from the printer:

    flash_bed_mcu /usr/data/klipper/fw/K1/bed0_110_G21-bed0_117_001.bin

Do not flash blindly. Backup your existing firmware/config first.

## Toolchain and build instructions

These instructions are intentionally minimal. They are for a Debian / Ubuntu /
WSL style build host.

Install the build dependencies:

    sudo apt update
    sudo apt install -y git python3 build-essential make \
        gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi \
        libusb-1.0-0-dev pkg-config stm32flash dfu-util

Clone this repository and select the experimental branch:

    git clone https://github.com/MicioMax-Klipper/k1-klipper-firmware.git
    cd k1-klipper-firmware
    git checkout k1max-creality-prtouch-bed

Build only the K1 Max bed MCU firmware:

    ./_build.sh bed

The compiled firmware is written here:

    outfw/bed0_110_G21-bed0_117_001.bin

Known tested binary:

    019927067a20d1706c27456fca16dadb2c887b83f894f0cfc71fbe8bd70b572f  outfw/bed0_110_G21-bed0_117_001.bin

Copy it to the printer, for example:

    scp outfw/bed0_110_G21-bed0_117_001.bin root@k1max:/usr/data/klipper/fw/K1/bed0_110_G21-bed0_117_001.bin

Then flash it from the printer side:

    flash_bed_mcu /usr/data/klipper/fw/K1/bed0_110_G21-bed0_117_001.bin

Read the host/config repository documentation too. This firmware alone is not a
complete working system.
