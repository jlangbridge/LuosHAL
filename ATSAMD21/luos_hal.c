/******************************************************************************
 * @file luosHAL
 * @brief Luos Hardware Abstration Layer. Describe Low layer fonction 
 * @MCU Family ATSAMD21
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/
#include "luos_hal.h"

#include <stdbool.h>
#include <string.h>
#include "reception.h"
#include "context.h"

//MCU dependencies this HAL is for family Atmel ATSAMD21 you can find


/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
volatile uint32_t tick;
uint32_t Timer_Reload = ((MCUFREQ /DEFAULTBAUDRATE)*20); //20us//2*10bits

typedef struct
{
    uint8_t Pin;
    uint8_t Port;
    uint8_t Irq;
} Port_t;

Port_t PTP[NBR_PORT];
/*******************************************************************************
 * Function
 ******************************************************************************/
static void LuosHAL_SystickInit(void);
static void LuosHAL_FlashInit(void);
static void LuosHAL_CRCInit(void);
static void LuosHAL_TimeoutInit(void);
static void LuosHAL_ResetTimeout(void);
static inline void LuosHAL_ComTimeout(void);
static void LuosHAL_GPIOInit(void);
static void LuosHAL_FlashEraseLuosMemoryInfo(void);
static inline void LuosHAL_ComReceive(void);
static inline void LuosHAL_GPIOProcess(uint16_t GPIO);
static void LuosHAL_RegisterPTP(void);

/////////////////////////Luos Library Needed function///////////////////////////

/******************************************************************************
 * @brief Luos HAL general initialisation
 * @param None
 * @return None
 ******************************************************************************/
void LuosHAL_Init(void)
{
    //Systick Initialization
    LuosHAL_SystickInit();
    
    //IO Initialization
    LuosHAL_GPIOInit();

    // Flash Initialization
    LuosHAL_FlashInit();

    // CRC Initialization
    LuosHAL_CRCInit();

    //Com Initialization
    LuosHAL_ComInit(DEFAULTBAUDRATE);
}
/******************************************************************************
 * @brief Luos HAL general disable IRQ
 * @param None
 * @return None
 ******************************************************************************/
void LuosHAL_SetIrqState(uint8_t Enable)
{
    if (Enable == true)
    {
        __enable_irq();
    }
    else
    {
        __disable_irq();
    }
}
/******************************************************************************
 * @brief Luos HAL general systick tick at 1ms initialize
 * @param None
 * @return tick Counter
 ******************************************************************************/
static void LuosHAL_SystickInit(void)
{
    SysTick->CTRL = 0;
	SysTick->VAL = 0;
	SysTick->LOAD = 0xbb80 - 1;
	SysTick->CTRL = SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
}
/******************************************************************************
 * @brief Luos HAL general systick tick at 1ms
 * @param None
 * @return tick Counter
 ******************************************************************************/
uint32_t LuosHAL_GetSystick(void)
{
    return tick;
}
/******************************************************************************
 * @brief Luos HAL general systick tick at 1ms
 * @param None
 * @return tick Counter
 ******************************************************************************/
void SysTick_Handler(void)
{
    tick++;
}
/******************************************************************************
 * @brief Luos HAL Initialize Generale communication inter node
 * @param Select a baudrate for the Com
 * @return none
 ******************************************************************************/
