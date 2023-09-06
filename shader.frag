#version 130

// camera stuff
uniform float c, d, e, g; // cosYaw, cosPitch, sinYaw, sinPitch
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
    // Z = vec4(0);

    // vec2 centerDist = gl_FragCoord.xy * vec2(1,-1) - vec2(428,-240);

    //vec2 absCenterDist = abs(centerDist);
    
    // draw crosshair
    // if(length(step(abs(centerDist), vec2(2)) + step(abs(centerDist), vec2(10))) > 2)
    //     Z += 1;

    // vec2 frustumRay = centerDist / r;

    // rotate frustum space to world space
    vec3 rayDir = vec3((gl_FragCoord.x - 428) / r.x * c + (d + (-gl_FragCoord.y + 240) / r.y * g) * e,
                       (-gl_FragCoord.y + 240) / r.y * d - g,
                       (d + (-gl_FragCoord.y + 240) / r.y * g) * c - (gl_FragCoord.x - 428) / r.x * e);

    // raymarch outputs

    // the distance to the closest voxel boundary in units of rayDir
    vec3 dist = (-fract(P) + step(vec3(0), rayDir)) / rayDir;

    float rayTravelDist;

    while (rayTravelDist < 20) // TODO replace RENDER_DIST
    {
        int axis = (dist.y < dist.x) ? 1 + int(dist.y > dist.z) : 2 * int(dist.x > dist.z);
        rayTravelDist = dist[axis];
        dist[axis] += abs(1 / rayDir)[axis];

        // exit check for performance, removed for code size :c
        //if(!inWorld(ijk))
        //    break;

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

        // bit confusing: we repurpose hitPos to fetch the block that was hit
        hitPos[axis] -= step(0., -rayDir[axis]);

        // get block from world
        // TODO replace WORLD_DIMENSIONS
        texFetch.x += texture(W, hitPos.yxz / 64).x * 0xFF;

        // TODO replace TEXTURE_RES
        vec4 textureColor = texture(T, (trunc(texFetch * 16) + .5) / vec2(112, 48));

        // highlight hovered block
        // multiply by 9 to make sure it's white
        textureColor += int(ivec3(hitPos) == b && max(abs(fract(texFetch) - .5).x, abs(fract(texFetch) - .5).y) > .44) * 9;

        if (textureColor.a > 0) { // pixel is not transparent, so output color
            
            // TODO replace RENDER_DIST
            Z += mix(textureColor, vec4(0), rayTravelDist / 20);
            return;
        }
    }

    //Z = vec4(0);
}

// vim: set ft=glsl:
