/***********************************************************************************

Filename:	    lime2.c

Description:	    Code for the "lime2" node.
                    Puts the CC1110 in listen mode on SPI and radio.
                    The CC1110 is supposed to be connected to a MASTER SYSTEM via SPI
                    and to another CC1110 via radio.

                    As soon as a command is received from the MASTER SYSTEM via SPI
                    that command is sent over radio and the system listens for an ACK
                    (over radio). The MASTER SYSTEM can query the ACK status by issuing
                    a "STATUS" command over SPI.

                    Each command over SPI can have a "transaction ID", that is a number in
                    range [0-255] that will identify that command. The ACK will provide back
                    the transaction ID so that it's possible to associate each command with its
                    ACK.

                    The "lime2" node is supposed to have no power constraints (no battery)
                    and thus implements no special lower power policy.

Notes:              Because of the way SPI operates it's important that the MASTER SYSTEM
                    sends the "STATUS" command at least twice; the first reply to the STATUS
                    command actually contains the reply for an older command.
                    Additionally, when the lime2 node is busy doing radio operations each
                    "STATUS" command will return a NULL reply (all zeros back over SPI).
                    The MASTER SYSTEM must keep polling the lime2 node with STATUS commands
                    until an "ACK" is returned (for about DURATION_TX_RETRIES_MSEC time);
                    if the command was delivered successfully it
                    will contain the "transaction ID" of the last non-STATUS command sent.
                    STATUS commands must be sent with the special transaction ID zero.

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

#define HOLDOFF_TIME_AFTER_CMD_MSEC                        (5000)
#define DURATION_TX_RETRIES_MSEC                           (10000)              // must be bigger than the time the remote node sleeps (WAIT_TIME_RADIOOFF_MSEC)
#define DELAY_AFTER_EACH_TX_MSEC                           (250)
#define NUM_TX_RETRIES                                     (DURATION_TX_RETRIES_MSEC/DELAY_AFTER_EACH_TX_MSEC)

// getting input commands via GPIO is now deprecated (SPI is used!):
#define ENABLE_INPUTS_VIA_GPIO                             (0)

#define __bsp_CONFIG_AS_INPUT__(bit,port,ddr,low)          st( ddr &= ~BV(bit); )

#define BSP_TURN_ON_LED_SPI                                BSP_TURN_ON_LED1
#define BSP_TURN_OFF_LED_SPI                               BSP_TURN_OFF_LED1
#define BSP_TOGGLE_LED_SPI                                 BSP_TOGGLE_LED1

#define BSP_TURN_ON_LED_RADIO                              BSP_TURN_ON_LED2
#define BSP_TURN_OFF_LED_RADIO                             BSP_TURN_OFF_LED2
#define BSP_TOGGLE_LED_RADIO                               BSP_TOGGLE_LED2


/***********************************************************************************
* LOCAL VARIABLES
*/

// SPI and radio SLAVE->MASTER variables
static          uint8_t       g_noAckCount = 0;
static          uint8_t       g_lastRemoteBatteryRead = 0;
static          uint8_t       g_lastRemoteAckTransactionID = 0;

// SPI and radio MASTER->SLAVE variables
static          command_e     g_lastSpiCommand = CMD_MAX;
static          uint8_t       g_lastSpiCommandTransactionID = 0;
static          uint8_t       g_lastSpiCommandParameter = 0;
static          mrfiPacket_t  g_pktTx;

// SPI:
static          uint8_t       g_rxBufferSPISlave[SPI_COMMAND_MAX_LEN];
static          uint8_t       g_rxBufferLastIdx = 0;

// for tx we adopt a double-buffered tecnique:
static          uint8_t       g_txBufferSPISlaveACTIVE[SPI_COMMAND_MAX_LEN];
static          uint8_t       g_txBufferSPISlaveNEXT[SPI_COMMAND_MAX_LEN];
static          uint8_t       g_txBufferLastIdx = 0;


/***********************************************************************************
* LOCAL FUNCTIONS
*/

static uint8_t IsValidRadioACK()
{
    uint8_t len = MRFI_GET_PAYLOAD_LEN(&g_pktRx);
    uint8_t* radioMsg = MRFI_P_PAYLOAD(&g_pktRx);

    if (len == REPLY_LEN+REPLY_POSTFIX_LEN &&
      memcmp(radioMsg, g_ack, REPLY_LEN)==0)                    /* Acknowledge successfully received */
    {
        g_lastRemoteAckTransactionID = radioMsg[REPLY_LEN+0];      // last byte contains transaction ID
        g_lastRemoteBatteryRead = radioMsg[REPLY_LEN+1];      // last byte contains measurement: 80=FULL BATTERY (about 13V), 20=DEPLETED BATTERY (about 3.3V)
        g_noAckCount = 0;
        return 1;
    }

    return 0;
}

