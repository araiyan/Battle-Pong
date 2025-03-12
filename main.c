// Group Name: Raiyan Sazid, Mohammad Ayub Hanif Saleh

//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

//*****************************************************************************
//
// Application Name     - Battle Pong
// Application Overview - The demo application focuses on showing the required 
//                        initialization sequence to enable the CC3200 SPI 
//                        module in full duplex 4-wire master and slave mode(s).
//
//*****************************************************************************


//*****************************************************************************
//
//! \addtogroup SPI_Demo
//! @{
//
//*****************************************************************************

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>


// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"
#include "gpio.h"
#include "gpio_if.h"

// Common interface includes
#include "uart_if.h"
#include "pin_mux_config.h"

#include "i2c_if.h"

// Adafruit libraries
#include "oled/Adafruit_SSD1351.h"
#include "oled/Adafruit_GFX.h"
#include "oled/oled_test.h"
#include "oled/glcdfont.h"

// Game Libraries
#include "dot_tracker.h"
#include "game.h"

#define APPLICATION_VERSION     "1.4.0"

#define SPI_IF_BIT_RATE  16000000

#define MASTER_MSG       "This is CC3200 SPI Master Application\n\r"
#define SLAVE_MSG        "This is CC3200 SPI Slave Application\n\r"


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


volatile int* upgradeTrigger;
struct BattlePongGame* battlePongGame;
volatile char boardCommand[64];
volatile int board_idx = 0;
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************
static void GPIOA2IntHandler(void) {
    uint64_t ulStatus = GPIOIntStatus(UPGRADE_POWER_BUTTON_BASE, true);
    GPIOIntClear(UPGRADE_POWER_BUTTON_BASE, ulStatus);

    if (ulStatus & UPGRADE_POWER_BUTTON_PIN) {
        *upgradeTrigger = true;
    }

    return;
}


void UARTIntHandler(void) {
    unsigned char recvChar;
    UARTIntDisable(UARTA1_BASE, UART_INT_RX);

    battlePongGame->cmdIdx = 0;
    while (UARTCharsAvail(UARTA1_BASE) || UARTSpaceAvail(UARTA1_BASE)) {
        recvChar = UARTCharGet(UARTA1_BASE);
        if (recvChar == '\n') break;
        battlePongGame->cmdRecvBuffer[battlePongGame->cmdIdx++] = recvChar;
        UtilsDelay(200);
    }
    battlePongGame->cmdRecvBuffer[battlePongGame->cmdIdx] = '\0';

    UARTIntClear(UARTA1_BASE, UART_INT_RX);
    UARTIntEnable(UARTA1_BASE, UART_INT_RX);
}


//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

static void
SPIInit(void)
{
    //
    // Enable the SPI module clock
    //
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    //
    // Reset SPI
    //
    MAP_SPIReset(GSPI_BASE);

    //
    // Reset the peripheral
    //
    MAP_PRCMPeripheralReset(PRCM_GSPI);
    // Configure SPI interface
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_8));


    // Enable SPI for communication
    MAP_SPIEnable(GSPI_BASE);
}

static void
ButtonInit(void)
{
    GPIOIntRegister(UPGRADE_POWER_BUTTON_BASE, GPIOA2IntHandler);
    GPIOIntTypeSet(UPGRADE_POWER_BUTTON_BASE, UPGRADE_POWER_BUTTON_PIN, GPIO_FALLING_EDGE);
    GPIOIntClear(UPGRADE_POWER_BUTTON_BASE, UPGRADE_POWER_BUTTON_PIN);

    // Enable Interrupts
    GPIOIntEnable(UPGRADE_POWER_BUTTON_BASE, UPGRADE_POWER_BUTTON_PIN);
}

static void
UARTInit(void)
{
    // Initialize partner device over UART 1
    PRCMPeripheralReset(PRCM_UARTA1);
    UARTConfigSetExpClk(
            UARTA1_BASE,
            PRCMPeripheralClockGet(PRCM_UARTA1),
            UART_BAUD_RATE,
            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE)
    );
    UARTIntRegister(UARTA1_BASE, UARTIntHandler);
    UARTIntEnable(UARTA1_BASE, UART_INT_RX);
    UARTFIFOLevelSet(UARTA1_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);
    UARTEnable(UARTA1_BASE);
}



//*****************************************************************************
//
//! Main function for spi demo application
//!
//! \param none
//!
//! \return None.
//
//*****************************************************************************
void main(){
    //
    // Initialize Board configurations
    //
    BoardInit();

    //
    // Muxing UART and SPI lines.
    //
    PinMuxConfig();

    //
    // Initialising the Terminal.
    //
    InitTerm();

    //
    // I2C Init
    //
    I2C_IF_Open(I2C_MASTER_MODE_FST);

    //
    // Clearing the Terminal.
    //
    ClearTerm();

    SPIInit();
    ButtonInit();
    UARTInit();


    Adafruit_Init();
    fillScreen(BLACK);
    battlePongGame = CreateBattlePongGame(0);

    upgradeTrigger = &(battlePongGame->upgradePowerTrigger);

    battlePongGame->play(battlePongGame);

    while(1);
}

