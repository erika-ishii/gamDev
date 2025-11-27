/*********************************************************************************************
 \file      VfxHelpers.cpp
 \par       SofaSpuds
 \author      yimo.kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Utility helpers for spawning common gameplay VFX as GameObjectCompositions.
 \details   This module centralizes logic for creating one-off visual effects such as hit
            impact flashes. Responsibilities:
             - Ensures required VFX textures are loaded into the Resource_Manager.
             - Spawns lightweight GameObjects with Transform / Render / Sprite /
               SpriteAnimation components configured for the desired effect.
             - Returns a pointer to the newly created composition so systems can
               track or immediately forget the object (letting the level handle
               lifetime via animation/health systems).
            Current helpers:
             - SpawnHitImpactVFX(): Creates a short non-looping impact sprite-sheet
               animation at a given world position.
            Designed to keep gameplay/attack code clean by hiding the boilerplate
            VFX setup behind simple functions.
 © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*********************************************************************************************/

#include "Systems/VfxHelpers.h"

#include "Component/RenderComponent.h"
#include "Component/SpriteAnimationComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TransformComponent.h"
#include "Common/ComponentTypeID.h"
#include "Core/PathUtils.h"
#include "Factory/Factory.h"
#include "Resource_Manager/Resource_Manager.h"

#include <string>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework {
    namespace {

        /*************************************************************************************
         \brief Texture key used for the impact VFX sprite-sheet in the Resource_Manager.
        *************************************************************************************/
        constexpr std::string_view kImpactVfxTextureKey = "impact_vfx_sheet";

        /*************************************************************************************
         \brief Convenience std::string copy of the impact VFX texture key.
        *************************************************************************************/
        const std::string kImpactVfxTextureKeyStr{ kImpactVfxTextureKey };

        /*************************************************************************************
         \brief Ensure the impact VFX sprite-sheet texture is loaded into Resource_Manager.

         \details
            - Resolves the asset path for: Textures/Character/Ming_Sprite/ImpactVFX_Sprite.png
            - Checks if a texture with key kImpactVfxTextureKeyStr already exists.
            - If not present, calls Resource_Manager::load() with the resolved path.
            - Safe to call repeatedly; only loads if missing.
        *************************************************************************************/
        void EnsureImpactTextureLoaded()
        {
            const auto path = ResolveAssetPath("Textures/Character/Ming_Sprite/ImpactVFX_Sprite.png");
            if (!Resource_Manager::getTexture(kImpactVfxTextureKeyStr))
            {
                Resource_Manager::load(kImpactVfxTextureKeyStr, path.string());
            }
        }
    } // anonymous namespace

    /*****************************************************************************************
     \brief  Spawn a hit impact VFX GameObject at the specified world position.

     \param  worldPos
             World-space (x,y) where the impact effect should appear.

     \return Pointer to the newly created GOC, or nullptr if the factory is missing
             or the composition could not be created.

     \details
        - Verifies FACTORY is valid.
        - Ensures the impact VFX texture is loaded.
        - Creates an empty composition via FACTORY->CreateEmptyComposition().
        - Assigns:
            * TransformComponent: positioned at worldPos.
            * RenderComponent: small quad on layer 1 (0.25 x 0.25 units).
            * SpriteComponent: using the impact sprite-sheet texture key.
            * SpriteAnimationComponent:
                - Single "impact" animation clip.
                - 8 frames in a 1x8 sprite-sheet.
                - Non-looping at 20 FPS.
        - Sets activeAnimation to index 0 and calls SetActiveAnimation(0) so the
          animation is ready to play from the first frame.
        - The caller is responsible for letting a system (e.g., HealthSystem,
          VfxSystem, or a lifetime component) destroy this object after the
          animation finishes.
    *****************************************************************************************/
    GOC* SpawnHitImpactVFX(const glm::vec2& worldPos)
    {
        if (!FACTORY)
            return nullptr;

        EnsureImpactTextureLoaded();

        GOC* vfx = FACTORY->CreateEmptyComposition();
        if (!vfx)
            return nullptr;

        // Name the object for easier debugging/identification in the editor.
        vfx->SetObjectName(std::string(kHitImpactVfxName));

        // ------------------
        // Transform Component
        // ------------------
        auto* tr = vfx->EmplaceComponent<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        tr->x = worldPos.x;
        tr->y = worldPos.y;

        // ----------------
        // Render Component
        // ----------------
        auto* render = vfx->EmplaceComponent<RenderComponent>(ComponentTypeId::CT_RenderComponent);
        render->w = 0.25f;
        render->h = 0.25f;
        render->layer = 1;

        // ---------------
        // Sprite Component
        // ---------------
        auto* sprite = vfx->EmplaceComponent<SpriteComponent>(ComponentTypeId::CT_SpriteComponent);
        sprite->texture_key = kImpactVfxTextureKeyStr;
        sprite->texture_id = Resource_Manager::getTexture(sprite->texture_key);

        // -------------------------
        // Sprite Animation Component
        // -------------------------
        auto* anim = vfx->EmplaceComponent<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
        SpriteAnimationComponent::SpriteSheetAnimation impact{};
        impact.name = "impact";
        impact.textureKey = kImpactVfxTextureKeyStr;
        impact.spriteSheetPath = ResolveAssetPath("Textures/Character/Ming_Sprite/ImpactVFX_Sprite.png").string();
        impact.config.totalFrames = 8;
        impact.config.rows = 1;
        impact.config.columns = 8;
        impact.config.startFrame = 0;
        impact.config.endFrame = 7;
        impact.config.fps = 20.0f;
        impact.config.loop = false;
        impact.textureId = Resource_Manager::getTexture(impact.textureKey);

        anim->animations.push_back(impact);
        anim->activeAnimation = 0;
        anim->SetActiveAnimation(0);

        return vfx;
    }

} // namespace Framework
