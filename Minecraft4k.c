#define GL_GLEXT_PROTOTYPES

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include "shader.h"

#include "Constants.h"

// OpenGL IDs
GLuint shader;
GLuint worldTex;
GLuint textureAtlasTex;

static uint64_t currentTime()
{
    return SDL_GetTicks64();
}

// It's just the Java Random class
const uint64_t RANDOM_multiplier = 0x5DEECE66D;
const uint64_t RANDOM_addend = 0xBL;
const uint64_t RANDOM_mask = (((uint64_t)1) << 48) - 1;

typedef uint64_t Random;

static uint64_t RANDOM_next(Random* rand, const int bits)
{
    *rand = (*rand * RANDOM_multiplier + RANDOM_addend) & RANDOM_mask;

    return *rand >> (48 - bits);
}

uint32_t nextInt(Random* rand)
{
    return RANDOM_next(rand, 32);
}

uint32_t nextIntBound(Random* rand, int32_t bound)
{
    uint32_t r = RANDOM_next(rand, 31);
    const uint32_t m = bound - 1;

    // TODO why does this exist??
    //if ((bound & m) == 0)  // i.e., bound is a power of 2
    //    r = (uint32_t)(bound * (((uint64_t)r) >> 31));
    //else {
        for(uint32_t u = r;
            u - (r = u % bound) + m < 0;
            u = RANDOM_next(rand, 31));
    //}

    return r;
}

static Random makeRandom(uint64_t seed)
{
    return (seed ^ RANDOM_multiplier) & RANDOM_mask;
}

// TODO tune this
#define TRIG_PRECISION 20
static float my_sin(float x)
{
    double t = x;
    double sine = t;
    for (int a=1; a < TRIG_PRECISION; ++a)
    {
        double mult = -x*x/((2*a+1)*(2*a));
        t *= mult;
        sine += t;
    }
    return (float)sine;
}

static float my_cos(float x)
{
    return my_sin(x + M_PI / 2.0f);
}

float clamp(float x, float min, float max)
{
    if(x < min)
        return min;
    if(x > max)
        return max;
    return x;
}

uint8_t world[WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE];

static void setBlock(int x, int y, int z, uint8_t block)
{
    world[x + y * WORLD_SIZE + z * WORLD_SIZE * WORLD_HEIGHT] = block;

    glBindTexture(GL_TEXTURE_3D, worldTex);
    glTexSubImage3D(GL_TEXTURE_3D,              // target
        0,                                      // level
        x, y, z,                                // offset
        1, 1, 1,                                // size
        GL_RED,                                 // format
        GL_UNSIGNED_BYTE,                       // type
        &block);                                // data
}

static uint8_t getBlock(int x, int y, int z)
{
    return world[x + y * WORLD_SIZE + z * WORLD_SIZE * WORLD_HEIGHT];
}

static int isWithinWorld(int x, int y, int z)
{
    return x >= 0.0f && y >= 0.0f && z >= 0.0f &&
           x < WORLD_SIZE && y < WORLD_HEIGHT && z < WORLD_SIZE;
}

int hoverBlockX = -1, hoverBlockY = -1, hoverBlockZ = -1;

static void placeBlock(uint8_t block)
{
    if(hoverBlockX == -1)
        return;

    setBlock(hoverBlockX, hoverBlockY, hoverBlockZ, block);
}

SDL_Window* window;

float cameraPitch = 0;
float cameraYaw = 0;

float playerVelocityX = 0, playerVelocityY = 0, playerVelocityZ = 0;

// spawn player at world center
float playerPosX = WORLD_SIZE / 2.0f + 0.5f, playerPosY = 1, playerPosZ = WORLD_SIZE / 2.0f + 0.5f;

uint32_t lastMouseState = 0;
static void updateMouse()
{
    int x, y;
    uint32_t mouseState = SDL_GetRelativeMouseState(&x, &y);

    cameraYaw += x / 500.0f;
    cameraPitch -= y / 500.0f;

    if(fabs(cameraYaw) > M_PI)
    {
        if (cameraYaw > 0)
            cameraYaw = -M_PI - (cameraYaw - M_PI);
        else
            cameraYaw = M_PI + (cameraYaw + M_PI);
    }
    cameraPitch = clamp(cameraPitch, -M_PI / 2.0f, M_PI / 2.0f);

    //hoverBlockX = playerPosX; hoverBlockY = playerPosY + 2; hoverBlockZ = playerPosZ;

    if((mouseState & SDL_BUTTON_LMASK) && !(lastMouseState & SDL_BUTTON_LMASK))
    {
        placeBlock(BLOCK_AIR);
    }

    if((mouseState & SDL_BUTTON_RMASK) && !(lastMouseState & SDL_BUTTON_RMASK))
    {
        placeBlock(BLOCK_DIRT);
    }

    lastMouseState = mouseState;
}

