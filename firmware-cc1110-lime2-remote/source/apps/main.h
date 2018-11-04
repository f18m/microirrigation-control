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

#define DELAY_ABOUT_QUARTER_A_SECOND_WITH_INTERRUPTS   DelayMsWithInterrupts(250)            /* this one runs with interrupts enabled! */

#define MRFI_CHANNEL                  0

#define SLEEP_31_25_US_RESOLUTION     0
#define SLEEP_1_MS_RESOLUTION         1
#define SLEEP_32_MS_RESOLUTION        2
#define SLEEP_1_S_RESOLUTION          3

#define MASTER_BUTTON                 1
#define SLAVE_BUTTON                  2
#define BOTH_BUTTONS                  3

// NOTE: __mrfi_MAX_PAYLOAD_SIZE__ is 20bytes:
#define MAX_RADIO_PKT_LEN             16

// we have no hard limits on SPI max command lenght but it should be
// bigger than COMMAND_LEN+COMMAND_POSTFIX_LEN and REPLY_LEN+REPLY_POSTFIX_LEN:
#define SPI_COMMAND_MAX_LEN           16

typedef enum
{
  NODE_LIME2,
  NODE_REMOTE
} NodeType_t;


/***********************************************************************************
* COMMANDS OVER SPI AND OVER RADIO
*/

// direction MASTER -> SLAVE:
 //  COMMAND_LEN bytes + 
 //  1 byte containing the "transaction ID", i.e., a number that will be provided in the ACK
 //  to allow the master to associate the cmd with its ack +
 //  1 byte of command parameters
#define COMMAND_LEN                                    (7)
#define COMMAND_POSTFIX_LEN                            (2)

// direction SLAVE -> MASTER:
 // after REPLY_LEN bytes, we provide 1 byte containing the
 // "transaction ID" and 1 byte containing the "last remote battery read"
#define REPLY_LEN                                      (4)
#define REPLY_POSTFIX_LEN                              (2)

typedef enum
{
    CMD_TURN_ON = 0,  // can be sent both on SPI and on the radio
    CMD_TURN_OFF,  // can be sent both on SPI and on the radio
    CMD_NO_OP,  // can be sent both on SPI and on the radio: used to get battery level from remote
    CMD_GET_STATUS, // can be sent only on SPI
    CMD_MAX
} command_e;


/***********************************************************************************
* GLOBALS
*/
extern volatile uint8_t       g_sRxCallbackSemaphore;
extern mrfiPacket_t           g_pktRx;
extern const char*            g_commands[CMD_MAX];
extern const char*            g_ack;

/***********************************************************************************
* FUNCTIONS
*/

void sLime2Node(void);
void sRemoteNode(void);
int ShouldRESET(void);
void CopyAddress(uint8_t* destAddr, NodeType_t type);
command_e String2Command(const uint8_t* buf, uint16_t len);

/* Delay loop support. Requires mrfi.h. MRFI will disable interrupts while sleeping.
   If this is not desired, use BSP_DELAY_USECS() instead.
   Note that DelayMsNOInterrupts() accepts values in uint16_t range. */
#define DelayMsNOInterrupts(milliseconds)                MRFI_DelayMs(milliseconds)

/* Note that DelayMsWithInterrupts() accepts values in uint16_t range. */
void DelayMsWithInterrupts(uint16_t milliseconds);

#endif





