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
*   BSP (Extended Board Support Package)
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

/* ------------------------------------------------------------------------------------------------
*                                            Includes
* ------------------------------------------------------------------------------------------------
*/
#include "bsp.h"
#include "mrfi.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "bsp_extended.h"


/* ------------------------------------------------------------------------------------------------
*                                            Local Variables
* ------------------------------------------------------------------------------------------------
*/
static          uint8_t sButtonPushed;
static const    uint8_t pm2Buf[7] = {0x06,0x06,0x06,0x06,0x06,0x06,0x04};
static const    uint8_t pm3Buf[7] = {0x07,0x07,0x07,0x07,0x07,0x07,0x04};
static __xdata  uint8_t dmaDesc[8] = {0x00,0x00,0xDF,0xBE,0x00,0x07,0x20,0x42};


/* ------------------------------------------------------------------------------------------------
*                                            Local Functions
* ------------------------------------------------------------------------------------------------
*/
void bsp_PowerMode(uint8_t mode);


/**************************************************************************************************
* @fn          BSP_ButtonIsr
*
* @brief       -
*
* @param       none
*
* @return      none
**************************************************************************************************
*/
/* Depending on board, the BSP_ISR_FUNCTION will use different port
   interrupt vectors. The prerocessor deterimines vector to be used*/

#ifdef BSP_BOARD_SRFCCXX10            /*Check which bsp is in use*/
  BSP_ISR_FUNCTION( BSP_ButtonIsr, P1INT_VECTOR)
#endif
#ifdef BSP_BOARD_SRF04EB              /*Check which bsp is in use*/
  BSP_ISR_FUNCTION( BSP_ButtonIsr, P0INT_VECTOR)
#endif 
{
  BSP_DISABLE_INTERRUPTS();
  
#ifdef BSP_BOARD_SRFCCXX10 
    //Using the SRFCCxx10 board 
  
    //check whitch button was pressed
    if(P1IFG==0x04) 
      sButtonPushed=1;
    if(P1IFG==0x08)
      sButtonPushed=2;
  
    P1IFG = 0;                      // Clear Port1 Local Interrupt Flags
    P1IF = 0;                       // Clear MCU Interrupt Flag for P1
    IEN2 &= ~0x10;                  // Disable Port 1 Interrupt
 
#endif

#ifdef BSP_BOARD_SRF04EB
    //Using the SRF04 board
     
    //check whitch button was pressed
    if(P0IFG==0x02) 
      sButtonPushed=1;
    if(P0IFG==0x20)
      sButtonPushed=2;
  
    P0IFG = 0;                      // Clear Port1 Local Interrupt Flags
    P0IF = 0;                       // Clear MCU Interrupt Flag for P1
    IEN1 &= ~0x20;                  // Disable Port 1 Interrupt
  
#endif
    
  SLEEP &= ~0x03;                 // Clear sleep mode bits
  
  BSP_ENABLE_INTERRUPTS();
}


/**************************************************************************************************
* @fn          BSP_SleepIsr
*
* @brief       -
*
* @param       none
*
* @return      none
**************************************************************************************************
*/
BSP_ISR_FUNCTION( BSP_SleepIsr, ST_VECTOR)
{
  BSP_DISABLE_INTERRUPTS();
  
  STIF = 0;                       // Clear MCU flag
  WORIRQ = 0x02;                  // Clear ST local flag and disable interupt
  STIE = 0;                       // Disable Sleep Timer interupt.
  
  SLEEP &= ~0x03;                 // Clear sleep mode bits
  BSP_ENABLE_INTERRUPTS();
}


