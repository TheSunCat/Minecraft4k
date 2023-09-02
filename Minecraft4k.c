#define GL_GLEXT_PROTOTYPES

#include <dlfcn.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include "shader.h"

#include "Constants.h"

// functions we will use

// SDL2
typedef SDL_Window*(*SDL_CreateWindow_t)(const char*, int, int, int, int, Uint32);
typedef SDL_GLContext(*SDL_GL_CreateContext_t)(SDL_Window*);
typedef int(*SDL_SetRelativeMouseMode_t)(SDL_bool);
typedef int(*SDL_PollEvent_t)(SDL_Event*);
typedef void(*SDL_GL_SwapWindow_t)(SDL_Window*);
typedef Uint64(*SDL_GetTicks64_t)(void);
typedef const Uint8*(*SDL_GetKeyboardState_t)(int*);

static SDL_GetTicks64_t sym_SDL_GetTicks64;
static SDL_GetKeyboardState_t sym_SDL_GetKeyboardState;

typedef int(*rand_t)(void);
static rand_t sym_rand;

// OpenGL IDs
static GLuint shader;
static GLuint worldTex;
static GLuint textureAtlasTex;

static uint64_t currentTime()
{
    return sym_SDL_GetTicks64();
}

static const int X = 0, Y = 1, Z = 2;

// size: 17
// TODO tune this, or use inline x86 ASM
#define TRIG_PRECISION 20
static float my_sin(float x)
{
//    return sinf(x); // looks like this is slightly smaller for now, welp
    
    float t = x;
    float sine = x;
    for (int a=1; a < TRIG_PRECISION; ++a)
    {
        float mult = -x*x/((2*a+1)*(2*a));
        t *= mult;
        sine += t;
    }
    return sine;
}

static float my_cos(float x)
{
    return my_sin(x + M_PI / 2.0f);
}

static float clamp(float x, float min, float max)
{
    if(x < min)
        return min;
    if(x > max)
        return max;
    return x;
}

static uint8_t world[WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE];

// hacky workaround: remember block placed this frame so we can delete if colliding with
static int blockSetThisFrame = -1;
// size: 134!!
static void setBlock(int x, int y, int z, uint8_t block)
{
    blockSetThisFrame = y + x * WORLD_SIZE + z * WORLD_SIZE * WORLD_HEIGHT;

    world[blockSetThisFrame] = block;

    glBindTexture(GL_TEXTURE_3D, worldTex);
    glTexSubImage3D(GL_TEXTURE_3D,              // target
        0,                                      // level
        y, x, z,                                // offset
        1, 1, 1,                                // size
        GL_RED,                                 // format
        GL_UNSIGNED_BYTE,                       // type
        &block);                                // data
}

// size: 22
static uint8_t getBlock(int x, int y, int z)
{
    return world[y + x * WORLD_SIZE + z * WORLD_SIZE * WORLD_HEIGHT];
}

// size: 23
static int isWithinWorld(int x, int y, int z)
{
    return x >= 0.0f && y >= 0.0f && z >= 0.0f &&
           x < WORLD_SIZE && y < WORLD_HEIGHT && z < WORLD_SIZE;
}

static int hoverBlockX = -100, hoverBlockY = -100, hoverBlockZ = -100;
static int placeBlockX = -100, placeBlockY = -100, placeBlockZ = -100;

// size: none
static void breakBlock()
{
    if(hoverBlockX == -100)
        return;

    setBlock(hoverBlockX, hoverBlockY, hoverBlockZ, BLOCK_AIR);
}

// size: none
static void placeBlock(uint8_t block)
{
    if(placeBlockX == -100)
        return;

    // TODO maybe shouldn't place if in player, but would require a whole collision check here
    setBlock(placeBlockX, placeBlockY, placeBlockZ, block);
}

static SDL_Window* window;
static int SCR_WIDTH = SCR_WIDTH_DEFAULT * (float) (1 << SCR_DETAIL);
static int SCR_HEIGHT = SCR_HEIGHT_DEFAULT * (float) (1 << SCR_DETAIL);

static float cameraPitch = 0;
static float cameraYaw = 0;

static float playerVelocityX = 0, playerVelocityY = 0, playerVelocityZ = 0;

// spawn player at world center
static float playerPosX = WORLD_SIZE / 2.0f + 0.5f, playerPosY = 1, playerPosZ = WORLD_SIZE / 2.0f + 0.5f;

// TODO fix bad

// ---------------
// BAD STARTS HERE
// ---------------

