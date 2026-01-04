#pragma once

#include "script_object.h"

#ifdef PROJECTOR_NEW
#include "PhysicsShellHolder.h"
#include "PHSkeleton.h"
#include "../xrphysics/PHUpdateObject.h"
#include "script_game_object.h"

struct SProjectorLight;
struct SProjectorControl;
#endif

class CLAItem;

#ifdef PROJECTOR_NEW
class CProjector : public CPhysicsShellHolder
, public CPHUpdateObject
, public CPHSkeleton
#else
class CProjector : public CScriptObject
#endif
{
#ifdef PROJECTOR_NEW
	typedef CPhysicsShellHolder inherited;
#else
	typedef CScriptObject inherited;

	friend void BoneCallbackX(CBoneInstance* B);
	friend void BoneCallbackY(CBoneInstance* B);

	float fBrightness;
	CLAItem* lanim;
	Fvector m_pos;
	ref_light light_render;
	ref_glow glow_render;

	u16 guid_bone;

	struct SBoneRot
	{
		float velocity;
		u16 id;
	} bone_x, bone_y;

	struct
	{
		float yaw;
		float pitch;
	} _start, _current, _target;
#endif

public:
	CProjector();
	virtual ~CProjector();

	virtual void Load(LPCSTR section);
	virtual BOOL net_Spawn(CSE_Abstract* DC);
#ifdef PROJECTOR_NEW
	virtual void net_Destroy();
#endif
	virtual void shedule_Update(u32 dt); // Called by sheduler
	virtual void UpdateCL(); // Called each frame, so no need for dt
	virtual void renderable_Render();

	virtual BOOL UsedAI_Locations();

#ifdef PROJECTOR_NEW
	/* Remove. */
#else
	virtual bool bfAssignWatch(CScriptEntityAction* tpEntityAction);
	virtual bool bfAssignObject(CScriptEntityAction* tpEntityAction);
#endif

	Fvector GetCurrentDirection();

#ifdef PROJECTOR_NEW
	/* Remove. */
#else
private:
	void TurnOn();
	void TurnOff();

	// Rotation routines
	static void _BCL BoneCallbackX(CBoneInstance* B);
	static void _BCL BoneCallbackY(CBoneInstance* B);

	void SetTarget(const Fvector& target_pos);
#endif

#ifdef PROJECTOR_NEW
private:
	bool m_active;

	xr_vector<SProjectorLight> m_lights;
	xr_vector<SProjectorControl> m_controls;

	void CreateSkeleton(CSE_Abstract *po);
	virtual void PhDataUpdate(float step) {};
	virtual void PhTune(float step) {};
	virtual CPhysicsShellHolder *PPhysicsShellHolder() { return static_cast<CPhysicsShellHolder *>(this); }

protected:
	virtual void SpawnInitPhysics(CSE_Abstract *D);
	virtual void net_Save(NET_Packet &P);
	virtual BOOL net_SaveRelevant();

public:
	virtual BOOL AlwaysTheCrow();
	virtual bool is_ai_obstacle() const;
	virtual CPhysicsShellHolder *cast_physics_shell_holder() { return this; }

	IC bool IsActive() { return m_active; }

	enum
	{
		eActive = 0,
		eSwitch,
		eDesiredPos,
		eDesiredDir,
		eDesiredAng,
	};
	void Action(int id, u32 flags, LPCSTR sec);
	void SetParam(int id, Fvector val, LPCSTR sec);

public:
DECLARE_SCRIPT_REGISTER_FUNCTION
#endif
};

#ifdef PROJECTOR_NEW
	struct SProjectorLight
{
	shared_str m_sec;
	CProjector *O;

	CLAItem *m_lanim;
	ref_light m_light;
	ref_glow m_glow;
	float m_brightness;
	u16 m_light_bid;
	u16 m_guide_bid;

	SProjectorLight(CProjector *obj, LPCSTR sec);
	~SProjectorLight();
	bool IsSection(LPCSTR sec);
	void Load(const CInifile *ini, LPCSTR section);
	void Switch(bool val);
	void UpdateCL();

	void SetLanim(LPCSTR str);
	void SetType(int type);
	void SetCone(float val);
	void SetColor(Fcolor clr);
	void SetRange(float val);
	void SetTexture(LPCSTR str);

	void SetGlowRange(float val);
	void SetGlowTexture(LPCSTR str);
};

struct SProjectorControl
{
	shared_str m_sec;
	CProjector *O;

	u16 m_rotate_x_bone;
	u16 m_rotate_y_bone;
	float m_rotate_x_speed, m_rotate_y_speed;
	float m_tgt_x_rot, m_tgt_y_rot, m_cur_x_rot, m_cur_y_rot, m_bind_x_rot, m_bind_y_rot;
	Fmatrix m_i_bind_x_xform, m_i_bind_y_xform;
	Fvector2 m_lim_x_rot, m_lim_y_rot;
	Fvector m_desire_dir;
	Fvector m_desire_ang;
	bool m_enable_ang = true;

	SProjectorControl(CProjector *obj, LPCSTR sec);
	~SProjectorControl();
	bool IsSection(LPCSTR sec);
	void Load(const CInifile *ini, LPCSTR section);
	void UpdateCL();
	void SetBoneCallbacks(bool enable);
	void ClampRotationHorz(float &tgt_val, const float &cur_val, const float &lim_min, const float &lim_max);
	static void _BCL BoneCallbackX(CBoneInstance *B);
	static void _BCL BoneCallbackY(CBoneInstance *B);
	void SetParam(int id, Fvector val);
};
#endif