#!/usr/bin/env python
#
# lime2_valve_ctrl.py
# Author: Francesco Montorsi
# Creation Date: Oct 2017
# Project website: https://github.com/f18m/microirrigation-control
#
# This script is the companion script of the "firmware-cc1110-lime2-remote" firmware project.
# This script assumes it runs on a Linux system connected via SPI to the "lime2" node of the project.
#
# This script supports the following modes:
#  OPEN
#  CLOSE
#  TEST: Every 5sec command the radio module attached to the LIME2
#        to OPEN or CLOSE the idraulic VALVE attached
#

from pyA20Lime2.gpio import gpio
from pyA20Lime2.gpio import port
from pyA20Lime2.gpio import connector
import time, sys

# CONSTANTS

LIME2_RADIOMOD_GPIO1 = port.PC22
LIME2_RADIOMOD_GPIO2 = port.PC23
LIME2_RADIOMOD_HOLDOFF_TIME_SEC = 5				# specifies how many secs the radio TX module will stay quiescent after receiving a command


# FUNCTIONS 

def reset_gpio():
	gpio.output(LIME2_RADIOMOD_GPIO1, gpio.LOW)
	gpio.output(LIME2_RADIOMOD_GPIO2, gpio.LOW)

def valve_open():
	gpio.output(LIME2_RADIOMOD_GPIO1, gpio.HIGH)
	gpio.output(LIME2_RADIOMOD_GPIO2, gpio.LOW)
	time.sleep(LIME2_RADIOMOD_HOLDOFF_TIME_SEC-1)			 # we sleep close to the hold off time to make sure the radio TX module receives the command
	reset_gpio()
	print "Valve open"

def valve_close():
	gpio.output(LIME2_RADIOMOD_GPIO1, gpio.LOW)
	gpio.output(LIME2_RADIOMOD_GPIO2, gpio.HIGH)
	time.sleep(LIME2_RADIOMOD_HOLDOFF_TIME_SEC-1)			 # we sleep close to the hold off time to make sure the radio TX module receives the command
	reset_gpio()
	print "Valve closed"

def valve_test():
	while True:
		print "REST POSITION"
		time.sleep(5)
		print "VALVE OPEN!"
		valve_open()
		time.sleep(5)
		print "VALVE CLOSE!"
		valve_close()
		time.sleep(5)


# MAIN PROGRAM

gpio.init() #Initialize module. Always called first

gpio.setcfg(LIME2_RADIOMOD_GPIO1, gpio.OUTPUT)
gpio.setcfg(LIME2_RADIOMOD_GPIO2, gpio.OUTPUT)
reset_gpio()

if len(sys.argv)>=2:
	mode = sys.argv[1]

if mode  == "open":
	valve_open()
elif mode  == "close":
	valve_close()
elif mode  == "test":
	valve_test()
else:
	print "Wrong mode provided: %s" % mode
	exit(1)
	
exit(0)

