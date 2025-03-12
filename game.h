#ifndef __GAME_H__
#define __GAME_H__

#include "dot_tracker.h"
#include "scroll_pad.h"
//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

// OLED Configuration
#define WINDOW_WIDTH 128
#define WINDOW_HEIGHT 128

// UART Config
#define UART_SIGNAL_DELAY 200

#define WIN_CON  -1
#define LOSE_CON -2


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
#define BALL_SPEED 0.01

#define HIT_BOX_GOAL_SIZE 4
#define HIT_BOX_COLOR GREEN
#define HIT_BOX_BG    BLACK
#define HIT_SCORE_COLOR BLUE

// CANNON FIRE CONFIGURATION
#define CANNON_RELOAD_TIME 100
#define CANNON_MAX_NUM_SHOTS 10
#define CANNON_SIZE 1

// *********************************************************** //
// Useful Structures
struct BattlePongGame {
    struct Vector2DI boxPos;
    struct Vector2DI boxSize;
    struct OledBall* dotBall;
    struct ScrollPad* sPad;
    struct PongBall* pBall;

    struct OledBall* cannonShots;
    int numShots;

    unsigned int bgColor;
    int score;
    int winCondition;

    int redrawCount;

    int cannonTrigger;
    int cannonBuffer;

    int upgradePowerTrigger;
    int upgradePowerBuffer;

    void (*play)(struct BattlePongGame*);
    void (*collisionDetection)(struct BattlePongGame*);
    void (*fireHandler)(struct BattlePongGame*);
    void (*upgradeHandler)(struct BattlePongGame*);
    void (*pongBallRecv)(struct BattlePongGame*, float, float, int);
    void (*cannonShotRecv)(struct BattlePongGame*, float, float, int);
};

struct PongBall {
    struct Vector2DF pos;
    struct Vector2DF velocity;

    unsigned int relativeSpeed;
    unsigned int color;
    unsigned int radius;

    void (*update)(struct PongBall*, struct BattlePongGame*);
};


//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern void BattlePongGamePlay(struct BattlePongGame* game);
extern void BattlePongGameDrawGoalBox(struct BattlePongGame* game);
extern void BattlePongGameBoxUpdate(struct BattlePongGame* game);
extern void BattlePongGameFireHandler(struct BattlePongGame* game);
extern void BattlePongGameUpgradeHandler(struct BattlePongGame* game);
extern struct BattlePongGame* CreateBattlePongGame();

extern void PongBallUpdate(struct PongBall* pBall, struct BattlePongGame* game);
extern void PongBallNeutral(struct PongBall* pBall, struct BattlePongGame* game);
extern void PongBallRecv(struct BattlePongGame* game, float recvVelX, float recvVelY, int recvPosX);
extern struct PongBall* CreatePongBallObject(int x, int y);

extern void CannonShotRecv(struct BattlePongGame* game, float recvVelX, float recvVelY, int recvPosX);
//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //__GAME_H__