void LuosHAL_ComInit(uint32_t Baudrate)
{
    uint32_t baud = 0;
    //initialize clock
    LUOS_COM_CLOCK_ENABLE();

    /* Disable the USART before configurations */
    LUOS_COM->USART_INT.SERCOM_CTRLA &= ~SERCOM_USART_INT_CTRLA_ENABLE_Msk;
    
    /* Configure Baud Rate */
    baud = 65536 - ((uint64_t)65536 * 16 * Baudrate) / MCUFREQ;
    LUOS_COM->USART_INT.SERCOM_BAUD = SERCOM_USART_INT_BAUD_BAUD(baud);

    //Configures USART Clock Mode/ TXPO and RXPO/ Data Order/ Standby Mode/ Sampling rate/ IBON
    LUOS_COM->USART_INT.SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK | SERCOM_USART_INT_CTRLA_RXPO(COM_RX_POS) 
                                        | SERCOM_USART_INT_CTRLA_TXPO(COM_TX_POS) | SERCOM_USART_INT_CTRLA_DORD_Msk 
                                        | SERCOM_USART_INT_CTRLA_IBON_Msk | SERCOM_USART_INT_CTRLA_FORM(0x0) 
                                        | SERCOM_USART_INT_CTRLA_SAMPR(0) ;
 
    //Configures RXEN/ TXEN/ CHSIZE/ Parity/ Stop bits
    LUOS_COM->USART_INT.SERCOM_CTRLB = SERCOM_USART_INT_CTRLB_CHSIZE_8_BIT | SERCOM_USART_INT_CTRLB_SBMODE_1_BIT 
    									| SERCOM_USART_INT_CTRLB_RXEN_Msk | SERCOM_USART_INT_CTRLB_TXEN_Msk
                                        | SERCOM_USART_INT_CTRLB_SFDE_Msk ;

    /* Enable the UART after the configurations */
    LUOS_COM->USART_INT.SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;

    /* Wait for sync */
    while(LUOS_COM->USART_INT.SERCOM_SYNCBUSY);

    /* Clean IT */
    LUOS_COM->USART_INT.SERCOM_INTENSET = SERCOM_USART_INT_INTENSET_RESETVALUE;
    
    /* Enable error interrupt */
    LUOS_COM->USART_INT.SERCOM_INTENSET = SERCOM_USART_INT_INTENSET_ERROR_Msk;

    /* Enable Receive Complete interrupt */
    LUOS_COM->USART_INT.SERCOM_INTENSET = SERCOM_USART_INT_INTENSET_RXC_Msk;
    
    NVIC_SetPriority(LUOS_COM_IRQ, 3);
    NVIC_EnableIRQ(LUOS_COM_IRQ);

    //Timeout Initialization
    Timer_Reload = ((MCUFREQ /Baudrate)*20); //20us//2*10bits
    LuosHAL_TimeoutInit();
}
/******************************************************************************
 * @brief Tx enable/disable relative to com
 * @param None
 * @return None
 ******************************************************************************/
void LuosHAL_SetTxState(uint8_t Enable)
{
    if (Enable == true)
    {
        PORT_REGS->GROUP[COM_TX_PORT].PORT_PINCFG[COM_TX_PIN] &= ~PORT_PINCFG_PULLEN; //TX  push pull
        if ((TX_EN_PIN != DISABLE) || (TX_EN_PORT != DISABLE))
    	{
			PORT_REGS->GROUP[TX_EN_PORT].PORT_OUTSET = (1 << TX_EN_PIN); //enable Tx
		}
    }
    else
    {
        PORT_REGS->GROUP[COM_TX_PORT].PORT_PINCFG[COM_TX_PIN] |= PORT_PINCFG_PULLEN;//Tx Open drain
        if ((TX_EN_PIN != DISABLE) || (TX_EN_PORT != DISABLE))
    	{
        	PORT_REGS->GROUP[TX_EN_PORT].PORT_OUTCLR = (1 << TX_EN_PIN); //disable Tx
        }
        LUOS_COM->USART_INT.SERCOM_INTFLAG |= SERCOM_USART_EXT_INTFLAG_RXS(0);
    }
}

/******************************************************************************
 * @brief Rx enable/disable relative to com
 * @param
 * @return
 ******************************************************************************/
void LuosHAL_SetRxState(uint8_t Enable)
{
    if (Enable == true)
    {
        LUOS_COM->USART_INT.SERCOM_DATA;//clear data buffer
        LUOS_COM->USART_INT.SERCOM_CTRLB |= SERCOM_USART_INT_CTRLB_RXEN_Msk;
        if ((RX_EN_PIN != DISABLE) || (RX_EN_PORT != DISABLE))
    	{
        	PORT_REGS->GROUP[RX_EN_PORT].PORT_OUTCLR = (1 << RX_EN_PIN); //enable rx
        }
    }
    else
    {
    	if ((RX_EN_PIN != DISABLE) || (RX_EN_PORT != DISABLE))
    	{
        	PORT_REGS->GROUP[RX_EN_PORT].PORT_OUTSET = (1 << RX_EN_PIN); //disable rx
        }
        LUOS_COM->USART_INT.SERCOM_CTRLB &= ~SERCOM_USART_INT_CTRLB_RXEN_Msk;
    }
}
/******************************************************************************
 * @brief Process data receive
 * @param None
 * @return None
 ******************************************************************************/
