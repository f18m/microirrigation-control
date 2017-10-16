/***********************************************************************************

Filename:	    main.c

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




/***********************************************************************************
* LOCAL VARIABLES
*/

/***********************************************************************************
* GLOBAL VARIABLES
*/
volatile uint8_t       g_sRxCallbackSemaphore = 0;
         mrfiPacket_t  g_pktRx;


/***********************************************************************************
* LOCAL FUNCTIONS
*/
void SetRxAddressFilter(void);

/***********************************************************************************
* GLOBAL FUNCTIONS
*/
extern void       sLime2Node(void);
extern void       sRemoteNode(void);


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

  // Initialize the MRFI RF link layer
  MRFI_Init();
  MRFI_WakeUp();
  MRFI_SetLogicalChannel(MRFI_CHANNEL);
  MRFI_RxOn();
  //SetRxAddressFilter();
  
  /* Turn on LEDs indicating power on */
  BSP_TURN_ON_LED1();
  BSP_TURN_ON_LED2();
  NWK_DELAY_NO_INTERRUPTS(1000);
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
      
      NWK_DELAY_NO_INTERRUPTS(2000);//BSP_SleepFor( POWER_MODE_2, SLEEP_1_MS_RESOLUTION, 2000);
      
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



