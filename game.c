#include "stdio.h"
#include "stdlib.h"


#include "hw_types.h"
#include "hw_adc.h"
#include "hw_memmap.h"

#include "adc.h"
#include "pin.h"
#include "game.h"
#include "math.h"
#include "utils.h"

#include "oled/Adafruit_SSD1351.h"
#include "oled/Adafruit_GFX.h"
#include "oled/glcdfont.h"

#define NO_OF_SAMPLES 1

// Analog Read From JoyStick
float readJoyStickADC(int uiAdcInputPin) {
    unsigned int  uiChannel;
    unsigned int  uiIndex=0;
    unsigned long ulSample;
    unsigned long pulAdcSamples[4096];

#ifdef CC3200_ES_1_2_1
    //
    // Enable ADC clocks.###IMPORTANT###Need to be removed for PG 1.32
    //
    HWREG(GPRCM_BASE + GPRCM_O_ADC_CLK_CONFIG) = 0x00000043;
    HWREG(ADC_BASE + ADC_O_ADC_CTRL) = 0x00000004;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE0) = 0x00000100;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE1) = 0x0355AA00;
#endif
    PinTypeADC(uiAdcInputPin,PIN_MODE_255);

    uiChannel = ADC_CH_1;

    ADCTimerConfig(ADC_BASE,2^17);
    ADCTimerEnable(ADC_BASE);
    ADCEnable(ADC_BASE);
    ADCChannelEnable(ADC_BASE, uiChannel);

    while(uiIndex < 5)
    {
        if(ADCFIFOLvlGet(ADC_BASE, uiChannel))
        {
            ulSample = ADCFIFORead(ADC_BASE, uiChannel);
            pulAdcSamples[uiIndex++] = ulSample;
        }


    }

    ADCChannelDisable(ADC_BASE, uiChannel);

    uiIndex = 0;

    float adcTime = ((float)((pulAdcSamples[0] >> 0 ) & 0x3FF0))/4096;

    float normalizedAxis = ((float)(((pulAdcSamples[4]) & 0x3FF0))) / 4096;
    if (normalizedAxis > 3) {
        return 0;
    } else if (normalizedAxis < 0.5) {
        return 1;
    } else if (adcTime > 2) {
        return -1;
    }
    return 0;

}

// ************* Game Logic **************** \\
///////////////////////////////////////////////

void BattlePongGamePlay(struct BattlePongGame* game) {
    drawRect(game->boxPos.y, game->boxPos.x, game->boxSize.x, game->boxSize.y, HIT_BOX_COLOR);
    ScrollPadDraw(game->sPad);

    while (game->winCondition > 0) {
        game->dotBall->update(game->dotBall);
        game->update(game);
        game->sPad->update(game->sPad, game->bgColor);
        game->pBall->update(game->pBall, game);

        if (game->redrawCount == 100) {
            drawRect(game->boxPos.y, game->boxPos.x, game->boxSize.x, game->boxSize.y, HIT_BOX_COLOR);
            ScrollPadDraw(game->sPad);
        }
        game->redrawCount++;

        UtilsDelay(20000);
    }

    // End Game Logic

    if (game->winCondition == WIN_CON) {
        printf("Game Won!");
        setCursor(20, 64);
        Outstr("Game Won!!!");
    } else if (game->winCondition == LOSE_CON) {
        printf("Lost Game");
        setCursor(20, 64);
        Outstr("Game Lost :(");
    }
}

// update the position of the rectangle by removing the old box and
// creating a new box in a random location
void BattlePongGameDrawGoalBox(struct BattlePongGame* game) {
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
    return;
}

// Collision detection of the ball with the box
void BattlePongGameBoxUpdate(struct BattlePongGame* game) {
    if (game->dotBall->pos.x >= (game->boxPos.x - 1) && game->dotBall->pos.y >= (game->boxPos.y - 1)
            && game->dotBall->pos.x < (game->boxPos.x + game->boxSize.x)
            && game->dotBall->pos.y < (game->boxPos.y + game->boxSize.y)) {

        BattlePongGameDrawGoalBox(game);
    }
}

struct BattlePongGame* CreateBattlePongGame() {
    struct BattlePongGame* game = (struct BattlePongGame*)malloc(sizeof(struct BattlePongGame));

    game->bgColor = BLACK;
    game->dotBall = CreateDotObject();
    game->sPad = CreateScrollPadObject();
    game->pBall = CreatePongBallObject(64, 16);

    game->boxPos.x = 64;
    game->boxPos.y = 64;
    game->boxSize.x = HIT_BOX_GOAL_SIZE;
    game->boxSize.y = HIT_BOX_GOAL_SIZE;
    game->score = 0;
    game->winCondition = 1;