static inline void LuosHAL_ComReceive(void)
{
    uint8_t data = 0;

    LuosHAL_ResetTimeout();
    
    // check if we receive a data
    if((LUOS_COM->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_RXC_Msk) == SERCOM_USART_INT_INTFLAG_RXC_Msk)
    {
        //clean start bit detection
        data = LUOS_COM->USART_INT.SERCOM_DATA;
        ctx.rx.callback(&data);
    }
    // check error on ligne
    else if((LUOS_COM->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_ERROR_Msk) == SERCOM_USART_INT_INTFLAG_ERROR_Msk)
    {
    	if((LUOS_COM->USART_INT.SERCOM_STATUS & SERCOM_USART_INT_STATUS_FERR_Msk) == SERCOM_USART_INT_STATUS_FERR_Msk)
    	{
    		ctx.rx.status.rx_framing_error = true;
    	}
    	LUOS_COM->USART_INT.SERCOM_STATUS = SERCOM_USART_INT_STATUS_PERR_Msk | SERCOM_USART_INT_STATUS_FERR_Msk | SERCOM_USART_INT_STATUS_BUFOVF_Msk;
    	LUOS_COM->USART_INT.SERCOM_INTFLAG |= SERCOM_USART_INT_INTENCLR_ERROR(0);
        
        while((LUOS_COM->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_RXC_Msk) == SERCOM_USART_INT_INTFLAG_RXC_Msk)
        {
            data = LUOS_COM->USART_INT.SERCOM_DATA;
        }
    }
    LUOS_COM->USART_INT.SERCOM_INTFLAG |= SERCOM_USART_EXT_INTFLAG_RXS(0);
}
/******************************************************************************
 * @brief Process data transmit
 * @param None
 * @return None
 ******************************************************************************/
uint8_t LuosHAL_ComTransmit(uint8_t *data, uint16_t size)
{
	for (uint16_t i = 0; i < size; i++)
    {
        ctx.tx.lock = true;
        while ((LUOS_COM->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk) != SERCOM_USART_INT_INTFLAG_DRE_Msk);
        LUOS_COM->USART_INT.SERCOM_DATA = *(data + i);
        if (ctx.tx.collision)
        {
            // There is a collision
            ctx.tx.collision = FALSE;
            return 1;
        }
        LuosHAL_ResetTimeout();
    }
    LUOS_TIMER->COUNT16.TC_INTENCLR = TC_INTFLAG_OVF_Msk;
    LUOS_COM->USART_INT.SERCOM_INTFLAG |= SERCOM_USART_EXT_INTFLAG_RXS(0);
    return 0;
}
/******************************************************************************
 * @brief Luos Timeout for Tx communication
 * @param None
 * @return None
 ******************************************************************************/
void LuosHAL_ComTxComplete(void)
{
    while((LUOS_COM->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_TXC_Msk) != SERCOM_USART_INT_INTFLAG_TXC_Msk);
    LuosHAL_ResetTimeout();
}
/******************************************************************************
 * @brief set state of Txlock detection pin
 * @param None
 * @return Lock status
 ******************************************************************************/
void LuosHAL_SetTxLockDetecState(uint8_t Enable)
{
    if (TX_LOCK_DETECT_IRQ != DISABLE)
    {
        EIC_REGS->EIC_INTFLAG = (1 << TX_LOCK_DETECT_PIN); //clear IT flag
        if (Enable == true)
        {
            EIC_REGS->EIC_INTENSET = (1 << TX_LOCK_DETECT_PIN);// enable IT
        }
        else
        {
            EIC_REGS->EIC_INTENCLR = (1 << TX_LOCK_DETECT_PIN);
        }
    }
}
/******************************************************************************
 * @brief get Lock Com transmit status this is the HW that can generate lock TX
 * @param None
 * @return Lock status
 ******************************************************************************/
