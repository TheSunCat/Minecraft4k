#define GL_GLEXT_PROTOTYPES

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <SDL2/SDL.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include "shader.h"

#include "Constants.h"

static uint64_t currentTime()
{
    return /* TODO time(NULL)*/100;
}

// It's just the Java Random class
#define RANDOM_seedUniquifier 8682522807148012
#define RANDOM_multiplier 0x5DEECE66D
#define RANDOM_addend 0xBL
#define RANDOM_mask ((((uint64_t)1) << 48) - 1)

typedef struct Random
{
    uint64_t seed;
} Random;

static uint64_t RANDOM_initialScramble(const uint64_t seed)
{
    return (seed ^ RANDOM_multiplier) & RANDOM_mask;
}

static uint64_t RANDOM_next(Random* rand, const int bits)
{
    rand->seed = (rand->seed * RANDOM_multiplier + RANDOM_addend) & RANDOM_mask;

    return rand->seed >> (48 - bits);
}

static float nextFloat(Random* rand)
{
    return ((float) RANDOM_next(rand, 24)) / ((float) (1 << 24));
}

uint32_t nextInt(Random* rand)
{
    return RANDOM_next(rand, 32);
}

uint32_t nextIntBound(Random* rand, uint32_t bound)
{
    uint32_t r = RANDOM_next(rand, 31);
    const uint32_t m = bound - 1;
    if ((bound & m) == 0)  // i.e., bound is a power of 2
        r = ((uint32_t)bound) * (((uint64_t)r) >> 31);
    else {
        for (uint32_t u = r;
            u - (r = u % bound) + m < 0;
            u = RANDOM_next(rand, 31));
    }
    return r;
}

uint64_t nextLong(Random* rand)
{
    return (((uint64_t)RANDOM_next(rand, 32)) << 32) + RANDOM_next(rand, 32);
}

static Random makeRandom(uint64_t seed)
{
    return (Random) { RANDOM_initialScramble(seed) };
}

// Perlin noise
#define EXTRA_PRECISION true

static float my_cos(float x)
{
    float tp = 1./(2.*M_PI);
    x *= tp;
    x -= .25 + floor(x + .25);
    x *= 16. * (fabs(x) - .5);
#if EXTRA_PRECISION
    x += .225 * x * (fabs(x) - 1.);
#endif
    return x;
}

static float my_sin(float x)
{
    return my_cos(x - M_PI / 2.0f);
}

float scaled_cosine(const float i) {
    return 0.5f * (1.0f - my_cos(i * M_PI));
}

#define PERLIN_RES 1024

#define PERLIN_OCTAVES 4.0f /* default to medium smooth */
#define PERLIN_AMP_FALLOFF 0.5f /* 50% reduction/octave */

#define PERLIN_YWRAPB 4
#define PERLIN_YWRAP (1 << PERLIN_YWRAPB)
#define PERLIN_ZWRAPB 8
#define PERLIN_ZWRAP (1 << PERLIN_ZWRAPB)

float perlin[PERLIN_RES + 1];

float noise(float x, float y) { // stolen from Processing
    if (perlin[0] == 0) {
        Random r = makeRandom(18295169L);

        for (int i = 0; i < PERLIN_RES + 1; i++)
            perlin[i] = nextFloat(&r);
    }

    if (x < 0)
        x = -x;
    if (y < 0)
        y = -y;

    int xi = (int) x;
    int yi = (int) y;

    float xf = x - xi;
    float yf = y - yi;

    float r = 0;
    float ampl = 0.5f;

    for (int i = 0; i < PERLIN_OCTAVES; i++) {
        int of = xi + (yi << PERLIN_YWRAPB);

        const float rxf = scaled_cosine(xf);
        const float ryf = scaled_cosine(yf);

        float n1 = perlin[of % PERLIN_RES];
        n1 += rxf * (perlin[(of + 1) % PERLIN_RES] - n1);
        float n2 = perlin[(of + PERLIN_YWRAP) % PERLIN_RES];
        n2 += rxf * (perlin[(of + PERLIN_YWRAP + 1) % PERLIN_RES] - n2);
        n1 += ryf * (n2 - n1);

        of += PERLIN_ZWRAP;
        n2 = perlin[of % PERLIN_RES];
        n2 += rxf * (perlin[(of + 1) % PERLIN_RES] - n2);
        float n3 = perlin[(of + PERLIN_YWRAP) % PERLIN_RES];
        n3 += rxf * (perlin[(of + PERLIN_YWRAP + 1) % PERLIN_RES] - n3);
        n2 += ryf * (n3 - n2);

        n1 += scaled_cosine(0) * (n2 - n1);

        r += n1 * ampl;
        ampl *= PERLIN_AMP_FALLOFF;
        xi <<= 1;
        xf *= 2;
        yi <<= 1;
        yf *= 2;

        if (xf >= 1.0) {
            xi++;
            xf--;
        }

        if (yf >= 1.0) {
            yi++;
            yf--;
        }
    }

    return r;
}

