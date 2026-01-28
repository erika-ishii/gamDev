/*********************************************************************************************
 \file      ParticleSystem.h
 \par       SofaSpuds
 \author    
 \brief     Defines a lightweight particle system for one-off gameplay effects.
 \details   Spawns and updates short-lived circle particles. Designed for simple
            VFX bursts such as enemy death impacts without requiring sprite sheets.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once

#include "Common/System.h"
#include "Composition/Composition.h"
#include <glm/vec2.hpp>
#include <random>
#include <vector>

namespace Framework {

    /*****************************************************************************************
      \class ParticleSystem
      \brief Simple runtime particle system for short-lived circle particles.

      Provides a spawn helper for enemy death bursts and updates particle motion/fade
      each frame. Internally stores particle metadata keyed by GOC IDs so particles
      can be destroyed safely by the factory.
    *****************************************************************************************/
    class ParticleSystem : public ISystem {
    public:
        ParticleSystem();

        void Initialize() override;
        void Update(float dt) override;
        void Shutdown() override;

        std::string GetName() override { return "ParticleSystem"; }

        static ParticleSystem* Instance();

        void SpawnEnemyDeathParticles(const glm::vec2& worldPos, std::size_t count = 12);

    private:
        struct Particle {
            GOCId id{};
            glm::vec2 velocity{};
            float life = 0.0f;
            float totalLife = 0.0f;
            float startRadius = 0.0f;
            float endRadius = 0.0f;
            float startAlpha = 1.0f;
            float endAlpha = 0.0f;
        };

        std::vector<Particle> particles;
        std::mt19937 rng;

        static ParticleSystem* instance;
    };

} // namespace Framework