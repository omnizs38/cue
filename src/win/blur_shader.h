#pragma once

// OUTPUT is before the separate OSD/subtitle overlay composition. Dynamic
// uniforms keep one compiled pipeline across fades, dragging and window sizes.
inline constexpr const char* kBlurCloudShader = R"CUE(
//!PARAM cloud_alpha
//!TYPE DYNAMIC float
//!MINIMUM 0
//!MAXIMUM 1
0
//!PARAM cloud_x
//!TYPE DYNAMIC float
0.5
//!PARAM cloud_y
//!TYPE DYNAMIC float
0.8
//!PARAM cloud_rx
//!TYPE DYNAMIC float
0.5
//!PARAM cloud_ry
//!TYPE DYNAMIC float
0.15
//!PARAM cloud_sx
//!TYPE DYNAMIC float
0.01
//!PARAM cloud_sy
//!TYPE DYNAMIC float
0.02
//!PARAM cue_on
//!TYPE DYNAMIC float
0
//!PARAM cue_dark
//!TYPE DYNAMIC float
0
//!PARAM cue_px
//!TYPE DYNAMIC float
1
//!PARAM cue_gray
//!TYPE DYNAMIC float
0
//!PARAM cue_cell
//!TYPE DYNAMIC float
16
//!PARAM cue_ir
//!TYPE DYNAMIC float
-1
//!PARAM cue_ax
//!TYPE DYNAMIC float
0.5
//!PARAM cue_ay
//!TYPE DYNAMIC float
0.5
//!PARAM cue_ar
//!TYPE DYNAMIC float
10
//!PARAM cue_bx
//!TYPE DYNAMIC float
0.5
//!PARAM cue_by
//!TYPE DYNAMIC float
0.5
//!PARAM cue_br
//!TYPE DYNAMIC float
0
//!PARAM cov_on
//!TYPE DYNAMIC float
0
//!PARAM cov_x
//!TYPE DYNAMIC float
0.5
//!PARAM cov_y
//!TYPE DYNAMIC float
0.3796
//!PARAM cov_hw
//!TYPE DYNAMIC float
0.135
//!PARAM cov_rx
//!TYPE DYNAMIC float
0
//!PARAM cov_ry
//!TYPE DYNAMIC float
0
//!PARAM cov_k
//!TYPE DYNAMIC float
0
//!PARAM cov_hide
//!TYPE DYNAMIC float
0
//!PARAM lit_on
//!TYPE DYNAMIC float
0
//!PARAM lit_x
//!TYPE DYNAMIC float
0.5
//!PARAM lit_y
//!TYPE DYNAMIC float
0.5
//!PARAM lit_hw
//!TYPE DYNAMIC float
0.135
//!PARAM lit_k
//!TYPE DYNAMIC float
0
//!PARAM lit_e
//!TYPE DYNAMIC float
0
//!PARAM lit_s
//!TYPE DYNAMIC float
0
//!PARAM lit_br
//!TYPE DYNAMIC float
0
//!PARAM edge_on
//!TYPE DYNAMIC float
0
//!PARAM mono
//!TYPE DYNAMIC float
0
//!PARAM bg_on
//!TYPE DYNAMIC float
0
//!PARAM bg_x
//!TYPE DYNAMIC float
0.5
//!PARAM bg_y
//!TYPE DYNAMIC float
0.5
//!PARAM bg_hw
//!TYPE DYNAMIC float
0.1
//!PARAM bg_k
//!TYPE DYNAMIC float
0
//!PARAM wing_on
//!TYPE DYNAMIC float
0
//!PARAM wing_all
//!TYPE DYNAMIC float
0
//!PARAM wing_x
//!TYPE DYNAMIC float
0.5
//!PARAM wing_y
//!TYPE DYNAMIC float
0.5
//!PARAM wing_hw
//!TYPE DYNAMIC float
0.1
//!PARAM edge_l
//!TYPE DYNAMIC float
0
//!PARAM edge_r
//!TYPE DYNAMIC float
0
//!PARAM edge_t
//!TYPE DYNAMIC float
0
//!PARAM edge_b
//!TYPE DYNAMIC float
0
//!PARAM veil
//!TYPE DYNAMIC float
0
//!PARAM edge_cr
//!TYPE DYNAMIC float
0
//!PARAM edge_cg
//!TYPE DYNAMIC float
0
//!PARAM edge_cb
//!TYPE DYNAMIC float
0
//!PARAM lit_ry
//!TYPE DYNAMIC float
1
//!PARAM lit_t
//!TYPE DYNAMIC float
0
//!PARAM lit_cov
//!TYPE DYNAMIC float
1
//!PARAM lit_r
//!TYPE DYNAMIC float
0.78
//!PARAM lit_g
//!TYPE DYNAMIC float
0.79
//!PARAM lit_b
//!TYPE DYNAMIC float
0.86
//!PARAM lit_w0r
//!TYPE DYNAMIC float
0
//!PARAM lit_w0a
//!TYPE DYNAMIC float
0
//!PARAM lit_w1r
//!TYPE DYNAMIC float
0
//!PARAM lit_w1a
//!TYPE DYNAMIC float
0
//!PARAM lit_w2r
//!TYPE DYNAMIC float
0
//!PARAM lit_w2a
//!TYPE DYNAMIC float
0
//!PARAM lit_c0
//!TYPE DYNAMIC float
0
//!PARAM lit_c1
//!TYPE DYNAMIC float
0
//!PARAM lit_c2
//!TYPE DYNAMIC float
0
//!PARAM lit_c3
//!TYPE DYNAMIC float
0
//!PARAM lit_p0r
//!TYPE DYNAMIC float
0.59
//!PARAM lit_p0g
//!TYPE DYNAMIC float
0.6
//!PARAM lit_p0b
//!TYPE DYNAMIC float
0.66
//!PARAM lit_p1r
//!TYPE DYNAMIC float
0.59
//!PARAM lit_p1g
//!TYPE DYNAMIC float
0.6
//!PARAM lit_p1b
//!TYPE DYNAMIC float
0.66
//!PARAM lit_p2r
//!TYPE DYNAMIC float
0.59
//!PARAM lit_p2g
//!TYPE DYNAMIC float
0.6
//!PARAM lit_p2b
//!TYPE DYNAMIC float
0.66
//!PARAM lit_p3r
//!TYPE DYNAMIC float
0.59
//!PARAM lit_p3g
//!TYPE DYNAMIC float
0.6
//!PARAM lit_p3b
//!TYPE DYNAMIC float
0.66
//!PARAM lit_p4r
//!TYPE DYNAMIC float
0.59
//!PARAM lit_p4g
//!TYPE DYNAMIC float
0.6
//!PARAM lit_p4b
//!TYPE DYNAMIC float
0.66
//!PARAM lit_p5r
//!TYPE DYNAMIC float
0.59
//!PARAM lit_p5g
//!TYPE DYNAMIC float
0.6
//!PARAM lit_p5b
//!TYPE DYNAMIC float
0.66
//!PARAM lit_p6r
//!TYPE DYNAMIC float
0.59
//!PARAM lit_p6g
//!TYPE DYNAMIC float
0.6
//!PARAM lit_p6b
//!TYPE DYNAMIC float
0.66
//!PARAM lit_p7r
//!TYPE DYNAMIC float
0.59
//!PARAM lit_p7g
//!TYPE DYNAMIC float
0.6
//!PARAM lit_p7b
//!TYPE DYNAMIC float
0.66

