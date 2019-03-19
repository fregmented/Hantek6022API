/**
 * Copyright (C) 2009 Ubixum, Inc. 
 * Copyright (C) 2015 Jochen Hoenicke
 * Copyright (C) 2017 Sebastian Zagrodzki
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 **/

#include <fx2macros.h>
#include <setupdat.h>
#include <i2c.h>
#include <eputils.h>
#include <delay.h>

#define SYNCDELAY SYNCDELAY7

#ifdef DEBUG_FIRMWARE
#include <stdio.h>
#else
#define printf(...)
#endif

#include "hw6022.inc"
#include "set_voltage.inc"


// change to support as many interfaces as you need
BYTE altiface = 0; // alt interface
extern volatile WORD ledcounter;
extern volatile __bit GREEN;
WORD samplerate;
BYTE numchannels;


void set_aadj() {
	if (samplerate >= 24000 / numchannels) {
		EP2ISOINPKTS &= 0x7f;
	} else {
		// set AADJ bit if the iso transfer rate is less than 24MBps
		EP2ISOINPKTS |= 0x80;
	}
}

BOOL set_numchannels(BYTE n)
{
    if (n == 1 || n == 2) {
        BYTE fifocfg = 0x08 + (n - 1);  //AUTO_IN, WORD=numchannels-1
        EP2FIFOCFG = fifocfg;
        EP6FIFOCFG = fifocfg;
        numchannels = n;
        set_aadj();
        return TRUE;
    }
    return FALSE;
}

void clear_fifo()
{
    BYTE fifocfg = 0x08 + (numchannels - 1);  //AUTO_IN, WORD=numchannels-1
    GPIFABORT = 0xff;
    SYNCDELAY3;
    while (!(GPIFTRIG & 0x80)) {
        ;
    }
    // see section 9.3.13 in EZ-USB trm
    FIFORESET = 0x80;
    SYNCDELAY3;
    EP2FIFOCFG = 0;
    SYNCDELAY3;
    EP6FIFOCFG = 0;
    SYNCDELAY3;
    FIFORESET = 0x02;
    SYNCDELAY3;
    FIFORESET = 0x06;
    SYNCDELAY3;
    EP2FIFOCFG = fifocfg;
    SYNCDELAY3;
    EP6FIFOCFG = fifocfg;
    SYNCDELAY3;
    FIFORESET = 0;
}

void stop_sampling()
{
    GPIFABORT = 0xff;
    SYNCDELAY3;
    if (altiface == 0) {
        INPKTEND = 6;
    } else {
        INPKTEND = 2;
    }
}

void start_sampling()
{
    SET_ANALOG_MODE();
    clear_fifo();

    SYNCDELAY3;
    GPIFTCB1 = 0x28;
    SYNCDELAY3;
    GPIFTCB0 = 0;
    if (altiface == 0)
        GPIFTRIG = 6;
    else
        GPIFTRIG = 4;

    // set green led
    // don't clear led
    ledcounter = 1000;
    LED_RED = LED_OFF;
    LED_GREEN = LED_ON;
    GREEN = TRUE;
}

extern __code BYTE highspd_dscr;
extern __code BYTE fullspd_dscr;
void select_interface(BYTE alt)
{
    const BYTE *pPacketSize = (USBCS & bmHSM ? &highspd_dscr : &fullspd_dscr)
        + ( 9 + 9 + 4 + alt * (9 + 7) );
    altiface = alt;
    if (alt == 0) {
        // bulk on port 6
        EP2CFG = 0x00;
        EP6CFG = 0xe0;
        EP6GPIFFLGSEL = 1;

        EP6AUTOINLENL = pPacketSize[0];
        EP6AUTOINLENH = pPacketSize[1];
    } else {
        // iso on port 2
        EP2CFG = 0xd8;
        EP6CFG = 0x00;
        EP2GPIFFLGSEL = 1;

        EP2AUTOINLENL = pPacketSize[0];
        EP2AUTOINLENH = pPacketSize[1] & 0x7;
        EP2ISOINPKTS = (pPacketSize[1] >> 3) + 1;
        set_aadj();
    }
}


const struct samplerate_info {
    BYTE rateid;
    WORD rateksps;
    BYTE wait0;
    BYTE wait1;
    BYTE opc0;
    BYTE opc1;
    BYTE out0;
    BYTE ifcfg;
} samplerates[] = {
    { 48, 48000, 0x80,   0, 3, 0, 0x00, 0xea },
    { 30, 30000, 0x80,   0, 3, 0, 0x00, 0xaa },
    { 24, 24000,    1,   0, 2, 1, OUT0, 0xca },
    { 16, 16000,    1,   1, 2, 0, OUT0, 0xca },
    { 12, 12000,    2,   1, 2, 0, OUT0, 0xca },
    {  8,  8000,    3,   2, 2, 0, OUT0, 0xca },
    {  4,  4000,    6,   5, 2, 0, OUT0, 0xca },
    {  2,  2000,   12,  11, 2, 0, OUT0, 0xca },
    {  1,  1000,   24,  23, 2, 0, OUT0, 0xca },
    { 50,   500,   48,  47, 2, 0, OUT0, 0xca },
    { 20,   200,  120, 119, 2, 0, OUT0, 0xca },
    { 10,   100,  240, 239, 2, 0, OUT0, 0xca }
};

BOOL set_samplerate(BYTE rateid)
{
    BYTE i = 0;
    while (samplerates[i].rateid != rateid) {
        i++;
        if (i == sizeof(samplerates)/sizeof(samplerates[0]))
            return FALSE;
    }
    samplerate = samplerates[i].rateksps;
    set_aadj();

    IFCONFIG = samplerates[i].ifcfg;

    AUTOPTRSETUP = 7;
    AUTOPTRH2 = 0xE4;
    AUTOPTRL2 = 0x00;

    /* The program for low-speed, e.g. 1 MHz, is
     * wait 24, CTL2=0, FIFO
     * wait 23, CTL2=1
     * jump 0, CTL2=1
     *
     * The program for 24 MHz is
     * wait 1, CTL2=0, FIFO
     * jump 0, CTL2=1
     *
     * The program for 30/48 MHz is:
     * jump 0, CTL2=Z, FIFO, LOOP
     */

    EXTAUTODAT2 = samplerates[i].wait0;
    EXTAUTODAT2 = samplerates[i].wait1;
    EXTAUTODAT2 = 1;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;

    EXTAUTODAT2 = samplerates[i].opc0;
    EXTAUTODAT2 = samplerates[i].opc1;
    EXTAUTODAT2 = 1;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;

    EXTAUTODAT2 = samplerates[i].out0;
    EXTAUTODAT2 = OUT1;
    EXTAUTODAT2 = OUT1;
    EXTAUTODAT2 = 0x00;
    EXTAUTODAT2 = 0x00;
    EXTAUTODAT2 = 0x00;
    EXTAUTODAT2 = 0x00;
    EXTAUTODAT2 = 0x00;

    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;

    for (i = 0; i < 96; i++)
        EXTAUTODAT2 = 0;
    return TRUE;
}

BOOL handle_get_descriptor() {
  return FALSE;
}

//************************** Configuration Handlers *****************************

// set *alt_ifc to the current alt interface for ifc
BOOL handle_get_interface(BYTE ifc, BYTE* alt_ifc) {
    (void) ifc; // ignore unused parameter
    *alt_ifc=altiface;
    return TRUE;
}
// return TRUE if you set the interface requested
// NOTE this function should reconfigure and reset the endpoints
// according to the interface descriptors you provided.
BOOL handle_set_interface(BYTE ifc,BYTE alt_ifc) {  
    printf ( "Set Interface.\n" );
    if (ifc == 0) {
        select_interface(alt_ifc);
    }
    return TRUE;
}

// handle getting and setting the configuration
// 1 is the default.  We don't support multiple configurations.
BYTE handle_get_configuration() { 
    return 0;
}

BOOL handle_set_configuration(BYTE cfg) { 
    (void) cfg; // ignore unused parameter
    return TRUE;
}

// handle getting and setting EEPROM data
BOOL eeprom() {
    WORD addr=SETUP_VALUE(), len=SETUP_LENGTH();
    // wait for ep0 not busy
    while ( EP0CS & bmEPBUSY )
        ;
    switch ( SETUP_TYPE ) {
        case 0xc0:
            while ( len ) { // still have bytes to read
                BYTE cur_read = len > 64 ? 64 : len; // can't read more than 64 bytes at a time
                while ( EP0CS&bmEPBUSY ) // can't do this until EP0 is ready
                    ;
                eeprom_read( 0x51, addr, cur_read, EP0BUF );
                EP0BCH=0;
                SYNCDELAY;
                EP0BCL=cur_read;
                len -= cur_read;
                addr += cur_read;
            }
            break;
        case 0x40:
            while ( len ) {
                BYTE cur_write;
                // printf ( "Len More Bytes %d\n" , len );
                EP0BCL = 0; // allow pc transfer in
                while ( EP0CS & bmEPBUSY ) // wait
                    ;
                cur_write = EP0BCL;
                // printf ( "Writing %d Bytes to %d..\n", cur_write, addr );
                if ( !eeprom_write( 0x51, addr, cur_write, EP0BUF ) )
                    return FALSE;
                addr += cur_write;
                len -= cur_write;
            }
            break;
        default:
            return FALSE; // bad type
    }
    return TRUE;
}

//******************* VENDOR COMMAND HANDLERS **************************

BOOL handle_vendorcommand(BYTE cmd) {
    stop_sampling();
    // Set Red LED
    LED_RED = LED_ON;
    LED_GREEN = LED_OFF;
    GREEN = FALSE;
    ledcounter = 1000; // monoflop
    switch (cmd) {
    case 0xa2:
        return eeprom();
    case 0xe0:
    case 0xe1:
        EP0BCH=0;
        EP0BCL=0;
        while (EP0CS & bmEPBUSY)
            ;
        set_voltage(cmd - 0xe0, EP0BUF[0]);
        return TRUE;
    case 0xe2:
        EP0BCH=0;
        EP0BCL=0;
        while (EP0CS & bmEPBUSY)
            ;
        set_samplerate(EP0BUF[0]);
        return TRUE;
    case 0xe3:
        EP0BCH=0;
        EP0BCL=0;
        while (EP0CS & bmEPBUSY)
            ;
        if (EP0BUF[0] == 1)
            start_sampling();
        return TRUE;
    case 0xe4:
        EP0BCH=0;
        EP0BCL=0;
        while (EP0CS & bmEPBUSY)
            ;
        set_numchannels(EP0BUF[0]);
        return TRUE;
    }
    return FALSE; // not handled by handlers
}


//********************  INIT ***********************

void main_init() {
    EP4CFG = 0;
    EP8CFG = 0;

    SET_ANALOG_MODE();

    // in idle mode tristate all outputs
    GPIFIDLECTL = 0x00;
    GPIFCTLCFG = 0x80;
    GPIFWFSELECT = 0x00;
    GPIFREADYSTAT = 0x00;

    stop_sampling();
    set_voltage(0, 1);
    set_voltage(1, 1);
    set_samplerate(1);
    set_numchannels(2);
    select_interface(0);

    printf ( "Initialization Done.\n" );
}


void main_loop() {
}
