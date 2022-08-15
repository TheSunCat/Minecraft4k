#version 130

// WORLD_DIMENSIONS
#define WD vec3(512, 64, 512)

// TEXTURE_RES
#define TR 16

// RENDER_DIST
#define RD 80.0

struct C // Camera
{
    float cY;    // cosYaw
    float cP;    // cosPitch
    float sY;    // sinYaw
    float sP;    // sinPitch
    vec2 f;     // frustumDiv
    vec3 P;     // Position
};
uniform C c; // camera

// world (texture 0)
uniform sampler3D W;

// textureAtlas (texture 1)
uniform sampler2D T;

// SCREEN_SIZE
uniform vec2 S;

// fragColor
out vec4 F;

// get the block at the specified position in the world
int getBlock(ivec3 coords)
{
    return int(length(texture(W, coords / WD) * 8));
}

bool inWorld(ivec3 pos)
{
    vec3 lessThanWorld = step(vec3(0, -2, 0), pos);
    vec3 greaterThanWorld = step(WD, pos);

    return dot(lessThanWorld, lessThanWorld) * dot(greaterThanWorld, greaterThanWorld) == 0;
}

vec3 getPixel(in vec2 pixel_coords)
{
    //return texture(W, vec3(pixel_coords.xy, 1) / WD).rgb * 16;
    return texture(T, pixel_coords.xy / S).rgb;


    vec2 frustumRay = (pixel_coords - (0.5 / S)) / c.f; // TODO do I mult by SCREEN_SIZE?

    // rotate frustum space to world space
    float temp = c.cP + frustumRay.y * c.sP;
    
    vec3 rayDir = normalize(vec3(frustumRay.x * c.cY + temp * c.sY,
                                 frustumRay.y * c.cP - c.sP,
                                 temp * c.cY - frustumRay.x * c.sY));

    // raymarch outputs

    // ~~stolen~~ took "inspiration" from https://github.com/Vercidium/voxel-ray-marching/blob/master/source/Map.cs
    // Voxel ray marching from http://www.cse.chalmers.se/edu/year/2010/course/TDA361/grid.pdf
    // Optimized by keeping block lookups within the current chunk, which minimizes bitshifts, masks and multiplication operations

    // Determine the chunk-relative position of the ray using a bit-mask
    ivec3 ijk = ivec3(c.P);

    // The amount to increase i, j and k in each axis (either 1 or -1)
    ivec3 ijkStep = ivec3(sign(rayDir));

    // This variable is used to track the current progress throughout the ray march
    vec3 vInverted = abs(1 / rayDir);

    // The distance to the closest voxel boundary in units of rayTravelDist
    vec3 dist = -fract(c.P) * ijkStep;
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
            vec3 hitPos = c.P + rayDir * rayTravelDist;
            
            // side of block
            int texFetchX = int(mod((hitPos.x + hitPos.z) * TR, TR));
            int texFetchY = int(mod(hitPos.y * TR, TR) + TR);

            if (axis == 3) // Y. we hit the top/bottom of block
            {
                texFetchX = int(mod(hitPos.x * TR, TR));
                texFetchY = int(mod(hitPos.z * TR, TR));

                if (rayDir.y < 0.0F) // looking at the underside of a block
                    texFetchY += TR * 2;
            }

            //return vec3(float(blockHit) /8.f);

            vec3 textureColor = vec3(texture(T,
                                            vec2(float(texFetchX + (blockHit * TR) + 0.5) / float(TR * 16.0),
                                                 float(texFetchY + 0.5)                   / float(TR * 3.0))));

            return textureColor;
        

            if (dot(textureColor, textureColor) != 0) { // pixel is not transparent
            
                hitPos = start + rayDir * (rayTravelDist - 0.01f);


                float lightIntensity = 1 + (-sign(rayDir[axis]) * l[axis]) / 2.0f;

                // storing in vInverted to work around Shader_Minifier bug
                float fogIntensity = ((rayTravelDist / RD)) * (0xFF - (axis + 2) % 3 * 50) / 0xFF;
                vInverted = mix(textureColor, vec3(0), fogIntensity);
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
                // This allows us to reflect particle rayDir on the correct axis
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

    // storing in vInverted to work around Shader_Minifier bug
    vInverted = vec3(0);

    return vInverted;
}

void main()
{
    F = vec4(getPixel(vec2(gl_FragCoord.x, S.y - gl_FragCoord.y)), 1.0);
}
