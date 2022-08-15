//#include "Vector.h"

#define CLASSIC

// PERFORMANCE OPTIONS

//#defineint WINDOW_WIDTH = 856;
//#defineint WINDOW_HEIGHT = 480;

#ifdef CLASSIC
#define RENDER_DIST 20
#else
#define RENDER_DIST 80
#endif


#define TEXTURE_RES 16

#ifdef CLASSIC
#define WORLD_SIZE 64
#else
#define WORLD_SIZE 512
#endif

#define WORLD_HEIGHT 64

// END OF PERFORMANCE OPTIONS


#define BLOCK_AIR 0
#define BLOCK_GRASS 1
#define BLOCK_DEFAULT_DIRT 2
#define BLOCK_STONE 4
#define BLOCK_BRICKS 5
#define BLOCK_WOOD 7
#define BLOCK_LEAVES 8
#define BLOCK_MIRROR 9

#define PLAYER_REACH 5.0f
#define FOV 90.0f

// COLORS

// S = Sun, A = Amb, Y = skY
/*
#define vec3 SC_DAY = vec3(1);
#define vec3 AC_DAY = vec3(0.5f, 0.5f, 0.5f);
#define vec3 YC_DAY = vec3(0.317f, 0.729f, 0.969f);

#define vec3 SC_TWILIGHT = vec3(1, 0.5f, 0.01f);
#define vec3 AC_TWILIGHT = vec3(0.6f, 0.5f, 0.5f);
#define vec3 YC_TWILIGHT = vec3(0.27f, 0.24f, 0.33f);

#define vec3 SC_NIGHT = vec3(0.3f, 0.3f, 0.5f);
#define vec3 AC_NIGHT = vec3(0.3f, 0.3f, 0.5f);
#define vec3 YC_NIGHT = vec3(0.004f, 0.004f, 0.008f);
 */

#define SCR_WIDTH_DEFAULT 214
#define SCR_HEIGHT_DEFAULT 120

#define SCR_DETAIL 2

#define SCR_WIDTH (SCR_WIDTH_DEFAULT * (float) (1 << SCR_DETAIL))
#define SCR_HEIGHT (SCR_HEIGHT_DEFAULT * (float) (1 << SCR_DETAIL))
// #define KEY_HANDLING
