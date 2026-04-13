#version 330 core

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D sourceTexture;
uniform sampler2D prevAccum;

uniform float scanlineIntensity;
uniform float scanlineWidth;
uniform float blurRadius;
uniform float bloomIntensity;
uniform float bloomRadius;
uniform float afterglowStrength;
uniform float afterglowDecaySpeed;
uniform int tintMode;
uniform float tintStrength;
uniform int maskType;
uniform float maskIntensity;
uniform float curvatureAmount;
uniform float vignetteStrength;
uniform float dtMs;
uniform vec2 outputSize;
uniform vec2 sourceSize;

// Barrel distortion for screen curvature
vec2 applyCurvature(vec2 uv, float amount) {
    vec2 centered = uv * 2.0 - 1.0;
    float r2 = dot(centered, centered);
    float k = amount * 0.2;
    centered *= 1.0 + k * r2;
    return centered * 0.5 + 0.5;
}

// 9-tap blur sampling
vec4 sampleBlurred(vec2 uv, float radius) {
    vec2 texelSize = vec2(1.0 / sourceSize.x, 1.0 / sourceSize.y);
    vec2 offset = texelSize * radius * 2.0;

    vec4 color = texture(sourceTexture, uv) * 0.25;
    color += texture(sourceTexture, uv + vec2(offset.x, 0.0)) * 0.125;
    color += texture(sourceTexture, uv - vec2(offset.x, 0.0)) * 0.125;
    color += texture(sourceTexture, uv + vec2(0.0, offset.y)) * 0.125;
    color += texture(sourceTexture, uv - vec2(0.0, offset.y)) * 0.125;
    color += texture(sourceTexture, uv + offset) * 0.0625;
    color += texture(sourceTexture, uv - offset) * 0.0625;
    color += texture(sourceTexture, uv + vec2(-offset.x, offset.y)) * 0.0625;
    color += texture(sourceTexture, uv + vec2(offset.x, -offset.y)) * 0.0625;

    return color;
}

// 13-tap bloom with luminance weighting
vec3 computeBloom(vec2 uv, vec3 baseColor) {
    vec2 texelSize = vec2(1.0 / sourceSize.x, 1.0 / sourceSize.y);
    vec2 near = texelSize * bloomRadius * 3.0;
    vec2 far = near * 2.0;
    float diag = 0.707;

    vec3 bloom = baseColor * 0.16;
    bloom += texture(sourceTexture, uv + vec2(near.x, 0.0)).rgb * 0.10;
    bloom += texture(sourceTexture, uv - vec2(near.x, 0.0)).rgb * 0.10;
    bloom += texture(sourceTexture, uv + vec2(0.0, near.y)).rgb * 0.10;
    bloom += texture(sourceTexture, uv - vec2(0.0, near.y)).rgb * 0.10;
    bloom += texture(sourceTexture, uv + vec2(near.x * diag, near.y * diag)).rgb * 0.06;
    bloom += texture(sourceTexture, uv + vec2(-near.x * diag, near.y * diag)).rgb * 0.06;
    bloom += texture(sourceTexture, uv + vec2(near.x * diag, -near.y * diag)).rgb * 0.06;
    bloom += texture(sourceTexture, uv - vec2(near.x * diag, near.y * diag)).rgb * 0.06;
    bloom += texture(sourceTexture, uv + vec2(far.x, 0.0)).rgb * 0.05;
    bloom += texture(sourceTexture, uv - vec2(far.x, 0.0)).rgb * 0.05;
    bloom += texture(sourceTexture, uv + vec2(0.0, far.y)).rgb * 0.05;
    bloom += texture(sourceTexture, uv - vec2(0.0, far.y)).rgb * 0.05;

    float luma = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));
    float weight = smoothstep(0.1, 0.8, luma) * bloomIntensity;

    vec3 result = baseColor + bloom * weight;
    // Shoulder compression
    vec3 threshold = vec3(0.8);
    vec3 over = max(result - threshold, vec3(0.0));
    result = min(result, threshold) + over / (1.0 + over * 2.5);
    return result;
}

// Sinusoidal scanlines mapped to source lines
float computeScanline(vec2 uv) {
    float sourceLine = uv.y * sourceSize.y;
    float fr = fract(sourceLine);
    float exponent = mix(1.0, 8.0, scanlineWidth);
    float scanline = pow(sin(fr * 3.14159265), exponent);
    return mix(1.0, scanline, scanlineIntensity);
}