static void SendCommandOverRadioWithACK()
{
    /* Build and send command */
    MRFI_SET_PAYLOAD_LEN(&g_pktTx, COMMAND_LEN+COMMAND_POSTFIX_LEN);
    uint8_t* cmdMsg = MRFI_P_PAYLOAD(&g_pktTx);
    memcpy(cmdMsg, g_commands[g_lastSpiCommand], COMMAND_LEN);
    cmdMsg[COMMAND_LEN+0] = g_lastSpiCommandTransactionID;
    cmdMsg[COMMAND_LEN+1] = g_lastSpiCommandParameter;

    // note that SetRxAddressFilter() is commented out in the MAIN (does not work for whatever reason) so the following
    // two lines are useless:
    //CopyAddress(MRFI_P_SRC_ADDR(&g_pktTx), NODE_LIME2);
    //CopyAddress(MRFI_P_DST_ADDR(&g_pktTx), NODE_REMOTE);

    uint8_t ackOk = 0;
    for( uint8_t x = 0; x < NUM_TX_RETRIES; x++ )
    {
        // show we are transmitting blinking radio LED
        BSP_TOGGLE_LED_RADIO();

        // tx!
        MRFI_Transmit(&g_pktTx, MRFI_TX_TYPE_CCA);

        /* Turn on RX. default is RX Idle. */
        MRFI_RxOn();

        DelayMsWithInterrupts(DELAY_AFTER_EACH_TX_MSEC);  /* Might have to be longer for bigger payloads */
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
    }

    /* Radio IDLE to save power */
    MRFI_RxIdle();

    if( ackOk )
    {
        // show we received the ACK back, turning on radio LED:
        BSP_TURN_ON_LED_RADIO();
        DELAY_ABOUT_QUARTER_A_SECOND_WITH_INTERRUPTS;
        BSP_TURN_OFF_LED_RADIO();
    }
    else /* No ACK */
    {
        //BSP_TURN_ON_LED_RADIO();
        //DELAY_ABOUT_QUARTER_A_SECOND_WITH_INTERRUPTS;
        //BSP_TURN_OFF_LED_RADIO();
        g_noAckCount++;
    }

    /* No ACK at after NO_ACK_THRESHOLD. Wait until another command has to be sent. */
    if( 0 ) //g_noAckCount == NO_ACK_THRESHOLD )
    {
        g_noAckCount = 0;
        /* Radio IDLE to save power */
        MRFI_RxIdle();//SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXIDLE, 0);

        BSP_TURN_ON_LED_RADIO();
        DelayMsNOInterrupts(3000);//BSP_SleepFor( POWER_MODE_2, SLEEP_1_MS_RESOLUTION, 3000);
        BSP_TURN_OFF_LED_RADIO();

        //BSP_SleepUntilButton( POWER_MODE_3, MASTER_BUTTON);
    }
}

#if ENABLE_INPUTS_VIA_GPIO
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
#endif

static void ResetSPIRx()
{
    // reset SPI index
    g_rxBufferLastIdx=0;
    memset(g_rxBufferSPISlave, 0, SPI_COMMAND_MAX_LEN);
}
static void ResetSPITx()
{
    // reset SPI index
    g_txBufferLastIdx=0;
    memset(g_txBufferSPISlaveACTIVE, 0, SPI_COMMAND_MAX_LEN);

    // reset also "next" buffer so that until a valid command will be received
    // we will continue sending zeros on the SPI:
    memset(g_txBufferSPISlaveNEXT, 0, SPI_COMMAND_MAX_LEN);

    // avoid sending out a first character on the SPI due to old contents:
    U0DBUF=0;
}

static void SPITxCopyNEXTinACTIVE()             // call this only when no SPI TX is ongoing!!!!
{
    memcpy(g_txBufferSPISlaveACTIVE, g_txBufferSPISlaveNEXT, SPI_COMMAND_MAX_LEN);
    g_txBufferLastIdx = 0;
    // avoid sending out a first character on the SPI due to old contents:
    U0DBUF=0;
}

