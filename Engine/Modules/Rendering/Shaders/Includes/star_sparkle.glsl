#ifndef GE_STAR_SPARKLE_GLSL
#define GE_STAR_SPARKLE_GLSL

// The four-pointed star the night sky draws for its stars (sky_stars_billboard.frag) and for
// the heads of its falling stars (sky_render.frag). Shared with the probe that measures the
// outline (star_sparkle_probe.comp), so the star a camera sees and the star the test measures
// cannot drift apart.
//
// The trap is exposure. A star is a point source on the scene-linear scale, so a bright
// exposure (the Scene View's Scene Light mode at night, a low auto-exposure floor) drives
// most of the sprite past the tonemapper's white point, and what reads on screen is the
// outline of the clipped region: a level set of the falloff's metric. The points' metric is
// (sqrt|x| + sqrt|y|)^2, homogeneous of degree one, so every level set of the points alone is a
// scaled copy of the same concave four-pointed star. A convex metric such as |x| + |y| has
// square level sets and clips into a hard-edged rotated square. The drawn star is not
// homogeneous: the round core under the points (and a star's twinkle glow) makes a light clip
// round, and the points appear as the clip deepens. At the default star the reach along a point
// over the reach between two points is about 1.5 at 0.9 of the peak and 2.8 at 0.01 with the
// glow at its mean twinkle (1.0 and 2.3 without it); a falling-star head, whose points match its
// core, holds 2 sqrt(2) at every level.

// Distance from the star's centre in the star's own frame (points along +-x and +-y), in
// units of the sparkle's radius: equal to |x| along an axis and to 2 sqrt(2) times the
// distance along a diagonal, so a point reaches 2 sqrt(2) times as far as the flank between
// two points at every level. It is never less than the Euclidean distance, so a sparkle with
// radius 1 lies inside the unit disc.
float GE_StarPointMetric(vec2 q)
{
    vec2 s = sqrt(abs(q));
    float m = s.x + s.y;
    return m * m;
}

// Falloff of a normalised distance d in [0, 1]: 1 across the core (d <= coreSize), then
// (1 - t)^falloff to exactly 0 at d = 1, so the sprite needs no edge fade at its bounds.
float GE_StarFalloff(float d, float coreSize, float falloff)
{
    float n = clamp(d, 0.0, 1.0);
    float t = (n <= coreSize) ? 0.0 : (n - coreSize) / (1.0 - coreSize);
    return pow(1.0 - t, falloff);
}

// The star's intensity at q (star frame, units of the points' reach): a round core of
// radius coreRadius under four points of relative brightness pointAmount.
float GE_StarSparkle(vec2 q, float coreRadius, float pointAmount, float coreSize, float falloff)
{
    float core = GE_StarFalloff(length(q) / coreRadius, coreSize, falloff);
    float points = GE_StarFalloff(GE_StarPointMetric(q), coreSize, falloff);
    return max(core, points * pointAmount);
}

// The glow a star's twinkle brightens: the sparkle's own four-pointed metric under a soft
// falloff, (1 - m)^kStarGlowFalloff, so it keeps the star's shape, adds a little around the core
// and reaches further along the points, and is exactly zero at the points' reach. Its integral
// over the plane, 2 A / ((k + 1) (k + 2)) with A = 2/3 the area inside m = 1, is the one
// ComputeStarSparkleEnergy normalises (kStarGlowFalloff in StarSparkleEnergy.h).
const float kStarGlowFalloff = 3.0;

float GE_StarGlow(vec2 q)
{
    return pow(1.0 - clamp(GE_StarPointMetric(q), 0.0, 1.0), kStarGlowFalloff);
}

// The four taps of a rotated grid over one pixel, in pixels from its centre. They sum to zero,
// so a star centred on a pixel is drawn symmetric under a half turn; a star off the pixel's
// centre (most are: the projection lands anywhere) shows its sub-pixel offset as a point one
// faint pixel longer on one side.
const vec2 kStarTaps[4] = vec2[4](vec2(0.125, 0.375), vec2(0.375, -0.125), vec2(-0.125, -0.375),
                                  vec2(-0.375, 0.125));

// GE_StarSparkle averaged over the pixel's footprint, spread by the screen derivatives of q. A
// star spans a handful of pixels and its points are narrower than one, so a single tap at the
// pixel centre breaks the points into detached pixels; the average keeps them as thin, fading
// tips. The caller takes dFdx(q) and dFdy(q) in uniform control flow.
float GE_StarSparkleFiltered(vec2 q, vec2 dqdx, vec2 dqdy, float coreRadius, float pointAmount,
                             float coreSize, float falloff)
{
    float sum = 0.0;
    for (int i = 0; i < 4; ++i)
        sum += GE_StarSparkle(q + kStarTaps[i].x * dqdx + kStarTaps[i].y * dqdy, coreRadius, pointAmount,
                              coreSize, falloff);
    return 0.25 * sum;
}

// GE_StarGlow averaged over the pixel's footprint, as GE_StarSparkleFiltered.
float GE_StarGlowFiltered(vec2 q, vec2 dqdx, vec2 dqdy)
{
    float sum = 0.0;
    for (int i = 0; i < 4; ++i)
        sum += GE_StarGlow(q + kStarTaps[i].x * dqdx + kStarTaps[i].y * dqdy);
    return 0.25 * sum;
}

// A 32-bit integer hash (PCG output permutation), for per-star choices that need no catalog data.
uint GE_StarHash(uint x)
{
    x = x * 747796405u + 2891336453u;
    uint w = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    return (w >> 22u) ^ w;
}

float GE_StarHash01(uint x)
{
    return float(GE_StarHash(x) & 0xFFFFu) / 65535.0;
}

// Each star's own twinkle range, from its catalog index: x is its peak brightness as a fraction
// of full, y its low phase as a fraction of its own peak. Most stars twinkle with a peak of
// 0.2 + 0.8 h^4 and a low phase of 0.4 to 0.7 of that peak; one star in five stays faint and
// barely twinkles (peak 0.2, low phase 0.9). The whole star dims: core, points and glow together.
//
// Full is the star's light at the catalog brightness and Star Brightness. At full, a star's core
// sits on the tone curve's shoulder at any night exposure, so a range of 0.6 to 1.0 still read as
// white at every peak; the peaks are spread low enough that most cores land on the curve's slope.
// Calibrated under auto exposure at the Basic 3D template's 22 h pose: the per-pixel maximum over
// a run puts the cores' peaks at 177 / 233 / 248 of 255 (p10 / p50 / p90), with 7 % at 250 or
// above. Over the catalog and a twinkle period a star carries 0.25 of its full light.
const float kStarFaintShare = 0.2;
const vec2 kStarFaintRange = vec2(0.2, 0.9);

vec2 GE_StarTwinkleRange(uint starIndex)
{
    if (GE_StarHash01(starIndex + 0x632BE5ABu) < kStarFaintShare)
        return kStarFaintRange;
    float h = GE_StarHash01(starIndex);
    float peak = 0.2 + 0.8 * h * h * h * h;
    float low = 0.4 + 0.3 * GE_StarHash01(starIndex ^ 0x9E3779B9u);
    return vec2(peak, low);
}

// A star's brightness at a twinkle phase in [0, 1] (0 the low phase, 1 the peak).
float GE_StarTwinkleBrightness(vec2 twinkleRange, float phase)
{
    return twinkleRange.x * mix(twinkleRange.y, 1.0, phase);
}

#endif // GE_STAR_SPARKLE_GLSL
