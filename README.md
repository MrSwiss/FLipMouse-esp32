# FLipMouse goes ESP32

This repository contains the ongoing progress on the firmware for the FLipMouse version3.
It will be API compatible with version 2, but with major improvements on speed (instead of one CortexM0 we will have a dual-core Tensilica and an additional CortexM0 for USB).

If you want to have a look at the FLipMouse, follow [this](https://github.com/asterics/FLipMouse) link to the main repository.

The BLE-HID stuff is based on the other [repository](https://github.com/asterics/esp32_mouse_keyboard), where we use the ESP32 as BLE module replacement (EZKey modules are really buggy and expensive).

__Following 3 steps are necessary to completely update/flash the firmware and the necessary support files/devices:__

## Building the ESP32 firmware

Please follow the step-by-step instructions of Espressif's ESP-IDF manual to setup the build infrastructure:
[Setup Toolchain](https://esp-idf.readthedocs.io/en/latest/get-started/index.html#setup-toolchain)

After a successful setup, you should be able to build the FLipMouse/FABI firmware by executing 'make flash monitor'.

## Building the LPC11U14 firmware (USB bridging chip)

The dedicated USB chip firmware is located in this repository: [usb_bridge](https://github.com/benjaminaigner/usb_bridge/).
Please clone this repository, and import it into the MCUXpresso IDE.

You can download the IDE here:
[MCUXpresso IDE](http://www.nxp.com/mcuxpresso/ide)

After a successful build, you need to flash the firmware to the LPC11U14 chip. We recommend using a development board by Embedded Artists,
because it is very useful for initial testing & debugging. It is possible to split this board into two pieces, one LPC11U14 target and a JTAG
programmer, which we use on a fully assembled FLipMouse. Please refer to the corresponding documents of this board how to connect the JTAG header to
our FLipMouse (will be announced as soon as the PCB is final).

LPCXpresso development board:

[LPCXpresso 11U14](http://embeddedartists.com/products/lpcxpresso/lpc11U14_xpr.php)

## Uploading the WebGUI

The FLipMouse with ESP32 contains a web interface for customizing the parameters. The web page itself is located in a SPIFFS image.

Flashing onto the FLipMouse is done with the script *flashspiffs.sh*. Please note: if you have a different COM port than **ttyUSB0**, you need
to adjust this setting in the script!

*Note:* This will NOT build a new SPIFFS image. If you want to update the web page, you need to install [mkspiffs](https://github.com/igrr/mkspiffs) and
execute the script *mkspiffs*


## WARNING: THIS IS EARLY WORK IN PROGRESS

