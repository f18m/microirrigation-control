# SPI Protocol to Lime2 node #

This small document contains the description of the protocol used over SPI to
communicate to the Lime2 node.
Typically the SPI master is a Linux system (Olimex Lime2 in this project), while
the CC1110 is the SPI slave.

## Overview ##

The idea is to follow KISS principles: the Lime2 node firmware just takes whatever
is received over SPI and bridges the received commands over radio to the "remote node".
The commands are acknowledged. 
Each command is identified by a "transaction ID" byte.
The transaction ID is a number that will be provided in the ACK response to allow the 
SPI master to associate a sent command with its ACK.

## Commands ##

All commands have a fixed length defined to be 7 bytes plus 1 "transaction ID" byte.
The commands themselves are ASCII strings. The transaction byte is not ASCII-encoded.
3 command strings are supported so far:
 1. TURNON_: signals the remote note that the relay must be turned on
 2. TURNOFF: signals the remote note that the relay must be turned off
 3. STATUS_: reports battery level of remote node to SPI master

As soon as the Lime2 node receives an SPI command, it will try to communicate with the
remote node for about 40 times before giving up (about 10secs).

## Acknowledge ##

Every command will be acknowledged with a string "ACK_" followed by 1 transaction ID byte.

## Testing communication ##

To test commands toward the Lime2 node, you must first verify you have a working setup:
1) log into the Linux system attached via SPI to the Lime2 node.
2) verify you have a device named "/dev/spidev2.0"
3) run
      cd /opt && git clone https://github.com/f18m/microirrigation-control.git
      /opt/microirrigation-control/software-lime2/bin/lime2node_cli_test.php TURNON
   to send a test command.
4) verify that the YELLOW LED on the "Lime2 node" turns on and the RED LED starts blinking
   slowly. In current firmware implementation each blink of the RED LED means a TX attempt.
5) if the remote node acknowledges the transmission, the PHP utility should return almost immediately.