/**************************************************************************************************
* @fn          BSP_Sleep
*
* @brief       Sets the desired power mode according to datasheet and errata note for CC251x & CC111x
*
* @param       mode : power mode, can be 0-3
*
* @return      none
**************************************************************************************************
*/
void bsp_PowerMode(uint8_t mode)
{
  
  if (mode == POWER_MODE_2 || mode == POWER_MODE_3)
  {
    // NOTE! At this point, make sure all interrupts that will not be used to
    // wake from PM are disabled as described in chapter 13.1.3 of the datasheet.
    
    // Store current DMA channel 0 descriptor and abort any ongoing transfers if the channel is in use
    uint8_t storedDescHigh = DMA0CFGH;
    uint8_t storedDescLow = DMA0CFGL;
    uint8_t storedDmaArm = DMAARM;
    
    DMAARM |= 0x81;
    
    // Update descriptor with correct source 
    if( mode == 2 )
    {  
      dmaDesc[0] = (uint8_t) ((uint16_t)&pm2Buf >> 8);
      dmaDesc[1] = (uint8_t) (uint16_t)&pm2Buf;
    }
    else
    {  
      dmaDesc[0] = (uint8_t) ((uint16_t)&pm3Buf >> 8);
      dmaDesc[1] = (uint8_t) (uint16_t)&pm3Buf;
    }
    
    // Associate the descriptor with DMA channel 0 and arm the DMA channel 
    DMA0CFGH = (uint16_t)&dmaDesc >> 8;
    DMA0CFGL = (uint16_t)&dmaDesc;
    DMAARM = 0x01;
    
    // Allign with positive 32 kHz clock edge as described in chapter 13.8.2
    // in the datasheet.
    uint8_t temp = WORTIME0;
    while( temp == WORTIME0);
    
    // Make sure XOSC is powered down when entering PM2/3 and that cache of flash is disabled.
    MEMCTR |= 0x02;
    SLEEP = 0x04 | mode;
    
    // Enter power mode as described in chapter 13.1.3 in the datasheet.
    asm("NOP");
    asm("NOP");
    asm("NOP");
    if( SLEEP & 0x03 ) {
      asm("MOV 0xD7,#0x01");      // DMAREQ = 0x01;
      asm("NOP");                 // Needed to perfectly align the DMA transfer 
      asm("ORL 0x87,#0x01");      // PCON |= 0x01; 
      asm("NOP");
    }
    
    MEMCTR &= ~0x02;
    
    // Update with original DMA descriptor and arm.    
    DMA0CFGH = storedDescHigh;
    DMA0CFGL = storedDescLow;
    DMAARM = storedDmaArm;
  }
  else if ( mode == POWER_MODE_1 )
  {
    uint8_t temp = WORTIME0;
    while( temp == WORTIME0);
    
    SLEEP |= mode;
    asm("NOP");
    asm("NOP");
    asm("NOP");
    if ( SLEEP & 0x03 ) 
    {
      PCON |= 0x01;
      asm("NOP");
    }
  }
  else if ( mode == POWER_MODE_0 )
  {           
    PCON |= 0x01;
    asm("NOP");
  }
  else 
  {
    /* Not valid */    
  }
}


/**************************************************************************************************
* @fn          BSP_SleepFor
*
* @brief       Enter sleep mode, Set up DMA workaround according to errata note for CC251x & CC111x
*              Will wake up on Sleep Timer interrupt
*
* @param       mode   : Power mode, can be 0-3
*              res    : Set the sleep timer resolution (0 to 4)
*              steps  : Set number of steps for the sleep timer. Sleep time = res * steps (0 to 655535)
*
* @return      none
**************************************************************************************************
*/
void BSP_SleepFor(uint8_t mode, uint8_t res, uint16_t steps)
{
  //  Set radio to sleep
  
    /* go to sleep mode. */
    MRFI_RxIdle();
    MRFI_Sleep();// SMPL_Ioctl(IOCTL_OBJ_RADIO,IOCTL_ACT_RADIO_SLEEP,0);
  
  IEN2 &= ~0x01;                          // Disable RF interrupt
  BSP_DISABLE_INTERRUPTS();
  
  BSP_SET_MAIN_CLOCK_RC();                // Set main clock source to RC oscillator
  
  // Update Sleep timer
  WORCTL = 0x04 + (res&0x03);             // Reset timer and set resolution, mask out the 2 LSB's.
  uint8_t temp = WORTIME0;                // Wait one full 32kHz cycle to allow sleep timer to reset
  while( temp == WORTIME0);
  temp = WORTIME0;
  while( temp == WORTIME0);
  
  WOREVT1 = (steps>>8);                   // Set timer high byte
  WOREVT0 = (uint8_t)steps;               // Set timer Low byte
  
  STIF = 0;                               // Clear Sleep Timer flag in IRCON
  WORIRQ = 0x10;                          // 0x12 Enable Interupts and clear module flag
  STIE = 1;                               // Enable Sleep Timer interrupt.
  
  BSP_ENABLE_INTERRUPTS();
  
  bsp_PowerMode(mode);
  
  BSP_SET_MAIN_CLOCK_XOSC();              // Set main clock source to External oscillator
  
  // Wake up the radio and set it in Idle 
    MRFI_WakeUp();

#if !defined( END_DEVICE )
    MRFI_RxOn();
#endif
  //SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE,0);
  IEN2 |= 0x01;                           // Enable RF interrupt
}