//!HOOK OUTPUT
//!WHEN cloud_alpha 0 >
//!BIND HOOKED
//!SAVE CLOUD_H
//!WIDTH HOOKED.w 2 /
//!HEIGHT HOOKED.h 2 /
//!DESC Cue cloud continuous horizontal Gaussian
vec4 hook() {
    vec4 original = HOOKED_tex(HOOKED_pos);
    vec2 q = abs(HOOKED_pos - vec2(cloud_x,cloud_y));
    if (cloud_alpha < 0.0001 || q.x > cloud_rx + 3.0*cloud_sx || q.y > cloud_ry + 3.0*cloud_sy) return original;
    // Half-resolution texture, densely sampled at subpixel spacing for a soft 6 px radius.
    // Pair adjacent taps through bilinear filtering: no sparse square-grid replicas.
    vec3 sum = original.rgb;
    float norm = 1.0;
    for (int i=1; i<=13; i+=2) {
        float a=float(i), b=a+1.0;
        float wa=exp(-0.5*a*a/49.0), wb=exp(-0.5*b*b/49.0);
        float weight=wa+wb;
        float offset=(a+wb/weight)*cloud_sx/7.0;
        vec2 lo=clamp(HOOKED_pos-vec2(offset,0.0),HOOKED_pt*.5,vec2(1.0)-HOOKED_pt*.5);
        vec2 hi=clamp(HOOKED_pos+vec2(offset,0.0),HOOKED_pt*.5,vec2(1.0)-HOOKED_pt*.5);
        sum += (HOOKED_tex(lo).rgb+HOOKED_tex(hi).rgb)*weight;
        norm += 2.0*weight;
    }
    return vec4(sum/norm,original.a);
}

//!HOOK OUTPUT
//!WHEN cloud_alpha 0 >
//!BIND CLOUD_H
//!SAVE CLOUD_V
//!WIDTH CLOUD_H.w
//!HEIGHT CLOUD_H.h
//!DESC Cue cloud continuous vertical Gaussian
vec4 hook() {
    vec4 original=CLOUD_H_tex(CLOUD_H_pos);
    vec2 q=abs(CLOUD_H_pos-vec2(cloud_x,cloud_y));
    if (cloud_alpha < 0.0001 || q.x>cloud_rx || q.y>cloud_ry) return original;
    vec3 sum=original.rgb;
    float norm=1.0;
    for (int i=1; i<=13; i+=2) {
        float a=float(i), b=a+1.0;
        float wa=exp(-0.5*a*a/49.0), wb=exp(-0.5*b*b/49.0);
        float weight=wa+wb;
        float offset=(a+wb/weight)*cloud_sy/7.0;
        vec2 lo=clamp(CLOUD_H_pos-vec2(0.0,offset),CLOUD_H_pt*.5,vec2(1.0)-CLOUD_H_pt*.5);
        vec2 hi=clamp(CLOUD_H_pos+vec2(0.0,offset),CLOUD_H_pt*.5,vec2(1.0)-CLOUD_H_pt*.5);
        sum += (CLOUD_H_tex(lo).rgb+CLOUD_H_tex(hi).rgb)*weight;
        norm += 2.0*weight;
    }
    return vec4(sum/norm,original.a);
}

