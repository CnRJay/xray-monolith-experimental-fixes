#include "stdafx.h"
#include "player_hud_legs.h"
#include "player_hud.h"
#include "actor.h"
#include "inventory_item.h"
#include "Inventory.h"
#include "../Include/xrRender/Kinematics.h"
#include "../Include/xrRender/KinematicsAnimated.h"

extern BOOL g_legs_enabled;

void player_legs_controller::destroy()
{
    if (!m_model)
        return;

    IRenderVisual* v = m_model->dcast_RenderVisual();
    if (v)
        ::Render->model_Delete(v);

    m_model = nullptr;
    m_visual_name = "";
}

bool player_legs_controller::resolve_config(CActor* actor, shared_str& sect, shared_str& model)
{
    PIItem outfit = actor->inventory().ItemFromSlot(OUTFIT_SLOT);
    shared_str current_outfit = outfit
        ? outfit->object().cNameSect()
        : shared_str("");

    if (m_last_outfit_sect != current_outfit)
    {
        m_last_outfit_sect = current_outfit;
    }

    if (outfit)
    {
        if (pSettings->line_exist(current_outfit, "legs_visual"))
        {
            sect = current_outfit;
            model = pSettings->r_string(current_outfit, "legs_visual");
            return true;
        }

        if (pSettings->line_exist(current_outfit, "actor_visual"))
        {
            sect = current_outfit;
            model = pSettings->r_string(current_outfit, "actor_visual");
            return true;
        }

        warn_once("outfit [%s] has no legs_visual or actor_visual, fallback to default", current_outfit.c_str());
    }

    // default
    if (pSettings->line_exist("actor", "legs_visual"))
    {
        sect = "actor";
        model = pSettings->r_string("actor", "legs_visual");
        return true;
    }

    if (pSettings->line_exist("actor", "visual"))
    {
        sect = "actor";
        model = pSettings->r_string("actor", "visual");
        return true;
    }

    warn_once("actor has no legs_visual or visual");

    return false;
}

void player_legs_controller::warn_once(const char* fmt, ...)
{
    string512 buf;
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buf, sizeof(buf), fmt, args);
    va_end(args);

    xr_string s = make_string("! [player_legs] %s", buf).c_str();
    static xr_unordered_flat_set<xr_string> warnings;
    if (warnings.find(s) == warnings.end())
    {
        warnings.insert(s);
        Msg(s.c_str());
    }
}

bool player_legs_controller::ensure_model(const shared_str& sect, const shared_str& model)
{
    if (m_model && m_visual_name == model)
        return true;

    destroy();

    IRenderVisual* raw = ::Render->model_Create(model.c_str());
    if (!raw)
    {
        warn_once("failed to create model [%s]", model.c_str());
        return false;
    }

    IKinematics* K = smart_cast<IKinematics*>(raw);
    if (!K)
    {
        ::Render->model_Delete(raw);
        warn_once("model [%s] is not a skeleton", model.c_str());
        return false;
    }

    m_model = K;
    m_visual_name = model;

    if (pSettings->line_exist(sect, "legs_fwd_offset"))
        m_fwd_offset = pSettings->r_float(sect, "legs_fwd_offset");
    else
        m_fwd_offset = std::nullopt;

    return true;
}

// clean up later
void player_legs_controller::copy_bones_from_actor(CActor* actor)
{
    if (!actor || !m_model)
        return;

    IKinematics* actor_K = actor->Visual()->dcast_PKinematics();
    if (!actor_K)
        return;

    actor_K->CalculateBones(TRUE);

    m_model->CalculateBones_Invalidate();
    m_model->CalculateBones(TRUE);

    u16 legs_root = m_model->LL_GetBoneRoot();
    CBoneInstance& root_bi = m_model->LL_GetBoneInstance(legs_root);
    root_bi.mTransform.identity();
    root_bi.mRenderTransform.mul_43(root_bi.mTransform,
        m_model->LL_GetData(legs_root).m2b_transform);

    u16 bone_count = m_model->LL_BoneCount();
    for (u16 i = 0; i < bone_count; ++i)
    {
        if (i == legs_root)
            continue;

        LPCSTR bone_name = m_model->LL_BoneName_dbg(i);
        u16 actor_bone_id = actor_K->LL_BoneID(bone_name);

        if (actor_bone_id == BI_NONE)
            continue;

        CBoneInstance& src = actor_K->LL_GetBoneInstance(actor_bone_id);
        CBoneInstance& dst = m_model->LL_GetBoneInstance(i);

        dst.mTransform.set(src.mTransform);
        dst.mRenderTransform.mul_43(dst.mTransform,
            m_model->LL_GetData(i).m2b_transform);
    }

    for (u16 i = 0; i < bone_count; ++i)
    {
        if (i == legs_root)
            continue;

        LPCSTR bone_name = m_model->LL_BoneName_dbg(i);
        u16 actor_bone_id = actor_K->LL_BoneID(bone_name);

        if (actor_bone_id == BI_NONE)
        {
            CBoneInstance& dst = m_model->LL_GetBoneInstance(i);
            dst.mTransform.identity();
            dst.mRenderTransform.mul_43(dst.mTransform,
                m_model->LL_GetData(i).m2b_transform);
        }
    }

    u16 bone_id = m_model->LL_BoneID("bip01_head");
    if (bone_id != BI_NONE)
    {
        m_model->LL_SetBoneVisible(bone_id, false, true);
    }

    bone_id = m_model->LL_BoneID("bip01_l_upperarm");
    if (bone_id != BI_NONE)
    {
        m_model->LL_SetBoneVisible(bone_id, false, true);
    }

    bone_id = m_model->LL_BoneID("bip01_r_upperarm");
    if (bone_id != BI_NONE)
    {
        m_model->LL_SetBoneVisible(bone_id, false, true);
    }
    
}

void player_legs_controller::update(CActor* actor)
{
    if (!g_legs_enabled || !actor)
    {
        destroy();
        return;
    }

    if (actor->Holder() != nullptr)
    {
        destroy();
        return;
    }

    shared_str sect;
    shared_str model;
    if (!resolve_config(actor, sect, model))
    {
        destroy();
        return;
    }

    if (!ensure_model(sect, model))
        return;

    copy_bones_from_actor(actor);
}

float legs_fwd_offset = -0.6f;
void player_legs_controller::render()
{
    if (!g_legs_enabled || !m_model)
        return;

    CActor* actor = Actor();
    if (!actor)
        return;

    u32 move_state = actor->MovingState();
    if (move_state & mcClimb)
        return;

    IRenderVisual* visual = m_model->dcast_RenderVisual();
    if (!visual)
        return;

    m_legs_transform.set(actor->XFORM());

    Fvector fwd;
    fwd.set(m_legs_transform.k);
    fwd.y = 0.f;
    fwd.normalize_safe();

    float offset = m_fwd_offset.has_value() ? m_fwd_offset.value() : legs_fwd_offset;
    m_legs_transform.c.mad(fwd, offset);

    ::Render->set_Transform(&m_legs_transform);
    ::Render->add_Visual(visual);
}

void player_hud::update_legs(const Fmatrix& cam_trans)
{
    m_legs_controller.update(g_actor);
}

void player_hud::render_legs()
{
    m_legs_controller.render();
}

void player_hud::delete_legs_model()
{
    m_legs_controller.destroy();
}
