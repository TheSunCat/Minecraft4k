#define SCR_WIDTH_DEFAULT 214
#define SCR_HEIGHT_DEFAULT 120

// PERFORMANCE OPTIONS

#define RENDER_DIST 20
#define TEXTURE_RES 16

#define WORLD_SIZE 64
#define WORLD_HEIGHT 64

#define WORLD_WRAP

#define SCR_DETAIL 2

// END OF PERFORMANCE OPTIONS

#define X 0
#define Y 1
#define Z 2

#define BLOCK_AIR 0
#define BLOCK_GRASS 1
#define BLOCK_DIRT 2
#define BLOCK_STONE 3
#define BLOCK_BRICKS 4
#define BLOCK_WOOD 5
#define BLOCK_LEAVES 6

#define PLAYER_REACH 5.0f
#define FOV 90.0f

#define SHADER_UNIFORM_c 0
#define SHADER_UNIFORM_r 1
#define SHADER_UNIFORM_P 2
#define SHADER_UNIFORM_W 3
#define SHADER_UNIFORM_T 4
#define SHADER_UNIFORM_b 5

// from https://iquilezles.org/articles/float4k/
// better compressible floats
#define p0d003 0.00299072265625f  // 0.003    0x3b440000
#define p0d02 0.0200195313f       // 0.02f    0x3ca40000
#define p0d10 0.1000976563f       // 0.10f    0x3dcd0000
#define p0d30 0.3007812500f       // 0.30f    0x3e9a0000
#define p0d60 0.6015625000f       // 0.60f    0x3f1a0000
#define p0d65 0.6484375000f       // 0.65f    0x3f260000
#define p0d80 0.8007812500f       // 0.80f    0x3f4d0000
#define p0d99 0.9882812500f       // 0.99f    0x3f7d0000

#define HALF_PI 1.5703125f  // 1.5703125 0x3fc90000
