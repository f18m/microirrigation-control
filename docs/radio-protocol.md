# Radio Protocol between Lime2 node and remote node #

This small documnet ocntains the description of the radio protocol 
used between the Lime2 node (the CC1110 radio attached to the Linux
SoC) and the remote node (the CC1110 radio attached to the relays used
for turning on/off the actuators - electrovalves in our case).

## Overview ##

The protocol is very simple and for simplicity is just embedding over
radio the same command formats used over SPI.
See the full list of SPI commands 
[available here](spi-protocol-cc1110-lime2.md).

Note that all commands are Lime2-initiated. That is the remote node,
in order to save power, will always stay silent until it receives a
command.


## Low-power policy and retry mechanism ##

Given that the radio channel is intrinsically not reliable a certain
number of retries will be done both in the Lime2 node to remote node
direction and the viceversa.
Moreover the acknowledge to one of Lime2 node commands may not arrive
because at the time it was sent the remote node may be sleeping.

Lime2 node cycles on 2 states:
 - sleep mode: radio is off, microcontroller is in power mode 2;
   this mode lasts for about 6 seconds (roughly: the micro will tick
   using the low-power timer which is not very accurate)
 - RX mode: radio is on in RX; this RX window lasts about 2 seconds;

Given numbers above the Lime2 node will attempt TX about 40 times,
so that it covers with TX commands a time window of about 10 seconds.
During these retry attempts the remote node must be able to discard
duplicate commands, using the transaction ID embedded in the radio
packets.

If no acknowledge is received by the Lime2 node after 40 attempts,
then the command is declared lost.