/**************************************************************************************************
* @fn          BSP_SleepUntilButton
*
* @brief       Enter sleep mode, Set up DMA workaround according to errata note for CC251x & CC111x
*              Will wake up on Button pushes
*
* @param       mode   : Power mode, can be 0-3
*              button : Button to wake up on, can be 1 (master), 2 (slave) or 3 (both)
*
* @return      none
**************************************************************************************************
*/
uint8_t BSP_SleepUntilButton(uint8_t mode, uint8_t button)
{
  
  //  Set radio to sleep
  
    /* go to sleep mode. */
    MRFI_RxIdle();
    MRFI_Sleep();//SMPL_Ioctl(IOCTL_OBJ_RADIO,IOCTL_ACT_RADIO_SLEEP,0);  

  IEN2 &= ~0x01;                          // Disable RF interrupt
  BSP_ENABLE_INTERRUPTS();
  
  BSP_SET_MAIN_CLOCK_RC();                // Set main clock source to RC oscillator
 
#ifdef BSP_BOARD_SRFCCXX10 
    //Using SRFCCxx10 board
  
    /* Clear Port 1 Interrupt flags */
    P1IFG = 0x00;
    P1IF = 0;   
  
    P1IEN &= ~0x0C;
    P1IEN |= (button<<2);                   // Enable Port interrupts
  
    /* Enable CPU Interrupt for Port 1 (IEN2.P1IE = 1) */
    IEN2 |= 0x10;
#endif
    
#ifdef BSP_BOARD_SRF04EB
     //Using SFR04 board
    
    /* Clear Port 0 Interrupt flags */ 
    P0IFG = 0x00;
    P0IF = 0;
        
    PICTL &= ~0x18;
    PICTL |= (button<<3);                   // Enable Port interrupts
    
    /* Enable CPU Interrupt for Port 0 (IEN1.P0IE = 1) */
    IEN1 |= 0x20;
#endif
  
  BSP_ENABLE_INTERRUPTS();
  
  bsp_PowerMode(mode);
  
  BSP_SET_MAIN_CLOCK_XOSC();              // Set main clock source to External oscillator
  
  // Wake up the radio and set it in Idle 
      MRFI_WakeUp();

#if !defined( END_DEVICE )
    MRFI_RxOn();
#endif
    //SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE,0);
  IEN2 |= 0x01;                           // Enable RF interrupt
  
  return sButtonPushed;
}


/**************************************************************************************************
* @fn          BSP_createRandomAddress
*
* @brief       Creates a random address for this device
*
* @param       addr  - pointer to the address object to be used to set my address.
*
* @return      none
**************************************************************************************************
*/
void BSP_createRandomAddress(addr_t* addr)
{
  /* Seed for the Random Number Generator */
  {
    uint16_t rndSeed = 0x0;
    volatile static uint8_t dummyRead;
    
    /* Sample temerature twice and use lower noisy bits as seed */
    ADCCON3 = 0x0E;         /* Sample temperature */
    while(!(ADCCON1 & 0x80));
    rndSeed |= ((uint16_t) ADCL) << 8;
    dummyRead = ADCH;
    
    ADCCON3 = 0x0E;         /* Sample temperature */
    while(!(ADCCON1 & 0x80));
    rndSeed |= ADCL;
    dummyRead = ADCH;
    
    /* Need to write to RNDL twice to seed it */
    RNDL = rndSeed & 0xFF;
    RNDL = rndSeed >> 8;
  }
  
  addr_t *a;
  a = (addr_t *) addr;
  
  for (uint8_t x=0; x < NET_ADDR_SIZE; x++)
    a->addr[x] = MRFI_RandomByte();
}


/**************************************************************************************************
*/
