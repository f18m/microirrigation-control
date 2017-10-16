/***********************************************************************************

Filename:	    main.h

Description:	

Operation:    

***********************************************************************************/

#ifndef __MAIN_H__
#define __MAIN_H__


/***********************************************************************************
* INCLUDES
*/
#include "bsp.h"
#include "mrfi.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "bsp_extended.h"


/***********************************************************************************
* CONSTANTS and DEFINITIONS
*/

/* Delay loop support. Requires mrfi.h. MRFI will disable interrupts while sleeping.
   If this is not desired, use BSP_DELAY_USECS() instead. */
#define NWK_DELAY_NO_INTERRUPTS(spinMs)      MRFI_DelayMs(spinMs)
#define NWK_REPLY_DELAY()                    MRFI_ReplyDelay()

#define SPIN_ABOUT_QUARTER_A_SECOND   NWK_DELAY_NO_INTERRUPTS(250)
#define SPIN_ABOUT_100_MS             NWK_DELAY_NO_INTERRUPTS(100)
#define MRFI_CHANNEL                  0

#define SLEEP_31_25_US_RESOLUTION     0
#define SLEEP_1_MS_RESOLUTION         1
#define SLEEP_32_MS_RESOLUTION        2
#define SLEEP_1_S_RESOLUTION          3

#define MASTER_BUTTON                 1
#define SLAVE_BUTTON                  2
#define BOTH_BUTTONS                  3

#define MAX_RADIO_PKT_LEN             10
#define ACK_PKT_LEN                   4
#define CMD_PKT_LEN                   4

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   GPIO #1 ---> 
 *      LIME2: attached to LIME2 PC22
 *      REMOTE: attached to VALVE_CTRL1
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Polarity  :  Active High
 *   GPIO      :  P0.3
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define LIME2_GPIO1_PORT__           P0
#define LIME2_GPIO1_BIT__            3
#define LIME2_GPIO1_DDR__            P0DIR
#define REMOTE_GPIO1_PORT__          P0
#define REMOTE_GPIO1_BIT__           3
#define REMOTE_GPIO1_DDR__           P0DIR

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   GPIO #2 --->
 *      LIME2: attached to LIME2 PC23
 *      REMOTE: attached to VALVE_CTRL2
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Polarity  :  Active High
 *   GPIO      :  P0.2
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define LIME2_GPIO2_PORT__           P0
#define LIME2_GPIO2_BIT__            2
#define LIME2_GPIO2_DDR__            P0DIR
#define REMOTE_GPIO2_PORT__          P0
#define REMOTE_GPIO2_BIT__           2
#define REMOTE_GPIO2_DDR__           P0DIR

typedef enum
{
  NODE_LIME2,
  NODE_REMOTE
} NodeType_t;

   

/***********************************************************************************
* GLOBALS
*/
extern volatile uint8_t       g_sRxCallbackSemaphore;
extern mrfiPacket_t           g_pktRx;


/***********************************************************************************
* FUNCTIONS
*/
int ShouldRESET(void);
void CopyAddress(uint8_t* destAddr, NodeType_t type);

#endif





