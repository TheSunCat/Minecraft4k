#version 130

// WORLD_DIMENSIONS
#define WD vec3(64)

// TEXTURE_RES
#define TR 16

// RENDER_DIST
#define RD 20.0

// camera stuff
uniform float c;    // cosYaw
uniform float d;    // cosPitch
uniform float e;    // sinYaw
uniform float f;    // sinPitch
uniform vec2 r;     // frustumDiv
uniform vec3 P;     // Position

// world (texture 0)
uniform sampler3D W;

// textureAtlas (texture 1)
uniform sampler2D T;

// SCREEN_SIZE
uniform vec2 S;

// hoverBlock
uniform ivec3 b;

// fragColor
out vec4 Z;

// get the block at the specified position in the world
int getBlock(ivec3 coords)
{
    return int(length(texture(W, coords / WD).xyz) * 36 * 9); // TODO is this correct? seems random
}

bool inWorld(ivec3 pos)
{
    vec3 lessThanWorld = step(vec3(0, -2, 0), pos);
    vec3 greaterThanWorld = step(WD, pos);

    return dot(lessThanWorld, lessThanWorld) * dot(greaterThanWorld, greaterThanWorld) == 0;
}

void main()
{
    vec2 frustumRay = (vec2(gl_FragCoord.x, S.y - gl_FragCoord.y) - (0.5 * S)) / (r);

    // rotate frustum space to world space
    float temp = d + frustumRay.y * f;

    vec3 rayDir = normalize(vec3(frustumRay.x * c + temp * e,
                                 frustumRay.y * d - f,
                                 temp * c - frustumRay.x * e));

    // raymarch outputs

    // ~~stolen~~ took "inspiration" from https://github.com/Vercidium/voxel-ray-marching/blob/master/source/Map.cs
    // Voxel ray marching from http://www.cse.chalmers.se/edu/year/2010/course/TDA361/grid.pdf
    // Optimized by keeping block lookups within the current chunk, which minimizes bitshifts, masks and multiplication operations

    // Determine the chunk-relative position of the ray using a bit-mask
    ivec3 ijk = ivec3(P);


    // The amount to increase i, j and k in each axis (either 1 or -1)
    ivec3 ijkStep = ivec3(sign(rayDir));

    // This variable is used to track the current progress throughout the ray march
    vec3 vInverted = abs(1 / rayDir);

    // The distance to the closest voxel boundary in units of rayTravelDist
    vec3 dist = -fract(P) * ijkStep;
    dist += max(ijkStep, vec3(0));
    dist *= vInverted;

    int axis = 0; // X

    float rayTravelDist = 0;

    while (rayTravelDist <= RD) // RENDER_DIST
    {
        // Exit check
        if(!inWorld(ijk))
            break;

        int blockHit = getBlock(ijk);

        if (blockHit != 0) // BLOCK_AIR
        {
            vec3 hitPos = P + rayDir * rayTravelDist;
            
            // side of block
            vec2 texFetch = fract(vec2(hitPos.x + hitPos.z, hitPos.y));
            texFetch.y += 1;

            if (axis == 1) // Y. we hit the top/bottom of block
            {
                texFetch = fract(hitPos.xz);

                if (rayDir.y < 0.0f) // looking at the underside of a block
                    texFetch.y += 2;
            }

            texFetch.x += blockHit;

            vec3 textureColor = texture(T, (trunc(texFetch * TR) + 0.5) / (TR * vec2(16, 3))).xyz;

            // highlight hovered block
            // multiply by 9 to make sure it's white
            textureColor += int(ijk == b && any(greaterThan(abs(fract(texFetch) - 0.5), vec2(.4375)))) * 9;

            if (dot(textureColor, textureColor) != 0) { // pixel is not transparent
                
                float fogIntensity = (rayTravelDist / RD) * (0xFF - (axis + 2) % 3 * 5) / 0xFF;
                Z = vec4(mix(textureColor, vec3(0), fogIntensity), 1);
                return;
            }
        }

        // Determine the closest voxel boundary
        if (dist.y < dist.x)
        {
            if (dist.y < dist.z)
            {
                axis = 1; // Y
            }
            else
            {
                axis = 2; // Z
            }
        }
        else if (dist.x < dist.z)
        {
            axis = 0; // X
        }
        else
        {
            axis = 2; // Z
        }

        // Advance to the closest voxel boundary in the axis direction

        // Increment the chunk-relative position and the block access position
        ijk[axis] += ijkStep[axis];

        // Update our progress in the ray 
        rayTravelDist = dist[axis];

        // Set the new distance to the next voxel Y boundary
        dist[axis] += vInverted[axis];

    }

    Z = vec4(0);
}