static void PrepareSPIAck()
{
    // prepare the reply in the "next" buffer to avoid corrupting an already-ongoing TX
    // using the "active" buffer
    memcpy(g_txBufferSPISlaveNEXT, g_ack, REPLY_LEN);
    g_txBufferSPISlaveNEXT[REPLY_LEN+0]=g_lastRemoteAckTransactionID;
    g_txBufferSPISlaveNEXT[REPLY_LEN+1]=g_lastRemoteBatteryRead;
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

    // before enabling any interrupt, make sure SPI variables are ready:
    ResetSPIRx();
    PrepareSPIAck();
    SPITxCopyNEXTinACTIVE();

    // Enable interrupt from USART0 RX by setting [IEN0.URX0IE=1]
    URX0IE = 1;
    // Enable interrupt from USART0 TX by setting [IEN2.UTX0IE=1]
    //UTX0IE = 1;
    IEN2 |= 0x4;


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

static void HandleSPI()
{
    int usart0_active = U0CSR & 0x1; // first bit at 1 means USART 0 busy in transmit or receive mode
    if (usart0_active)
        return;

    if (g_rxBufferLastIdx == 0)
        return;           // nothing received!

    
    // the idea is that we will enter this function from time to time.
    // SPI communication is pretty fast so by the time this function checks the
    // SPI RX buffer, we should find all bytes there. For this reason we reject
    // what we received if its length is not correct!    
    if (g_rxBufferLastIdx != COMMAND_LEN + COMMAND_POSTFIX_LEN /* transaction ID byte */)
    {
        // default: garbage command... do not provide a valid ACK on SPI:
        ResetSPITx();

        // get ready for next command:
        ResetSPIRx();
        return;
    }


    // Received a command!!

    g_lastSpiCommand = String2Command(g_rxBufferSPISlave, COMMAND_LEN + COMMAND_POSTFIX_LEN);

    switch (g_lastSpiCommand)
    {
    case CMD_TURN_ON:
    case CMD_TURN_OFF:
    case CMD_NO_OP:
        // Read the transaction ID
        g_lastSpiCommandTransactionID = g_rxBufferSPISlave[COMMAND_LEN+0];
        g_lastSpiCommandParameter = g_rxBufferSPISlave[COMMAND_LEN+1];
                // fallthrough!

        // IMPORTANT: the transaction ID / cmdParameter given in the STATUS command is ignored:
        //            the STATUS command is used to retrieve ACK of other commands!
    case CMD_GET_STATUS:
        // signal we received a valid command:
        BSP_TOGGLE_LED_SPI();

        // create the ACK over SPI:
        PrepareSPIAck();
        SPITxCopyNEXTinACTIVE();            // because of initial check on usart0_active we are sure no TX is ongoing!
        break;

    default:
        // default: garbage command... do not provide a valid ACK on SPI:
        ResetSPITx();
        break;
    }

    // get ready for next command:
    ResetSPIRx();
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
    // enable commands RX via SPI
    PinConfigLime2_SPI_INPUT();
#if ENABLE_INPUTS_VIA_GPIO
    PinConfigLime2_GPIO_INPUT();
#endif

    // now wait forever for commands from LIME2 Linux SPI master
    // that we will bridge over radio toward the "remote" node:
    unsigned int count1=0, count2=0;
    while (1)
    {
        if (g_lastSpiCommand == CMD_TURN_ON || g_lastSpiCommand == CMD_TURN_OFF || g_lastSpiCommand == CMD_NO_OP)
        {
            SendCommandOverRadioWithACK();        // this can take up to DURATION_TX_RETRIES_MSEC!
            g_lastSpiCommand = CMD_MAX;
            //DelayMsNOInterrupts(HOLDOFF_TIME_AFTER_CMD_MSEC);       // after sending a command over radio we cannot handle any new command for a while
        }

        count1++;
        if ((count1 % 16) == 0)
        {
            count2++;
            int handle_spi = ((count2 % 16) == 0);
            if (handle_spi)
            {
                // with current settings for count1 and count2, this part is
                // executed every ??? secs

                HandleSPI();            // this is very fast, no delay
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
        ResetSPIRx();
    }
}

#pragma vector = UTX0_VECTOR
__interrupt void ut0tx_isr(void)
{
    // Clear the CPU UTX0IF interrupt flag
    UTX0IF = 0;

    // Write next byte to buffer
    if (g_txBufferLastIdx == SPI_COMMAND_MAX_LEN)//REPLY_LEN+REPLY_POSTFIX_LEN)
    {
        // finished TX buffer; just send out zeros.
        // If this point is reached it means that the main microcontroller loop is
        // busy with the radio and HandleSPI() function is not getting called but
        // the SPI master keeps sending us commands.
        U0DBUF = 0;
    }
    else
    {
        U0DBUF = g_txBufferSPISlaveACTIVE[g_txBufferLastIdx++];
    }
}

#endif  // LIME2




