struct VertexInput {
  float2 position;
  float3 color;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 color;
};

[shader("vertex")]
VertexOutput vertex_main(VertexInput input) {
    VertexOutput output;
    output.sv_position = float4(input.position, 0.5, 1.0);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragment_main(VertexOutput vs_output) : SV_Target
{
    return float4(vs_output.color, 1.0);
}