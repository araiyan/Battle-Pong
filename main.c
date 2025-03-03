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
// Application Name     - SPI Demo
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

// Common interface includes
#include "uart_if.h"
#include "pin_mux_config.h"

#include "i2c_if.h"

// Adafruit libraries
#include "oled/Adafruit_SSD1351.h"
#include "oled/Adafruit_GFX.h"
#include "oled/oled_test.h"
#include "oled/glcdfont.h"


#define APPLICATION_VERSION     "1.4.0"
//*****************************************************************************
//
// Application Master/Slave mode selector macro
//
// MASTER_MODE = 1 : Application in master mode
// MASTER_MODE = 0 : Application in slave mode
//
//*****************************************************************************
#define MASTER_MODE      0

#define SPI_IF_BIT_RATE  100000
#define TR_BUFF_SIZE     100

#define MASTER_MSG       "This is CC3200 SPI Master Application\n\r"
#define SLAVE_MSG        "This is CC3200 SPI Slave Application\n\r"

// Color definitions
#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF
#define PINK            0xFAFA

// OLED BALL CONFIGURATION
#define BALL_SPEED 0.05

#define HIT_BOX_GOAL_SIZE 4
#define HIT_BOX_COLOR GREEN
#define HIT_BOX_BG    BLACK
#define HIT_SCORE_COLOR BLUE

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************



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

// Sleep for specified milli seconds
void sleep(unsigned long time) {
    time *= 1000;
    volatile unsigned long delay;
    for (delay = 0; delay < time; delay++);
}

void grabAccelerometerData(int *x, int *y) {
    unsigned char accelerometerData[64];
    unsigned char accRegOffset = 0x3;

    I2C_IF_Write(0x18, &accRegOffset, 1, 0);
    I2C_IF_Read(0x18, &accelerometerData[0], 3);

    *x = accelerometerData[0];
    *y = accelerometerData[2];

    // Normalize x and y tilts
    if (*x > 127) *x -= 256;
    if (*y > 127) *y -= 256;
}


// *********************************************************** //
// Useful Structures
struct Vector2DI {
    int x, y;
};

struct Vector2DF {
    float x, y;
};
// *********************************************************** //
// Ball Logic
struct OledBall {
    struct Vector2DF pos;
    struct Vector2DI prevPos;

    unsigned int color;
    unsigned int bgColor;

    void (*update)(struct OledBall*, int, int);
};

void ballUpdate(struct OledBall* ball, int accX, int accY) {
    ball->pos.x += (accX * BALL_SPEED);
    ball->pos.y += (accY * BALL_SPEED);

    // Border control;
    ball->pos.x = fmax(fmin(ball->pos.x, 127), 1);
    ball->pos.y = fmax(fmin(ball->pos.y, 127), 1);

    int newX = round(ball->pos.x);
    int newY = round(ball->pos.y);

    if (newX != ball->prevPos.x || newY != ball->prevPos.y) {
        drawPixel(ball->prevPos.y, ball->prevPos.x, ball->bgColor);
        drawPixel(newY, newX, ball->color);
        ball->prevPos.x = newX;
        ball->prevPos.y = newY;
    }
}

// *********************************************************** //
// Game Logic
struct HitBoxGame {
    struct Vector2DI boxPos;
    struct Vector2DI boxSize;
    struct OledBall* ball;

    int score;

    void (*play)(struct HitBoxGame*);
    void (*update)(struct HitBoxGame*);
};

void HitBoxGamePlay(struct HitBoxGame* game) {
    int accX, accY;

    drawRect(game->boxPos.x, game->boxPos.y, game->boxSize.x, game->boxSize.y, HIT_BOX_COLOR);
    while (1) {
        grabAccelerometerData(&accX, &accY);
        game->ball->update(game->ball, accX, accY);
        game->update(game);

        sleep(10);
    }
}

// update the position of the rectangle by removing the old box and
// creating a new box in a random location
void HitBoxGameDrawGoalBox(struct HitBoxGame* game) {
    drawRect(game->boxPos.y, game->boxPos.x, game->boxSize.y, game->boxSize.x, HIT_BOX_BG);

    // Print Score to the screen
    game->score += 1;
    setCursor(4, 4);
    Outstr("Score: ");

    char scr[20];
    sprintf(scr, "%d", game->score);
    fillRoundRect(40, 4, 40, 10, 0, HIT_BOX_BG);
    setCursor(40, 4);
    Outstr(scr);


    int newBoxX = (rand() % 96) + 8;
    int newBoxY = (rand() % 96) + 8;

    game->boxPos.x = newBoxX;
    game->boxPos.y = newBoxY;

    drawRect(game->boxPos.y, game->boxPos.x, game->boxSize.y, game->boxSize.x, HIT_BOX_COLOR);
    return NULL;
}

// Collision detection of the ball with the box
void HitBoxGameBoxUpdate(struct HitBoxGame* game) {
    if (game->ball->pos.x >= (game->boxPos.x - 1) && game->ball->pos.y >= (game->boxPos.y - 1)
            && game->ball->pos.x < (game->boxPos.x + game->boxSize.x)
            && game->ball->pos.y < (game->boxPos.y + game->boxSize.y)) {

        HitBoxGameDrawGoalBox(game);
    }
}


void printFullCharacterSet() {
    unsigned long row = 0, column = 0, i;

    for (i = 0; i < sizeof(font); i += 5) {
        setCursor(column, row);
        drawChar(row, column, font[i], WHITE - i, BLACK, 1);

        column += 8;
        if (column >= 128) {
            row += 8;
            column = 0;
        }
    }

}

void horizontal8Bands() {
    unsigned int colors[8] = {WHITE, BLUE, RED, GREEN, CYAN, MAGENTA, YELLOW, PINK};
    unsigned long i;
    for (i = 0; i < 8; i++){
        drawLine(0, 14*i, 128, 14*i, colors[i]);
    }
}

void vertical8Bands() {
    unsigned int colors[8] = {WHITE, BLUE, RED, GREEN, CYAN, MAGENTA, YELLOW, PINK};
    unsigned long i;
    for (i = 0; i < 8; i++){
        drawLine(14*i, 0, 14*i, 128, colors[i]);
    }
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
void main()
{
    //
    // Initialize Board configurations
    //
    BoardInit();

    //
    // Muxing UART and SPI lines.
    //
    PinMuxConfig();

    //
    // Enable the SPI module clock
    //
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

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

    //
    // Display the Banner
    //
    Message("\n\n\n\r");
    Message("\t\t   ********************************************\n\r");
    Message("\t\t        CC3200 OLED Demo Application  \n\r");
    Message("\t\t   ********************************************\n\r");
    Message("\n\n\n\r");

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

    struct OledBall oledBall = {64, 64, 0, 0, WHITE, BLACK, ballUpdate};
    struct HitBoxGame hitBoxGame = {{64, 64}, {HIT_BOX_GOAL_SIZE, HIT_BOX_GOAL_SIZE},
                                    &oledBall, 0, HitBoxGamePlay, HitBoxGameBoxUpdate};

    Adafruit_Init();
    fillScreen(BLACK);

    /*
    int accX, accY;
    while (1) {
        grabAccelerometerData(&accX, &accY);
        oledBall.update(&oledBall, accX, accY);

        sleep(10);
    } */

    hitBoxGame.play(&hitBoxGame);
}

