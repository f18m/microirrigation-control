/***********************************************************************************

Filename:	    lime2.c

Description:	    Puts the CC1110 in listen mode on SPI and radio.
                    The CC1110 is supposed to be connected to a MASTER SYSTEM via SPI
                    and to another CC1110 via radio.
                    As soon as the command 

Operation:    

***********************************************************************************/

/***********************************************************************************
* INCLUDES
*/
#include "bsp.h"
#include "mrfi.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "bsp_extended.h"
#include "main.h"

#include <string.h>                     // for memcmp()
#include <ioCC1110.h>
#include "hal_cc8051.h"
#include "ioCCxx10_bitdef.h"

#if LIME2

/***********************************************************************************
* CONSTANTS
*/
#define ALLOW_BUTTONS_TO_OVERRIDE_LIME2                    (0)
#define NUM_TX_RETRIES                                     (200)                      /* each retry takes about 1/4 sec => 10 sec max TX time */
#define HOLDOFF_TIME_AFTER_CMD_MSEC                        (5000)

// getting input commands via GPIO is now deprecated (SPI is used!):
#define ENABLE_INPUTS_VIA_GPIO                             (0)

#define SPI_COMMAND_MAX_LEN                                (16)
#define SPI_COMMAND_LEN                                    (9)

#define __bsp_CONFIG_AS_INPUT__(bit,port,ddr,low)          st( ddr &= ~BV(bit); )

static          char*         g_spiCommands[] =
{
     // keep all the same lenght == SPI_COMMAND_LEN
    "SENDON___",
    "SENDOFF__",
    "GETSTATUS"
};

typedef enum 
{
    SPI_CMD_SEND_ON = 0,
    SPI_CMD_SEND_OFF,
    SPI_CMD_GET_STATUS,
    SPI_CMD_MAX
} spiCommand_e;



/***********************************************************************************
* LOCAL VARIABLES
*/

static          uint8_t       g_noAckCount = 0;
static          mrfiPacket_t  g_pktTx;
static          uint8_t       g_lastRemoteBatteryRead = 0;
static          spiCommand_e  g_lastSpiCommand = SPI_CMD_MAX;

// SPI:
static          uint8_t       g_rxBufferSPISlave[SPI_COMMAND_MAX_LEN];
static          uint8_t       g_rxBufferLastIdx = 0;
static          uint8_t       g_txBufferSPISlave[SPI_COMMAND_MAX_LEN];
static          uint8_t       g_txBufferLastIdx = 0;


/***********************************************************************************
* LOCAL FUNCTIONS
*/
static int ShouldSendONCommand()
{
  if (g_lastSpiCommand == SPI_CMD_SEND_ON)
  {
    g_lastSpiCommand = SPI_CMD_MAX;
    return 1;
  }
#if ENABLE_INPUTS_VIA_GPIO
  /* If MASTER SYSTEM is asking to send the command, do it */
  int gpio_high = ((LIME2_GPIO2_PORT__) & BV(LIME2_GPIO2_BIT__));
  if (gpio_high)
    return 1;
#endif
  
#if ALLOW_BUTTONS_TO_OVERRIDE_LIME2
  /* If SLAVE button pushed, force transmission of ON command */
  if( BSP_BUTTON2() )
  {
    BSP_TURN_ON_LED1();
    BSP_TURN_ON_LED2();
    
    NWK_DELAY_NO_INTERRUPTS(3000);//BSP_SleepFor( POWER_MODE_2, SLEEP_1_MS_RESOLUTION, 5000);
    
    BSP_TURN_OFF_LED1();
    BSP_TURN_OFF_LED2();
    
    return 1;
  }
#endif    
  return 0;
}

static int ShouldSendOFFCommand()
{
  if (g_lastSpiCommand == SPI_CMD_SEND_OFF)
  {
    g_lastSpiCommand = SPI_CMD_MAX;
    return 1;
  }
#if ENABLE_INPUTS_VIA_GPIO
  /* If MASTER SYSTEM is asking to send the command, do it */
  int gpio_high = ((LIME2_GPIO1_PORT__) & BV(LIME2_GPIO1_BIT__));
  if (gpio_high)
    return 1;
#endif
  
#if ALLOW_BUTTONS_TO_OVERRIDE_LIME2
  /* If MASTER button pushed, force transmission of OFF command */
  if( BSP_BUTTON1() )
  {
    BSP_TURN_ON_LED1();
    BSP_TURN_ON_LED2();
    
    NWK_DELAY_NO_INTERRUPTS(3000);//BSP_SleepFor( POWER_MODE_2, SLEEP_1_MS_RESOLUTION, 5000);
    
    BSP_TURN_OFF_LED1();
    BSP_TURN_OFF_LED2();
    
    return 1;
  }
#endif    
  return 0;
}

