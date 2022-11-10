constexpr char shaderHLSL[] = R"_(
struct VSInput {
    uint instId : SV_InstanceID;
    uint vertexId : SV_VertexID;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(VSInput input) {
	PSInput output;
	output.uv = float2(input.vertexId%2, input.vertexId%4/2);
	output.position = float4((output.uv.x-0.5f)*2.0, -(output.uv.y-0.5f)*2, 0.0, 1.0);
	return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 sampledTexture = g_texture.Sample(g_sampler, input.uv); // float4(input.uv.x, input.uv.y, 0.0, 1.0);
	return float4(sampledTexture.x, sampledTexture.y, sampledTexture.z, 1.0);
}
)_";


struct constantBufferShader {
	float renderWidth;
	float renderHeight;
	float swapchainWidth;
	float swapchainHeight;
	float eyeSeparation;
	float showWholeScreen; // this mode could be used to show each display a part of the screen
	float showSingleScreen; // this mode shows the same picture in each eye
	float singleScreenScale;
	float zoomOutLevel;
	float gap0;
	float gap1;
	float gap2;
};

constexpr unsigned short screenIndices[] = {
	0, 1, 2,
	2, 1, 3,
};