uint8_t LuosHAL_GetTxLockState(void)
{
    uint8_t result = false;
    if ((LUOS_COM->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_RXS_Msk) == SERCOM_USART_INT_INTFLAG_RXS_Msk)
    {
        LUOS_COM->USART_INT.SERCOM_INTFLAG |= SERCOM_USART_EXT_INTFLAG_RXS(0);
        LuosHAL_ResetTimeout();
        result = true;
    }
    else
    {
        if ((TX_LOCK_DETECT_PIN != DISABLE) && (TX_LOCK_DETECT_PORT != DISABLE))
        {
            if(TX_LOCK_DETECT_IRQ == DISABLE)
            {
                result = ((PORT_REGS->GROUP[TX_LOCK_DETECT_PORT].PORT_IN >> TX_LOCK_DETECT_PIN) & 0x01);
            }
        } 
    }
    return result;
}
/******************************************************************************
 * @brief Luos Timeout initialisation
 * @param None
 * @return None
 ******************************************************************************/
static void LuosHAL_TimeoutInit(void)
{
    //initialize clock
    LUOS_TIMER_LOCK_ENABLE();
    LUOS_TIMER->COUNT16.TC_CTRLA = TC_CTRLA_RESETVALUE;
    while((LUOS_TIMER->COUNT16.TC_STATUS & TC_STATUS_SYNCBUSY_Msk));
    /* Configure counter mode & prescaler */
    LUOS_TIMER->COUNT16.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV1 | TC_CTRLA_WAVEGEN_MPWM ;
    LUOS_TIMER->COUNT16.TC_CTRLBSET = TC_CTRLBSET_ONESHOT_Msk;

    //LUOS_TIMER->COUNT16.TC_DBGCTRL = TC_DBGCTRL_DBGRUN_Msk;
    /* Clear all interrupt flags */
    LUOS_TIMER->COUNT16.TC_INTENSET = TC_INTFLAG_RESETVALUE;
    
    NVIC_SetPriority(LUOS_TIMER_IRQ, 3);
    NVIC_EnableIRQ(LUOS_TIMER_IRQ);
}
/******************************************************************************
 * @brief Luos Timeout for Rx communication
 * @param None
 * @return None
 ******************************************************************************/
static void LuosHAL_ResetTimeout(void)
{
    NVIC_ClearPendingIRQ(LUOS_TIMER_IRQ);// clear IT pending
    LUOS_TIMER->COUNT16.TC_INTFLAG |= TC_INTFLAG_OVF(0);
    LUOS_TIMER->COUNT16.TC_COUNT = 0xFFFF - Timer_Reload;
    LUOS_TIMER->COUNT16.TC_INTENSET = TC_INTFLAG_OVF_Msk;
    LUOS_TIMER->COUNT16.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    while((LUOS_TIMER->COUNT16.TC_STATUS & TC_STATUS_SYNCBUSY_Msk));
}
/******************************************************************************
 * @brief Luos Timeout for Rx communication
 * @param None
 * @return None
 ******************************************************************************/
static inline void LuosHAL_ComTimeout(void)
{
    if((LUOS_TIMER->COUNT16.TC_INTFLAG  & TC_INTFLAG_OVF_Msk) == TC_INTFLAG_OVF_Msk)
    {
        LUOS_TIMER->COUNT16.TC_INTFLAG |= TC_INTFLAG_OVF(0);
        if (ctx.tx.lock == true)
        {
            Recep_Timeout();
        } 
    }
}
/******************************************************************************
 * @brief Initialisation GPIO
 * @param None
 * @return None
 ******************************************************************************/