// size: 4
static float my_fract(float x)
{
    return x - (float)((int) x);
}

// size: 10
static int my_sign(float x)
{
    if(x < 0)
        return -1;
    return 1;
}

// size: 415!!
static void raycastWorld(float sinYaw, float cosYaw, float sinPitch, float cosPitch)
{
    // rotate frustum space to world space
    const float rayDirX = cosPitch * sinYaw;
    const float rayDirY = -sinPitch;
    const float rayDirZ = cosPitch * cosYaw;

    float i = (int)playerPosX;
    float j = (int)playerPosY;
    float k = (int)playerPosZ;

    const float iStep = my_sign(rayDirX);
    const float jStep = my_sign(rayDirY);
    const float kStep = my_sign(rayDirZ);

    const float vInvertedX = fabs(1/rayDirX);
    const float vInvertedY = fabs(1/rayDirY);
    const float vInvertedZ = fabs(1/rayDirZ);

    float distX = -my_fract(playerPosX) * iStep;
    float distY = -my_fract(playerPosY) * jStep;
    float distZ = -my_fract(playerPosZ) * kStep;

    distX += iStep == 1; distY += jStep == 1; distZ += kStep == 1;
    distX *= vInvertedX; distY *= vInvertedY; distZ *= vInvertedZ;

    int axis = X;

    float rayTravelDist = 0;
    while(rayTravelDist < PLAYER_REACH)
    {
        // exit check
        if(!isWithinWorld(i, j, k))
            break;

        int blockHit = getBlock(i, j, k);

        if(blockHit != 0) // BLOCK_AIR
        {
            hoverBlockX = i; hoverBlockY = j; hoverBlockZ = k;
            placeBlockX = i; placeBlockY = j; placeBlockZ = k;
            switch(axis) {
            case X:
                placeBlockX -= my_sign(rayDirX);
                break;
            case Y:
                placeBlockY -= my_sign(rayDirY);
                break;
            case Z:
                placeBlockZ -= my_sign(rayDirZ);
            }
            return;
        }

        // Determine the closest voxel boundary
        if (distY < distX)
        {
            if (distY < distZ)
            {
                axis = Y;
                // Advance to the closest voxel boundary in the Y direction
                // Increment the chunk-relative position and the block access position
                j += jStep;

                // Update our progress in the ray 
                rayTravelDist = distY;

                // Set the new distance to the next voxel Y boundary
                distY += vInvertedY;
            }
            else
            {
                axis = Z;

                k += kStep;

                rayTravelDist = distZ;
                distZ += vInvertedZ;
            }
        }
        else if (distX < distZ)
        {
            axis = X;

            i += iStep;

            rayTravelDist = distX;
            distX += vInvertedX;
        }
        else
        {
            axis = Z;

            k += kStep;

            rayTravelDist = distZ;
            distZ += vInvertedZ;
        }
    }

    // only X is checked
    hoverBlockX = -100; placeBlockX = -100;
}

// -------------
// BAD ENDS HERE
// -------------

static const uint8_t* kb = NULL;

// size: 505!!
static void updateController()
{
    kb = sym_SDL_GetKeyboardState(NULL);
}

static uint64_t lastFrameTime = 0;
static uint64_t lastUpdateTime;

