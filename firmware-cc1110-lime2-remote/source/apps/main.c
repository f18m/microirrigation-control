/***********************************************************************************

Filename:	    main.c

Description:        Contains initialization/utility code common to the "remote" and "lime2"
                    projects.

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

#include <string.h>


/***********************************************************************************
* LOCAL VARIABLES
*/

/***********************************************************************************
* GLOBAL VARIABLES
*/
volatile uint8_t       g_sRxCallbackSemaphore = 0;
         mrfiPacket_t  g_pktRx;

const char* g_commands[] =
{
     // keep all the same length == COMMAND_LEN
    "TURNON_",
    "TURNOFF",
    "STATUS_"
};

const char* g_ack = "ACK_";


/***********************************************************************************
* LOCAL FUNCTIONS
*/
void SetRxAddressFilter(void);



/***********************************************************************************
* @fn          main
*
* @brief       This is the main entry of the SMPL link application. It sets
*              random addresses for the nodes, initalises and runs
*              MASTER and SLAVE tasks sequentially in an endless loop.
*
* @return      none
*/
void main (void)
{
    //uint8_t buttonPushed;
    BSP_Init();

    // assert some preconditions:
    for (unsigned int cmdi=0; cmdi < CMD_MAX; cmdi++)
    {
        if (strlen(g_commands[cmdi]) != COMMAND_LEN)
        {
            BSP_TURN_ON_LED1();
            BSP_TURN_ON_LED2();
            while (1)
                ;
        }
    }

    // Initialize the MRFI RF link layer
    MRFI_Init();
    MRFI_WakeUp();
    MRFI_SetLogicalChannel(MRFI_CHANNEL);
    MRFI_RxOn();
    //SetRxAddressFilter();  // LEAVE DISABLED, FOR SOME REASON DOES NOT WORK

    /* Turn on LEDs indicating power on */
    BSP_TURN_ON_LED1();
    BSP_TURN_ON_LED2();
    DelayMsNOInterrupts(1000);
    BSP_TURN_OFF_LED1();
    BSP_TURN_OFF_LED2();

#if LIME2
    sLime2Node();
#endif
#if REMOTE
    sRemoteNode();
#endif

  while (1);
}

/***********************************************************************************
* @fn          CopyAddress
*
* @brief
*
* @return
*/
void CopyAddress(uint8_t* destAddr, NodeType_t type)
{
    int i;
    for (i=0; i<MRFI_ADDR_SIZE; i++)
    destAddr[i]=0;
    if (type == NODE_LIME2)
    {
        destAddr[0]=0xAA;
        destAddr[1]=0xAB;
        destAddr[2]=0xAC;
        destAddr[MRFI_ADDR_SIZE-1]=0xFF;                    // MRFI_ADDR_SIZE=4 !!!!
    }
    else if (type == NODE_REMOTE)
    {
        destAddr[0]=0xBF;
        destAddr[1]=0xBE;
        destAddr[2]=0xBD;
        destAddr[MRFI_ADDR_SIZE-1]=0xFF;
    }
}



/***********************************************************************************
* @fn          DelayMsWithInterrupts
*
* @brief       IMPORTANT: INTERRUPTS ARE ENABLED WHILE SLEEPING!
*
* @return
*/
void DelayMsWithInterrupts(uint16_t milliseconds)
{
    while (milliseconds)
    {
        // sleep 1ms
        for (unsigned int i=0; i < 1000/15 ; i++)
            BSP_DELAY_USECS( 15 );                      // max value is 255/clockMhz = 19
        milliseconds--;
    }
}

/***********************************************************************************
* @fn          SetRxAddressFilter
*
* @brief
*
* @return
*/
void SetRxAddressFilter()
{
  /* Create and set random address for this device. */
  uint8_t ownAddr[MRFI_ADDR_SIZE];
#if LIME2
  CopyAddress(ownAddr, NODE_LIME2);
#elif REMOTE
  CopyAddress(ownAddr, NODE_REMOTE);
#endif
  MRFI_SetRxAddrFilter(ownAddr);
  MRFI_EnableRxAddrFilter();
}

/***********************************************************************************
* @fn          String2Command
*
* @brief
*
* @return
*/
command_e String2Command(const uint8_t* buf, uint16_t len)
{
    if (len < COMMAND_LEN+COMMAND_POSTFIX_LEN)
        return CMD_MAX;

    command_e ret = CMD_MAX;
    for (unsigned int cmdi=0; cmdi < CMD_MAX; cmdi++)
    {
        if (memcmp(g_commands[cmdi], buf, COMMAND_LEN) == 0)
        {
          ret = (command_e)cmdi;
          break;
        }
    }

    return ret;
}

/***********************************************************************************
* @fn          ShouldRESET
*
* @brief
*
* @return      0 - frame left for application to read
*              1 - frame could be overwritten
*/
int ShouldRESET()
{
    /* If MASTER button pushed, force re-estabilishing of the connection */
    if( BSP_BUTTON1() )
    {
      BSP_TURN_ON_LED1();
      BSP_TURN_ON_LED2();

      DelayMsNOInterrupts(2000);//BSP_SleepFor( POWER_MODE_2, SLEEP_1_MS_RESOLUTION, 2000);

      BSP_TURN_OFF_LED1();
      BSP_TURN_OFF_LED2();

      return 1;
    }

    return 0;
}

/***********************************************************************************
* @fn          sRxCallback
*
* @brief
*
* @return      0 - frame left for application to read
*              1 - frame could be overwritten
*/
void MRFI_RxCompleteISR()
{
    MRFI_Receive(&g_pktRx);
    g_sRxCallbackSemaphore = 1;
}