// ---------------
// BAD STARTS HERE
// ---------------

float my_fract(float x)
{
    return x - (float)((int) x);
}

float my_max(float a, float b)
{
    if(a > b)
        return a;
    return b;
}

int my_sign(float x)
{
    if(x < 0)
        return -1;
    return 1;
}

static void raycastWorld(float sinYaw, float cosYaw, float sinPitch, float cosPitch, float frustumDivX, float frustumDivY)
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

    distX += my_max(iStep, 0); distY += my_max(jStep, 0); distZ += my_max(kStep, 0);
    distX *= vInvertedX; distY *= vInvertedY; distZ *= vInvertedZ;

    float rayTravelDist = 0;
    while(rayTravelDist < PLAYER_REACH)
    {
        // exit check
        if(!isWithinWorld(i, j, k))
            break;

        int blockHit = getBlock(i, j, k);

        if(blockHit != 0) // BLOCK_AIR
        {
            // TODO set placeBlock as well
           
            hoverBlockX = i; hoverBlockY = j; hoverBlockZ = k;
            return;
        }

        // Determine the closest voxel boundary
        if (distY < distX)
        {
            if (distY < distZ)
            {
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
                k += kStep;

                rayTravelDist = distZ;
                distZ += vInvertedZ;
            }
        }
        else if (distX < distZ)
        {
            i += iStep;

            rayTravelDist = distX;
            distX += vInvertedX;
        }
        else
        {
            k += kStep;

            rayTravelDist = distZ;
            distZ += vInvertedZ;
        }
    }

    hoverBlockX = -1; hoverBlockY = -1; hoverBlockZ = -1;
}

// -------------
// BAD ENDS HERE
// -------------

const uint8_t* kb = NULL;

static void updateController()
{
    kb = SDL_GetKeyboardState(NULL);
}

uint64_t lastFrameTime = 0;
uint64_t lastUpdateTime;

