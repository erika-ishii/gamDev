#include "DecisionNodeHelpers.h"
#ifdef _DEBUG
#define new DBG_NEW       
#endif
namespace Framework
{
	float GetAnimationDuration(GOC* goc, const std::string& name)
	{
        if (!goc) return 0.2f;
        auto* anim = goc->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
        if (!anim) return 0.2f;
        for (auto& a : anim->animations)
        {if (a.name == name)return a.config.totalFrames / a.config.fps;}
        return 0.2f;
	}

    void PlayAnimationIfAvailable(GOC* goc, std::string_view name, bool forceRestart)
    {
        if (!goc) return;
        auto* anim = goc->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
        if (!anim) return;

        auto equalsIgnoreCase = [](std::string_view a, std::string_view b)
        {
                if (a.size() != b.size()) return false;
                for (size_t i = 0; i < a.size(); ++i)
                    if (std::tolower(a[i]) != std::tolower(b[i])) return false;
                return true;
        };

        int idx = -1;
        for (size_t i = 0; i < anim->animations.size(); ++i)
        {
            if (equalsIgnoreCase(anim->animations[i].name, name))
            {idx = static_cast<int>(i);break;}
        }

        if (idx >= 0 && idx != anim->ActiveAnimationIndex()||forceRestart)
        {anim->SetActiveAnimation(idx);}
    }

    bool IsPlayerNear(GOC* enemy, float radius)
    {
        if (!enemy) return false;
        GOC* player = nullptr;
        for (auto& kv : FACTORY->Objects())
        {
            GOC* goc = kv.second.get();
            if (!goc) continue;
            if (goc->GetComponent(ComponentTypeId::CT_PlayerComponent)) { player = goc; break; }
        }
        if (!player) return false;

        auto* trEnemy = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        auto* trPlayer = player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        if (!trEnemy || !trPlayer) return false;

        float dx = trEnemy->x - trPlayer->x;
        float dy = trEnemy->y - trPlayer->y;
        return (dx * dx + dy * dy) <= (radius * radius);
    }
    
    void Patrol(void* enemy, float dt)
    {
        if (!enemy) return;

        auto* e = static_cast<GOC*>(enemy);
        auto* rb = e->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
        auto* tr = e->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        auto* ai = e->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);

        if (!rb || !tr || !ai) return;

        const float speed = 0.2f;
        const float range = 0.5f;
        const float pause = 2.0f;

        if (ai->pauseTimer > 0.0f)
        {
            ai->pauseTimer -= dt;
            rb->velX = 0.0f;
            return;
        }

        rb->velX = speed * ai->dir;
        rb->velY = 0.0f;

        tr->x += rb->velX * dt;