uint8_t world[WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE];

static void setBlock(int x, int y, int z, uint8_t block)
{
    world[x + y * WORLD_SIZE + z * WORLD_SIZE * WORLD_HEIGHT] = block;
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

GLuint shader;
GLuint worldTexture;
GLuint textureAtlas;

GLuint textureAtlasTex;

float cameraPitch = 0;
float cameraYaw = 0;

float playerVelocityX = 0, playerVelocityY = 0, playerVelocityZ = 0;
float playerPosX = 0, playerPosY = 0, playerPosZ = 0;

uint64_t lastFrameTime = 0;
uint64_t lastUpdateTime;

static void on_render ()
{
    const uint64_t frameTime = currentTime();
    if(lastFrameTime == 0)
    {
        lastFrameTime = frameTime - 16;
        lastUpdateTime = frameTime;
    }

    const uint64_t deltaTime = frameTime - lastFrameTime;
    lastFrameTime = frameTime;

    //updateMouse();
    //updateController();

    //if (needsResUpdate) {
    //    updateScreenResolution();
    //}

    float sinYaw = my_sin(cameraYaw);
    float cosYaw = my_cos(cameraYaw);
    float sinPitch = my_sin(cameraPitch);
    float cosPitch = my_cos(cameraPitch);

    //lightDirection.y = my_sin(frameTime / 10000.0f);
    //lightDirection.x = my_lightDirection.y * 0.5f;
    //lightDirection.z = my_cos(frameTime / 10000.0f);

    //lightDirection.normalize();

    /*if (lightDirection.y < 0.0f)
    {
        sunColor = lerp(SC_TWILIGHT, SC_DAY, -lightDirection.y);
        ambColor = lerp(AC_TWILIGHT, AC_DAY, -lightDirection.y);
        skyColor = lerp(YC_TWILIGHT, YC_DAY, -lightDirection.y);
    }
    else {
        sunColor = lerp(SC_TWILIGHT, SC_NIGHT, lightDirection.y);
        ambColor = lerp(AC_TWILIGHT, AC_NIGHT, lightDirection.y);
        skyColor = lerp(YC_TWILIGHT, YC_NIGHT, lightDirection.y);
    }*/
    
    while (currentTime() - lastUpdateTime > 10)
    {
        const float inputX = /*controller.right*/1 * 0.02F;
        const float inputZ = /*controller.forward*/1 * 0.02F;

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
            const float newPlayerPosZ =  playerPosZ + playerVelocityZ * (axis == 2);

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
                        if (/*controller.jump*/1 && playerVelocityY > 0.0f) {

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

    //raycast(SCR_RES / 2.0f, hoveredBlockPos, placeBlockPos);

    //prints(hoveredBlockPos); prints("\n");


    // Compute the raytracing!
    float frustumDivX = (SCR_WIDTH * FOV) / 120.f;
    float frustumDivY = (SCR_HEIGHT * FOV) / 214.f;

    glBindImageTexture(0, worldTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
    glUniform2f(glGetUniformLocation(shader, "S"), SCR_WIDTH, SCR_HEIGHT);

    glBindTexture(GL_TEXTURE_2D, textureAtlasTex);
    glUniform1i(glGetUniformLocation(shader, "t"), 0);

    glUniform1f(glGetUniformLocation(shader, "c.cY"), my_cos(cameraYaw));
    glUniform1f(glGetUniformLocation(shader, "c.cP"), my_cos(cameraPitch));
    glUniform1f(glGetUniformLocation(shader, "c.sY"), my_sin(cameraYaw));
    glUniform1f(glGetUniformLocation(shader, "c.sP"), my_sin(cameraPitch));
    glUniform2f(glGetUniformLocation(shader, "c.fD"), frustumDivX, frustumDivY);
    glUniform3f(glGetUniformLocation(shader, "c.P"), playerPosX, playerPosY, playerPosZ);
/*
#ifdef CLASSIC

#else
    computeShader.setVec3(PASS_STR("l"), lightDirection);
    computeShader.setVec3(PASS_STR("k"), skyColor);
    computeShader.setVec3(PASS_STR("a"), ambColor);
    computeShader.setVec3(PASS_STR("s"), sunColor);
#endif
*/
    /*glInvalidateTexImage(screenTexture, 0);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(GLuint((SCR_RES.x + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE), GLuint((SCR_RES.y + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE), 1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    glUseProgram(0)*/;

    // render!!
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glRecti(-1,-1,1,1);
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, TEXTURE_RES * 16, TEXTURE_RES * 3);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXTURE_RES * 16, TEXTURE_RES * 3, GL_BGRA, GL_UNSIGNED_BYTE, textureAtlas);
    glGenerateMipmap(GL_TEXTURE_2D); // TODO needed??
    glBindTexture(GL_TEXTURE_2D, 0);
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

                if (y > maxTerrainHeight + nextIntBound(&rand, 8))
                    block = nextIntBound(&rand, 8) + 1;
                else
                    block = BLOCK_AIR;

                if (x == WORLD_SIZE)
                    continue;

                setBlock(x, y, z, block);
            }
        }
    }
}

