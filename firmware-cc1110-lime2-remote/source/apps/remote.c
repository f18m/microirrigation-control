/***********************************************************************************

Filename:	    remote.c

Description:        Code for the "remote" node.
                    Puts the CC1110 in listen mode on SPI and radio.
                    The CC1110 is supposed to be connected to a MASTER SYSTEM via radio
                    and to an actuator (group of relays).
                    As soon as a radio command is received, the remote node will send
                    back an ACK over radio with the "transaction ID" received in the
                    over-radio comamnd. Then the actuator system is
                    turned on/off (depending on the command).
                    The remote node is supposed to be battery-powered and thus implements
                    a low power policy.
***********************************************************************************/

/***********************************************************************************
* INCLUDES
*/
#include <ioCC1110.h>
#include <string.h>

#include "bsp.h"
#include "mrfi.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "bsp_extended.h"
#include "main.h"

#include "ioCCxx10_bitdef.h"
#include "hal_cc8051.h"

#if REMOTE


/***********************************************************************************
* CONSTANTS
*/

#define ENABLE_LOWPOWER_MODE                               (0)

#define ACTUATOR_IMPULSE_DURATION_MSEC                     (3000)
#define WAIT_TIME_RADIOOFF_MSEC                            (6000)

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   GPIO #1 ---> attached to VALVE_CTRL1a
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Polarity  :  Active High
 *   GPIO      :  P0.3
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define REMOTE_GPIO1_PORT__          P0
#define REMOTE_GPIO1_BIT__           3
#define REMOTE_GPIO1_DDR__           P0DIR

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   GPIO #2 ---> attached to VALVE_CTRL1b
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Polarity  :  Active High
 *   GPIO      :  P0.2
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define REMOTE_GPIO2_PORT__          P0
#define REMOTE_GPIO2_BIT__           2
#define REMOTE_GPIO2_DDR__           P0DIR

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   GPIO #3 ---> attached to VALVE_CTRL2a
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Polarity  :  Active High
 *   GPIO      :  P0.4
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define REMOTE_GPIO3_PORT__          P0
#define REMOTE_GPIO3_BIT__           4
#define REMOTE_GPIO3_DDR__           P0DIR

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   GPIO #4 ---> attached to VALVE_CTRL2b
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Polarity  :  Active High
 *   GPIO      :  P0.5
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define REMOTE_GPIO4_PORT__          P0
#define REMOTE_GPIO4_BIT__           1
#define REMOTE_GPIO4_DDR__           P0DIR

   
// global active low/high switch
#define REMOTE_GPIO_ACTIVE_LOW       0



/***********************************************************************************
* MACROS
*/


#define TURN_OUTPUT_PORT_ON(bit,port,ddr,low)  \
  st( if (low) { port &= ~BV(bit); } else { port |= BV(bit); } )

#define TURN_OUTPUT_PORT_OFF(bit,port,ddr,low)  \
  st( if (low) { port |= BV(bit); } else { port &= ~BV(bit); } )



/***********************************************************************************
* LOCAL VARIABLES
*/

static          mrfiPacket_t  g_pktTx;
static          command_e     g_lastCmdRx = CMD_MAX;
static          uint8_t       g_lastTransactionIDRX = 0;
static          uint8_t       g_lastTransactionIDApplied = 0;
static          uint8_t       g_lastCmdParameter = 0;
static          uint16_t      g_last_adc_result = 0;


/***********************************************************************************
* LOCAL FUNCTIONS
*/

static uint8_t CheckCmdAndReplyWithAck()                // will leave the radio back in RX mode
{
    /* Put Radio in IDLE to save power */
    MRFI_RxIdle();
    g_sRxCallbackSemaphore = 0;                 // reset semaphore

    // g_pktRx contains the received packet:

    uint8_t len = MRFI_GET_PAYLOAD_LEN(&g_pktRx);
    uint8_t* radioMsg = MRFI_P_PAYLOAD(&g_pktRx);

    g_lastCmdRx = String2Command(radioMsg, len);
    if (g_lastCmdRx == CMD_MAX)
    {
        MRFI_RxOn();
        return 0;                 // invalid command received!
    }

    // retrieve the transaction ID
    g_lastTransactionIDRX = radioMsg[COMMAND_LEN+0];            // this should be ASCII encoded
    g_lastCmdParameter = radioMsg[COMMAND_LEN+1];            // this should be ASCII encoded

    // Build and immediately send the acknowledge for this transaction
    // otherwise the "lime2" node will keep sending us the same command
    // over and over...
    MRFI_SET_PAYLOAD_LEN(&g_pktTx, REPLY_LEN+REPLY_POSTFIX_LEN);
#if 1
    uint8_t* ackMsg = MRFI_P_PAYLOAD(&g_pktTx);

    memcpy(ackMsg, g_ack, REPLY_LEN);
    ackMsg[REPLY_LEN+0] = g_lastTransactionIDRX;
    ackMsg[REPLY_LEN+1] = (uint8_t)g_last_adc_result;

    //CopyAddress(MRFI_P_SRC_ADDR(&g_pktTx), NODE_REMOTE);
    //CopyAddress(MRFI_P_DST_ADDR(&g_pktTx), NODE_LIME2);
#endif
    MRFI_Transmit(&g_pktTx, MRFI_TX_TYPE_CCA);

    // signal visually we are transmitting the ACK
    BSP_TURN_ON_LED1();
    //DELAY_ABOUT_QUARTER_A_SECOND_WITH_INTERRUPTS;
    DelayMsNOInterrupts(250);
    BSP_TURN_OFF_LED1();

    MRFI_RxOn();
    return 1;    // we received something!
}