static void LuosHAL_GPIOInit(void)
{
    uint32_t Position = 0;
    uint32_t Config = 0;

    //Activate Clock for PIN choosen in luosHAL
    PORT_CLOCK_ENABLE();

    if ((RX_EN_PIN != DISABLE) || (RX_EN_PORT != DISABLE))
    {
	    /*Configure GPIO pins : RxEN_Pin */ 
	    PORT_REGS->GROUP[RX_EN_PORT].PORT_PINCFG[RX_EN_PIN] = PORT_PINCFG_RESETVALUE; //no pin mux / no input /  no pull / low streght
	    PORT_REGS->GROUP[RX_EN_PORT].PORT_PINCFG[RX_EN_PIN] |= PORT_PINCFG_DRVSTR_Msk; //hight streght drive
	    PORT_REGS->GROUP[RX_EN_PORT].PORT_DIRSET = (1 << RX_EN_PIN); //Output
	    PORT_REGS->GROUP[RX_EN_PORT].PORT_OUTCLR = (1 << RX_EN_PIN); //disable Tx set output low
	}

    if ((TX_EN_PIN != DISABLE) || (TX_EN_PORT != DISABLE))
    {
	    /*Configure GPIO pins : TxEN_Pin */
	    PORT_REGS->GROUP[TX_EN_PORT].PORT_PINCFG[TX_EN_PIN] = PORT_PINCFG_RESETVALUE; //no pin mux / no input /  no pull / low streght
	    PORT_REGS->GROUP[TX_EN_PORT].PORT_PINCFG[TX_EN_PIN] |= PORT_PINCFG_DRVSTR_Msk; //hight streght drive
	    PORT_REGS->GROUP[TX_EN_PORT].PORT_DIRSET = (1 << TX_EN_PIN); //Output
	    PORT_REGS->GROUP[TX_EN_PORT].PORT_OUTCLR = (1 << TX_EN_PIN); //disable Tx set output low
	}

    /*Configure GPIO pins : TX_LOCK_DETECT_Pin */
    if ((TX_LOCK_DETECT_PIN != DISABLE) || (TX_LOCK_DETECT_PORT != DISABLE))
    {
		PORT_REGS->GROUP[TX_LOCK_DETECT_PORT].PORT_PINCFG[TX_LOCK_DETECT_PIN] = PORT_PINCFG_RESETVALUE; //no pin mux / no input /  no pull / low streght
		PORT_REGS->GROUP[TX_LOCK_DETECT_PORT].PORT_PINCFG[TX_LOCK_DETECT_PIN] |= PORT_PINCFG_INEN_Msk; //enable input 
    	PORT_REGS->GROUP[TX_LOCK_DETECT_PORT].PORT_OUTSET = (1 << TX_LOCK_DETECT_PIN); //pull up
        if (TX_LOCK_DETECT_IRQ != DISABLE)
        {
            PORT_REGS->GROUP[TX_LOCK_DETECT_PORT].PORT_PMUX[TX_LOCK_DETECT_PIN>>1] |= (0<<(4*(TX_LOCK_DETECT_PIN%2)));
            if(TX_LOCK_DETECT_IRQ < 8)
            {
                Config = 0;
                Position = TX_LOCK_DETECT_IRQ << 2;
                
            }
            else
            {
                Config = 1;
                Position = (TX_LOCK_DETECT_IRQ - 8) << 2;
            }
        }
        EIC_REGS->EIC_CONFIG[Config] &=~ (EIC_CONFIG_SENSE0_Msk << Position);//reset sense mode
        EIC_REGS->EIC_CONFIG[Config] |= EIC_CONFIG_SENSE0_FALL_Val << Position;// Falling EDGE
        EIC_REGS->EIC_INTFLAG = (1 << TX_LOCK_DETECT_IRQ); //clear IT flag
        EIC_REGS->EIC_INTENSET = (1 << TX_LOCK_DETECT_IRQ);// enable IT
    }

    /*Configure GPIO pin : TxPin */
    PORT_REGS->GROUP[COM_TX_PORT].PORT_PINCFG[COM_TX_PIN] = PORT_PINCFG_RESETVALUE; //no pin mux / no input /  no pull / low streght
    PORT_REGS->GROUP[COM_TX_PORT].PORT_PINCFG[COM_TX_PIN] |= PORT_PINCFG_PULLEN;
    PORT_REGS->GROUP[COM_TX_PORT].PORT_PINCFG[COM_TX_PIN] |= PORT_PINCFG_PMUXEN_Msk; //mux en 
    PORT_REGS->GROUP[COM_TX_PORT].PORT_PMUX[COM_TX_PIN>>1] |= (COM_TX_AF<<(4*(COM_TX_PIN%2))); //mux to sercom
    
    /*Configure GPIO pin : RxPin */
    PORT_REGS->GROUP[COM_RX_PORT].PORT_PINCFG[COM_RX_PIN] = PORT_PINCFG_RESETVALUE; //no pin mux / no input /  no pull / low streght
    PORT_REGS->GROUP[COM_RX_PORT].PORT_PINCFG[COM_RX_PIN] |= PORT_PINCFG_PMUXEN_Msk; //mux en 
    PORT_REGS->GROUP[COM_RX_PORT].PORT_PMUX[COM_RX_PIN>>1] |= (COM_TX_AF<<(4*(COM_RX_PIN%2))); //mux to sercom
    
    //configure PTP
    LuosHAL_RegisterPTP();
    for (uint8_t i = 0; i < NBR_PORT; i++) /*Configure GPIO pins : PTP_Pin */
    {
        // Setup PTP lines //Mux all PTP to EIC
        PORT_REGS->GROUP[PTP[NBR_PORT].Port].PORT_PMUX[PTP[NBR_PORT].Pin>>1] |= (0<<(4*(PTP[NBR_PORT].Pin%2)));
        LuosHAL_SetPTPDefaultState(i);
    }
    
    NVIC_SetPriority(EIC_IRQn, 3);
    NVIC_EnableIRQ(EIC_IRQn);
    
    //Enable EIC interrupt
    EIC_REGS->EIC_CTRL |= EIC_CTRL_ENABLE_Msk;

    //while(EIC_REGS->EIC_STATUS.EIC_SYNCBUSY);
    
}
/******************************************************************************
 * @brief Register PTP
 * @param void
 * @return None
 ******************************************************************************/
static void LuosHAL_RegisterPTP(void)
{
#if (NBR_PORT >= 1)
    PTP[0].Pin = PTPA_PIN;
    PTP[0].Port = PTPA_PORT;
    PTP[0].Irq = PTPA_IRQ;
#endif

#if (NBR_PORT >= 2)
    PTP[1].Pin = PTPB_PIN;
    PTP[1].Port = PTPB_PORT;
    PTP[1].Irq = PTPB_IRQ;
#endif

#if (NBR_PORT >= 3)
    PTP[2].Pin = PTPC_PIN;
    PTP[2].Port = PTPC_PORT;
    PTP[2].Port = PTPC_PORT;
#endif

#if (NBR_PORT >= 4)
    PTP[3].Pin = PTPD_PIN;
    PTP[3].Port = PTPD_PORT;
    PTP[3].Port = PTPD_PORT;
#endif
}
/******************************************************************************
 * @brief callback for GPIO IT
 * @param GPIO IT line
 * @return None
 ******************************************************************************/
static inline void LuosHAL_GPIOProcess(uint16_t GPIO)
{
    ////Process for Tx Lock Detec
    if (GPIO == TX_LOCK_DETECT_PIN)
    {
        ctx.tx.lock = true;
        EIC_REGS->EIC_INTENCLR = (1 << TX_LOCK_DETECT_IRQ);
    }
    else
    {
        for (uint8_t i = 0; i < NBR_PORT; i++)
        {
            if (GPIO == PTP[i].Pin)
            {
                PortMng_PtpHandler(i);
                break;
            }
        }
    }
}
/******************************************************************************
 * @brief Set PTP for Detection on branch
 * @param PTP branch
 * @return None
 ******************************************************************************/
void LuosHAL_SetPTPDefaultState(uint8_t PTPNbr)
{
    uint32_t Position = 0;
    uint32_t Config = 0;

    // Pull Down / IT mode / Rising Edge
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] = PORT_PINCFG_RESETVALUE;  //no pin mux / no input /  no pull / low streght
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] |= PORT_PINCFG_PMUXEN_Msk; //mux en 
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] |= PORT_PINCFG_PULLEN_Msk; //pull en
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_OUTCLR = (1 << PTP[PTPNbr].Pin); //pull down
    if(PTP[PTPNbr].Irq < 8)
    {
        Config = 0;
        Position = PTP[PTPNbr].Irq << 2;
        
    }
    else
    {
        Config = 1;
        Position = (PTP[PTPNbr].Irq - 8) << 2;
    }
    EIC_REGS->EIC_CONFIG[Config] &=~ (EIC_CONFIG_SENSE0_Msk << Position);//reset sense mode
    EIC_REGS->EIC_CONFIG[Config] |= EIC_CONFIG_SENSE0_RISE_Val << Position;// Rising EDGE
    EIC_REGS->EIC_INTFLAG = (1 << PTP[PTPNbr].Irq); //clear IT flag
    EIC_REGS->EIC_INTENSET = (1 << PTP[PTPNbr].Irq);// enable IT
}
/******************************************************************************
 * @brief Set PTP for reverse detection on branch
 * @param PTP branch
 * @return None
 ******************************************************************************/