static void on_realize()
{
    // compile shader
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &shader_frag, NULL);
    glCompileShader(f);

#ifdef DEBUG
    GLint isCompiled = 0;
    glGetShaderiv(f, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE) {
        const int maxLength = 256;
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
        const int maxLength = 256;
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
    
    // Upload world to GPU
    glGenTextures(1, &worldTexture);
    glBindTexture(GL_TEXTURE_3D, worldTexture);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexStorage3D(GL_TEXTURE_3D,               // target
        1,                                      // levels
        GL_R8,                                  // internal format
        WORLD_SIZE, WORLD_HEIGHT, WORLD_SIZE);  // size

    glTexSubImage3D(GL_TEXTURE_3D,              // target
        0,                                      // level
        0, 0, 0,                                // offsets
        WORLD_SIZE, WORLD_HEIGHT, WORLD_SIZE,   // size
        GL_RED,                                 // format
        GL_UNSIGNED_BYTE,                       // type
        world);                                 // pixels

    glBindTexture(GL_TEXTURE_3D, 0);

    generateTextures();
}

void _start() {
    asm volatile("sub $8, %rsp\n");

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window* mainwindow = SDL_CreateWindow(
        "",
        0,
        0,
        SCR_WIDTH,
        SCR_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
    );

    SDL_GL_CreateContext(mainwindow);
    SDL_ShowCursor(false);
    // TODO SDL_SetRelativeMouseMode(true);
    
    on_realize();

    while (true) {
        SDL_Event Event;
        while (SDL_PollEvent(&Event)) {
            if (Event.type == SDL_QUIT
#if defined(KEY_HANDLING)
                || (Event.type == SDL_KEYDOWN && Event.key.keysym.sym == SDLK_ESCAPE)
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
            if (Event.type == SDL_WINDOWEVENT && Event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                on_render();
                SDL_GL_SwapWindow(mainwindow);
            }
        }
    }
    __builtin_unreachable();
}