// size: 1611
static void on_render()
{
    const uint64_t frameTime = currentTime();
    if(lastFrameTime == 0)
    {
        lastUpdateTime = frameTime;
    }

    lastFrameTime = frameTime;

    const float sinYaw = my_sin(cameraYaw);
    const float cosYaw = my_cos(cameraYaw);
    const float sinPitch = my_sin(cameraPitch);
    const float cosPitch = my_cos(cameraPitch);
 
    // update position for placing/destroying blocks
    raycastWorld(sinYaw, cosYaw, sinPitch, cosPitch);

    updateController();
  
    // calculate physics
    while (currentTime() - lastUpdateTime > 10)
    {
        const float inputX = (-kb[SDL_SCANCODE_A] + kb[SDL_SCANCODE_D]) * 0.02F;
        const float inputZ = (-kb[SDL_SCANCODE_S] + kb[SDL_SCANCODE_W]) * 0.02F;

        playerVelocityX *= 0.5F;
        playerVelocityY *= 0.99F;
        playerVelocityZ *= 0.5F;

        playerVelocityX += sinYaw * inputZ + cosYaw * inputX;
        playerVelocityZ += cosYaw * inputZ - sinYaw * inputX;
        playerVelocityY += 0.003F; // gravity

        // calculate collision per-axis
        for (int axis = 0; axis < 3; ++axis) {
            bool moveValid = true;

            const float newPlayerPosX = playerPosX + playerVelocityX * (axis == X);
            const float newPlayerPosY = playerPosY + playerVelocityY * (axis == Y);
            const float newPlayerPosZ = playerPosZ + playerVelocityZ * (axis == Z);

            for (int colliderIndex = 0; colliderIndex < 12; ++colliderIndex) {
                // magic
                const int colliderBlockPosX = newPlayerPosX + (colliderIndex   % 2) * 0.6f - 0.3f;
                const int colliderBlockPosY = newPlayerPosY + (colliderIndex/4 - 1) * 0.8f + 0.65f;
                const int colliderBlockPosZ = newPlayerPosZ + (colliderIndex/2 % 2) * 0.6f - 0.3f;

                if (colliderBlockPosY < 0) // ignore collision above the world height limit
                    continue;

                // hacky: delete block if it was placed this frame to prevent getting stuck
                const int colliderBlockIndex = colliderBlockPosY + colliderBlockPosX * WORLD_SIZE + colliderBlockPosZ * WORLD_SIZE * WORLD_HEIGHT;
                if(colliderBlockIndex == blockSetThisFrame)
                    setBlock(colliderBlockPosX, colliderBlockPosY, colliderBlockPosZ, BLOCK_AIR);

                // check collision with world bounds and blocks
                if (!isWithinWorld(colliderBlockPosX, colliderBlockPosY, colliderBlockPosZ)
                    || getBlock(colliderBlockPosX, colliderBlockPosY, colliderBlockPosZ) != BLOCK_AIR) {

                    if (axis == 1) // AXIS_Y
                    {
                        // if we're falling, colliding, and we press space
                        if ((kb[SDL_SCANCODE_SPACE]) && playerVelocityY > 0.0f) {

                            playerVelocityY = -0.1F; // jump
                        }
                        else { // we're on the ground, not jumping

                            playerVelocityY = 0.0f; // prevent accelerating downwards infinitely
                        }
                    }

                    moveValid = false;
                    break;
                }
            }

            if (moveValid) {
                playerPosX = newPlayerPosX;
                playerPosY = newPlayerPosY;
                playerPosZ = newPlayerPosZ;
            }
        }

        lastUpdateTime += 10;
    }

    // size: 333!!
    
    // Compute the raytracing!
    //glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, worldTex);
    //glUniform1i(glGetUniformLocation(shader, "W"), 0); // textures are set to 0 by default... this is cursed

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureAtlasTex);
    glUniform1i(glGetUniformLocation(shader, "T"), 1);

    glUniform2f(glGetUniformLocation(shader, "S"), SCR_WIDTH, SCR_HEIGHT);
    
    glUniform1f(glGetUniformLocation(shader, "c"), cosYaw);
    glUniform1f(glGetUniformLocation(shader, "d"), cosPitch);
    glUniform1f(glGetUniformLocation(shader, "e"), sinYaw);
    glUniform1f(glGetUniformLocation(shader, "g"), sinPitch);
    glUniform2f(glGetUniformLocation(shader, "r"), (SCR_WIDTH * FOV) / 214.f, (SCR_HEIGHT * FOV) / 120.f);
    glUniform3f(glGetUniformLocation(shader, "P"), playerPosX, playerPosY, playerPosZ);

    glUniform3i(glGetUniformLocation(shader, "b"), hoverBlockX, hoverBlockY, hoverBlockZ);

    // render!
    glRecti(-1,-1,1,1);

    blockSetThisFrame = -1;
}

// size: 154
static void generateWorld()
{
    const float maxTerrainHeight = WORLD_HEIGHT / 2.0f;

    //long long seed = 18295169L;

    for (int i = 0; i < WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE; ++i) {
        uint8_t block;

        int randInt = sym_rand() % 8;

        int y = i % (WORLD_SIZE);

        // TODO make dirt more common
        if (y > (maxTerrainHeight + randInt))
            block = (sym_rand() % 5) + 1;
        else
            block = BLOCK_AIR;

        world[i] = block;
    }
    
    // Upload world to GPU
    glGenTextures(1, &worldTex);
    glBindTexture(GL_TEXTURE_3D, worldTex);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage3D(GL_TEXTURE_3D,                 // target
        0,                                      // level
        GL_RED,                                 // internal format
        WORLD_SIZE, WORLD_HEIGHT, WORLD_SIZE,   // size
        0,                                      // border
        GL_RED,                                 // format
        GL_UNSIGNED_BYTE,                       // type
        world);                                 // pixels
}