void LuosHAL_SetPTPReverseState(uint8_t PTPNbr)
{
    uint32_t Position = 0;
    uint32_t Config = 0;
    // Pull Down / IT mode / Falling Edge
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] = PORT_PINCFG_RESETVALUE;  //no pin mux / no input /  no pull / low streght
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] |= PORT_PINCFG_PMUXEN_Msk; //mux en 
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] |= PORT_PINCFG_PULLEN_Msk; //pull en
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_OUTCLR = (1 << PTP[PTPNbr].Pin); //pull down
    if(PTP[PTPNbr].Irq < 8)
    {
        Config = 0;
        Position = PTP[PTPNbr].Irq << 2;
        
    }
    else
    {
        Config = 1;
        Position = (PTP[PTPNbr].Irq - 8) << 2;
    }
    EIC_REGS->EIC_CONFIG[Config] &=~ (EIC_CONFIG_SENSE0_Msk << Position);//reset sense mode
    EIC_REGS->EIC_CONFIG[Config] |= EIC_CONFIG_SENSE0_FALL_Val << Position;// Falling EDGE
    EIC_REGS->EIC_INTFLAG = (1 << PTP[PTPNbr].Irq); //clear IT flag
    EIC_REGS->EIC_INTENSET = (1 << PTP[PTPNbr].Irq);// enable IT
}
/******************************************************************************
 * @brief Set PTP line
 * @param PTP branch
 * @return None
 ******************************************************************************/
void LuosHAL_PushPTP(uint8_t PTPNbr)
{
    // Pull Down / Output mode
    EIC_REGS->EIC_INTENCLR = (1 << PTP[PTPNbr].Irq);// disable IT
    EIC_REGS->EIC_INTFLAG = (1 << PTP[PTPNbr].Irq); //clear IT flag
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] = PORT_PINCFG_RESETVALUE;  //no pin mux / no input /  no pull / low streght
    //PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] |= PORT_PINCFG_PULLEN_Msk; //pull en
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_DIRSET = (1 << PTP[PTPNbr].Pin); //Output  
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_OUTSET = (1 << PTP[PTPNbr].Pin); //pull down
	
}
/******************************************************************************
 * @brief Get PTP line
 * @param PTP branch
 * @return Line state
 ******************************************************************************/
uint8_t LuosHAL_GetPTPState(uint8_t PTPNbr)
{
    // Pull Down / Input mode
    EIC_REGS->EIC_INTENCLR = (1 << PTP[PTPNbr].Irq);// disable IT
    EIC_REGS->EIC_INTFLAG = (1 << PTP[PTPNbr].Irq); //clear IT flag
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] = PORT_PINCFG_RESETVALUE;  //no pin mux / no input /  no pull / low streght
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] |= PORT_PINCFG_INEN_Msk; //input
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_PINCFG[PTP[PTPNbr].Pin] |= PORT_PINCFG_PULLEN_Msk; //pull en
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_DIRCLR = (1 << PTP[PTPNbr].Pin); //Output 
    PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_OUTCLR = (1 << PTP[PTPNbr].Pin); //pull down 
    return (((PORT_REGS->GROUP[PTP[PTPNbr].Port].PORT_IN >> PTP[PTPNbr].Pin)) & 0x01);
}
/******************************************************************************
 * @brief Initialize CRC Process
 * @param None
 * @return None
 ******************************************************************************/
static void LuosHAL_CRCInit(void)
{

}
/******************************************************************************
 * @brief Compute CRC
 * @param None
 * @return None
 ******************************************************************************/
void LuosHAL_ComputeCRC(uint8_t *data, uint8_t *crc)
{
    for (uint8_t i = 0; i < 1; ++i)
    {
        uint16_t dbyte = data[i];
        *(uint16_t *)crc ^= dbyte << 8;
        for (uint8_t j = 0; j < 8; ++j)
        {
            uint16_t mix = *(uint16_t *)crc & 0x8000;
            *(uint16_t *)crc = (*(uint16_t *)crc << 1);
            if (mix)
                *(uint16_t *)crc = *(uint16_t *)crc ^ 0x0007;
        }
    }
}
/******************************************************************************
 * @brief Flash Initialisation
 * @param None
 * @return None
 ******************************************************************************/
