#include "stdio.h"
#include "stdlib.h"

#include "dot_tracker.h"
#include "i2c_if.h"
#include "math.h"
#include "utils.h"

#include "oled/Adafruit_SSD1351.h"
#include "oled/Adafruit_GFX.h"
#include "oled/glcdfont.h"


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
// Ball Logic


void ballUpdate(struct OledBall* ball) {
    int prevX = round(ball->pos.x);
    int prevY = round(ball->pos.y);

    int accX, accY;
    grabAccelerometerData(&accX, &accY);

    ball->velocity.x += accX * BALL_SPEED;
    ball->velocity.y += accY * BALL_SPEED;

    ball->pos.x += ball->velocity.x * BALL_SPEED;
    ball->pos.y += ball->velocity.y * BALL_SPEED;

    // Border control;
    if (ball->pos.x > 127) {
        ball->pos.x = 127;
        ball->velocity.x = 0.7 * ball->velocity.x * -1;
    } else if (ball->pos.x < 1) {
        ball->pos.x = 1;
        ball->velocity.x = 0.7 * ball->velocity.x * -1;
    }

    if (ball->pos.y > 127) {
        ball->pos.y = 127;
        ball->velocity.y = 0.7 * ball->velocity.y * -1;
    } else if (ball->pos.y < 1) {
        ball->pos.y = 1;
        ball->velocity.y = 0.7 * ball->velocity.y * -1;
    }

    //ball->pos.x = fmax(fmin(ball->pos.x, 127), 1);
    //ball->pos.y = fmax(fmin(ball->pos.y, 127), 1);


    int newX = round(ball->pos.x);
    int newY = round(ball->pos.y);

    if (prevX != newX || prevY != newY) {
        drawPixel(prevY, prevX, ball->bgColor);
        drawPixel(newY, newX, ball->color);
    }
}

// *********************************************************** //
// Game Logic

void HitBoxGamePlay(struct HitBoxGame* game) {
    int accX, accY;

    drawRect(game->boxPos.x, game->boxPos.y, game->boxSize.x, game->boxSize.y, HIT_BOX_COLOR);
    while (1) {
        grabAccelerometerData(&accX, &accY);
        game->ball->update(game->ball);
        game->update(game);

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

struct OledBall* CreateDotObject(){
    struct OledBall* oledBall = (struct OledBall*)malloc(sizeof(struct OledBall));
    oledBall->pos.x = 64;
    oledBall->pos.y = 64;
    oledBall->velocity.x = 0;
    oledBall->velocity.y = 0;
    oledBall->color = WHITE;
    oledBall->bgColor = BLACK;
    oledBall->update = ballUpdate;

    return oledBall;
}