    game->redrawCount = 0;

    game->play = BattlePongGamePlay;
    game->update = BattlePongGameBoxUpdate;

    return game;
}

// ************* Scroll Pad **************** \\
///////////////////////////////////////////////

// Scroll Pad Movement and draw
void ScrollPadUpdate(struct ScrollPad* sPad, unsigned int bgColor) {
    float joyAccX = readJoyStickADC(PIN_58);
    int xDiff = sPad->size / 2;
    int oldCenterX = round(sPad->pos.x);
    sPad->pos.x += (joyAccX * sPad->speed);
    sPad->pos.x = fmax(fmin(sPad->pos.x, 128 - xDiff), xDiff);
    int newCenterX = round(sPad->pos.x);

    int posDiff = newCenterX - oldCenterX;

    /*
    if (posDiff < 0) {
        drawPixel(oldCenterX + xDiff + 1, sPad->pos.y, sPad->bgColor);
    } else if (posDiff > 0) {
        drawPixel(oldCenterX - xDiff - 1, sPad->pos.y, sPad->bgColor);
    }*/

    while (posDiff != 0) {
        if (posDiff < 0) {
            drawPixel(oldCenterX + xDiff + 1, sPad->pos.y, bgColor);
            drawPixel(newCenterX - xDiff, sPad->pos.y, sPad->color);
            posDiff++;
        } else {
            drawPixel(oldCenterX - xDiff - 1, sPad->pos.y, bgColor);
            drawPixel(newCenterX + xDiff, sPad->pos.y, sPad->color);
            posDiff--;
        }
    }

}

void ScrollPadDraw(struct ScrollPad* sPad) {
    int i = 0;
    int posX = round(sPad->pos.x);
    int xDiff = sPad->size / 2;
    for (i = posX - xDiff; i < posX + xDiff; i++) {
        drawPixel(i, sPad->pos.y, sPad->color);
    }
}

struct ScrollPad* CreateScrollPadObject() {
    struct ScrollPad* sPad = (struct ScrollPad*)malloc(sizeof(struct ScrollPad));
    sPad->color = BLUE;
    sPad->pos.x = 64;
    sPad->pos.y = SCROLL_PAD_BASE_Y;
    sPad->size = SCROLL_PAD_INITIAL_SIZE;
    sPad->speed = 0.1;
    sPad->update = ScrollPadUpdate;

    return sPad;
}

// ************* Pong Ball **************** \\
//////////////////////////////////////////////

void PongBallUpdate(struct PongBall* pBall, struct BattlePongGame* game) {
    int oldX, oldY;
    oldX = round(pBall->pos.x);
    oldY = round(pBall->pos.y);

    drawCircle(oldX, oldY, pBall->radius, game->bgColor);

    pBall->pos.x += pBall->velocity.x;
    pBall->pos.y += pBall->velocity.y;

    if (pBall->pos.x < pBall->radius) {
        pBall->velocity.x *= -1;
        pBall->pos.x = pBall->radius;
    } else if (pBall->pos.x > (WINDOW_WIDTH - pBall->radius)) {
        pBall->velocity.x *= -1;
        pBall->pos.x = WINDOW_WIDTH - pBall->radius;
    }

    // Collision Detection
    if (pBall->pos.y > (game->sPad->pos.y - pBall->radius)) {
        int boardDiff = game->sPad->size / 2;
        if (pBall->pos.x <= game->sPad->pos.x + boardDiff && pBall->pos.x >= game->sPad->pos.x - boardDiff) {
            pBall->velocity.y *= -1;
            pBall->pos.y = abs(pBall->pos.y) % (WINDOW_WIDTH - pBall->radius);
            game->redrawCount = 0;
        } else {
            game->winCondition = LOSE_CON;
        }
    } else if (pBall->pos.y < pBall->radius) {
         // ************************************************ \\
        // ADD AWS CONFIGURATION HERE WHEN BALL LEAVES SCREEN \\

        pBall->velocity.y *= -1;
        pBall->pos.y = pBall->radius;
    }

    drawCircle(round(pBall->pos.x), round(pBall->pos.y), pBall->radius, pBall->color);
}

struct PongBall* CreatePongBallObject(int x, int y) {
    struct PongBall* pBall = (struct PongBall*)malloc(sizeof(struct PongBall));
    pBall->pos.x = x;
    pBall->pos.y = y;
    pBall->velocity.x = 0.1;
    pBall->velocity.y = 0.1;

    pBall->color = MAGENTA;
    pBall->radius = 1;

    pBall->update = PongBallUpdate;

    return pBall;
}