static int textureAtlas[TEXTURE_RES * TEXTURE_RES * 3 * 7];

// size: 624!!
static void generateTextures()
{
    // set random seed to generate textures
    //long long seed = 151910774187927L;


    // procedurally generates the 8x3 textureAtlas
    // gsd = grayscale detail
    for (int blockID = 1; blockID < 7; ++blockID) {
        int gsd_tempA = 0xFF - (sym_rand() % 0x60);

        for (int y = 0; y < TEXTURE_RES * 3; ++y) {
            for (int x = 0; x < TEXTURE_RES; ++x) {
                // gets executed per pixel/texel

                if (blockID != BLOCK_STONE || (sym_rand() % 3) == 0) // if the block type is stone, update the noise value less often to get a stretched out look
                    gsd_tempA = 0xFF - (sym_rand() % 0x60);

                int tint = 0x966C4A; // brown (dirt)
                switch (blockID)
                {
                case BLOCK_STONE:
                {
                    tint = 0x7F7F7F; // grey
                    break;
                }
                case BLOCK_GRASS:
                {
                    if (y < ((x * x * 3 + x * 81) >> 2 & 0x3) + (TEXTURE_RES * 1.125f)) // grass + grass edge
                        tint = 0x6AAA40; // green
                    else if (y < ((x * x * 3 + x * 81) >> 2 & 0x3) + (TEXTURE_RES * 1.1875f)) // grass edge shadow
                        gsd_tempA = gsd_tempA * 2 / 3;
                    break;
                }
                case BLOCK_WOOD:
                {
                    tint = 0x675231; // brown (bark)
                    if (!(y >= TEXTURE_RES && y < TEXTURE_RES * 2) && // second row = stripes
                        x > 0 && x < TEXTURE_RES - 1 &&
                        ((y > 0 && y < TEXTURE_RES - 1) || (y > TEXTURE_RES * 2 && y < TEXTURE_RES * 3 - 1))) { // wood side area
                        tint = 0xBC9862; // light brown

                        // the following code repurposes 2 gsd variables making it a bit hard to read
                        // but in short it gets the absolute distance from the tile's center in x and y direction 
                        // finds the max of it
                        // uses that to make the gray scale detail darker if the current pixel is part of an annual ring
                        // and adds some noise as a finishing touch
                        int woodCenter = TEXTURE_RES / 2 - 1;

                        int dx = x - woodCenter;
                        int dy = (y % TEXTURE_RES) - woodCenter;

                        if (dx < 0)
                            dx = 1 - dx;

                        if (dy < 0)
                            dy = 1 - dy;

                        if (dy > dx)
                            dx = dy;

                        gsd_tempA = 196 - (sym_rand() % 32) + dx % 3 * 32;
                    }
                    else if ((sym_rand() % 2) == 0) {
                        // make the gsd 50% brighter on random pixels of the bark
                        // and 50% darker if x happens to be odd
                        gsd_tempA = gsd_tempA * (150 - (x & 1) * 100) / 100;
                    }
                    break;
                }
                case BLOCK_BRICKS:
                {
                    tint = 0xB53A15; // red
                    if ((x + y / 4 * 4) % 8 == 0 || y % 4 == 0) // gap between bricks
                        tint = 0xBCAFA5; // reddish light grey
                    break;
                }
                }

                int gsd_constexpr = gsd_tempA;
                if (y >= TEXTURE_RES * 2) // bottom side of the block
                    gsd_constexpr /= 2; // make it darker, baked "shading"

                if (blockID == BLOCK_LEAVES) {
                    tint = 0x50D937; // green
                    if ((sym_rand() % 2) == 0) {
                        tint = 0;
                        gsd_constexpr = 0xFF;
                    }
                }

                // multiply tint by the grayscale detail
                const int col = ((tint & 0xFFFFFF) == 0 ? 0 : 0xFF) << 24 |
                    (tint >> 16 & 0xFF) * gsd_constexpr / 0xFF << 16 |
                    (tint >> 8 & 0xFF) * gsd_constexpr / 0xFF << 8 |
                    (tint & 0xFF) * gsd_constexpr / 0xFF << 0;

                // write pixel to the texture atlas
                textureAtlas[x + (TEXTURE_RES * blockID) + y * (TEXTURE_RES * 7)] = col;
            }
        }
    }

    // upload texture atlas to GPU
    glGenTextures(1, &textureAtlasTex);
    glBindTexture(GL_TEXTURE_2D, textureAtlasTex);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_RES * 7, TEXTURE_RES * 3, 0, GL_BGRA, GL_UNSIGNED_BYTE, textureAtlas);
}