//!HOOK OUTPUT
//!WHEN cloud_alpha 0 >
//!BIND HOOKED
//!BIND CLOUD_V
//!DESC Cue frameless radial cloud composition
vec4 hook() {
    vec4 original=HOOKED_tex(HOOKED_pos);
    vec2 q=(HOOKED_pos-vec2(cloud_x,cloud_y))/max(vec2(cloud_rx,cloud_ry),vec2(.000001));
    float mask=.35*cloud_alpha*(1.0-smoothstep(.25,1.0,length(q)));
    if(mask<.0001) return original;
    vec3 softened=CLOUD_V_tex(CLOUD_V_pos).rgb;
    return vec4(mix(original.rgb,softened*.95,mask),original.a);
}

//!HOOK OUTPUT
//!WHEN wing_on 0 >
//!BIND HOOKED
//!DESC Cue music card: the spectrum's wings step out (a song's mini player: the cover, its words and its light)
// The picture keeps its wings (rebuilding the engine's graph would cut the sound): they are only
// not shown, before the room's light and the light show are drawn over the black they leave.
vec4 hook() {
    vec4 o = HOOKED_tex(HOOKED_pos);
    // the spectrum-alone look in the card: the whole spectrum steps out (Cue draws the cover over
    // its square; the room's light is drawn over the black)
    if (wing_all > 0.0) return vec4(o.rgb * (1.0 - clamp(wing_all, 0.0, 1.0)), o.a);
    vec2 sz = HOOKED_size;
    vec2 c = vec2(wing_x, wing_y) * sz;
    float h = wing_hw * sz.x;
    float u = h / 260.0;
    vec2 q = HOOKED_pos * sz - c;
    if (abs(q.x) > h + 18.0 * u && abs(q.x) < h + 290.0 * u && abs(q.y) < 300.0 * u)
        return vec4(o.rgb * (1.0 - clamp(wing_on, 0.0, 1.0)), o.a);
    return o;
}

