// Sprite rendering shader with procedural arrow/triangle pattern
// Uses transformation matrix for position, rotation, and scale

struct Uniforms {
    transform: mat4x4<f32>,
    color: vec4<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

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
    // Create a simple arrow/triangle sprite pattern
    let uv = in.uv - vec2<f32>(0.5, 0.5);
    
    // Arrow body (rectangle)
    let body = abs(uv.x) < 0.15 && uv.y > -0.3 && uv.y < 0.2;
    
    // Arrow head (triangle pointing up)
    let head_y = uv.y - 0.2;
    let head = head_y > 0.0 && head_y < 0.3 && abs(uv.x) < (0.3 - head_y);
    
    if (body || head) {
        return uniforms.color;
    }
    
    // Transparent background - return fully transparent
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
}
