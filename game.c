#include "stdio.h"
#include "stdlib.h"
#include "time.h"

#include "hw_types.h"
#include "hw_memmap.h"

#include "pin.h"
#include "game.h"
#include "math.h"
#include "utils.h"
#include "i2c_if.h"
#include "uart.h"
#include "string.h"

#include "systick.h"

#include "oled/Adafruit_SSD1351.h"
#include "oled/Adafruit_GFX.h"
#include "oled/glcdfont.h"


// Function Prototypes
void PongBallPadBounceMechanic(struct PongBall* pBall, struct ScrollPad* sPad);
void CreateCannonShot(struct BattlePongGame* game);
void DrawScore(int score);

void grabAccelerometerZ(int *z) {
    unsigned char accelerometerData[64];
    unsigned char accRegOffset = 0x7;

    I2C_IF_Write(0x18, &accRegOffset, 1, 0);
    I2C_IF_Read(0x18, &accelerometerData[0], 1);

    *z = accelerometerData[0];

    // Normalize z tilt
    if (*z > 127) *z -= 256;
}

void cmdParser(char* cmd, struct BattlePongGame* game) {
    char cmdType[4];
    int velX, velY, posX;
    if (sscanf(cmd, "%s %d %d %d", cmdType, &velX, &velY, &posX) == 4) {
        if (strcmp(cmdType, "BAL") == 0) {
            game->pongBallRecv(game, velX/100.f, velY/100.f, posX);
        } else if (strcmp(cmdType, "CAN") == 0) {
            game->cannonShotRecv(game, velX/100.f, velY/100.f, posX);
        } else if (strcmp(cmdType, "WIN") == 0) {
            game->winCondition = WIN_CON;
        }
    }
}

void sendUARTPongCommand(float velX, float velY, float posX) {
    char sendPongCmd[64];
    int size = sprintf(sendPongCmd, "BAL %d %d %d\n",
                       (int)lround(velX * 100.0),
                       (int)lround(velY * 100.0),
                       (int)lround(posX));
    int j;
    //printf("Sending %s\n", sendPongCmd);
    UARTIntDisable(UARTA1_BASE, UART_INT_RX);
    for (j = 0; j < size; j++) {
        UARTCharPut(UARTA1_BASE, sendPongCmd[j]);
        UtilsDelay(UART_SIGNAL_DELAY);
    }
    UARTIntEnable(UARTA1_BASE, UART_INT_RX);
}

void sendUARTCannonCommand(float velX, float velY, float posX) {
    // Cannon Shot Send Logic
    char sendPongCmd[64];
    int size = sprintf(sendPongCmd, "CAN %d %d %d\n",
                       (int)lround(velX * 100.0),
                       (int)lround(velY * 100.0),
                       (int)lround(posX));
    int j;
    //printf("Sending %s\n", sendPongCmd);
    UARTIntDisable(UARTA1_BASE, UART_INT_RX);
    for (j = 0; j < size; j++) {
        UARTCharPut(UARTA1_BASE, sendPongCmd[j]);
        UtilsDelay(UART_SIGNAL_DELAY);
    }
    UARTIntEnable(UARTA1_BASE, UART_INT_RX);
}

void sendUARTWinCommand() {
    char sendWinCmd[64];
    strcpy(sendWinCmd, "WIN 0 0 0\n");
    int j;
    UARTIntDisable(UARTA1_BASE, UART_INT_RX);
    for (j = 0; j < 10; j++) {
        UARTCharPut(UARTA1_BASE, sendWinCmd[j]);
        UtilsDelay(UART_SIGNAL_DELAY);
    }
    UARTIntEnable(UARTA1_BASE, UART_INT_RX);
}

// ************* Game Logic **************** \\
///////////////////////////////////////////////

void BattlePongGamePlay(struct BattlePongGame* game) {
    drawRect(game->boxPos.y, game->boxPos.x, game->boxSize.x, game->boxSize.y, HIT_BOX_COLOR);
    DrawScore(game->score);
    ScrollPadDraw(game->sPad);

    while (game->winCondition > 0) {
        game->dotBall->update(game->dotBall);
        game->collisionDetection(game);
        game->sPad->update(game->sPad, game->bgColor);
        game->pBall->update(game->pBall, game);
        game->fireHandler(game);
        game->upgradeHandler(game);

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
        sendUARTWinCommand();
        printf("Lost Game");
        setCursor(20, 64);
        Outstr("Game Lost :(");
    }
}

