/***********************************************************************************

Filename:	    remote.c

Description:	

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


#if REMOTE


#define ENABLE_LOWPOWER_MODE                    (0)


/***********************************************************************************
* LOCAL VARIABLES
*/
//static          linkID_t      sLinkID;
//static          uint8_t       sNoAckCount = 0;
static          mrfiPacket_t  g_pktTx;
static          uint8_t       g_lastCmdRx = 0;
static          uint16_t      g_last_adc_result;


/***********************************************************************************
* LOCAL FUNCTIONS
*/

static uint8_t CheckCmdAndReplyWithAck()
{
  //uint8_t  radioMsg[MAX_RADIO_PKT_LEN], len;   
  
  /* Put Radio in IDLE to save power */
  MRFI_RxIdle();//SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXIDLE, 0);
  //MRFI_Receive(&g_pktRx); //SMPL_Receive(sLinkID, radioMsg, &len);
  sRxCallbackSemaphore = 0;
  
  uint8_t len = MRFI_GET_PAYLOAD_LEN(&g_pktRx);   
  uint8_t* radioMsg = MRFI_P_PAYLOAD(&g_pktRx);
  if (len == CMD_PKT_LEN &&
        radioMsg[0] == 'C' &&
        radioMsg[1] == 'M' &&
        radioMsg[2] == 'D')     
  {
    // retrieve the command bit
    g_lastCmdRx = radioMsg[3];
    
    #if 0
    /* Check and adjust wanted output power */
    info.lid = sLinkID;
    SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SIGINFO, &info);
    if( sRequestPwrLevel > MINIMUM_OUTPUT_POWER )
      if( info.sigInfo.rssi  > RSSI_UPPER_THRESHOLD )
        sRequestPwrLevel--;
    if( sRequestPwrLevel < MAXIMUM_OUTPUT_POWER )
      if( info.sigInfo.rssi  < RSSI_LOWER_THRESHOLD )
        sRequestPwrLevel++;
    #endif
    
    /* Build and send acknowledge */
    MRFI_SET_PAYLOAD_LEN(&g_pktTx, ACK_PKT_LEN);   
    uint8_t* ackMsg = MRFI_P_PAYLOAD(&g_pktTx);
    ackMsg[0] = 'A';
    ackMsg[1] = 'C';
    ackMsg[2] = 'K';
    ackMsg[3] = (uint8_t)g_last_adc_result;

    CopyAddress(MRFI_P_SRC_ADDR(&g_pktTx), NODE_REMOTE);
    CopyAddress(MRFI_P_DST_ADDR(&g_pktTx), NODE_LIME2);
    
    MRFI_Transmit(&g_pktTx, MRFI_TX_TYPE_CCA);
    //SMPL_Send(sLinkID, radioMsg, ACK_PKT_LEN);
    
    // signal we are transmitting the ACK
    BSP_TURN_ON_LED1();
    SPIN_ABOUT_QUARTER_A_SECOND;
    BSP_TURN_OFF_LED1();  
    
    // we got something!
    return 1;
  }
  
  /* Turn on RX. default is RX off. */
  //MRFI_RxOn();//SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXON, 0);

  // leave radio in IDLE
  //MRFI_RxIdle();
  return 0;
}

static void ApplyCmdRx()
{
  if (g_lastCmdRx)
  {
    __bsp_LED_TURN_ON__( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, 0 /* is active low */ );
    __bsp_LED_TURN_OFF__( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, 0 /* is active low */ );
  } else {
    __bsp_LED_TURN_OFF__( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, 0 /* is active low */ );
    __bsp_LED_TURN_ON__( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, 0 /* is active low */ );
  }

  NWK_DELAY(4000);              // BSP_SleepFor(uint8_t mode, uint8_t res, uint16_t steps)

  __bsp_LED_TURN_OFF__( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, 0 /* is active low */ );
  __bsp_LED_TURN_OFF__( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, 0 /* is active low */ );
}

static void WaitInLowPowerMode()
{
#if ENABLE_LOWPOWER_MODE
  MRFI_RxIdle();
  NWK_DELAY(3000);              // BSP_SleepFor(uint8_t mode, uint8_t res, uint16_t steps)
  MRFI_RxOn();                  // before leaving restore radio RX status
#endif
}


#include "ioCCxx10_bitdef.h"
#include "hal_cc8051.h"

#include <ioCC1110.h>

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
  
  g_last_adc_result = g_last_adc_result*6/100;
}

static void PinConfigRemote()
{
  /* I/O-Port configuration :
   * configure our 2 output pins
   * PIN0_7 is configured to an ADC input pin.
   */

  __bsp_LED_CONFIG__( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, 0 /* is active low */ );
  __bsp_LED_CONFIG__( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, 0 /* is active low */ );
  __bsp_LED_TURN_OFF__( REMOTE_GPIO1_BIT__, REMOTE_GPIO1_PORT__, REMOTE_GPIO1_DDR__, 0 /* is active low */ );
  __bsp_LED_TURN_OFF__( REMOTE_GPIO2_BIT__, REMOTE_GPIO2_PORT__, REMOTE_GPIO2_DDR__, 0 /* is active low */ );

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
  //ioctlRadioSiginfo_t info;
  //uint8_t       sCurrentPwrLevel;

  // I/O-Port configuration 
  PinConfigRemote();
  
  /* turn on RX. default is RX off. */
  //sCurrentPwrLevel  = MAXIMUM_OUTPUT_POWER;
  //SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SETPWR, &sCurrentPwrLevel);
  //SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXON, 0 );
  
  ReadBatteryVoltage();

  unsigned int count1=0, count2=0;
  unsigned int go_low_power=0;
  unsigned int do_battery_meas=0;
  while (1)
  {
    MRFI_RxOn();
    if( sRxCallbackSemaphore )                    /* Command successfully received? */
    {
      if (CheckCmdAndReplyWithAck())
      {
        ApplyCmdRx();
        
        // we just received something; it's unlikely we're going to receive
        // another command shortly... we can sleep a little bit
        go_low_power=1;
      }
    }  
    
    count1++;
    go_low_power |= ((count1 % 0xFF00) == 0);
    if (go_low_power)
    {
      WaitInLowPowerMode();
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



