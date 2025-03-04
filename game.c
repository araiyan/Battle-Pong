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
    float axisValue;
    if (normalizedAxis > 3) {
        axisValue = 0;
    } else if (adcTime < 2) {
        axisValue = ((4 - normalizedAxis) / 4);
    } else {
        axisValue = ((4 - normalizedAxis) * -1) + 1;
    }

    return axisValue;
}



// *********************************************************** //
// Game Logic

void HitBoxGamePlay(struct HitBoxGame* game) {
    drawRect(game->boxPos.x, game->boxPos.y, game->boxSize.x, game->boxSize.y, HIT_BOX_COLOR);
    while (1) {
        game->ball->update(game->ball);
        game->update(game);
        float joystickX = readJoyStickADC(PIN_58);
        printf("X: %f \n", joystickX);
        UtilsDelay(10000);
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
    return;
}

// Collision detection of the ball with the box
void HitBoxGameBoxUpdate(struct HitBoxGame* game) {
    if (game->ball->pos.x >= (game->boxPos.x - 1) && game->ball->pos.y >= (game->boxPos.y - 1)
            && game->ball->pos.x < (game->boxPos.x + game->boxSize.x)
            && game->ball->pos.y < (game->boxPos.y + game->boxSize.y)) {

        HitBoxGameDrawGoalBox(game);
    }
}
