#include <dlfcn.h>

#include <stdio.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include <GL/gl.h>

#include "shader.h"

#include "Constants.h"

// functions we will use

// SDL2
typedef SDL_Window*(*SDL_CreateWindow_t)(const char*, int, int, int, int, Uint32);
typedef SDL_GLContext(*SDL_GL_CreateContext_t)(SDL_Window*);
typedef int(*SDL_SetRelativeMouseMode_t)(SDL_bool);
typedef int(*SDL_PollEvent_t)(SDL_Event*);
typedef void(*SDL_GL_SwapWindow_t)(SDL_Window*);
typedef const Uint8*(*SDL_GetKeyboardState_t)(int*);

SDL_GetKeyboardState_t sym_SDL_GetKeyboardState;

// GL
typedef GLuint(*glCreateShader_t)(GLenum);
typedef void(*glShaderSource_t)(GLuint, GLsizei, const GLchar *const *, const GLint*);
typedef void(*glCompileShader_t)(GLuint);
typedef GLuint(*glCreateProgram_t)();
typedef void(*glAttachShader_t)(GLuint, GLuint);
typedef void(*glLinkProgram_t)(GLuint);
typedef void(*glUseProgram_t)(GLuint);
typedef void(*glGenTextures_t)(GLsizei,GLuint*);
typedef void(*glBindTexture_t)(GLenum, GLuint);
typedef void(*glTexParameteri_t)(GLenum, GLenum, GLint);
typedef void(*glTexImage3D_t)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
typedef void(*glTexSubImage3D_t)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *);
typedef void(*glTexImage2D_t)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
typedef void(*glActiveTexture_t)(GLenum);
typedef void(*glUniform1i_t)(GLint, GLint);
typedef void(*glUniform1f_t)(GLint, GLfloat);
typedef void(*glUniform2f_t)(GLint, GLfloat, GLfloat);
typedef void(*glUniform3f_t)(GLint, GLfloat, GLfloat, GLfloat);
typedef void(*glUniform3i_t)(GLint, GLint, GLint, GLint);
typedef void(*glRecti_t)(GLint, GLint, GLint, GLint);

glCreateShader_t sym_glCreateShader;
glShaderSource_t sym_glShaderSource;
glCompileShader_t sym_glCompileShader;
glCreateProgram_t sym_glCreateProgram;
glAttachShader_t sym_glAttachShader;
glLinkProgram_t sym_glLinkProgram;
glUseProgram_t sym_glUseProgram;
glGenTextures_t sym_glGenTextures;
glBindTexture_t sym_glBindTexture;
glTexParameteri_t sym_glTexParameteri;
glTexImage3D_t sym_glTexImage3D;
glTexSubImage3D_t sym_glTexSubImage3D;
glTexImage2D_t sym_glTexImage2D;
glActiveTexture_t sym_glActiveTexture;
glUniform1i_t sym_glUniform1i;
glUniform1f_t sym_glUniform1f;
glUniform2f_t sym_glUniform2f;
glUniform3f_t sym_glUniform3f;
glUniform3i_t sym_glUniform3i;
glRecti_t sym_glRecti;

// OpenGL IDs
GLuint shader;
GLuint worldTex;
GLuint textureAtlasTex;

#define X 0
#define Y 1
#define Z 2

// size: 352
float my_sin(float x)
{
    float sine;
    
    asm (
        "fsin;"
        "fstps %0;"
        : "=m" (sine)
        : "t" (x)
    );

    return sine;
}

// size: 128
static float my_cos(float x)
{
    return my_sin(x + M_PI / 2.0f);
}

uint8_t world[WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE];

// size: 32
uint32_t toIndex(uint32_t x, uint32_t y, uint32_t z)
{
    return y + x * WORLD_SIZE + z * WORLD_SIZE * WORLD_HEIGHT;
}

// size: 160
static void setBlock(uint32_t x, uint32_t y, uint32_t z, uint8_t block)
{
    world[toIndex(x, y, z)] = block;

    // glBindTexture(GL_TEXTURE_3D, worldTex);
    sym_glTexSubImage3D(GL_TEXTURE_3D,              // target
        0,                                      // level
        y, x, z,                                // offset
        1, 1, 1,                                // size
        GL_RED,                                 // format
        GL_UNSIGNED_BYTE,                       // type
        &block);                                // data
}

