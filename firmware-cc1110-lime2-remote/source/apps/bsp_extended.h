/**************************************************************************************************
  Revised:        
  Revision:       

  Copyright 2007 Texas Instruments Incorporated.  All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights granted under
  the terms of a software license agreement between the user who downloaded the software,
  his/her employer (which must be your employer) and Texas Instruments Incorporated (the
  "License"). You may not use this Software unless you agree to abide by the terms of the
  License. The License limits your use, and you acknowledge, that the Software may not be
  modified, copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio frequency
  transceiver, which is integrated into your product. Other than for the foregoing purpose,
  you may not use, reproduce, copy, prepare derivative works of, modify, distribute,
  perform, display or sell this Software and/or its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE PROVIDED “AS IS”
  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY
  WARRANTY OF MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
  IN NO EVENT SHALL TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER LEGAL EQUITABLE
  THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES INCLUDING BUT NOT LIMITED TO ANY
  INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST
  DATA, COST OF PROCUREMENT OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY
  THIRD PARTIES (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com. 
**************************************************************************************************/

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 *   BSP (Board Support Package)
 *   Target : Texas Instruments Chipcon SRFCCxx10
 *            "CC2510/CC1110 Mini Development Kit"
 *   Extended definition file.
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 */


#ifndef BSP_EXTENDED_H
#define BSP_EXTENDED_H

/* ------------------------------------------------------------------------------------------------
 *                                          Includes
 * ------------------------------------------------------------------------------------------------
 */
#include "bsp.h"
#include "bsp_macros.h"

/* ------------------------------------------------------------------------------------------------
 *                                           Defines
 * ------------------------------------------------------------------------------------------------
 */
#define BSP_SET_MAIN_CLOCK_RC()       st(SLEEP &= ~0x04;            /* turn on both oscs */     \
                                      while(!(SLEEP & 0x20));       /* wait for RC osc */       \
                                      CLKCON = (0x49 | BV(7));      /* select RC osc */         \
                                      /* wait for requested settings to take effect */          \
                                      while (CLKCON != (0x49 | BV(7)));                         \
                                      SLEEP |= 0x04;)               /* turn off XOSC */

#define BSP_SET_MAIN_CLOCK_XOSC()     st(SLEEP &= ~0x04;            /* turn on both oscs */     \
                                      while(!(SLEEP & 0x40));       /* wait for XOSC osc */     \
                                      CLKCON = (0x09 | BV(7));      /* select XOSC osc */       \
                                      /* wait for requested settings to take effect */          \
                                      while (CLKCON != (0x09 | BV(7)));                         \
                                      SLEEP |= 0x04;)               /* turn off RC */

#define POWER_MODE_0                  0
#define POWER_MODE_1                  1
#define POWER_MODE_2                  2
#define POWER_MODE_3                  3

/* ------------------------------------------------------------------------------------------------
 *                                        Prototypes
 * ------------------------------------------------------------------------------------------------
 */
uint8_t BSP_SleepUntilButton(uint8_t mode, uint8_t button);
void BSP_SleepFor(uint8_t mode, uint8_t res, uint16_t steps);


#define NET_ADDR_SIZE      MRFI_ADDR_SIZE   /* size of address in bytes */

#ifdef  FREQUENCY_AGILITY
#define NWK_FREQ_TBL_SIZE  MRFI_NUM_LOGICAL_CHANS
#else
#define NWK_FREQ_TBL_SIZE  1
#endif

typedef struct
{
  uint8_t  addr[NET_ADDR_SIZE];
} addr_t;

void BSP_createRandomAddress(addr_t * );

/**************************************************************************************************
 */
#endif