//!HOOK OUTPUT
//!WHEN bg_on 0 >
//!BIND HOOKED
//!DESC Cue music room light: a soft glow around the cover, centred on it, in its own colours
// The music's picture is the cover (and its wings) on black; the room's light is drawn here, on
// the graphics card, instead of a full-size picture made 60 times a second by the processor. The
// eight colours of the cover's border (lit_p*, from the left, clockwise) face their own side; the
// glow is strongest at the cover's edge and fades into the room. The cover keeps its picture.
vec3 bg_p(int i) {
    if (i == 0) return vec3(lit_p0r, lit_p0g, lit_p0b);
    if (i == 1) return vec3(lit_p1r, lit_p1g, lit_p1b);
    if (i == 2) return vec3(lit_p2r, lit_p2g, lit_p2b);
    if (i == 3) return vec3(lit_p3r, lit_p3g, lit_p3b);
    if (i == 4) return vec3(lit_p4r, lit_p4g, lit_p4b);
    if (i == 5) return vec3(lit_p5r, lit_p5g, lit_p5b);
    if (i == 6) return vec3(lit_p6r, lit_p6g, lit_p6b);
    return vec3(lit_p7r, lit_p7g, lit_p7b);
}
vec4 hook() {
    vec4 o = HOOKED_tex(HOOKED_pos);
    vec2 sz = HOOKED_size;
    vec2 c = vec2(bg_x, bg_y) * sz;
    float h = bg_hw * sz.x;
    vec2 q = HOOKED_pos * sz - c;
    if (max(abs(q.x), abs(q.y)) < h - 1.0) return o;
    // distance from a rounded square inside the cover, in half-sides of the cover
    float d = length(max(abs(q) - vec2(h * 0.55), 0.0)) / h;
    float u = fract(atan(q.y, q.x) / 6.2831853 + 0.5) * 8.0;
    int i = int(floor(u));
    vec3 col = mix(bg_p(i), bg_p(i == 7 ? 0 : i + 1), smoothstep(0.0, 1.0, u - floor(u)));
    // (and, far from it, a wash of the same colours over the whole room, the way a blurred cover
    // fills it: the window's own colour, music_backdrop, is where it ends)
    float g = 0.36 * exp(-(d / 2.0) * (d / 2.0)) + 0.15 * exp(-(d / 9.0) * (d / 9.0));
    vec3 light = col * g * clamp(bg_k, 0.0, 1.0);
    // a whisper of noise: no banding in the soft gradient
    light += (fract(sin(dot(HOOKED_pos * sz, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    return vec4(1.0 - (1.0 - o.rgb) * (1.0 - clamp(light, 0.0, 1.0)), o.a);
}

//!HOOK OUTPUT
//!WHEN lit_on 0 >
//!BIND HOOKED
//!SAVE LIT_MASK
//!WIDTH HOOKED.w 4 /
//!HEIGHT HOOKED.h 4 /
//!DESC Cue music light: where the spectrum's dots are lit, at a quarter of the size
// Only the dots give light: the wings beside the cover (36 scene px from it, 220 wide in cells of
// 11, 528 tall, for a 520 cover), or the spectrum alone (768 x 320 of bars, rows of 16, above its
// reflection). The cover,
// the words (drawn later, by the OSD) and the dim cells of silence stay dark.
vec4 hook() {
    vec2 sz = HOOKED_size;
    vec2 c = vec2(lit_x, lit_y) * sz;
    float h = lit_hw * sz.x;
    float u = h / 260.0;  // one scene pixel, on screen
    vec2 q = HOOKED_pos * sz - c;
    // (the cells next to the cover, or along the spectrum's foot, stay lit for any sound at all:
    // left out, so only what the music lifts gives light)
    bool inside = lit_cov > 0.5 ? abs(q.x) > h + 60.0 * u && abs(q.x) < h + 262.0 * u && abs(q.y) < 270.0 * u
                                : abs(q.x) < 390.0 * u && q.y > -206.0 * u && q.y < 88.0 * u;
    if (!inside) return vec4(0.0, 0.0, 0.0, 1.0);
    vec2 d = HOOKED_pt;
    vec3 s = (HOOKED_tex(HOOKED_pos + vec2(-d.x, -d.y)).rgb + HOOKED_tex(HOOKED_pos + vec2(d.x, -d.y)).rgb +
              HOOKED_tex(HOOKED_pos + vec2(-d.x, d.y)).rgb + HOOKED_tex(HOOKED_pos + vec2(d.x, d.y)).rgb) * 0.25;
    // with the cover, the graph's own soft light shows between the dots: what is behind them is
    // read in the clear gap beside the cover and just past the wing, and taken away
    if (lit_cov > 0.5) {
        float sx = q.x < 0.0 ? -1.0 : 1.0;
        vec2 lo = HOOKED_pt * 0.5, hi = vec2(1.0) - HOOKED_pt * 0.5;
        vec3 near = HOOKED_tex(clamp((c + vec2(sx * (h + 18.0 * u), q.y)) / sz, lo, hi)).rgb;
        vec3 far = HOOKED_tex(clamp((c + vec2(sx * (h + 272.0 * u), q.y)) / sz, lo, hi)).rgb;
        vec3 behind = mix(near, far, clamp((abs(q.x) - h - 18.0 * u) / (254.0 * u), 0.0, 1.0));
        float lift = dot(max(s - behind, vec3(0.0)), vec3(.2126, .7152, .0722));
        return vec4(s * smoothstep(0.06, 0.35, lift), 1.0);
    }
    float l = dot(s, vec3(.2126, .7152, .0722));
    return vec4(s * smoothstep(0.17, 0.55, l), 1.0);
}

//!HOOK OUTPUT
//!WHEN lit_on 0 >
//!BIND LIT_MASK
//!SAVE LIT_RAYS
//!WIDTH LIT_MASK.w
//!HEIGHT LIT_MASK.h
//!DESC Cue music light: the light the dots throw into the room, away from the cover
// Each pixel looks back toward the source (the cover's centre, or under the spectrum alone) and
// gathers the dots it finds on the way, the nearer the brighter: soft shafts of light leave every
// bar, as long as the bar is tall, and reach further on the in-breath (lit_br). A per-pixel offset
// along the way turns the steps between the taps into grain the upscale smooths away.
vec4 hook() {
    vec2 p = LIT_MASK_pos;
    vec2 a = vec2(lit_x, lit_y);
    if (lit_cov < 0.5) a.y += lit_hw * LIT_MASK_size.x / LIT_MASK_size.y * (170.0 / 260.0);
    float reach = (0.27 + 0.06 * lit_br) * clamp(lit_ry, 0.0, 1.0);
    // most of the window is too far from any dot for its way back to meet one: nothing to gather
    // (the box where dots can be lit, a little wider for the upscale; the way's own box)
    // (with the cover: one box per wing, the cover between them gives no light)
    vec2 u = vec2(1.0, LIT_MASK_size.x / LIT_MASK_size.y) * lit_hw / 260.0, c = vec2(lit_x, lit_y);
    vec2 far = mix(p, a, reach), m = 3.0 * LIT_MASK_pt, wlo = min(p, far) - m, whi = max(p, far) + m;
    bool meets;
    if (lit_cov > 0.5) {
        vec2 rlo = c + u * vec2(320.0, -270.0), rhi = c + u * vec2(522.0, 270.0);  // the right wing
        vec2 llo = c - u * vec2(522.0, 270.0), lhi = c - u * vec2(320.0, -270.0);  // the left one
        meets = all(lessThanEqual(wlo, rhi)) && all(greaterThanEqual(whi, rlo)) ||
                all(lessThanEqual(wlo, lhi)) && all(greaterThanEqual(whi, llo));
    } else {
        vec2 blo = c - u * vec2(390.0, 206.0), bhi = c + u * vec2(390.0, 88.0);
        meets = all(lessThanEqual(wlo, bhi)) && all(greaterThanEqual(whi, blo));
    }
    if (!meets) return vec4(0.0, 0.0, 0.0, 1.0);
    float jit = fract(sin(dot(p * LIT_MASK_size, vec2(12.9898, 78.233))) * 43758.5453);
    vec3 sum = vec3(0.0);
    float w = 1.0, norm = 0.0;
    for (int i = 0; i < 16; i++) {
        vec2 s = mix(p, a, (float(i) + jit) / 16.0 * reach);
        sum += LIT_MASK_tex(s).rgb * w;
        norm += w;
        w *= 0.837;
    }
    return vec4(sum / norm, 1.0);
}

//!HOOK OUTPUT
//!WHEN lit_on 0 >
//!BIND HOOKED
//!BIND LIT_RAYS
//!DESC Cue music light show: the cover's light breathing with the song, pools in the corners
// Soft light only, no cells: shafts of light thrown by the spectrum's dots (LIT_RAYS, from the
// passes above), a bloom around the cover that follows the music's body (lit_e) and
// breathes in and out on its own slow cycle (lit_br, deeper when the music is full), with only a
// trace of each beat (lit_w*: rings going out); a pool of light in each corner of the window,
// breathing with it and barely nudged in turn by the beats (lit_c*); both in the cover's own
// colours (lit_p*: eight around it, from the left, clockwise), turned round it (lit_t). The
// spectrum's dots take the colour of the cover beside them, or, alone, of the light where they
// stand. Lights add to the picture (screen); the cover stays as is.
vec3 lit_p(int i) {
    if (i == 0) return vec3(lit_p0r, lit_p0g, lit_p0b);
    if (i == 1) return vec3(lit_p1r, lit_p1g, lit_p1b);
    if (i == 2) return vec3(lit_p2r, lit_p2g, lit_p2b);
    if (i == 3) return vec3(lit_p3r, lit_p3g, lit_p3b);
    if (i == 4) return vec3(lit_p4r, lit_p4g, lit_p4b);
    if (i == 5) return vec3(lit_p5r, lit_p5g, lit_p5b);
    if (i == 6) return vec3(lit_p6r, lit_p6g, lit_p6b);
    return vec3(lit_p7r, lit_p7g, lit_p7b);
}
vec3 lit_tone(vec3 col) {
    float mx = max(col.r, max(col.g, col.b));
    return mx > 0.03 ? col / mx * (0.55 + 0.45 * mx) : vec3(lit_r, lit_g, lit_b);
}
// the colour facing angle a (radians, from the cover's centre)
vec3 lit_at(float a) {
    float u = fract(a / 6.2831853 + 0.5 + lit_t) * 8.0;
    int i = int(floor(u));
    float t = smoothstep(0.0, 1.0, u - floor(u));
    return lit_tone(mix(lit_p(i), lit_p(i == 7 ? 0 : i + 1), t));
}
float lit_ring(float d, float r, float a) { float z = (d - r * 2.4) / 0.35; return a * exp(-z * z); }
float lit_pool(vec2 uv, vec2 corner, float asp, float reach, float kick) {
    vec2 v = (uv - corner) * vec2(asp, 1.0);
    return exp(-dot(v, v) / (reach * reach)) * (0.05 + 0.03 * lit_e + 0.025 * kick) * (1.0 + 0.35 * lit_br);
}
vec4 hook() {
    vec4 o = HOOKED_tex(HOOKED_pos);
    vec2 sz = HOOKED_size;
    vec2 c = vec2(lit_x, lit_y) * sz;
    float h = lit_hw * sz.x;
    vec2 q = HOOKED_pos * sz - c;
    // the cover keeps its own picture; the spectrum gets the light behind it too
    if (lit_cov > 0.5 && max(abs(q.x), abs(q.y)) < h - 1.0) return o;
    float k = clamp(lit_k, 0.0, 1.0);
    // the bloom around the cover
    // (round and centred for the spectrum, which has no square to hold it)
    float d = lit_cov > 0.5 ? length(max(abs(q) - vec2(h), 0.0)) / h : length(q) / h * 0.55;
    // it breathes (lit_br): wider and brighter on the in-breath, drawn back on the out-breath
    float R = 0.8 + 0.15 * lit_e + 0.14 * lit_br;
    float bloom = exp(-(d / R) * (d / R)) * (0.12 + 0.07 * lit_e) * (1.0 + 0.4 * lit_br) * (1.0 + 0.04 * lit_s);
    bloom += 0.03 * (lit_ring(d, lit_w0r, lit_w0a) + lit_ring(d, lit_w1r, lit_w1a) + lit_ring(d, lit_w2r, lit_w2a));
    vec3 light = lit_at(atan(q.y, q.x)) * bloom;
    // pools of light in the corners, each in the colour that faces it
    vec2 uv = HOOKED_pos;
    float asp = sz.x / max(sz.y, 1.0), reach = 0.55 + 0.06 * lit_e + 0.05 * lit_br;
    vec2 cc = vec2(lit_x, lit_y);
    light += lit_at(atan(-cc.y, -cc.x)) * lit_pool(uv, vec2(0.0, 0.0), asp, reach, lit_c0);
    light += lit_at(atan(-cc.y, 1.0 - cc.x)) * lit_pool(uv, vec2(1.0, 0.0), asp, reach, lit_c1);
    light += lit_at(atan(1.0 - cc.y, 1.0 - cc.x)) * lit_pool(uv, vec2(1.0, 1.0), asp, reach, lit_c2);
    light += lit_at(atan(1.0 - cc.y, -cc.x)) * lit_pool(uv, vec2(0.0, 1.0), asp, reach, lit_c3);
    // the shafts of light from the dots, in their own colours (the cover's beside them), or, for
    // the spectrum alone, in the light's colours; a little longer and brighter on the in-breath
    vec3 rays = LIT_RAYS_tex(LIT_RAYS_pos).rgb;
    float rl = dot(rays, vec3(.2126, .7152, .0722));
    vec3 rt = lit_cov > 0.5 ? lit_tone(rays) : lit_at(atan(q.y, q.x));
    light += rt * rl * (0.75 + 0.15 * lit_br) * clamp(lit_ry, 0.0, 1.0);
    light *= k;
    // the spectrum's dots take the colour of the cover beside them
    vec3 rgb = o.rgb;
    float l = dot(rgb, vec3(.2126, .7152, .0722));
    // (only the dots: the graph's soft light between them, read in the clear gap beside the cover
    // and just past the wing, is not a dot, however bright the cover makes it)
    float u = h / 260.0;
    if (lit_cov > 0.5 && abs(q.y) < 270.0 * u && abs(q.x) > h + 30.0 * u && abs(q.x) < h + 262.0 * u && l > 0.12) {
        float sx = q.x < 0.0 ? -1.0 : 1.0;
        vec2 lo = HOOKED_pt * .5, hi = vec2(1.0) - HOOKED_pt * .5;
        vec3 near = HOOKED_tex(clamp((c + vec2(sx * (h + 18.0 * u), q.y)) / sz, lo, hi)).rgb;
        vec3 far = HOOKED_tex(clamp((c + vec2(sx * (h + 272.0 * u), q.y)) / sz, lo, hi)).rgb;
        vec3 behind = mix(near, far, clamp((abs(q.x) - h - 18.0 * u) / (254.0 * u), 0.0, 1.0));
        float dot_k = smoothstep(0.04, 0.14, dot(max(rgb - behind, vec3(0.0)), vec3(.2126, .7152, .0722)));
        vec2 sp = (c + vec2(sx * h * 0.9, clamp(q.y, -h, h))) / sz;
        vec3 side = lit_tone(HOOKED_tex(clamp(sp, lo, hi)).rgb);
        rgb = mix(rgb, min(side * l * (1.3 + 0.12 * lit_br), vec3(1.0)), 0.55 * k * dot_k);
    }
    // the spectrum alone: its dots take the light's colour where they stand, a little brighter
    // on the in-breath
    if (lit_cov < 0.5 && l > 0.12) {
        vec3 tone = lit_at(atan(q.y, q.x));
        rgb = mix(rgb, min(tone * l * (1.25 + 0.12 * lit_br), vec3(1.0)), 0.4 * k);
    }
    // a whisper of noise: no banding in the soft gradients
    light += (fract(sin(dot(HOOKED_pos * sz, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    return vec4(1.0 - (1.0 - rgb) * (1.0 - clamp(light, 0.0, 1.0)), o.a);
}

//!HOOK OUTPUT
//!WHEN cov_on 0 >
//!BIND HOOKED
//!DESC Cue music cover: a soft shadow, and the tilt and lit edge of the cover under the pointer
// The cover is part of the picture (flat, at cov_x/cov_y, half side cov_hw of the width). Each pixel
// looks through a camera at the cover turned by cov_rx/cov_ry about its centre; where the turned
// cover no longer covers the flat one, the soft light around it is borrowed from just outside.
float cov_box(vec2 q, float h, float r) {
    vec2 d = abs(q) - vec2(h - r);
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}
vec4 hook() {
    vec2 sz = HOOKED_size;
    vec2 c = vec2(cov_x, cov_y) * sz;
    float h = cov_hw * sz.x;
    vec2 q = HOOKED_pos * sz - c;
    vec4 original = HOOKED_tex(HOOKED_pos);
    if (max(abs(q.x), abs(q.y)) > h * 1.4) return original;
    float k = clamp(cov_k, 0.0, 1.0);
    float cx = cos(cov_rx), sx = sin(cov_rx), cy = cos(cov_ry), sy = sin(cov_ry);
    vec3 e1 = vec3(cy, 0.0, -sy), e2 = vec3(sy * sx, cx, cy * sx), n = vec3(sy * cx, -sx, cy * cx);
    float d = 4.0 * h;
    float t = d * n.z / (n.x * q.x + n.y * q.y + n.z * d);
    vec3 P = vec3(t * q.x, t * q.y, t * d - d);
    vec2 uv = vec2(dot(P, e1), dot(P, e2)) / (1.0 + 0.025 * k);
    // what lies under the cover: the light just outside the flat cover, blended from its four
    // sides (the nearest side counts most), and a soft shadow
    float m = max(abs(q.x), abs(q.y));
    vec3 bg = original.rgb;
    if (m < h + 2.0) {
        float e = h + 8.0;
        vec2 cq = clamp(q, vec2(-e), vec2(e));
        vec2 lo = HOOKED_pt * .5, hi = vec2(1.0) - HOOKED_pt * .5;
        vec3 sl = HOOKED_tex(clamp((c + vec2(-e, cq.y)) / sz, lo, hi)).rgb;
        vec3 sr = HOOKED_tex(clamp((c + vec2(e, cq.y)) / sz, lo, hi)).rgb;
        vec3 st = HOOKED_tex(clamp((c + vec2(cq.x, -e)) / sz, lo, hi)).rgb;
        vec3 sb = HOOKED_tex(clamp((c + vec2(cq.x, e)) / sz, lo, hi)).rgb;
        float wl = 1.0 / max((q.x + e) * (q.x + e), 1.0), wr = 1.0 / max((e - q.x) * (e - q.x), 1.0);
        float wt = 1.0 / max((q.y + e) * (q.y + e), 1.0), wb = 1.0 / max((e - q.y) * (e - q.y), 1.0);
        bg = (sl * wl + sr * wr + st * wt + sb * wb) / (wl + wr + wt + wb);
    }
    // (the shadow leans with a tilt; it narrows with the cover when it turns right round, as a
    // song changes in the mini player's card)
    vec2 so = vec2(-clamp(cov_ry, -0.11, 0.11) * h * 0.35, h * 0.06 + cov_rx * h * 0.35);
    vec2 sq = abs(q - so) - (vec2(h * max(abs(cy), 0.02), h) - vec2(h * 0.02));
    float sd = length(max(sq, 0.0)) + min(max(sq.x, sq.y), 0.0) - h * 0.02;
    // (the card's song change: the shadow is Cue's own window's, the engine's fades out under it)
    bg *= 1.0 - (0.42 + 0.12 * k) * exp(-max(sd, 0.0) / (h * 0.10)) * (1.0 - clamp(cov_hide, 0.0, 1.0));
    float cover = clamp(0.5 - cov_box(uv, h, h * 0.02), 0.0, 1.0);
    // (the card's song change: Cue draws the turning cover and its shadow itself, over the picture;
    // the room stays the engine's)
    if (cover <= 0.0 || cov_hide > 0.0005) return vec4(bg, original.a);
    vec3 art = HOOKED_tex(clamp((c + uv) / sz, HOOKED_pt * .5, vec2(1.0) - HOOKED_pt * .5)).rgb;
    // only the rim catches a little light while the cover is turned
    float rim = clamp((max(abs(uv.x), abs(uv.y)) - (h - 3.0)) / 3.0, 0.0, 1.0) * 0.14 * k;
    art = min(art + vec3(rim), vec3(1.0));
    return vec4(mix(bg, art, cover), original.a);
}

//!HOOK OUTPUT
//!WHEN edge_on 0 >
//!BIND HOOKED
//!DESC Cue music edges: where the picture stops short of the window, it fades into the window's own colour
// The music's picture (and its light, drawn above) can only paint inside the picture's own frame.
// In a window much wider or taller than it, the window's colour beyond (the engine's background,
// the room's far light in the cover's colours: edge_c*) would meet it on a straight line: on those
// sides (edge_l/r/t/b: the fade's width, in shares of the frame) it eases into that colour.
vec4 hook() {
    vec4 o = HOOKED_tex(HOOKED_pos);
    vec2 p = HOOKED_pos;
    float k = 1.0;
    if (edge_l > 0.0) k *= smoothstep(0.0, edge_l, p.x);
    if (edge_r > 0.0) k *= smoothstep(0.0, edge_r, 1.0 - p.x);
    if (edge_t > 0.0) k *= smoothstep(0.0, edge_t, p.y);
    if (edge_b > 0.0) k *= smoothstep(0.0, edge_b, 1.0 - p.y);
    return vec4(mix(vec3(edge_cr, edge_cg, edge_cb), o.rgb, k), o.a);
}

//!HOOK OUTPUT
//!WHEN veil 0 >
//!BIND HOOKED
//!DESC Cue music list: the cover steps back into the dark while the list is open over it
vec4 hook() {
    vec4 o = HOOKED_tex(HOOKED_pos);
    return vec4(o.rgb * (1.0 - 0.86 * clamp(veil, 0.0, 1.0)), o.a);
}

//!HOOK OUTPUT
//!WHEN mono 0 >
//!BIND HOOKED
//!DESC Cue music in black and white: its spectrum and its light, as their own greys; the cover keeps its colours
// A choice for music (colour stays the default); mono eases 0..1 so the colour drains away and
// comes back instead of switching. Rec. 709 luma: each colour keeps its own brightness. The cover
// itself stays in colour: where it is drawn (as the cover pass turns it under the pointer, the
// same camera, so a tilted cover keeps every coloured pixel and nothing around it does).
float mono_box(vec2 q, float h, float r) {
    vec2 d = abs(q) - vec2(h - r);
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}
vec4 hook() {
    vec4 o = HOOKED_tex(HOOKED_pos);
    float k = clamp(mono, 0.0, 1.0);
    if (cov_on > 0.5 && cov_hide < 0.0005) {
        vec2 sz = HOOKED_size;
        float h = cov_hw * sz.x;
        vec2 q = HOOKED_pos * sz - vec2(cov_x, cov_y) * sz;
        if (max(abs(q.x), abs(q.y)) < h * 1.4) {
            float cx = cos(cov_rx), sx = sin(cov_rx), cy = cos(cov_ry), sy = sin(cov_ry);
            vec3 e1 = vec3(cy, 0.0, -sy), e2 = vec3(sy * sx, cx, cy * sx), n = vec3(sy * cx, -sx, cy * cx);
            float d = 4.0 * h;
            float t = d * n.z / (n.x * q.x + n.y * q.y + n.z * d);
            vec3 P = vec3(t * q.x, t * q.y, t * d - d);
            vec2 uv = vec2(dot(P, e1), dot(P, e2)) / (1.0 + 0.025 * clamp(cov_k, 0.0, 1.0));
            k *= 1.0 - clamp(0.5 - mono_box(uv, h, h * 0.02), 0.0, 1.0);
        }
    }
    float luma = dot(o.rgb, vec3(.2126, .7152, .0722));
    return vec4(mix(o.rgb, vec3(luma), k), o.a);
}

//!HOOK OUTPUT
//!WHEN cue_on 0 >
//!BIND HOOKED
//!DESC Cue snap / subtitle edit / opening iris: big pixels, grey, dark; the grey can spread in a pixel ring
// A ring of grey blocks grows from (ax, ay) as ar goes 0..1 (>= 9: everywhere at once); a second
// ring from (bx, by) gives the colour back as br goes 0..1. Edges step in four levels, in blocks.
float cue_ring(vec2 b, vec2 c, float k, vec2 sz, float cell) {
    float far = length(max(c, sz - c));
    float r = k * (far + 4.0 * cell);
    float x = clamp((r - length(b - c)) / (3.0 * cell), 0.0, 1.0);
    return ceil(x * 4.0) / 4.0;
}
vec4 hook() {
    // Opening-only path: black pixels need no video sample, grayscale or ring calculations.
    if (cue_ir >= 0.0) {
        float bc = max(cue_cell, 1.0);
        vec2 q = ((floor(HOOKED_pos * HOOKED_size / bc) + 0.5) * bc
                  - 0.5 * HOOKED_size) / (0.5 * HOOKED_size);
        float radius = cue_ir * 1.45;
        if (dot(q, q) > radius * radius) return vec4(0.0, 0.0, 0.0, 1.0);
        return HOOKED_tex(HOOKED_pos);
    }
    vec2 cell = max(cue_px, 1.0) * HOOKED_pt;
    vec2 p = cue_px > 1.0 ? (floor(HOOKED_pos / cell) + 0.5) * cell : HOOKED_pos;
    vec4 c = HOOKED_tex(clamp(p, HOOKED_pt * .5, vec2(1.0) - HOOKED_pt * .5));
    float m = 1.0;
    if (cue_ar < 9.0 || cue_br > 0.0) {
        vec2 sz = HOOKED_size;
        float bc = max(cue_cell, 1.0);
        vec2 b = (floor(HOOKED_pos * sz / bc) + 0.5) * bc;
        if (cue_ar < 9.0) m = cue_ring(b, vec2(cue_ax, cue_ay) * sz, cue_ar, sz, bc);
        if (cue_br > 0.0) m *= 1.0 - cue_ring(b, vec2(cue_bx, cue_by) * sz, cue_br, sz, bc);
    }
    float luma = dot(c.rgb, vec3(.2126, .7152, .0722));
    vec3 rgb = mix(c.rgb, vec3(luma), clamp(cue_gray, 0.0, 1.0) * m);
    rgb *= 1.0 - clamp(cue_dark, 0.0, 1.0) * m;
    // the opening iris (cue_ir >= 0): black outside an ellipse of the picture's own proportions,
    // in blocks of cue_cell, growing from the middle (0: all black, 1: past the corners)
    if (cue_ir >= 0.0) {
        vec2 sz = HOOKED_size;
        float bc = max(cue_cell, 1.0);
        vec2 q = ((floor(HOOKED_pos * sz / bc) + 0.5) * bc - 0.5 * sz) / (0.5 * sz);
        rgb *= step(length(q), cue_ir * 1.45);
    }
    return vec4(rgb, c.a);
}
)CUE";