static uint8_t IsValidRadioACK()
{
  uint8_t len = MRFI_GET_PAYLOAD_LEN(&g_pktRx);   
  uint8_t* radioMsg = MRFI_P_PAYLOAD(&g_pktRx);
  
  if (len == ACK_PKT_LEN &&
      radioMsg[0] == 'A' &&
      radioMsg[1] == 'C' &&
      radioMsg[2] == 'K')                    /* Acknowledge successfully received */
  {
    g_lastRemoteBatteryRead = radioMsg[3];      // last byte contains measurement: 80=FULL BATTERY (about 13V), 20=DEPLETED BATTERY (about 3.3V)
    g_noAckCount = 0;
    return 1;
  }
  
  return 0;
}

static void SendCommandOverRadioWithACK(int on_or_off)
{
  /* Build and send command */
  MRFI_SET_PAYLOAD_LEN(&g_pktTx, CMD_PKT_LEN);   
  uint8_t* cmdMsg = MRFI_P_PAYLOAD(&g_pktTx);
  cmdMsg[0] = 'C';
  cmdMsg[1] = 'M';
  cmdMsg[2] = 'D';
  cmdMsg[3] = on_or_off;
  
  // note that SetRxAddressFilter() is commented out in the MAIN (does not work for whatever reason) so the following
  // two lines are useless:
  CopyAddress(MRFI_P_SRC_ADDR(&g_pktTx), NODE_LIME2);
  CopyAddress(MRFI_P_DST_ADDR(&g_pktTx), NODE_REMOTE);
  
  BSP_TURN_ON_LED1();
  BSP_TURN_ON_LED2();
  uint8_t ackOk = 0;
  for( uint8_t x = 0; x < NUM_TX_RETRIES; x++ )
  {  
    MRFI_Transmit(&g_pktTx, MRFI_TX_TYPE_CCA);
        
    /* Turn on RX. default is RX Idle. */
    MRFI_RxOn();
    
    SPIN_ABOUT_QUARTER_A_SECOND;  /* Might have to be longer for bigger payloads */
    if( g_sRxCallbackSemaphore )    // Is ACK arrived? this flag is set by the RX callback in main.c
    {
      g_sRxCallbackSemaphore = 0;
      if (IsValidRadioACK())
      {
        ackOk=1;
        break;                // exit immediately
      }
      //else: TX the command another time!
    }
    
    // wait a very small amount of time before attempting TX again
    NWK_DELAY_NO_INTERRUPTS(50);
  }      
  BSP_TURN_OFF_LED1();
  BSP_TURN_OFF_LED2();
  
  /* Radio IDLE to save power */
  MRFI_RxIdle();
  
  if( ackOk )
  {
    // show we received the ACK back, turning on GREEN LED:
    BSP_TURN_ON_LED1();
    SPIN_ABOUT_QUARTER_A_SECOND;
    BSP_TURN_OFF_LED1();
  }
  else /* No ACK */ 
  {
    BSP_TURN_ON_LED2();
    SPIN_ABOUT_QUARTER_A_SECOND;
    BSP_TURN_OFF_LED2();
    g_noAckCount++;
  }
  
  /* No ACK at after NO_ACK_THRESHOLD. Wait until another command has to be sent. */ 
  if( 0 ) //g_noAckCount == NO_ACK_THRESHOLD )                              
  {
    g_noAckCount = 0;
    /* Radio IDLE to save power */
    MRFI_RxIdle();//SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXIDLE, 0);
    
    BSP_TURN_ON_LED1();
    BSP_TURN_ON_LED2();
    NWK_DELAY_NO_INTERRUPTS(3000);//BSP_SleepFor( POWER_MODE_2, SLEEP_1_MS_RESOLUTION, 3000);
    BSP_TURN_OFF_LED1();
    BSP_TURN_OFF_LED2();
    
    //BSP_SleepUntilButton( POWER_MODE_3, MASTER_BUTTON);
  }
}

