# Micro-irrigation Control #

This project contains source code for 
 - a Linux embedded system (in my case Olimex Lime2 A20 single board computer)
 - digital radio transceivers (in my case a couple of Texas Instruments CC1110 boards)

The system aim is to provide via a web interface the possibility to open or close a remote relais,
which in my case is attached to an electrovalve that opens or closes water flow (to my home irrigation system).
However the target of the system is much more generic and you can attach to the "remote" node pretty much
anything you like.


## Source code ##

firmware-cc1110-lime2-remote: this is the firmware for both the CC1110 "lime2" node and the "remote" node.


## Similar Projects ##

- http://www.logicaprogrammabile.it/come-pilotare-elettrovalvola-bistabile-usando-2-rele/ (in Italian)