// size: 1471
static void on_realize()
{
    glEnable(GL_TEXTURE_3D);

    // compile shader
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &shader_frag, NULL);
    glCompileShader(f);

#ifdef DEBUG
    GLint isCompiled = 0;
    glGetShaderiv(f, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE) {
        const int maxLength = 1024;
        GLchar* error[maxLength];
        glGetShaderInfoLog(f, maxLength, &maxLength, error);
        printf("%s\n", error);

        exit(-10);
    }
#endif

    // link shader
    shader = glCreateProgram();
    glAttachShader(shader, f);
    glLinkProgram(shader);

#ifdef DEBUG
    GLint isLinked = 0;
    glGetProgramiv(shader, GL_LINK_STATUS, (int *)&isLinked);
    if (isLinked == GL_FALSE) {
        const int maxLength = 1024;
        GLchar* error[maxLength]; 
        glGetProgramInfoLog(shader, maxLength, &maxLength,error);
        printf("%s\n", error);

        exit(-10);
    }
#endif

    glUseProgram(shader);

    // Game init
    generateWorld();
    
    generateTextures();
}

void _start() {
    asm volatile("sub $8, %rsp\n");

    // open libs we need
    void *libSDL = dlopen("libSDL2.so", RTLD_LAZY);
    void *libC = dlopen("libc.so", RTLD_LAZY);

    // get all functions
    SDL_CreateWindow_t sym_SDL_CreateWindow = (SDL_CreateWindow_t)dlsym(libSDL, "SDL_CreateWindow");
    SDL_GL_CreateContext_t sym_SDL_GL_CreateContext = (SDL_GL_CreateContext_t)dlsym(libSDL, "SDL_GL_CreateContext");
    SDL_SetRelativeMouseMode_t sym_SDL_SetRelativeMouseMode = (SDL_SetRelativeMouseMode_t)dlsym(libSDL, "SDL_SetRelativeMouseMode");
    SDL_PollEvent_t sym_SDL_PollEvent = (SDL_PollEvent_t)dlsym(libSDL, "SDL_PollEvent");
    SDL_GL_SwapWindow_t sym_SDL_GL_SwapWindow = (SDL_GL_SwapWindow_t)dlsym(libSDL, "SDL_GL_SwapWindow");
    sym_SDL_GetTicks64 = (SDL_GetTicks64_t)dlsym(libSDL, "SDL_GetTicks64");
    sym_SDL_GetKeyboardState = (SDL_GetKeyboardState_t)dlsym(libSDL, "SDL_GetKeyboardState");

    sym_rand = (rand_t)dlsym(libC, "rand");

        
    // technically not needed
    //SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    window = sym_SDL_CreateWindow("", 0, 0,
        SCR_WIDTH,
        SCR_HEIGHT,
        SDL_WINDOW_OPENGL// TODO freezes why??? | SDL_WINDOW_RESIZABLE//| SDL_WINDOW_FULLSCREEN
    );

    sym_SDL_GL_CreateContext(window);
    sym_SDL_SetRelativeMouseMode(SDL_TRUE);

    on_realize();

    while (true) {
        SDL_Event event;
        while (sym_SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT
#if defined(KEY_HANDLING)
                || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
#endif
            ) {
                // TODO why not return;
                asm volatile(".intel_syntax noprefix");
                asm volatile("push 231"); //exit_group
                asm volatile("pop rax");
                asm volatile("xor edi, edi");
                asm volatile("syscall");
                asm volatile(".att_syntax prefix");
                __builtin_unreachable();
            } else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                SCR_WIDTH = event.window.data1;
                SCR_HEIGHT = event.window.data2;

                glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
            } else if(event.type == SDL_MOUSEBUTTONDOWN)
            {
                if(event.button.button == SDL_BUTTON_LEFT)
                    breakBlock();
                else
                    placeBlock(BLOCK_DIRT);
            } else if(event.type == SDL_MOUSEMOTION)
            {
                cameraYaw += event.motion.xrel / 500.0f;
                cameraPitch -= event.motion.yrel / 500.0f;

                cameraYaw = fmod(cameraYaw, 2 * M_PI);
                cameraPitch = clamp(cameraPitch, -M_PI / 2.0f, M_PI / 2.0f);
            }
        }

        on_render();
        sym_SDL_GL_SwapWindow(window);
    }
    __builtin_unreachable();
}