static void PinConfigLime2_SPI_INPUT(void)
{ 
    /***************************************************************************
     * Setup I/O ports
     *
     * Port and pins used USART0 operating in SPI-mode are
     * MISO (MI): P0_2
     * MOSI (MO): P0_3
     * SSN (SS) : P0_4
     * SCK (C)  : P0_5
     *
     * These pins can be set to function as peripheral I/O so that they
     * can be used by USART0 SPI.
     */

    // Configure USART0 for Alternative 1 => Port P0 (PERCFG.U0CFG = 0)
    // To avoid potential I/O conflict with USART1:
    // configure USART1 for Alternative 2 => Port P1 (PERCFG.U1CFG = 1)
    PERCFG = (PERCFG & ~PERCFG_U0CFG) | PERCFG_U1CFG;

    // Give priority to USART 0 over USART 1 for port 0 pins (default)
    P2DIR = (P2DIR & ~P2DIR_PRIP0) | P2DIR_PRIP0_0;

    // Set function of relevant pins to peripheral I/O (see table 50)
    P0SEL |= (BIT5 | BIT4 | BIT3 | BIT2);

 
    /***************************************************************************
     * Configure SPI -- see datasheet Sec 12.14.2 SPI Mode
     */
#if 1                  // MRFI already does something similar without CLKCON_CLKSPD flag
    // Set system clock source to 26 Mhz XSOSC to support maximum transfer speed,
    // ref. [clk]=>[clk_xosc.c]
    SLEEP &= ~SLEEP_OSC_PD;
    while( !(SLEEP & SLEEP_XOSC_S) );
    CLKCON = (CLKCON & ~(CLKCON_CLKSPD | CLKCON_OSC)) | CLKSPD_DIV_1;
    while (CLKCON & CLKCON_OSC);
    SLEEP |= SLEEP_OSC_PD;
#endif
    // Set USART to SPI mode and ***Slave*** mode
    U0CSR = (U0CSR & ~U0CSR_MODE) | U0CSR_SLAVE;

    // Set:
    // - clock phase to be centered on first edge of SCK period
    // - negative clock polarity (SCK low when idle)
    // - bit order for transfers to MSB first
    U0GCR = U0GCR & ~(U0GCR_BAUD_E | U0GCR_CPOL | U0GCR_CPHA) | U0GCR_ORDER;
             
             
     /***************************************************************************
     * Setup interrupt
     */

    // Clear CPU interrupt flag for USART0 RX (TCON.URX0IF)
    URX0IF = 0;
    UTX0IF = 0;

    // Set individual interrupt enable bit in the peripherals SFR
    // Not any

    // Enable interrupt from USART0 RX by setting [IEN0.URX0IE=1]
    URX0IE = 1;
    // Enable interrupt from USART0 TX by setting [IEN0.UTX0IE=1]
    //UTX0IE = 1;

    // Enable global interrupts
    EA = 1;

    /***************************************************************************
     * NOTE: data receive
     *
     * An USART0 RX complete interrupt will be generated each time a byte is
     * received. See the interrupt routine (ut0rx_interrupt()).
     * At the same time the content of U0DBUF will be sent out...
     */
    
    // at boot, send out NULLs
    U0DBUF = 0;
}

static void PinConfigLime2_GPIO_INPUT()
{
  /* I/O-Port configuration :
   * configure our 2 input pins
   */

  __bsp_CONFIG_AS_INPUT__( LIME2_GPIO1_BIT__, LIME2_GPIO1_PORT__, LIME2_GPIO1_DDR__, 0 /* is active low */ );
  __bsp_CONFIG_AS_INPUT__( LIME2_GPIO2_BIT__, LIME2_GPIO2_PORT__, LIME2_GPIO2_DDR__, 0 /* is active low */ );
  
  // configure pull-down on the 2 input pins:
  P2INP |= (BIT5);
}

