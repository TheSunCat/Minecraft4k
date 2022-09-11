#version 130

// camera stuff
uniform float c, d, e, g; // cosYaw, cosPitch, sinYaw, sinPitch
uniform vec2 r,S;     // frustumDiv, SCREEN_SIZE
uniform vec3 P;     // Position

// world (texture 0)
uniform sampler3D W;

// textureAtlas (texture 1)
uniform sampler2D T;

// hoverBlock
uniform ivec3 b;

// fragColor
out vec4 Z;

void main()
{
    vec2 centerDist = vec2(gl_FragCoord.x, S.y - gl_FragCoord.y) - (.5 * S);
    //vec2 absCenterDist = abs(centerDist);
    
    // draw crosshair

    // three implementations of the same logic. It seems the center one compresses better.
    //if(any(lessThan(absCenterDist, vec2(3))) && all(lessThan(absCenterDist, vec2(10)))) {
    if(length(step(abs(centerDist), vec2(2)) + step(abs(centerDist), vec2(10))) > 2) {
    //if(min(absCenterDist.x, absCenterDist.y) < 3. && max(absCenterDist.x, absCenterDist.y) < 10.) {
        Z = vec4(1);
        //return;
    }

    vec2 frustumRay = centerDist / r;

    // rotate frustum space to world space
    float temp = d + frustumRay.y * g;

    vec3 rayDir = normalize(vec3(frustumRay.x * c + temp * e,
                                 frustumRay.y * d - g,
                                 temp * c - frustumRay.x * e));

    // raymarch outputs

    ivec3 ijk = ivec3(P);

    // The amount to increase i, j and k in each axis (either 1 or -1)
    ivec3 ijkStep = ivec3(sign(rayDir));

    // This variable is used to track the current progress throughout the ray march
    vec3 vInverted = abs(1 / rayDir);

    // The distance to the closest voxel boundary in units of rayTravelDist
    vec3 dist = (-fract(P) * ijkStep + max(ijkStep, vec3(0))) * vInverted;

    int axis; // X

    float rayTravelDist;

    while (rayTravelDist < 20) // TODO replace RENDER_DIST
    {
        // TODO exit check for performance
        //if(!inWorld(ijk))
        //    break;

        vec3 hitPos = P + rayDir * rayTravelDist;
        
        // side of block
        vec2 texFetch = fract(vec2(hitPos.x + hitPos.z, hitPos.y));
        texFetch.y += 1; // ++ compresses worse for some reason

        if (axis == 1) // Y. we hit the top/bottom of block
        {
            texFetch = fract(hitPos.xz);

            if (rayDir.y < 0) // looking at the underside of a block
                texFetch.y += 2;
        }

        // get block from world
        // TODO replace WORLD_DIMENSIONS
        texFetch.x += int(texture(W, ijk.yxz / 64.).x * 350); // TODO is this correct? seems random

        // TODO replace TEXTURE_RES
        vec4 textureColor = texture(T, (trunc(texFetch * 16) + .5) / (16 * vec2(7, 3)));

        // highlight hovered block
        // multiply by 9 to make sure it's white
        textureColor += int(ijk == b && any(greaterThan(abs(fract(texFetch) - .5), vec2(7/16.)))) * 9;

        if (length(textureColor) > 0) { // pixel is not transparent
            
            // TODO replace RENDER_DIST
            Z += mix(textureColor, vec4(0), rayTravelDist / 20);
            return;
        }

        // Determine the closest voxel boundary
        if (dist.y < dist.x)
            axis = 1 + int(dist.y > dist.z);
        else
            axis = 2 * int(dist.x > dist.z);

        // Advance to the closest voxel boundary in the axis direction

        // Increment the chunk-relative position and the block access position
        ijk[axis] += ijkStep[axis];

        // Update our progress in the ray 
        rayTravelDist = dist[axis];

        // Set the new distance to the next voxel Y boundary
        dist[axis] += vInverted[axis];

    }

    //Z = vec4(0);
}
