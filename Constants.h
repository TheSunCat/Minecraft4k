#define SCR_WIDTH_DEFAULT 214
#define SCR_HEIGHT_DEFAULT 120

// PERFORMANCE OPTIONS

#define RENDER_DIST 20
#define TEXTURE_RES 16

#define WORLD_SIZE 64
#define WORLD_HEIGHT 64

#define SCR_DETAIL 2

// END OF PERFORMANCE OPTIONS


#define BLOCK_AIR 0
#define BLOCK_GRASS 1
#define BLOCK_DEFAULT_DIRT 2
#define BLOCK_STONE 4
#define BLOCK_BRICKS 5
#define BLOCK_WOOD 7
#define BLOCK_LEAVES 8

#define PLAYER_REACH 5.0f
#define FOV 90.0f

#define SCR_WIDTH (SCR_WIDTH_DEFAULT * (float) (1 << SCR_DETAIL))
#define SCR_HEIGHT (SCR_HEIGHT_DEFAULT * (float) (1 << SCR_DETAIL))