// Draws the score on the screen
void DrawScore(int score) {
    setCursor(4, 4);
    Outstr("Points: ");

    char scr[20];
    sprintf(scr, "%d", score);
    fillRoundRect(40, 4, 40, 10, 0, HIT_BOX_BG);
    setCursor(40, 4);
    Outstr(scr);
}

// update the position of the rectangle by removing the old box and
// creating a new box in a random location
void BattlePongGameDrawGoalBox(struct BattlePongGame* game) {
    drawRect(game->boxPos.y, game->boxPos.x, game->boxSize.y, game->boxSize.x, HIT_BOX_BG);

    // Print Score to the screen
    DrawScore(game->score);

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
        game->score++;
        BattlePongGameDrawGoalBox(game);
    }
}

void swapOledBalls(struct OledBall shots[], int pos1, int pos2) {
    struct OledBall temp = shots[pos1];
    shots[pos1] = shots[pos2];
    shots[pos2] = temp;
}

void BattlePongGameFireHandler(struct BattlePongGame* game) {
    int accZ;
    grabAccelerometerZ(&accZ);

    if (accZ < -50) {
        if (game->cannonBuffer > CANNON_RELOAD_TIME && game->numShots < CANNON_MAX_NUM_SHOTS && game->score >= CANNON_SHOT_COST) {
            // Spawn the cannon fire
            CreateCannonShot(game);
            game->cannonBuffer = 0;
            game->score -= CANNON_SHOT_COST;
        }
    }

    // Handle drawing each cannon shot
    int i;
    for (i = 0; i < game->numShots; i++) {
        game->cannonShots[i].update(&(game->cannonShots[i]));
        if (game->cannonShots[i].pos.y < 0) {
            struct OledBall* ball1Ptr = &game->cannonShots[i];
            drawCircle(round(ball1Ptr->pos.x), round(ball1Ptr->pos.y), CANNON_SIZE, game->bgColor);
            game->numShots--;

            sendUARTCannonCommand(ball1Ptr->velocity.x, ball1Ptr->velocity.y, ball1Ptr->pos.x);

            if (game->numShots != i) {
                swapOledBalls(game->cannonShots, i, game->numShots);
            }
            i--;
        } else if (game->cannonShots[i].pos.y >= game->sPad->pos.y) {
            int sPadMinX = round(game->sPad->pos.x) - (game->sPad->size/2);
            int sPadMaxX = round(game->sPad->pos.x) + (game->sPad->size/2);
            int cannonX = game->cannonShots[i].pos.x;
            if (cannonX > sPadMinX && cannonX < sPadMaxX) {
                game->winCondition = LOSE_CON;
            } else {
                struct OledBall* ball1Ptr = &game->cannonShots[i];
                drawCircle(round(ball1Ptr->pos.x), round(ball1Ptr->pos.y), CANNON_SIZE, game->bgColor);
                game->numShots--;
                if (game->numShots != i) {
                    swapOledBalls(game->cannonShots, i, game->numShots);
                }
                i--;
            }
        }
    }

    game->cannonBuffer++;
}

void BattlePongGameUpgradeHandler(struct BattlePongGame* game) {
    if (game->upgradePowerTrigger) {
        game->upgradePowerTrigger = false;

        if (game->upgradePowerBuffer > (CANNON_RELOAD_TIME / 2) && game->score >= SCROLL_PAD_POWER_UPGRADE_COST) {
            game->upgradePowerBuffer = 0;
            game->sPad->power += SCROLL_PAD_POWER_UPGRADE_INCR;
            game->score -= SCROLL_PAD_POWER_UPGRADE_COST;

            DrawScore(game->score);
        }
    }

    game->upgradePowerBuffer++;
}

struct BattlePongGame* CreateBattlePongGame(int playerNum) {
    struct BattlePongGame* game = (struct BattlePongGame*)malloc(sizeof(struct BattlePongGame));

