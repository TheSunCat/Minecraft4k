#version 430
//layout(local_size_x = 16, local_size_y = 16) int SHADER_MINIFIER_WORKAROUND; // bugs galore
layout(rgba32f, binding = 0) writeonly uniform image2D img_output;
layout(r8ui, binding = 1) readonly uniform uimage3D world;

out vec4 fragColor;

// WORLD_SIZE
#define WS 512

// WORLD_HEIGHT
#define WH 64

// TEXTURE_RES
#define TR 16

// RENDER_DIST
#define RD 80.0

// textureAtlas
uniform sampler2D t;

struct C // Camera
{
    vec3 P;
    float cY;
    float cP;
    float sY;
    float sP;
    vec2 fD;
};
uniform C c; // camera

// screenSize
uniform vec2 S;


// lighting
uniform vec3 l; // lightDirection
uniform vec3 k; // skyColor
uniform vec3 a; // ambColor
uniform vec3 s; // sunColor

// get the block at the specified position in the world
int getBlock(ivec3 coords)
{
    return int(imageLoad(world, coords).x);
}

bool inWorld(ivec3 pos)
{
    const vec3 lessThanWorld = step(vec3(0, -2, 0), pos);
    const vec3 greaterThanWorld = step(vec3(WS, WH, WS), pos);

    return dot(lessThanWorld, lessThanWorld) * dot(greaterThanWorld, greaterThanWorld) == 0;
}

// ~~stolen~~ took "inspiration" from https://github.com/Vercidium/voxel-ray-marching/blob/master/source/Map.cs

// Voxel ray marching from http://www.cse.chalmers.se/edu/year/2010/course/TDA361/grid.pdf
// Optimized by keeping block lookups within the current chunk, which minimizes bitshifts, masks and multiplication operations
vec3 rayMarch(in vec3 start, in vec3 velocity, in float maximum, in vec3 fogColor, out bool hit, out vec3 hitPos, out float rayTravelDist)
{
    // Determine the chunk-relative position of the ray using a bit-mask
    ivec3 ijk = ivec3(start);

    // The amount to increase i, j and k in each axis (either 1 or -1)
    ivec3 ijkStep = ivec3(sign(velocity));

    // This variable is used to track the current progress throughout the ray march
    vec3 vInverted = abs(1 / velocity);

    // The distance to the closest voxel boundary in units of rayTravelDist
    vec3 dist = -fract(start) * ijkStep;
    dist += max(ijkStep, vec3(0));
    dist *= vInverted;

    int axis = 0; // X

    rayTravelDist = 0;

    while (rayTravelDist <= maximum)
    {
        // Exit check
        if(!inWorld(ijk))
            break;

        int blockHit = getBlock(ijk);

        if (blockHit != 0) // BLOCK_AIR
        {
            hitPos = start + velocity * rayTravelDist;
            

            // side of block
            int texFetchX = int(mod((hitPos.x + hitPos.z) * TR, TR));
            int texFetchY = int(mod(hitPos.y * TR, TR) + TR);


            if (axis == 1) // Y. we hit the top/bottom of block
            {
                texFetchX = int(mod(hitPos.x * TR, TR));
                texFetchY = int(mod(hitPos.z * TR, TR));

                if (velocity.y < 0.0F) // looking at the underside of a block
                    texFetchY += TR * 2;
            }

            vec3 textureColor = vec3(texture(t,
                                            vec2((texFetchX + (blockHit * TR) + 0.5) / float(TR * 16.0),
                                            (texFetchY + 0.5) / float(TR * 3.0))));
        

            if (dot(textureColor, textureColor) != 0) { // pixel is not transparent
            
                hit = true;
                hitPos = start + velocity * (rayTravelDist - 0.01f);


                float lightIntensity = 1 + (-sign(velocity[axis]) * l[axis]) / 2.0f;

                // storing in vInverted to work around Shader_Minifier bug
#ifdef CLASSIC
                float fogIntensity = ((rayTravelDist / RD)) * (0xFF - (axis + 2) % 3 * 50) / 0xFF;
                vInverted = mix(textureColor, fogColor, fogIntensity);
#else
                vInverted = textureColor * mix(a, s, lightIntensity);
#endif
                return vInverted;
            }
        }

        // Determine the closest voxel boundary
        if (dist.y < dist.x)
        {
            if (dist.y < dist.z)
            {
                // Advance to the closest voxel boundary in the Y direction

                // Increment the chunk-relative position and the block access position
                ijk.y += ijkStep.y;

                // Update our progress in the ray 
                rayTravelDist = dist.y;

                // Set the new distance to the next voxel Y boundary
                dist.y += vInverted.y;

                // For collision purposes we also store the last axis that the ray collided with
                // This allows us to reflect particle velocity on the correct axis
                axis = 1; // Y
            }
            else
            {
                ijk.z += ijkStep.z;

                rayTravelDist = dist.z;
                dist.z += vInverted.z;
                axis = 2; // Z
            }
        }
        else if (dist.x < dist.z)
        {
            ijk.x += ijkStep.x;

            rayTravelDist = dist.x;
            dist.x += vInverted.x;
            axis = 0; // X
        }
        else
        {
            ijk.z += ijkStep.z;

            rayTravelDist = dist.z;
            dist.z += vInverted.z;
            axis = 2; // Z
        }
    }

    hit = false;

    // storing in vInverted to work around Shader_Minifier bug
#ifdef CLASSIC
    vInverted = vec3(0);
#else
    vInverted = fogColor; // sky
#endif

    return vInverted;
}

vec3 getPixel(in vec2 pixel_coords)
{
    const vec2 frustumRay = (pixel_coords - (0.5 * S)) / c.fD;

    // rotate frustum space to world space
    const float temp = c.cP + frustumRay.y * c.sP;
    
    vec3 rayDir = normalize(vec3(frustumRay.x * c.cY + temp * c.sY,
                                 frustumRay.y * c.cP - c.sP,
                                 temp * c.cY - frustumRay.x * c.sY));


    const vec3 fogColor = mix(k, s, 0.5 * pow(clamp(dot(rayDir, l), 0, 1) + 0.2, 5));

    // raymarch outputs
    vec3 hitPos;
    bool hit;
    float hitDist;
    vec3 color = rayMarch(c.P, rayDir, RD, fogColor, hit, hitPos, hitDist);

#ifndef CLASSIC
    if(hit)
    {
        float shadowMult = (1 - l.y) * 0.3;

        if(l.y < 0) { // day
            float ignoreHitDist = 0;
            vec3 ignoreColor = rayMarch(hitPos, l, RD / 2, fogColor, hit, hitPos, ignoreHitDist);


            if(hit) // we can't see the sun
                shadowMult *= 1 + l.y * 0.3;
            
        } else {
            shadowMult = (1 - l.y) * 0.3; // night
        }

        color = color * shadowMult; // apply shadow

        if(hitDist > RD * 0.95) {
            float fogIntensity = ((hitDist - RD * 0.95) / (RD * 0.05));

            color = mix(color, fogColor, fogIntensity);
        }
    }
#endif

    return color;
}

void main()
{
    // Normalized pixel coordinates (from -1 to 1)
    vec2 iResolution = vec2(1920.0, 1080.0);
    vec2 uv = (gl_FragCoord.xy/iResolution.xy)*2.0 - vec2(1.0,1.0);
    uv.y *= iResolution.y/iResolution.x;

    fragColor = vec4(vec3(c.P.y/64.f), 1.0);//getPixel(gl_FragCoord);
}
