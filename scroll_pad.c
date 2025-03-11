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
    sPad->power = SCROLL_PAD_INITIAL_POWER;
    sPad->update = ScrollPadUpdate;

    return sPad;
}