    game->bgColor = BLACK;
    game->dotBall = CreateDotObject();
    game->sPad = CreateScrollPadObject();
    game->pBall = CreatePongBallObject(64, 16);
    game->cmdIdx = 0;

    game->cmdRecvBuffer = (char *)malloc(sizeof(char) * 64);

    game->cannonShots = (struct OledBall*)malloc(sizeof(struct OledBall) * CANNON_MAX_NUM_SHOTS);
    game->numShots = 0;

    game->boxPos.x = (rand() % 96) + 8;
    game->boxPos.y = (rand() % 96) + 8;
    game->boxSize.x = HIT_BOX_GOAL_SIZE;
    game->boxSize.y = HIT_BOX_GOAL_SIZE;
    game->score = 0;
    game->winCondition = 1;

    game->redrawCount = 0;

    game->cannonTrigger = false;
    game->cannonBuffer = 0;

    game->upgradePowerTrigger = false;
    game->upgradePowerBuffer = 0;

    game->play = BattlePongGamePlay;
    game->collisionDetection = BattlePongGameBoxUpdate;
    game->fireHandler = BattlePongGameFireHandler;
    game->upgradeHandler = BattlePongGameUpgradeHandler;
    game->pongBallRecv = PongBallRecv;
    game->cannonShotRecv = CannonShotRecv;

    if (playerNum == 1) {
        game->pBall->velocity.y = 0.f;
        game->pBall->pos.y = -game->pBall->radius;
    }

    return game;
}

// ************* Pong Ball **************** \\
//////////////////////////////////////////////

void PongBallRecv(struct BattlePongGame* game, float recvVelX, float recvVelY, int recvPosX) {
    //printf("Pong ball vel %f %f pos %d\n", recvVelX, recvVelY, recvPosX);

    game->pBall->radius = 1;
    game->pBall->pos.x = abs(recvPosX - WINDOW_WIDTH);
    game->pBall->pos.y = game->pBall->radius;
    game->pBall->velocity.x = -recvVelX;
    game->pBall->velocity.y = -recvVelY;

    //printf("Velocity: %f\n", game->pBall->velocity.y);

    game->pBall->relativeSpeed = INITIAL_RELATIVE_SPEED;
    game->pBall->color = MAGENTA;
}

void PongBallUpdate(struct PongBall* pBall, struct BattlePongGame* game) {
    if (game->cmdIdx > 0) {
        cmdParser(game->cmdRecvBuffer, game);
        game->cmdIdx = 0;
    }
    if (pBall->velocity.y == 0.f) {
        return;
    }
    int oldX, oldY;
    oldX = round(pBall->pos.x);
    oldY = round(pBall->pos.y);

    //drawCircle(oldX, oldY, pBall->radius, game->bgColor);

    drawPixel(oldX + 1, oldY, game->bgColor);
    drawPixel(oldX - 1, oldY, game->bgColor);
    drawPixel(oldX, oldY + 1, game->bgColor);
    drawPixel(oldX, oldY - 1, game->bgColor);

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
        if (pBall->pos.x <= (game->sPad->pos.x + (boardDiff + pBall->radius)) && pBall->pos.x >= (game->sPad->pos.x - (boardDiff + pBall->radius))) {
            PongBallPadBounceMechanic(pBall, game->sPad);
            game->score++;
            game->redrawCount = 0;
        } else {
            //pBall->velocity.y *= -1;
            //pBall->pos.y = game->sPad->pos.y - pBall->radius;
            game->winCondition = LOSE_CON;
        }
    } else if (pBall->pos.y < pBall->radius) {
         // ************************************************ \\
        // ADD AWS CONFIGURATION HERE WHEN BALL LEAVES SCREEN \\

        sendUARTPongCommand(pBall->velocity.x, pBall->velocity.y, pBall->pos.x);
        pBall->velocity.y = 0;
        pBall->pos.y = - pBall->radius;
        //pBall->velocity.y = 0.2;
        //pBall->pos.y = pBall->radius;
    }

    //drawCircle(round(pBall->pos.x), round(pBall->pos.y), pBall->radius, pBall->color);
    drawPixel(round(pBall->pos.x) + 1, round(pBall->pos.y), pBall->color);
    drawPixel(round(pBall->pos.x) - 1, round(pBall->pos.y), pBall->color);
    drawPixel(round(pBall->pos.x), round(pBall->pos.y) + 1, pBall->color);
    drawPixel(round(pBall->pos.x), round(pBall->pos.y) - 1, pBall->color);
}


