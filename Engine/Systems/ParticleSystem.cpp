/*********************************************************************************************
 \file      ParticleSystem.cpp
 \par       SofaSpuds
 \author   
 \brief     Implements a lightweight particle system for one-off gameplay effects.
 \details   Spawns and updates short-lived circle particles for effects such as
            enemy death bursts. Uses Transform + CircleRender components and
            Factory-managed lifetime.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Systems/ParticleSystem.h"

#include "Component/CircleRenderComponent.h"
#include "Component/TransformComponent.h"
#include "Common/ComponentTypeID.h"
#include "Factory/Factory.h"

#include <algorithm>
#include <cmath>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif

namespace Framework {

    ParticleSystem* ParticleSystem::instance = nullptr;

    ParticleSystem::ParticleSystem()
        : rng(std::random_device{}())
    {
        instance = this;
    }

    ParticleSystem* ParticleSystem::Instance()
    {
        return instance;
    }

    void ParticleSystem::Initialize()
    {
        particles.clear();
    }

    void ParticleSystem::Shutdown()
    {
        particles.clear();
        if (instance == this)
        {
            instance = nullptr;
        }
    }

    void ParticleSystem::Update(float dt)
    {
        if (!FACTORY)
            return;

        auto eraseParticle = [&](std::size_t index)
            {
                particles[index] = particles.back();
                particles.pop_back();
            };

        std::size_t index = 0;
        while (index < particles.size())
        {
            Particle& particle = particles[index];
            GOC* obj = FACTORY->GetObjectWithId(particle.id);
            if (!obj)
            {
                eraseParticle(index);
                continue;
            }

            particle.life = std::max(0.0f, particle.life - dt);
            if (particle.life <= 0.0f)
            {
                FACTORY->Destroy(obj);
                eraseParticle(index);
                continue;
            }

            auto* tr = obj->GetComponentType<TransformComponent>(
                ComponentTypeId::CT_TransformComponent);
            auto* circle = obj->GetComponentType<CircleRenderComponent>(
                ComponentTypeId::CT_CircleRenderComponent);

            if (!tr || !circle)
            {
                FACTORY->Destroy(obj);
                eraseParticle(index);
                continue;
            }

            tr->x += particle.velocity.x * dt;
            tr->y += particle.velocity.y * dt;

            const float t = 1.0f - (particle.life / std::max(particle.totalLife, 0.001f));
            circle->radius = particle.startRadius + (particle.endRadius - particle.startRadius) * t;
            circle->a = particle.startAlpha + (particle.endAlpha - particle.startAlpha) * t;

            particle.velocity *= (1.0f - std::min(dt * 1.5f, 0.9f));

            ++index;
        }
    }

    void ParticleSystem::SpawnEnemyDeathParticles(const glm::vec2& worldPos, std::size_t count)
    {
        if (!FACTORY || count == 0)
            return;

        std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
        std::uniform_real_distribution<float> speedDist(0.15f, 0.45f);
        std::uniform_real_distribution<float> lifeDist(0.35f, 0.6f);
        std::uniform_real_distribution<float> radiusDist(0.02f, 0.05f);
        std::uniform_real_distribution<float> hueJitter(-0.05f, 0.05f);

        for (std::size_t i = 0; i < count; ++i)
        {
            GOC* particleObj = FACTORY->CreateEmptyComposition();
            if (!particleObj)
                continue;

            particleObj->SetObjectName("EnemyDeathParticle");

            auto* tr = particleObj->EmplaceComponent<TransformComponent>(
                ComponentTypeId::CT_TransformComponent);
            tr->x = worldPos.x;
            tr->y = worldPos.y;

            auto* circle = particleObj->EmplaceComponent<CircleRenderComponent>(
                ComponentTypeId::CT_CircleRenderComponent);

            const float baseRadius = radiusDist(rng);
            circle->radius = baseRadius;
            circle->r = 1.0f;
            circle->g = 0.45f + hueJitter(rng);
            circle->b = 0.1f;
            circle->a = 0.95f;

            const float angle = angleDist(rng);
            const float speed = speedDist(rng);

            Particle particle{};
            particle.id = particleObj->GetId();
            particle.velocity = { std::cos(angle) * speed, std::sin(angle) * speed + 0.05f };
            particle.totalLife = lifeDist(rng);
            particle.life = particle.totalLife;
            particle.startRadius = baseRadius;
            particle.endRadius = baseRadius * 0.2f;
            particle.startAlpha = circle->a;
            particle.endAlpha = 0.0f;

            particles.push_back(particle);
        }
    }

} // namespace Framework