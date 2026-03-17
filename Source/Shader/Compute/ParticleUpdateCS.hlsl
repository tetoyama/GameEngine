#include "../commonDefine.h"

// パーティクルの更新をコンピュートシェーダーで行う
// CPU 側の構造体（PARTICLE）とメモリ配置を一致させること
// Keep this layout identical to the C++ PARTICLE struct to avoid silent corruption.
#define MAXPARTICLE 512
#define PARTICLE_THREAD_GROUP_SIZE 64
struct ParticleData
{
    float3 Position;
    float3 Speed;
    float  LifeTime;
};

cbuffer ParticleParam : register(b0)
{
    float3 AddSpeed;
    float  pad0;
    float3 MulSpeed;
    float  pad1;
    float  DeltaTime;
    float  ParticleLifeTime;
    float2 pad2;
};

// MAXPARTICLE は 512 固定 (ParticleComponent の固定長配列に合わせる)
// MAXPARTICLE is fixed at 512 to match the CPU-side ParticleComponent array size.
RWStructuredBuffer<ParticleData> g_Particles : register(u0);

[numthreads(PARTICLE_THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    const uint index = dtid.x;
    if (index >= MAXPARTICLE)
    {
        return;
    }

    ParticleData p = g_Particles[index];
    if (p.LifeTime > 0.0f)
    {
        const float life = p.LifeTime - DeltaTime;
        if (life <= 0.0f)
        {
            p.LifeTime = 0.0f;
        }
        else
        {
            p.LifeTime = life;
            p.Position += p.Speed * DeltaTime;

            const float3 mul =
            {
                pow(MulSpeed.x, DeltaTime),
                pow(MulSpeed.y, DeltaTime),
                pow(MulSpeed.z, DeltaTime)
            };

            p.Speed = (p.Speed + AddSpeed * DeltaTime) * mul;
        }
    }

    g_Particles[index] = p;
}