        if (tr->x < -range) { tr->x = -range; ai->dir = 1.0f; ai->pauseTimer = pause; }
        if (tr->x > range) { tr->x = range; ai->dir = -1.0f; ai->pauseTimer = pause; }
    }
 
    void Idle(void* enemy, float)
    {
        if (!enemy) return;
        auto* e = static_cast<GOC*>(enemy);
        if (auto* rb = e->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent))
        {
            rb->velX = 0.0f;
            rb->velY = 0.0f;
        }
    }

    //static void MeleeAttack(GOC* enemy, float dt, LogicSystem* logic)
    //{
    //    if (!enemy || !logic) return;

    //    auto* attack = enemy->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent);
    //    auto* rb = enemy->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
    //    auto* tr = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
    //    auto* ai = enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
    //    auto* typeComp = enemy->GetComponentType<EnemyTypeComponent>(ComponentTypeId::CT_EnemyTypeComponent);
    //    auto* audio = enemy->GetComponentType<AudioComponent>(ComponentTypeId::CT_AudioComponent);

    //    if (!attack || !rb || !tr || !ai)
    //        return;

    //    GOC* player = nullptr;
    //    for (auto& kv : FACTORY->Objects())
    //    {
    //        GOC* goc = kv.second.get();
    //        if (!goc) continue;
    //        if (goc->GetComponent(ComponentTypeId::CT_PlayerComponent)) { player = goc; break; }
    //    }
    //    if (!player) return;

    //    auto* trPlayer = player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
    //    if (!trPlayer) return;

    //    float dx = trPlayer->x - tr->x;
    //    float dy = trPlayer->y - tr->y;
    //    float distance = std::sqrt(dx * dx + dy * dy);

    //    ai->facing = (dx < 0.0f) ? Facing::LEFT : Facing::RIGHT;

    //    attack->attack_timer += dt;

    //    if (!typeComp || typeComp->Etype != EnemyTypeComponent::EnemyType::ranged)
    //    {
    //        if (attack->attack_timer >= attack->attack_speed && distance < 0.8f && !attack->hitbox->active)
    //        {
    //            attack->attack_timer = 0.0f;
    //            attack->hitbox->active = true;

    //            float direction = (ai->facing == Facing::LEFT) ? -1.0f : 1.0f;
    //            float hbWidth = rb->width * 1.2f;
    //            float hbHeight = rb->height * 0.8f;

    //            float spawnX = tr->x + direction * hbWidth * 0.25f;
    //            float spawnY = tr->y;

    //            attack->hitbox->duration = GetAnimationDuration(enemy, "slashattack");

    //            logic->hitBoxSystem->SpawnHitBox(
    //                enemy, spawnX, spawnY, hbWidth, hbHeight,
    //                static_cast<float>(attack->damage),
    //                attack->hitbox->duration,
    //                HitBoxComponent::Team::Enemy
    //            );

    //            if (audio) audio->TriggerSound("EnemyAttack");
    //            PlayAnimationIfAvailable(enemy, "slashattack", true);
    //        }

    //        if (attack->hitbox->active)
    //        {
    //            attack->hitboxElapsed += dt;
    //            if (attack->hitboxElapsed >= attack->hitbox->duration)
    //            {
    //                attack->hitbox->active = false;
    //                attack->hitboxElapsed = 0.0f;
    //            }
    //        }
    //    }
    //    else
    //    {
    //        // ---- Ranged logic ----
    //        if (attack->attack_timer >= attack->attack_speed && distance < 3.5f)
    //        {
    //            attack->attack_timer = -3.0f;

    //            float norm = (distance > 0.001f) ? distance : 1.0f;
    //            float dirX = dx / norm;
    //            float dirY = dy / norm;

    //            logic->hitBoxSystem->SpawnProjectile(
    //                enemy, tr->x, tr->y, dirX, dirY,
    //                0.2f, 0.3f, 0.15f,
    //                static_cast<float>(attack->damage),
    //                3.0f,
    //                HitBoxComponent::Team::Enemy
    //            );

    //            if (audio) audio->TriggerSound("EnemyAttack");
    //            PlayAnimationIfAvailable(enemy, "rangeattack", true);
    //        }
    //    }
    //}

    //void RangedAttack(GOC* enemy, float dt, LogicSystem* logic)
    //{
    //    if (!enemy || !logic) return;

    //    auto* e = static_cast<GOC*>(enemy);

    //    auto* attack = e->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent);
    //    auto* rb = e->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
    //    auto* tr = e->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
    //    auto* ai = e->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
    //    auto* audio = e->GetComponentType<AudioComponent>(ComponentTypeId::CT_AudioComponent);

    //    if (!attack || !rb || !tr || !ai) return;
    //    GOC* player = nullptr;
    //    for (auto& kv : FACTORY->Objects())
    //    {
    //        GOC* goc = kv.second.get();
    //        if (!goc) continue;
    //        if (goc->GetComponent(ComponentTypeId::CT_PlayerComponent))
    //        {
    //            player = goc;
    //            break;
    //        }
    //    }
    //    if (!player) return;
    //    auto* trPlayer = player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
    //    if (!trPlayer) return;
    //    float dx = trPlayer->x - tr->x;
    //    float dy = trPlayer->y - tr->y;
    //    float distance = std::sqrt(dx * dx + dy * dy);
    //    ai->facing = (dx < 0.0f) ? Facing::LEFT : Facing::RIGHT;
    //    const float speed = 1.0f;
    //    const float stopDistance = 1.0f;
    //    const float accel = 2.0f;

    //    if (distance > stopDistance)
    //    {
    //        float norm = (distance > 0.001f) ? distance : 1.0f;
    //        float targetVX = (dx / norm) * speed;
    //        float targetVY = (dy / norm) * speed;

    //        rb->velX += (targetVX - rb->velX) * std::min(accel * dt, 1.0f);
    //        rb->velY += (targetVY - rb->velY) * std::min(accel * dt, 1.0f);
    //    }
    //    else
    //    {
    //        rb->velX *= 0.5f;
    //        rb->velY *= 0.5f;
    //    }

    //    // Update attack timer
    //    attack->attack_timer += dt;

    //    if (attack->attack_timer >= attack->attack_speed && distance < 3.5f)
    //    {
    //        attack->attack_timer = 0.0f;

    //        float norm = (distance > 0.001f) ? distance : 1.0f;
    //        float dirX = dx / norm;
    //        float dirY = dy / norm;
    //        float spawnX = tr->x;
    //        float spawnY = tr->y;
    //        logic->hitBoxSystem->SpawnProjectile(
    //            enemy,
    //            spawnX, spawnY,
    //            dirX, dirY,
    //            0.2f, 0.3f, 0.15f,
    //            static_cast<float>(attack->damage),
    //            3.0f,
    //            HitBoxComponent::Team::Enemy
    //        );

    //        if (audio) audio->TriggerSound("EnemyAttack");

    //        PlayAnimationIfAvailable(e, "rangeattack", true);
    //    }
    //    else if (attack->attack_timer > 0.5f)
    //    {
    //        // fallback to idle after shooting
    //        PlayAnimationIfAvailable(e, "idle");
    //    }
    //}


}