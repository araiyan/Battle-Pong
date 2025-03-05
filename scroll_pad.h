#ifndef __SCROLL_PAD_H__
#define __SCROLL_PAD_H__


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

// Scroll Pad Configuration
#define SCROLL_PAD_BASE_Y 124
#define SCROLL_PAD_INITIAL_SIZE 12

// Scroll Pad Struct
struct ScrollPad {
    struct Vector2DF pos;

    int size;
    float speed;
    float power;
    unsigned int color;

    void (*update)(struct ScrollPad*, unsigned int);
};

//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern void ScrollPadUpdate(struct ScrollPad* sPad, unsigned int bgColor);
extern void ScrollPadDraw(struct ScrollPad* sPad);
extern struct ScrollPad* CreateScrollPadObject();

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //__SCROLL_PAD_H__
