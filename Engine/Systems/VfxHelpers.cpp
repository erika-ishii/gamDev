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

namespace Framework {
    namespace {
        constexpr std::string_view kImpactVfxTextureKey = "impact_vfx_sheet";
        const std::string          kImpactVfxTextureKeyStr{ kImpactVfxTextureKey };

        void EnsureImpactTextureLoaded()
        {
            const auto path = ResolveAssetPath("Textures/Character/Ming_Sprite/ImpactVFX_Sprite.png");
            if (!Resource_Manager::getTexture(kImpactVfxTextureKeyStr))
            {
                Resource_Manager::load(kImpactVfxTextureKeyStr, path.string());
            }
        }
    }

    GOC* SpawnHitImpactVFX(const glm::vec2& worldPos)
    {
        if (!FACTORY)
            return nullptr;

        EnsureImpactTextureLoaded();

        GOC* vfx = FACTORY->CreateEmptyComposition();
        if (!vfx)
            return nullptr;

        vfx->SetObjectName(std::string(kHitImpactVfxName));

        auto* tr = vfx->EmplaceComponent<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        tr->x = worldPos.x;
        tr->y = worldPos.y;

        auto* render = vfx->EmplaceComponent<RenderComponent>(ComponentTypeId::CT_RenderComponent);
        render->w = 0.25f;
        render->h = 0.25f;
        render->layer = 1;

        auto* sprite = vfx->EmplaceComponent<SpriteComponent>(ComponentTypeId::CT_SpriteComponent);
        sprite->texture_key = kImpactVfxTextureKeyStr;
        sprite->texture_id = Resource_Manager::getTexture(sprite->texture_key);

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