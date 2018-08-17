# Micro-irrigation Control #

This project contains source code for 
 - a Linux embedded system (in my case Olimex Lime2 A20 single board computer)
 - digital radio transceivers (in my case a couple of Texas Instruments CC1110 boards)

The aim of the system is to provide via a web interface the possibility to open or close remote relays,
which in my casea are attached to electrovalves that open or close some water flows (in my case to provide
irrigation in my garden).
However the target of the system is much more generic and you can attach to the "remote" node pretty much
anything you like.

## Prerequisites ##

This project assumes that you have:
1. an embedded Linux system, in particular Olimex Lime2 is assumed here.
   Moreover I tested this project only with a recent Debian-variant "armbian" installed, using DeviceTree overlays for
   accessing the SPI bus of the embedded system. See https://docs.armbian.com/User-Guide_Allwinner_overlays/.
   The DeviceTree config file used in my case is [available here](docs/armbianEnv.txt).
2. two digital radios, based on Texas Instruments CC1110, operating in the 433 or 868/915 Mhz ISM bands.
   See e.g., http://www.ti.com/tool/CC1110EMK868-915 for the commercial boards used in this project.
   This choice is motivated by the fact that these frequencies provide high wall penetration and low battery 
   consumption compared to other radio technologies like the well-known Wi-Fi.


## Architecture ##

See the architecture picture in docs folder:
<img src="docs/architecture.png" />


## Hardware Design ##

The hardware design for the remote note is available as Cadsoft Eagle schematics (see https://www.autodesk.com/products/eagle/overview)
in the hardware-remote folder. The design is based on 3 major parts:
1) the CC1110 evaluation module which provides the antenna, the CC1110 radio+micro and its programming interface. See http://www.ti.com/tool/CC1110EMK868-915
2) a commercial relay board like the SunFounder 4channel 5V relay shield module. See https://www.sunfounder.com/4-channel-5v-relay-shield-module.html
3) a custom "glue" board to provide right power and cabling between the other 2 parts. I built this on a simple stripboard (https://en.wikipedia.org/wiki/Stripboard).
   This board connects the battery source (a 12V lead-acid battery in my case) to the radio module and relay module.

This is the overview of the custom glue board (extremehely simple):

<img src="hardware-remote/remote_node_schematic.png" />

The current budget of the remote node when the firmware puts the radio in sleep mode is:
 - 100 uA for each current regulator (in the shown design an ADP3333 low dropout 300mA-max regulator was chosen)
 - 130 uA for the static resistor divider used for battery voltage probing
 - between 1 and 200 uA for the CC1110 depending on the power mode selected by firmware (power mode 1 or 2)

The current budget of the remote node when the firmware puts the radio in RX mode is dominated by the CC1110 and will be around 21mA.
Of course the "remote" node also needs to transmit an acknowledge to the "lime2" node raising current consumption up to 36mA but the TX
time is so short that can be neglected in computations.

Assuming that a 7Ah lead-acid battery is used for powering the system, and that the sensor will wake up once every 20sec to check for
commands over the radio channel, the battery life can be easily computed using e.g. https://oregonembedded.com/batterycalc.htm.
Data entered on that page in my case is:
 - 7000 mAh capacity rating
 - 0.5 mA current consumption of device during sleep
 - 21 mA current consumption of device during wake
 - 180 number of wakeups per hour
 - 1000 ms duration of wake time
The result is a battery duration of 162 days.


## Firmware and Software Design ##

Basic documentation for the [SPI protocol](docs/spi-protocol-cc1110-lime2.md)

Basic documentation for the [Radio protocol](docs/spi-protocol-cc1110-lime2.md)

TO BE WRITTEN


## Source code ##

Tree of contained source code is:

```
 firmware-cc1110-lime2-remote:  this is the firmware written in C for both the CC1110 "lime2" node and the "remote" node. 
  \-- source
       \-- apps    
            \-- lime2.c: contains the firmware source code for the node attached to the Linux embedded system over SPI
                remote.c: contains the firmware source code for the node attached to the electrovalve
  
 software-lime2: this is the PHP code for the Linux embedded system
  \-- spidev_test:  this is the Linux kernel utility written in C that provide an easy way to communicate with devices over SPI
      web:          this folder contains the PHP code implementing the specific communication protocol over-SPI;
                    this code must be run inside a regular web server (e.g. Apache, lighttpd or nginx) and uses a mix of PHP and
                    Javascript to open WebSockets to get updates about SPI activities.
      bin:          this folder contains PHP code to implement a WebSocket server using Ratchet (http://socketo.me/);  
                    the WebSocket server is used by Javascript code to fetch updates from SPI.
```

## Installation ##

TO BE WRITTEN


## Similar Projects ##

The most comprehensive project similar to this is MySensors: https://www.mysensors.org

Similar projects designed to drive relay boards that are directly connected to the Linux system (without the radio bridge):

- http://www.logicaprogrammabile.it/come-pilotare-elettrovalvola-bistabile-usando-2-rele/ (in Italian)
- https://github.com/ondrej1024/crelay
- https://github.com/darrylb123/usbrelay

Similar projects designed to drive REMOTE relay boards (not BATTERY powered though):
- https://github.com/shanet/RelayRemote