/***********************************************************************************
* @fn          sLime2Node
*
* @brief       Sends a packet and waits for ACK from slave.
*              Blinking green led indicates packet acknowledged
*              Blinking red led indicates packet not acknowledged
*              Adjust output power dynamically
*
* @param       none
*
* @return      none
*/
void sLime2Node(void)
{
  //uint8_t sCurrentPwrLevel = MAXIMUM_OUTPUT_POWER;
  //SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SETPWR, &sCurrentPwrLevel);

  // enable commands RX via SPI
  PinConfigLime2_SPI_INPUT();

#if ENABLE_INPUTS_VIA_GPIO
  PinConfigLime2_GPIO_INPUT();
#endif
  
  // now wait forever for commands from LIME2 to bridge over radio:
  unsigned int count1=0, count2=0;
  while (1)
  {
      if (ShouldSendONCommand())
      {
          SendCommandOverRadioWithACK(1);
          NWK_DELAY_NO_INTERRUPTS(HOLDOFF_TIME_AFTER_CMD_MSEC);       // after sending a command over radio we cannot handle any new command for a while
      }
      else if (ShouldSendOFFCommand())
      {
          SendCommandOverRadioWithACK(0);
          NWK_DELAY_NO_INTERRUPTS(HOLDOFF_TIME_AFTER_CMD_MSEC);        // after sending a command over radio we cannot handle any new command for a while
      }
      
      BSP_DELAY_USECS(1000);
      
      count1++;
      if ((count1 % 0xF00) == 0)
      {
          count2++;
          int reset_spi_garbage = ((count2 % 0x10) == 0);
          if (reset_spi_garbage)
          {
              int usart0_active = U0CSR & 0x1; // 1 means USART 0 busy in transmit or receive mode
              if (!usart0_active)
              {
                  // reset SPI index
                  g_rxBufferLastIdx=0;
                  g_txBufferLastIdx=0;
                  memset(g_txBufferSPISlave, 0, SPI_COMMAND_MAX_LEN);
                  U0DBUF=0;
              }
          }
      }
  }
}






/***********************************************************************************
* ISR FUNCTIONS
*/


/***********************************************************************************
* @fn          ut0rx_isr
*
* @brief       Interrupt routine which receives data from SPI master
*
* @param       none
*
* @return      0
*/

#pragma vector = URX0_VECTOR
__interrupt void ut0rx_isr(void)
{
    // Clear the CPU URX0IF interrupt flag
    URX0IF = 0;

    // Read received byte to buffer
    g_rxBufferSPISlave[g_rxBufferLastIdx++] = U0DBUF;
    
    // if buffer full avoid buffer overruns next time a byte is received!!
    if (g_rxBufferLastIdx == SPI_COMMAND_MAX_LEN)
    {
        // we received garbage... go back to the start... 
        g_rxBufferLastIdx = 0;
    }
    
    if (g_rxBufferLastIdx == SPI_COMMAND_LEN)
    {
        g_lastSpiCommand = SPI_CMD_MAX;
        for (unsigned int cmdi=0; cmdi < SPI_CMD_MAX; cmdi++)
        {
            if (memcmp(g_spiCommands[cmdi], g_rxBufferSPISlave, SPI_COMMAND_LEN) == 0)
            {
              g_lastSpiCommand = (spiCommand_e)cmdi;
              break;
            }
        }
        
        switch (g_lastSpiCommand)
        {
        case SPI_CMD_SEND_ON:
            BSP_TOGGLE_LED1();
            memcpy(g_txBufferSPISlave, "ACK", 3);
            g_txBufferLastIdx = 0;
            break;
        case SPI_CMD_SEND_OFF:
            BSP_TOGGLE_LED1();
            memcpy(g_txBufferSPISlave, "ACK", 3);
            g_txBufferLastIdx = 0;
            break;
        case SPI_CMD_GET_STATUS:
            BSP_TOGGLE_LED1();
            memcpy(g_txBufferSPISlave, "ACK", 3);
            g_txBufferLastIdx = 0;
            g_txBufferSPISlave[4]=g_lastRemoteBatteryRead;
            break;
          
        default:
            // default: garbage command... ignore and go back to start
            g_rxBufferLastIdx = 0;
            g_txBufferLastIdx = 0;
            ///BSP_TOGGLE_LED2();
            U0DBUF=0;
            break;
        }
    }
    
    if (g_lastSpiCommand != SPI_CMD_MAX)
    {
        // Write next byte to buffer
        U0DBUF = g_txBufferSPISlave[g_txBufferLastIdx++];
        if (g_txBufferLastIdx == SPI_COMMAND_MAX_LEN)
        {
            // start sending from the beginning...
            g_txBufferLastIdx = 0;
            memset(g_txBufferSPISlave, 0, SPI_COMMAND_MAX_LEN);
        }
    }
}

#pragma vector = UTX0_VECTOR
__interrupt void ut0tx_isr(void)
{
    // Clear the CPU UTX0IF interrupt flag
    //UTX0IF = 0;
    // FIXME: apparently this ISR is never called!
}

#endif  // LIME2