// size: 96
uint8_t getBlock(uint32_t x, uint32_t y, uint32_t z)
{
    return world[toIndex(x, y, z)];
}

// size: 64
bool isWithinWorld(uint32_t x, uint32_t y, uint32_t z)
{
    return (x < WORLD_SIZE) & (y < WORLD_HEIGHT) & (z < WORLD_SIZE);
}

uint32_t hoverBlockX = 0, hoverBlockY = 0, hoverBlockZ = 0;
uint32_t placeBlockX = 0, placeBlockY = 0, placeBlockZ = 0;

// size: 32
static void breakBlock()
{
    setBlock(hoverBlockX, hoverBlockY, hoverBlockZ, BLOCK_AIR);
}

// size: 32
static void placeBlock(uint8_t block)
{
    setBlock(placeBlockX, placeBlockY, placeBlockZ, block);
}

SDL_Window* window;
const uint32_t SCR_WIDTH = SCR_WIDTH_DEFAULT * (1 << SCR_DETAIL);
const uint32_t SCR_HEIGHT = SCR_HEIGHT_DEFAULT * (1 << SCR_DETAIL);

float cameraPitch = 0;
float cameraYaw = 0;

float playerVelocityX = 0, playerVelocityY = 0, playerVelocityZ = 0;

// spawn player at world center
float playerPosX = WORLD_SIZE / 2.0f + 0.5f, playerPosY = 1, playerPosZ = WORLD_SIZE / 2.0f + 0.5f;

// size: 64
float my_fract(float x)
{
    return x - (float)((int) x);
}

// size: 240
int8_t my_sign(float x)
{
    if(x < 0)
        return -1;
    return 1;
}

