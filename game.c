#include "stdio.h"
#include "stdlib.h"

#include "hw_types.h"
#include "hw_memmap.h"

#include "pin.h"
#include "game.h"
#include "math.h"
#include "utils.h"

#include "oled/Adafruit_SSD1351.h"
#include "oled/Adafruit_GFX.h"
#include "oled/glcdfont.h"


// Function Prototypes
void PongBallPadBounceMechanic(struct PongBall* pBall, struct ScrollPad* sPad);

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
        if (pBall->pos.x <= (game->sPad->pos.x + (boardDiff + pBall->radius)) && pBall->pos.x >= (game->sPad->pos.x - (boardDiff + pBall->radius))) {
            PongBallPadBounceMechanic(pBall, game->sPad);
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

    pBall->relativeSpeed = 2;
    pBall->color = MAGENTA;
    pBall->radius = 1;

    pBall->update = PongBallUpdate;

    return pBall;
}