// After collision bounces the pong ball back
void PongBallPadBounceMechanic(struct PongBall* pBall, struct ScrollPad* sPad) {
    double relativeX = pBall->pos.x - sPad->pos.x;
    double opp = sPad->size / 4;
    double angle = atan(opp/relativeX);

    pBall->velocity.y = - (pBall->relativeSpeed * sPad->power * sin(fabs(angle)));
    pBall->velocity.x = (pBall->relativeSpeed * sPad->power * cos(angle)) * (relativeX < 0 ? -1 : 1);

    pBall->pos.y = abs(pBall->pos.y) % (WINDOW_WIDTH - pBall->radius);
}

struct PongBall* CreatePongBallObject(int x, int y) {
    struct PongBall* pBall = (struct PongBall*)malloc(sizeof(struct PongBall));
    pBall->pos.x = x;
    pBall->pos.y = y;
    pBall->velocity.x = 0.1;
    pBall->velocity.y = 0.1;

    pBall->relativeSpeed = INITIAL_RELATIVE_SPEED;
    pBall->color = MAGENTA;
    pBall->radius = 1;

    pBall->update = PongBallUpdate;

    return pBall;
}

// Cannon Shot Logic
void CannonShotUpdate(struct OledBall* shot) {
    int oldX = round(shot->pos.x);
    int oldY = round(shot->pos.y);

    //drawCircle(oldX, oldY, CANNON_SIZE, shot->bgColor);

    drawPixel(oldX + 1, oldY, shot->bgColor);
    drawPixel(oldX - 1, oldY, shot->bgColor);
    drawPixel(oldX, oldY + 1, shot->bgColor);
    drawPixel(oldX, oldY - 1, shot->bgColor);



    shot->pos.y += shot->velocity.y;
    shot->pos.x += shot->velocity.x;

    int newX = round(shot->pos.x);
    int newY = round(shot->pos.y);

    drawPixel(newX + 1, newY, shot->color);
    drawPixel(newX - 1, newY, shot->color);
    drawPixel(newX, newY + 1, shot->color);
    drawPixel(newX, newY - 1, shot->color);

    if (shot->velocity.y < 0) {
        drawPixel(oldX, oldY + 3, shot->bgColor);
        drawPixel(newX, newY + 1, shot->color);
    } else if (shot->velocity.y > 0) {
        drawPixel(oldX, oldY - 3, shot->bgColor);
        drawPixel(newX, newY - 1, shot->color);
    }
}

void CannonShotRecv(struct BattlePongGame* game, float recvVelX, float recvVelY, int recvPosX) {
    struct OledBall* cannonShot = game->cannonShots;

    cannonShot[game->numShots].color = RED;
    cannonShot[game->numShots].bgColor = game->bgColor;
    cannonShot[game->numShots].pos.x = abs(recvPosX - WINDOW_WIDTH);
    cannonShot[game->numShots].pos.y = game->pBall->radius;
    cannonShot[game->numShots].velocity.x = -recvVelX;
    cannonShot[game->numShots].velocity.y = -recvVelY;
    cannonShot[game->numShots].update = CannonShotUpdate;

    game->numShots++;
}


void CreateCannonShot(struct BattlePongGame* game) {
    struct OledBall* cannonShot = game->cannonShots;

    cannonShot[game->numShots].color = RED;
    cannonShot[game->numShots].bgColor = game->bgColor;
    cannonShot[game->numShots].pos.x = fmin(fmax(round(game->dotBall->pos.y), 4), 124);
    cannonShot[game->numShots].pos.y = game->dotBall->pos.x;
    cannonShot[game->numShots].velocity.x = 0;
    cannonShot[game->numShots].velocity.y = -0.2;
    cannonShot[game->numShots].update = CannonShotUpdate;

    game->numShots++;
}