// size: 520
static void raycastWorld(float sinYaw, float cosYaw, float sinPitch, float cosPitch)
{
    // rotate frustum space to world space
    const float rayDirX = cosPitch * sinYaw;
    const float rayDirY = -sinPitch;
    const float rayDirZ = cosPitch * cosYaw;

    int i = (int)playerPosX;
    int j = (int)playerPosY;
    int k = (int)playerPosZ;

    const int8_t iStep = my_sign(rayDirX);
    const int8_t jStep = my_sign(rayDirY);
    const int8_t kStep = my_sign(rayDirZ);

    const float vInvertedX = 1.f / fabs(rayDirX);
    const float vInvertedY = 1.f / fabs(rayDirY);
    const float vInvertedZ = 1.f / fabs(rayDirZ);

    float distX = -my_fract(playerPosX) * iStep;
    float distY = -my_fract(playerPosY) * jStep;
    float distZ = -my_fract(playerPosZ) * kStep;

    distX += iStep == 1; distY += jStep == 1; distZ += kStep == 1;
    distX *= vInvertedX; distY *= vInvertedY; distZ *= vInvertedZ;

    uint8_t axis = X;

    float rayTravelDist = 0.f;
    while(rayTravelDist < PLAYER_REACH)
    {
        // exit check
        if(!isWithinWorld(i, j, k))
            break;

        uint8_t blockHit = getBlock(i, j, k);

        if(blockHit != BLOCK_AIR)
        {
            hoverBlockX = i;
            hoverBlockY = j;
            hoverBlockZ = k;

            placeBlockX = i - (axis == X) * iStep;
            placeBlockY = j - (axis == Y) * jStep;
            placeBlockZ = k - (axis == Z) * kStep;

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

    // focus the top of the world by default (hope the player won't see!)
    hoverBlockY = 0; placeBlockY = 0;
}

const uint8_t* kb = NULL;

// size: 32
static void updateController()
{
    kb = sym_SDL_GetKeyboardState(NULL);
}

// size: 1488
static void on_render()
{
    const float sinYaw = my_sin(cameraYaw);
    const float cosYaw = my_cos(cameraYaw);
    const float sinPitch = my_sin(cameraPitch);
    const float cosPitch = my_cos(cameraPitch);
 
    // update position for placing/destroying blocks
    raycastWorld(sinYaw, cosYaw, sinPitch, cosPitch);

    updateController();
  
    // calculate physics
    {
        const float inputX = (-kb[SDL_SCANCODE_A] + kb[SDL_SCANCODE_D]) * 0.02f;
        const float inputZ = (-kb[SDL_SCANCODE_S] + kb[SDL_SCANCODE_W]) * 0.02f;

        playerVelocityX *= 0.5f;
        playerVelocityY *= 0.99f;
        playerVelocityZ *= 0.5f;

        playerVelocityX += sinYaw * inputZ + cosYaw * inputX;
        playerVelocityZ += cosYaw * inputZ - sinYaw * inputX;
        playerVelocityY += 0.003f; // gravity

        // calculate collision per-axis
        for (uint8_t axis = 0; axis < 3; ++axis) {
            bool moveValid = true;

            // TODO implement WORLD_WRAP
            const float newPlayerPos[3] = {
                playerPosX + playerVelocityX * (axis == X),
                playerPosY + playerVelocityY * (axis == Y),
                playerPosZ + playerVelocityZ * (axis == Z)
            };

            for (int colliderIndex = 0; colliderIndex < 12; ++colliderIndex) {
                const float colliderBlockPos[3] = {
                    newPlayerPos[0] + (colliderIndex % 2) * 0.6f - 0.3f,
                    newPlayerPos[1] + (colliderIndex / 4 - 1) * 0.8f + 0.65f,
                    newPlayerPos[2] + (colliderIndex / 2 % 2) * 0.6f - 0.3f
                };

                if (!isWithinWorld(colliderBlockPos[0], colliderBlockPos[1], colliderBlockPos[2]) ||
                    getBlock(colliderBlockPos[0], colliderBlockPos[1], colliderBlockPos[2]) != BLOCK_AIR) {
                    if (axis == 1) {
                        if (kb[SDL_SCANCODE_SPACE] && playerVelocityY > 0.0f) {
                            playerVelocityY = -0.1f; // jump
                        } else {
                            playerVelocityY = 0.0f; // prevent accelerating downwards infinitely
                        }
                    }
                    moveValid = false;
                    break;
                }
            }

            if (moveValid) {
                playerPosX = newPlayerPos[0];
                playerPosY = newPlayerPos[1];
                playerPosZ = newPlayerPos[2];
            }
        }
    }

    // Compute the raytracing!
    //glActiveTexture(GL_TEXTURE0);
    sym_glBindTexture(GL_TEXTURE_3D, worldTex);
    //glUniform1i(glGetUniformLocation(shader, "W"), 0); // textures are set to 0 by default... this is cursed

    sym_glActiveTexture(GL_TEXTURE1);
    sym_glBindTexture(GL_TEXTURE_2D, textureAtlasTex);
    sym_glUniform1i(SHADER_UNIFORM_T, 1);

    // sym_glUniform2f(glGetUniformLocation(shader, "S"), SCR_WIDTH, SCR_HEIGHT);
    
    sym_glUniform1f(SHADER_UNIFORM_c, cosYaw);
    sym_glUniform1f(SHADER_UNIFORM_d, cosPitch);
    sym_glUniform1f(SHADER_UNIFORM_e, sinYaw);
    sym_glUniform1f(SHADER_UNIFORM_g, sinPitch);
    sym_glUniform2f(SHADER_UNIFORM_r, (SCR_WIDTH * FOV) / 214.f, (SCR_HEIGHT * FOV) / 120.f);
    sym_glUniform3f(SHADER_UNIFORM_P, playerPosX, playerPosY, playerPosZ);

    sym_glUniform3i(SHADER_UNIFORM_b, hoverBlockX, hoverBlockY, hoverBlockZ);
 
    // render!
    sym_glRecti(-1,-1,1,1);
}

// size: 224
static void generateWorld()
{
    const uint8_t maxTerrainHeight = WORLD_HEIGHT / 2;

    for (uint32_t i = 0; i < WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE; ++i) {
        uint8_t block = BLOCK_AIR;

        uint32_t randInt = rand() % 8;

        uint32_t y = i % (WORLD_SIZE);

        if (y > (maxTerrainHeight + randInt))
            block = (rand() % 6) + 1;

        world[i] = block;
    }
    
    // Upload world to GPU
    sym_glGenTextures(1, &worldTex);
    sym_glBindTexture(GL_TEXTURE_3D, worldTex);

    sym_glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
#ifndef WORLD_WRAP
    sym_glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    sym_glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
#endif

    sym_glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    sym_glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    sym_glTexImage3D(GL_TEXTURE_3D,                 // target
        0,                                      // level
        GL_RED,                                 // internal format
        WORLD_SIZE, WORLD_HEIGHT, WORLD_SIZE,   // size
        0,                                      // border
        GL_RED,                                 // format
        GL_UNSIGNED_BYTE,                       // type
        world);                                 // pixels
}

uint32_t textureAtlas[TEXTURE_RES * TEXTURE_RES * 3 * 7];

// size: 672
static void generateTextures()
{
    for (uint32_t blockID = 1; blockID < 7; ++blockID) {
        uint32_t brightness = 0xFF - (rand() % 0x60);

        for (uint32_t y = 0; y < TEXTURE_RES * 3; ++y) {
            for (uint32_t x = 0; x < TEXTURE_RES; ++x) {
                // gets executed per pixel/texel

                // if the block type is stone, update the noise value less often to get a stretched out look
                if ((blockID != BLOCK_STONE) | ((rand() % 3) == 0))
                    brightness = 0xFF - (rand() % 0x60);

                uint32_t tint = 0x966C4A; // brown (dirt)
                switch (blockID)
                {
                case BLOCK_STONE:
                {
                    tint = 0x7F7F7F; // grey
                    break;
                }
                case BLOCK_GRASS:
                {
                    uint32_t grassThreshold = ((x * x * 3 + x * 81) >> 2 & 0x3) + (uint32_t)(TEXTURE_RES * 1.125f);

                    if (y < grassThreshold) {
                        tint = 0x6AAA40; // green
                    } else if (y < (grassThreshold + (uint32_t)(TEXTURE_RES * 0.0625f))) {
                        brightness = brightness * 2 / 3;
                    }
                    break;
                }
                case BLOCK_WOOD:
                {
                    bool isWoodSide = !(y >= TEXTURE_RES && y < TEXTURE_RES * 2) &&
                                      x > 0 && x < TEXTURE_RES - 1 &&
                                      ((y > 0 && y < TEXTURE_RES - 1) || (y > TEXTURE_RES * 2 && y < TEXTURE_RES * 3 - 1));

                    tint = isWoodSide ? 0xBC9862 : 0x675231;

                    if (isWoodSide) {
                        uint8_t woodCenter = TEXTURE_RES / 2 - 1;
                        int8_t dx = abs(x - woodCenter);
                        int8_t dy = abs((y % TEXTURE_RES) - woodCenter);
                        dx = (dy > dx) ? dy : dx;
                        brightness = 196 - (rand() % 32) + dx % 3 * 32;
                    } else if (rand() % 2) {
                        brightness = brightness * (150 - (x & 1) * 100) / 100;
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
                case BLOCK_LEAVES:
                    tint = 0x50D937 * (rand() % 2); // green
                }

                if (y >= TEXTURE_RES * 2) // bottom side of the block
                    brightness /= 2; // make it darker, baked "shading"

                // multiply tint by the grayscale detail
                const uint32_t col = (tint) << 24 |
                    (tint >> 16 & 0xFF) * brightness / 0xFF << 16 |
                    (tint >> 8 & 0xFF) * brightness  / 0xFF << 8 |
                    (tint & 0xFF) * brightness       / 0xFF << 0;

                // write pixel to the texture atlas
                textureAtlas[x + (TEXTURE_RES * blockID) + y * (TEXTURE_RES * 7)] = col;
            }
        }
    }

    // upload texture atlas to GPU
    sym_glGenTextures(1, &textureAtlasTex);
    sym_glBindTexture(GL_TEXTURE_2D, textureAtlasTex);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    sym_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // sym_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    sym_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_RES * 7, TEXTURE_RES * 3, 0, GL_BGRA, GL_UNSIGNED_BYTE, textureAtlas);
}

// size: 1832
static void on_realize()
{
    // sym_glEnable(GL_TEXTURE_3D);

    // compile shader
    GLuint f = sym_glCreateShader(GL_FRAGMENT_SHADER);
    sym_glShaderSource(f, 1, &shader_frag, NULL);
    sym_glCompileShader(f);

#ifdef DEBUG_GL
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
    shader = sym_glCreateProgram();
    sym_glAttachShader(shader, f);
    sym_glLinkProgram(shader);

#ifdef DEBUG_GL
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

    sym_glUseProgram(shader);

    // Game init
    generateWorld();
    
    generateTextures();
}

void _start() {
    asm volatile("sub $8, %rsp\n");

    // open libs we need
    void* libSDL = dlopen("libSDL2.so", RTLD_LAZY);
    void* libGL  = dlopen("libGL.so", RTLD_NOW);

    // get all functions
    
    // SDL
    SDL_CreateWindow_t sym_SDL_CreateWindow = (SDL_CreateWindow_t)dlsym(libSDL, "SDL_CreateWindow");
    SDL_GL_CreateContext_t sym_SDL_GL_CreateContext = (SDL_GL_CreateContext_t)dlsym(libSDL, "SDL_GL_CreateContext");
    SDL_SetRelativeMouseMode_t sym_SDL_SetRelativeMouseMode = (SDL_SetRelativeMouseMode_t)dlsym(libSDL, "SDL_SetRelativeMouseMode");
    SDL_PollEvent_t sym_SDL_PollEvent = (SDL_PollEvent_t)dlsym(libSDL, "SDL_PollEvent");
    SDL_GL_SwapWindow_t sym_SDL_GL_SwapWindow = (SDL_GL_SwapWindow_t)dlsym(libSDL, "SDL_GL_SwapWindow");
    sym_SDL_GetKeyboardState = (SDL_GetKeyboardState_t)dlsym(libSDL, "SDL_GetKeyboardState");
  
    // GL
    sym_glCreateShader = (glCreateShader_t)dlsym(libGL, "glCreateShader");
    sym_glShaderSource = (glShaderSource_t)dlsym(libGL, "glShaderSource");
    sym_glCompileShader = (glCompileShader_t)dlsym(libGL, "glCompileShader");
    sym_glCreateProgram = (glCreateProgram_t)dlsym(libGL, "glCreateProgram");
    sym_glAttachShader = (glAttachShader_t)dlsym(libGL, "glAttachShader");
    sym_glLinkProgram = (glLinkProgram_t)dlsym(libGL, "glLinkProgram");
    sym_glUseProgram = (glUseProgram_t)dlsym(libGL, "glUseProgram");
    sym_glGenTextures = (glGenTextures_t)dlsym(libGL, "glGenTextures");
    sym_glBindTexture = (glBindTexture_t)dlsym(libGL, "glBindTexture");
    sym_glTexParameteri = (glTexParameteri_t)dlsym(libGL, "glTexParameteri");
    sym_glTexImage3D = (glTexImage3D_t)dlsym(libGL, "glTexImage3D");
    sym_glTexSubImage3D = (glTexSubImage3D_t)dlsym(libGL, "glTexSubImage3D");
    sym_glTexImage2D = (glTexImage2D_t)dlsym(libGL, "glTexImage2D");
    sym_glActiveTexture = (glActiveTexture_t)dlsym(libGL, "glActiveTexture");
    sym_glUniform1i = (glUniform1i_t)dlsym(libGL, "glUniform1i");
    sym_glUniform1f = (glUniform1f_t)dlsym(libGL, "glUniform1f");
    sym_glUniform2f = (glUniform2f_t)dlsym(libGL, "glUniform2f");
    sym_glUniform3f = (glUniform3f_t)dlsym(libGL, "glUniform3f");
    sym_glUniform3i = (glUniform3i_t)dlsym(libGL, "glUniform3i");
    sym_glRecti = (glRecti_t)dlsym(libGL, "glRecti");

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
            switch(event.type) {
              case SDL_QUIT:
                // TODO why not return;
                asm volatile(".intel_syntax noprefix");
                asm volatile("push 231"); //exit_group
                asm volatile("pop rax");
                asm volatile("xor edi, edi");
                asm volatile("syscall");
                asm volatile(".att_syntax prefix");
                __builtin_unreachable();
              // case SDL_WINDOWEVENT:
              //   if(event.window.event == SDL_WINDOWEVENT_RESIZED)
              //   {
              //       // SCR_WIDTH = event.window.data1;
              //       // SCR_HEIGHT = event.window.data2;

              //       sym_glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
              //   }
              //   break;
              case SDL_MOUSEBUTTONDOWN:
                if(event.button.button == SDL_BUTTON_LEFT)
                    breakBlock();
                else
                    placeBlock(BLOCK_DIRT);
                break;
              case SDL_MOUSEMOTION:
                cameraYaw += event.motion.xrel / 500.0f;
                cameraPitch -= event.motion.yrel / 500.0f;
                cameraPitch = fmin(M_PI / 2.f, fmax(-M_PI / 2.f, cameraPitch));
            }
        }

        on_render();
        sym_SDL_GL_SwapWindow(window);
    }
    __builtin_unreachable();
}