static void LuosHAL_FlashInit(void)
{
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_READMODE_NO_MISS_PENALTY | NVMCTRL_CTRLB_SLEEPPRM_WAKEONACCESS 
                                | NVMCTRL_CTRLB_RWS(1) | NVMCTRL_CTRLB_MANW_Msk;
}
/******************************************************************************
 * @brief Erase flash page where Luos keep permanente information
 * @param None
 * @return None
 ******************************************************************************/
static void LuosHAL_FlashEraseLuosMemoryInfo(void)
{
    uint32_t address = ADDRESS_ALIASES_FLASH;
    NVMCTRL_REGS->NVMCTRL_ADDR = address >> 1;
    NVMCTRL_REGS->NVMCTRL_CTRLA = NVMCTRL_CTRLA_CMD_ER_Val | NVMCTRL_CTRLA_CMDEX_KEY;
    NVMCTRL_REGS->NVMCTRL_ADDR = (address + 256) >> 1;
    NVMCTRL_REGS->NVMCTRL_CTRLA = NVMCTRL_CTRLA_CMD_ER_Val | NVMCTRL_CTRLA_CMDEX_KEY;
    NVMCTRL_REGS->NVMCTRL_ADDR = (address + 512) >> 1;
    NVMCTRL_REGS->NVMCTRL_CTRLA = NVMCTRL_CTRLA_CMD_ER_Val | NVMCTRL_CTRLA_CMDEX_KEY;
    NVMCTRL_REGS->NVMCTRL_ADDR = (address + 768) >> 1;
    NVMCTRL_REGS->NVMCTRL_CTRLA = NVMCTRL_CTRLA_CMD_ER_Val | NVMCTRL_CTRLA_CMDEX_KEY;
}
/******************************************************************************
 * @brief Write flash page where Luos keep permanente information
 * @param Address page / size to write / pointer to data to write
 * @return
 ******************************************************************************/
void LuosHAL_FlashWriteLuosMemoryInfo(uint32_t addr, uint16_t size, uint8_t *data)
{
    uint32_t i = 0;
    uint32_t * paddress = (uint32_t *)addr;
    
    // Before writing we have to erase the entire page
    // to do that we have to backup current falues by copying it into RAM
    uint8_t page_backup[16 * PAGE_SIZE];
    memcpy(page_backup, (void *)ADDRESS_ALIASES_FLASH, 16 * PAGE_SIZE);

    // Now we can erase the page
    LuosHAL_FlashEraseLuosMemoryInfo();

    // Then add input data into backuped value on RAM
    uint32_t RAMaddr = (addr - ADDRESS_ALIASES_FLASH);
    memcpy(&page_backup[RAMaddr], data, size);

    /* writing 32-bit data into the given address */
    for (i = 0; i < (PAGE_SIZE/4); i++)
    {
        *paddress++ = page_backup[i];
    }

     /* Set address and command */
    NVMCTRL_REGS->NVMCTRL_ADDR = addr >> 1;

    NVMCTRL_REGS->NVMCTRL_CTRLA = NVMCTRL_CTRLA_CMD_WP_Val | NVMCTRL_CTRLA_CMDEX_KEY;
}
/******************************************************************************
 * @brief read information from page where Luos keep permanente information
 * @param Address info / size to read / pointer callback data to read
 * @return
 ******************************************************************************/
void LuosHAL_FlashReadLuosMemoryInfo(uint32_t addr, uint16_t size, uint8_t *data)
{
    memcpy(data, (void *)(addr), size);
}

/////////////////////////Special LuosHAL function///////////////////////////
void PINOUT_IRQHANDLER()
{
    uint32_t FlagIT = 0;
    for (uint8_t i = 0; i < NBR_PORT; i++)
    {
        FlagIT = (EIC_REGS->EIC_INTFLAG & (1 << PTP[i].Pin));

        if (FlagIT)
        {
            LuosHAL_GPIOProcess(PTP[i].Pin);
            EIC_REGS->EIC_INTFLAG = (uint32_t)(1 << PTP[i].Pin);
        }
    } 
}
void LUOS_COM_IRQHANDLER()
{
    LuosHAL_ComReceive();
}

void LUOS_TIMER_IRQHANDLER()
{
    LuosHAL_ComTimeout();
}


