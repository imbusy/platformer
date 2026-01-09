// Text rendering shader for bitmap font rendering
// Uses texture sampling with alpha blending

struct TextUniforms {
    transform: mat4x4<f32>,
    color: vec4<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: TextUniforms;
@group(0) @binding(1) var font_texture: texture_2d<f32>;
@group(0) @binding(2) var font_sampler: sampler;

struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) uv: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.transform * vec4<f32>(in.position, 0.0, 1.0);
    out.uv = in.uv;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let tex_color = textureSample(font_texture, font_sampler, in.uv);
    
    // BMFont textures typically have white glyphs - use alpha channel
    // If alpha is 1 everywhere, fallback to using red/luminance channel
    var alpha = tex_color.a;
    if (alpha > 0.99) {
        alpha = max(max(tex_color.r, tex_color.g), tex_color.b);
    }
    
    // Return the text color with the sampled alpha
    return vec4<f32>(uniforms.color.rgb, uniforms.color.a * alpha);
}