static void on_render()
{
    const uint64_t frameTime = currentTime();
    if(lastFrameTime == 0)
    {
        lastFrameTime = frameTime - 16;
        lastUpdateTime = frameTime;
    }

    lastFrameTime = frameTime;

    float sinYaw = my_sin(cameraYaw);
    float cosYaw = my_cos(cameraYaw);
    float sinPitch = my_sin(cameraPitch);
    float cosPitch = my_cos(cameraPitch);
 
    float frustumDivX = (SCR_WIDTH * FOV) / 214.f;
    float frustumDivY = (SCR_HEIGHT * FOV) / 120.f;

    // update position for destroying blocks
    raycastWorld(sinYaw, cosYaw, sinPitch, cosPitch, frustumDivX, frustumDivY);

    updateMouse();
    updateController();

    //if (needsResUpdate) {
    //    updateScreenResolution();
    //}
   
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

        // calculate collision
        for (int axis = 0; axis < 3; axis++) {
            bool moveValid = true;

            const float newPlayerPosX = playerPosX + playerVelocityX * (axis == 0);
            const float newPlayerPosY = playerPosY + playerVelocityY * (axis == 1);
            const float newPlayerPosZ = playerPosZ + playerVelocityZ * (axis == 2);

            for (int colliderIndex = 0; colliderIndex < 12; colliderIndex++) {
                // magic
                const int colliderBlockPosX = newPlayerPosX + (colliderIndex   % 2) * 0.6f - 0.3f;
                const int colliderBlockPosY = newPlayerPosY + (colliderIndex/4 - 1) * 0.8f + 0.65f;
                const int colliderBlockPosZ = newPlayerPosZ + (colliderIndex/2 % 2) * 0.6f - 0.3f;

                if (colliderBlockPosY < 0) // ignore collision above the world height limit
                    continue;

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

        for (int colliderIndex = 0; colliderIndex < 12; colliderIndex++) {
            int magicX = (int) (playerPosX +        ( colliderIndex       & 1) * 0.6F - 0.3F);
            int magicY = (int) (playerPosY + (float)((colliderIndex >> 2) - 1) * 0.8F + 0.65F);
            int magicZ = (int) (playerPosZ +        ( colliderIndex >> 1  & 1) * 0.6F - 0.3F);

            // set block to air if inside player
            if (isWithinWorld(magicX, magicY, magicZ))
                setBlock(magicX, magicY, magicZ, BLOCK_AIR);
        }

        lastUpdateTime += 10;
    }

    // Compute the raytracing!
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, worldTex);
    glUniform1i(glGetUniformLocation(shader, "W"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureAtlasTex);
    glUniform1i(glGetUniformLocation(shader, "T"), 1);

    glUniform2f(glGetUniformLocation(shader, "S"), SCR_WIDTH, SCR_HEIGHT);

    glUniform1f(glGetUniformLocation(shader, "c.cY"), cosYaw);
    glUniform1f(glGetUniformLocation(shader, "c.cP"), cosPitch);
    glUniform1f(glGetUniformLocation(shader, "c.sY"), sinYaw);
    glUniform1f(glGetUniformLocation(shader, "c.sP"), sinPitch);
    glUniform2f(glGetUniformLocation(shader, "c.f"), frustumDivX, frustumDivY);
    glUniform3f(glGetUniformLocation(shader, "c.P"), playerPosX, playerPosY, playerPosZ);

    glUniform3i(glGetUniformLocation(shader, "b"), hoverBlockX, hoverBlockY, hoverBlockZ);
    // render!!
    glRecti(-1,-1,1,1);
}

static void generateWorld()
{
    float maxTerrainHeight = WORLD_HEIGHT / 2.0f;

    long long seed = 18295169L;
    Random rand = makeRandom(seed);

    for (int x = WORLD_SIZE; x >= 0; x--) {
        for (int y = 0; y < WORLD_HEIGHT; y++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                uint8_t block;

                int randInt = nextIntBound(&rand, 8);

                if (y > (maxTerrainHeight + randInt))
                    block = nextIntBound(&rand, 8) + 1;
                else
                    block = BLOCK_AIR;

                if (x == WORLD_SIZE) // TODO can't I just start at WORLD_SIZE - 1?
                    continue;

                setBlock(x, y, z, block);
            }
        }
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

static void generateTextures()
{
    long long seed = 151910774187927L;

    // set random seed to generate textures
    Random rand = makeRandom(seed);

    int textureAtlas[TEXTURE_RES * TEXTURE_RES * 3 * 16];

    // procedurally generates the 16x3 textureAtlas
    // gsd = grayscale detail
    for (int blockID = 1; blockID < 16; blockID++) {
        int gsd_tempA = 0xFF - nextIntBound(&rand, 0x60);

        for (int y = 0; y < TEXTURE_RES * 3; y++) {
            for (int x = 0; x < TEXTURE_RES; x++) {
                // gets executed per pixel/texel

                if (blockID != BLOCK_STONE || nextIntBound(&rand, 3) == 0) // if the block type is stone, update the noise value less often to get a stretched out look
                    gsd_tempA = 0xFF - nextIntBound(&rand, 0x60);

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

                        gsd_tempA = 196 - nextIntBound(&rand, 32) + dx % 3 * 32;
                    }
                    else if (nextIntBound(&rand, 2) == 0) {
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
                    if (nextIntBound(&rand, 2) == 0) {
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
                textureAtlas[x + (TEXTURE_RES * blockID) + y * (TEXTURE_RES * 16)] = col;
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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_RES * 16, TEXTURE_RES * 3, 0, GL_BGRA, GL_UNSIGNED_BYTE, textureAtlas);
}

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

    // initBuffers
    GLfloat vertices[] = {
        -1.f, -1.f,
        0.f, 1.f,
        -1.f, 1.f,
        0.f, 0.f,
        1.f, -1.f,
        1.f, 1.f,
        -1.f, 1.f,
        0.f, 0.f,
        1.f, -1.f,
        1.f, 1.f,
        1.f, 1.f,
        1.f, 0.f
    };

    GLuint vao, buffer;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Game init
    generateWorld();
    
    generateTextures();
}

void _start() {
    asm volatile("sub $8, %rsp\n");

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_SetHint("SDL_HINT_MOUSE_RELATIVE_MODE_WARP", "1");
    window = SDL_CreateWindow(
        "",
        0,
        0,
        SCR_WIDTH,
        SCR_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_GRABBED//| SDL_WINDOW_FULLSCREEN
    );

    SDL_GL_CreateContext(window);
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_CaptureMouse(SDL_TRUE); 
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDL_SetWindowMouseGrab(window, SDL_TRUE);

    on_realize();

    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT
#if defined(KEY_HANDLING)
                || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
#endif
            ) {
                asm volatile(".intel_syntax noprefix");
                asm volatile("push 231"); //exit_group
                asm volatile("pop rax");
                asm volatile("xor edi, edi");
                asm volatile("syscall");
                asm volatile(".att_syntax prefix");
                __builtin_unreachable();
            }
        }

        on_render();
        SDL_GL_SwapWindow(window);
    }
    __builtin_unreachable();
}
