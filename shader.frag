#version 130

// NOTE: we have to use short var names as the shader optimizer can't change uniform names

// camera stuff
uniform vec4 c;     // x: cosYaw, y: cosPitch, z: sinYaw, w: sinPitch
uniform vec2 r;     // frustumDiv
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
    // NOTE: have to init this to support AMD GPUs
    Z = vec4(0);

    // draw crosshair
    // vec2 centerDist = gl_FragCoord.xy * vec2(1,-1) - vec2(428,-240);
    // vec2 absCenterDist = abs(centerDist);
    //
    // if(length(step(abs(centerDist), vec2(2)) + step(abs(centerDist), vec2(10))) > 2)
    //     Z += 1;

    // vec2 frustumRay = centerDist / r;

    // rotate frustum space to world space
    vec2 offset = (vec2(gl_FragCoord) - vec2(428, 240)) / r;
    float wy = c.y - offset.y * c.w;
    vec3 rayDir = vec3(offset.x * c.x + wy * c.z,
                       -offset.y * c.y - c.w,
                       wy * c.x - offset.x * c.z);

    // raymarch outputs

    // the distance to the closest voxel boundary in units of rayDir
    vec3 dist = (step(vec3(0), rayDir) - fract(P)) / rayDir;

    float rayTravelDist = 0;

    while (rayTravelDist < 20)
    {
        int axis = (dist.y < dist.x) ? 1 + int(dist.y > dist.z) : 2 * int(dist.x > dist.z);
        rayTravelDist = dist[axis];
        dist[axis] += abs(1 / rayDir)[axis];

        vec3 hitPos = P + rayDir * rayTravelDist;
        
        // side of block
        vec2 texFetch = fract(vec2(hitPos.x + hitPos.z, hitPos.y));
        texFetch.y++;

        if (axis == 1) // Y. we hit the top/bottom of block
        {
            texFetch = fract(hitPos.xz);

            if (rayDir.y < 0) // looking at the underside of a block
                texFetch.y += 2;
        }

        // move the hit pos back if we hit a negative side
        hitPos[axis] -= step(rayDir[axis], 0);

        // get block from world
        texFetch.x += texture(W, hitPos.yxz / 64).x * 0xFF;

        // fetch texture color from atlas
        Z = texture(T, (trunc(texFetch * 16) + .5) / vec2(112, 48));

        // confusing: repurpose texFetch into the block-center-relative pos
        texFetch = abs(fract(texFetch) - .5);

        // highlight hovered block
        Z += trunc(hitPos) == b && max(texFetch.x, texFetch.y) > .44 ? 9 : 0;

        if (Z.a > 0) { // pixel is not transparent, so output color
            
            // apply fog
            Z *= 1-rayTravelDist / 20;
            break;
        }
    }
}

// vim: set ft=glsl:
