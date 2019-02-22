#!/usr/bin/python3
# flash the default firmware -> PyHT6022.HantekFirmware.custom_firmware
# does not (yet) work wth OpenHantek project

__author__ = 'Robert Cope'

from PyHT6022.LibUsbScope import Oscilloscope

scope = Oscilloscope()
scope.setup()
scope.open_handle()

scope.flash_firmware()

scope.close_handle()