static void ApplyCmdRx()
{
    // did we receive a new command or this is just an over-radio copy of the previous one?
    if (g_lastTransactionIDRX == g_lastTransactionIDApplied)
      return;           // repeated command... ignore


    // activate output relay:

    if (g_lastCmdRx == CMD_TURN_ON)
    {
        if (g_lastCmdParameter == '1')
        {
          TURN_OUTPUT_PORT_ON(  REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, REMOTE_GPIO_ACTIVE_LOW );
          TURN_OUTPUT_PORT_OFF( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, REMOTE_GPIO_ACTIVE_LOW );
        }
        else if (g_lastCmdParameter == '2')
        {
          TURN_OUTPUT_PORT_ON(  REMOTE_GPIO3_BIT__, REMOTE_GPIO3_PORT__, REMOTE_GPIO3_DDR__, REMOTE_GPIO_ACTIVE_LOW );
          TURN_OUTPUT_PORT_OFF( REMOTE_GPIO4_BIT__, REMOTE_GPIO4_PORT__, REMOTE_GPIO4_DDR__, REMOTE_GPIO_ACTIVE_LOW );
        }
    }
    else if (g_lastCmdRx == CMD_TURN_OFF)
    {
        if (g_lastCmdParameter == '1')
        {
          TURN_OUTPUT_PORT_OFF( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, REMOTE_GPIO_ACTIVE_LOW );
          TURN_OUTPUT_PORT_ON(  REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, REMOTE_GPIO_ACTIVE_LOW );
        }
        else if (g_lastCmdParameter == '2')
        {
          TURN_OUTPUT_PORT_OFF( REMOTE_GPIO3_BIT__, REMOTE_GPIO3_PORT__, REMOTE_GPIO3_DDR__, REMOTE_GPIO_ACTIVE_LOW );
          TURN_OUTPUT_PORT_ON(  REMOTE_GPIO4_BIT__, REMOTE_GPIO4_PORT__, REMOTE_GPIO4_DDR__, REMOTE_GPIO_ACTIVE_LOW );
        }
    }
    else
        return;


    // the duration of the pulse needs to be tuned for your specific application.
    // in my case the electrovalve I had took quite a lot of time to stabilize to the new
    // position!

    DelayMsNOInterrupts(ACTUATOR_IMPULSE_DURATION_MSEC);


    // then turn off relay:

    if (g_lastCmdParameter == '1')
    {
      TURN_OUTPUT_PORT_OFF( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, REMOTE_GPIO_ACTIVE_LOW );
      TURN_OUTPUT_PORT_OFF( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    }
    else if (g_lastCmdParameter == '2')
    {
      TURN_OUTPUT_PORT_OFF( REMOTE_GPIO3_BIT__, REMOTE_GPIO3_PORT__, REMOTE_GPIO3_DDR__, REMOTE_GPIO_ACTIVE_LOW );
      TURN_OUTPUT_PORT_OFF( REMOTE_GPIO4_BIT__, REMOTE_GPIO4_PORT__, REMOTE_GPIO4_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    }
    
    g_lastTransactionIDApplied = g_lastTransactionIDRX;
}

static void WaitInLowPowerMode()
{
    // IMPORTANT: we cannot sleep too much time because we must be able to
    //            ACK a radio command quickly enough!

#if ENABLE_LOWPOWER_MODE
    MRFI_RxIdle();              // this decreases very much power consumption!
    ///DelayMsNOInterrupts(WAIT_TIME_RADIOOFF_MSEC);                    // with this consumption remains around 14mA
    BSP_SleepFor( POWER_MODE_2, SLEEP_1_MS_RESOLUTION, WAIT_TIME_RADIOOFF_MSEC );
    MRFI_RxOn();                  // before leaving restore radio RX status
#else
    // no sleep policy: sleep with radio in RX and with interrupts enabled
    DelayMsNOInterrupts(WAIT_TIME_RADIOOFF_MSEC);
#endif
}

static void ReadBatteryVoltage()
{
    /* ADC conversion :
    * The ADC conversion is triggered by setting [ADCCON1.ST = 1].
    * The CPU will then poll [ADCCON1.EOC] until the conversion is completed.
    */

    /* Set [ADCCON1.ST] and await completion (ADCCON1.EOC = 1) */
    ADCCON1 |= ADCCON1_ST | BIT1 | BIT0;
    while( !(ADCCON1 & ADCCON1_EOC));

    /* Store the ADC result from the ADCH/L register to the adc_result variable.
    * The 4 LSBs in ADCL will not contain valid data, and are masked out.
    */
    g_last_adc_result = ADCL & 0xF0;

    // these are for 8bits resolution:
    //g_last_adc_result &= 0x00FF;

    // these are for 12bits resolution:
    g_last_adc_result |= (ADCH << 8);
    g_last_adc_result &= 0x0FFF;

    // convert ADC read range:
    // final result:
    //   80=FULL BATTERY (about 13V)
    //   20=DEPLETED BATTERY (about 3.3V)
    g_last_adc_result = g_last_adc_result*6/100;
}

static void PinConfigRemote()
{
    /* I/O-Port configuration :
    * configure our 2 output pins
    * PIN0_7 is configured to an ADC input pin.
    */

    __bsp_LED_CONFIG__  ( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    __bsp_LED_CONFIG__  ( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    __bsp_LED_CONFIG__  ( REMOTE_GPIO3_BIT__, REMOTE_GPIO3_PORT__, REMOTE_GPIO3_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    __bsp_LED_CONFIG__  ( REMOTE_GPIO4_BIT__, REMOTE_GPIO4_PORT__, REMOTE_GPIO4_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    __bsp_LED_TURN_OFF__( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    __bsp_LED_TURN_OFF__( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    __bsp_LED_TURN_OFF__( REMOTE_GPIO3_BIT__, REMOTE_GPIO3_PORT__, REMOTE_GPIO3_DDR__, REMOTE_GPIO_ACTIVE_LOW );
    __bsp_LED_TURN_OFF__( REMOTE_GPIO4_BIT__, REMOTE_GPIO4_PORT__, REMOTE_GPIO4_DDR__, REMOTE_GPIO_ACTIVE_LOW );

    // Set [ADCCFG.ADCCFG7 = 1].
    ADCCFG |= ADCCFG_7;

    /* ADC configuration :
    *  - [ADCCON1.ST] triggered
    *  - 8 bit resolution   (we could go up to 12 but 8 are easier to send over radio!)
    *  - Single-ended
    *  - Single-channel, due to only 1 pin is selected in the ADCCFG register
    *  - Reference voltage is VDD on AVDD pin

    *  Note: - [ADCCON1.ST] must always be written to 11
    *
    *  The ADC result is represented in two's complement.
    */

    // Set [ADCCON1.STSEL] according to ADC configuration */
    ADCCON1 = (ADCCON1 & ~ADCCON1_STSEL) | STSEL_ST | BIT1 | BIT0;

    // Set [ADCCON2.SREF/SDIV/SCH] according to ADC configuration */
    ADCCON2 = ADCCON2_SREF_AVDD | ADCCON2_SDIV_512 | ADCCON2_SCH_AIN7;
}


/***********************************************************************************
* @fn          sRemoteNode
*
* @brief       Waits for packet from Master and acknowledge this.
*              Red led lit means linked to master
*              Blinking green led indicates packet received
*              Adjust output power dynamically
*
* @param       none
*
* @return      none
*/
void sRemoteNode(void)
{
    // I/O-Port configuration
    PinConfigRemote();

    // do a first read of the battery voltage; this will enable us to inform
    // the MASTER SYSTEM about our battery status next time we must send an ACK
    ReadBatteryVoltage();

    /* turn on RX. default is RX off. */
    MRFI_RxOn();


    unsigned int count1=0, count2=0, counter_to_apply_lastcmd=0;
    unsigned int go_low_power=0;
    unsigned int do_battery_meas=0;
    while (1)
    {
        if( g_sRxCallbackSemaphore )                    /* Command successfully received? */
        {
            if (CheckCmdAndReplyWithAck())              // this is very quick and will leave the radio in RX
            {
                // we recognized a command from radio interface and we sent
                // an ACK back however do not execute immediately the command:
                // our ACK may not be received; in that case the MASTER will repeat
                // us the same command till he receives our ACK.
                // In the meantime we don't want to repeat the same commands
                // a lot of times!!

                // resetting the "counter to apply" we force restarting the
                // wait-before-exec
                //counter_to_apply_lastcmd = 0;
            }
        }

        count1++;
        go_low_power = ((count1 % 0xFF00) == 0);
        if (go_low_power)
        {
            if (g_lastCmdRx != CMD_MAX)
            {
                //counter_to_apply_lastcmd++;
                //if (counter_to_apply_lastcmd == 3)      // wait some time before applying RX command!
                {
                    ApplyCmdRx();               // this will take a lot of time!
                    g_lastCmdRx = CMD_MAX;
                    counter_to_apply_lastcmd = 0;
                    // we just received something; it's unlikely we're going to receive
                    // another command shortly... we can sleep a little bit
                    ///go_low_power=1;
                }
            }

            WaitInLowPowerMode();               // this may take a lot of time but will leave the radio in RX
            go_low_power = 0;

            // should we do a battery measurement?
            count2++;
            do_battery_meas = ((count2 % 0x20) == 0);
            if (do_battery_meas)
            {
                 // with current settings for count1 and count2, we enter
                 // this piece of code about every 40sec

                ReadBatteryVoltage();
                do_battery_meas = 0;
            }
        }
        //else: keep spinning with radio in Rx mode for a while
    }
}

#endif   // REMOTE