// Phosphor mask patterns
vec3 applyMask(vec2 pixelCoord) {
    vec3 mask = vec3(1.0);
    float dim = 1.0 - maskIntensity * 0.4;

    if (maskType == 1) {
        // Aperture grille: vertical RGB stripes
        int col = int(pixelCoord.x) % 3;
        if (col == 0) mask = vec3(1.0, dim, dim);
        else if (col == 1) mask = vec3(dim, 1.0, dim);
        else mask = vec3(dim, dim, 1.0);
    } else if (maskType == 2) {
        // Shadow mask: row-offset triads
        int row = int(pixelCoord.y);
        int col = (int(pixelCoord.x) + (row % 2) * 2) % 3;
        if (col == 0) mask = vec3(1.0, dim, dim);
        else if (col == 1) mask = vec3(dim, 1.0, dim);
        else mask = vec3(dim, dim, 1.0);
    } else if (maskType == 3) {
        // Slot mask: triads with horizontal gaps every 3rd row
        int row = int(pixelCoord.y);
        int col = (int(pixelCoord.x) + (row / 3 % 2) * 2) % 3;
        if (row % 3 == 2) {
            mask = vec3(dim);
        } else {
            if (col == 0) mask = vec3(1.0, dim, dim);
            else if (col == 1) mask = vec3(dim, 1.0, dim);
            else mask = vec3(dim, dim, 1.0);
        }
    }
    return mask;
}

// Tint color grading
vec3 applyTint(vec3 color) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));

    if (tintMode == 1) {
        // Amber
        vec3 tinted = vec3(luma, luma * 0.75, luma * 0.3);
        return mix(color, tinted, tintStrength);
    } else if (tintMode == 2) {
        // Green
        vec3 tinted = vec3(luma * 0.3, luma * 0.95, luma * 0.25);
        return mix(color, tinted, tintStrength);
    } else if (tintMode == 3) {
        // Monochrome
        return mix(color, vec3(luma), tintStrength);
    }
    return color;
}

// Vignette: radial corner darkening
float computeVignette(vec2 uv) {
    vec2 centered = uv * 2.0 - 1.0;
    float dist2 = dot(centered, centered);
    return smoothstep(0.0, 1.0, 1.0 - dist2 * vignetteStrength * 0.35);
}

// Afterglow with per-channel P22 phosphor decay
vec3 applyAfterglow(vec3 current, vec4 previous) {
    float dt_s = dtMs / 1000.0;
    vec3 channelRates = vec3(0.8, 1.0, 1.5) * afterglowDecaySpeed;
    vec3 decay = exp(-channelRates * dt_s);
    vec3 persisted = previous.rgb * decay;
    return max(current, persisted * afterglowStrength);
}

void main() {
    vec2 uv = vUV;
    vec2 straightUV = uv;

    // 1. Curvature
    if (curvatureAmount > 0.0) {
        uv = applyCurvature(uv, curvatureAmount);
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    // 2. Sample source (blur or sharp with UV snap)
    vec3 color;
    if (blurRadius > 0.0) {
        color = sampleBlurred(uv, blurRadius).rgb;
    } else {
        // Snap to source pixel centers for sharp rendering
        vec2 snapped;
        snapped.x = (floor(uv.x * sourceSize.x) + 0.5) / sourceSize.x;
        snapped.y = (floor(uv.y * sourceSize.y) + 0.5) / sourceSize.y;
        color = texture(sourceTexture, snapped).rgb;
    }

    // 3. Bloom
    if (bloomIntensity > 0.0) {
        color = computeBloom(uv, color);
    }

    // 4. Scanlines
    if (scanlineIntensity > 0.0) {
        float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
        float scanMul = computeScanline(straightUV);
        float resist = luma * 0.5;
        scanMul = mix(scanMul, 1.0, resist);
        color *= scanMul;
    }

    // 5. Phosphor mask
    if (maskType > 0) {
        color *= applyMask(gl_FragCoord.xy);
    }

    // 6. Tint
    if (tintMode > 0) {
        color = applyTint(color);
    }

    // 7. Vignette
    if (vignetteStrength > 0.0) {
        color *= computeVignette(uv);
    }

    // 8. Afterglow
    if (afterglowStrength > 0.0) {
        vec4 prev = texture(prevAccum, vUV);
        color = applyAfterglow(color, prev);
    }

    fragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
