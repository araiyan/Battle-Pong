#ifndef __GAME_H__
#define __GAME_H__

#include "dot_tracker.h"
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

// *** GLOBAL VARIABLES FOR DOT *** \\\

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



// *********************************************************** //
// Useful Structures
struct HitBoxGame {
    struct Vector2DI boxPos;
    struct Vector2DI boxSize;
    struct OledBall* ball;

    int score;

    void (*play)(struct HitBoxGame*);
    void (*update)(struct HitBoxGame*);
};


//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern void HitBoxGamePlay(struct HitBoxGame* game);
extern void HitBoxGameDrawGoalBox(struct HitBoxGame* game);
extern void HitBoxGameBoxUpdate(struct HitBoxGame* game);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //__GAME_H__
