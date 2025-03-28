/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== lights.cpp ========================================================

  spawn and think functions for editor-placed lights

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "UserMessages.h"

//RENDERERS START
#include "player.h"
//RENDERERS END

//LRC
int GetStdLightStyle(int iStyle)
{
	// Fran: Trinity uses fullbright everytime, because it uses its own styles in bsprenderer.cpp
	
//RENDERERS START
	return MAKE_STRING("m");
//RENDERERS END

	/*
	switch (iStyle)
	{
	// 0 normal
	case 0:
		return MAKE_STRING("m");

	// 1 FLICKER (first variety)
	case 1:
		return MAKE_STRING("mmnmmommommnonmmonqnmmo");

	// 2 SLOW STRONG PULSE
	case 2:
		return MAKE_STRING("abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	// 3 CANDLE (first variety)
	case 3:
		return MAKE_STRING("mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

	// 4 FAST STROBE
	case 4:
		return MAKE_STRING("mamamamamama");

	// 5 GENTLE PULSE 1
	case 5:
		return MAKE_STRING("jklmnopqrstuvwxyzyxwvutsrqponmlkj");

	// 6 FLICKER (second variety)
	case 6:
		return MAKE_STRING("nmonqnmomnmomomno");

	// 7 CANDLE (second variety)
	case 7:
		return MAKE_STRING("mmmaaaabcdefgmmmmaaaammmaamm");

	// 8 CANDLE (third variety)
	case 8:
		return MAKE_STRING("mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

	// 9 SLOW STROBE (fourth variety)
	case 9:
		return MAKE_STRING("aaaaaaaazzzzzzzz");

	// 10 FLUORESCENT FLICKER
	case 10:
		return MAKE_STRING("mmamammmmammamamaaamammma");

	// 11 SLOW PULSE NOT FADE TO BLACK
	case 11:
		return MAKE_STRING("abcdefghijklmnopqrrqponmlkjihgfedcba");

	// 12 UNDERWATER LIGHT MUTATION
	// this light only distorts the lightmap - no contribution
	// is made to the brightness of affected surfaces
	case 12:
		return MAKE_STRING("mmnnmmnnnmmnn");

	// 13 OFF (LRC)
	case 13:
		return MAKE_STRING("a");

	// 14 SLOW FADE IN (LRC)
	case 14:
		return MAKE_STRING("aabbccddeeffgghhiijjkkllmmmmmmmmmmmmmm");

	// 15 MED FADE IN (LRC)
	case 15:
		return MAKE_STRING("abcdefghijklmmmmmmmmmmmmmmmmmmmmmmmmmm");

	// 16 FAST FADE IN (LRC)
	case 16:
		return MAKE_STRING("acegikmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm");

	// 17 SLOW FADE OUT (LRC)
	case 17:
		return MAKE_STRING("llkkjjiihhggffeeddccbbaaaaaaaaaaaaaaaa");

	// 18 MED FADE OUT (LRC)
	case 18:
		return MAKE_STRING("lkjihgfedcbaaaaaaaaaaaaaaaaaaaaaaaaaaa");

	// 19 FAST FADE OUT (LRC)
	case 19:
		return MAKE_STRING("kigecaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

	default:
		return MAKE_STRING("m");
	}
	*/
}


class CLight : public CPointEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void Think() override;

// RENDERERS START
	void SendInitMessage(CBasePlayer* player) override;
	void EXPORT LightStyleThink();
// RENDERERS END

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	STATE GetState() override { return m_iState; }; //LRC

	static TYPEDESCRIPTION m_SaveData[];

	int GetStyle() { return m_iszCurrentStyle; }; //LRC
	void SetStyle(int iszPattern);				  //LRC

	void SetCorrectStyle(); //LRC

private:
	STATE m_iState;		   // current state
	int m_iOnStyle;		   // style to use while on
	int m_iOffStyle;	   // style to use while off
	int m_iTurnOnStyle;	   // style to use while turning on
	int m_iTurnOffStyle;   // style to use while turning off
	int m_iTurnOnTime;	   // time taken to turn on
	int m_iTurnOffTime;	   // time taken to turn off
	int m_iszPattern;	   // custom style to use while on
	int m_iszCurrentStyle; // current style string

// RENDERERS START
	bool m_bAlreadySent;   // State Message
	bool m_bSpawnDone;     // Don't send message before spawn
	// RENDERERS END
};
LINK_ENTITY_TO_CLASS(light, CLight);

TYPEDESCRIPTION CLight::m_SaveData[] =
	{
		DEFINE_FIELD(CLight, m_iState, FIELD_INTEGER),
		DEFINE_FIELD(CLight, m_iszPattern, FIELD_STRING),
		DEFINE_FIELD(CLight, m_iszCurrentStyle, FIELD_STRING),
		DEFINE_FIELD(CLight, m_iOnStyle, FIELD_INTEGER),
		DEFINE_FIELD(CLight, m_iOffStyle, FIELD_INTEGER),
		DEFINE_FIELD(CLight, m_iTurnOnStyle, FIELD_INTEGER),
		DEFINE_FIELD(CLight, m_iTurnOffStyle, FIELD_INTEGER),
		DEFINE_FIELD(CLight, m_iTurnOnTime, FIELD_INTEGER),
		DEFINE_FIELD(CLight, m_iTurnOffTime, FIELD_INTEGER),

// RENDERERS START
		DEFINE_FIELD(CLight, m_bAlreadySent, FIELD_BOOLEAN),
// RENDERERS END
};

IMPLEMENT_SAVERESTORE(CLight, CPointEntity);


//
// Cache user-entity-field values until spawn is called.
//
bool CLight::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iOnStyle"))
	{
		m_iOnStyle = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iOffStyle"))
	{
		m_iOffStyle = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOnStyle"))
	{
		m_iTurnOnStyle = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOffStyle"))
	{
		m_iTurnOffStyle = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOnTime"))
	{
		m_iTurnOnTime = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOffTime"))
	{
		m_iTurnOffTime = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		pev->angles.x = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "pattern"))
	{
		m_iszPattern = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget"))
	{
		pev->target = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}

void CLight::SetStyle(int iszPattern)
{
	if (m_iStyle < 32) // if it's using a global style, don't change it
		return;
	m_iszCurrentStyle = iszPattern;
	//	ALERT(at_console, "SetStyle %d \"%s\"\n", m_iStyle, (char *)STRING( iszPattern ));

	if (m_bSpawnDone)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgLightStyle, nullptr);
			WRITE_BYTE(m_iStyle);
			WRITE_STRING(STRING(m_iszCurrentStyle));
		MESSAGE_END();
		
	}
	else 
	{
		SetNextThink(0.1f); // Retry until spawn is done
	}

	LIGHT_STYLE(m_iStyle, (char*)STRING(iszPattern));
}

// regardless of what's been set by trigger_lightstyle ents, set the style I think I need
void CLight::SetCorrectStyle()
{
	if (m_iStyle >= 32)
	{
		switch (m_iState)
		{
		case STATE_ON:
			if (m_iszPattern != 0) // custom styles have priority over standard ones
				SetStyle(m_iszPattern);
			else if (m_iOnStyle != 0)
				SetStyle(GetStdLightStyle(m_iOnStyle));
			else
				SetStyle(MAKE_STRING("m"));
			break;
		case STATE_OFF:
			if (m_iOffStyle != 0)
				SetStyle(GetStdLightStyle(m_iOffStyle));
			else
				SetStyle(MAKE_STRING("a"));
			break;
		case STATE_TURN_ON:
			if (m_iTurnOnStyle != 0)
				SetStyle(GetStdLightStyle(m_iTurnOnStyle));
			else
				SetStyle(MAKE_STRING("a"));
			break;
		case STATE_TURN_OFF:
			if (m_iTurnOffStyle != 0)
				SetStyle(GetStdLightStyle(m_iTurnOffStyle));
			else
				SetStyle(MAKE_STRING("m"));
			break;
		}
	}
	else
	{
		m_iszCurrentStyle = GetStdLightStyle(m_iStyle);
	}
}

void CLight::Think()
{
	switch (GetState())
	{
	case STATE_TURN_ON:
		m_iState = STATE_ON;
		FireTargets(STRING(pev->target), this, this, USE_ON, 0);
		break;
	case STATE_TURN_OFF:
		m_iState = STATE_OFF;
		FireTargets(STRING(pev->target), this, this, USE_OFF, 0);
		break;
	}
	SetCorrectStyle();
}

void CLight::SendInitMessage(CBasePlayer* player)
{
	char szPattern[64];
	memset(szPattern, 0, sizeof(szPattern));

	if (m_iStyle >= 32)
	{
		if (FBitSet(pev->spawnflags, SF_LIGHT_START_OFF))
			strcpy(szPattern, "a");
		else if (m_iszPattern != 0)
			strcpy(szPattern, (char*)STRING(m_iszPattern));
		else
			strcpy(szPattern, "m");

		if (player != nullptr)
			MESSAGE_BEGIN(MSG_ONE, gmsgLightStyle, nullptr, player->pev);
		else
			MESSAGE_BEGIN(MSG_ALL, gmsgLightStyle, nullptr);

			WRITE_BYTE(m_iStyle);
			WRITE_STRING(STRING(m_iszPattern));
		MESSAGE_END();
	}

	m_bAlreadySent = true;
}

/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) LIGHT_START_OFF
Non-displayed light.
Default light value is 300
Default style is 0
If targeted, it will toggle between on or off.
*/

void CLight::Spawn()
{
	m_bSpawnDone = false;

	if (FStringNull(pev->targetname))
	{ // inert light
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	if (FBitSet(pev->spawnflags, SF_LIGHT_START_OFF))
		m_iState = STATE_OFF;
	else
		m_iState = STATE_ON;
	SetCorrectStyle();

	m_bSpawnDone = true;
}


void CLight::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (m_iStyle >= 32)
	{
		if (!ShouldToggle(useType))
			return;

		switch (GetState())
		{
		case STATE_ON:
		case STATE_TURN_ON:
			if (m_iTurnOffTime != 0)
			{
				m_iState = STATE_TURN_OFF;
				SetNextThink(m_iTurnOffTime);
			}
			else
				m_iState = STATE_OFF;
			break;
		case STATE_OFF:
		case STATE_TURN_OFF:
			if (m_iTurnOnTime != 0)
			{
				m_iState = STATE_TURN_ON;
				SetNextThink(m_iTurnOnTime);
			}
			else
				m_iState = STATE_ON;
			break;
		}
		SetCorrectStyle();
	}
}

//
// shut up spawn functions for new spotlights
//
LINK_ENTITY_TO_CLASS(light_spot, CLight);


class CEnvLight : public CLight
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Spawn() override;
};

LINK_ENTITY_TO_CLASS(light_environment, CEnvLight);

bool CEnvLight::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "_light"))
	{
		int r = 0, g = 0, b = 0, v = 0;
		char szColor[64];
		const int j = sscanf(pkvd->szValue, "%d %d %d %d\n", &r, &g, &b, &v);
		if (j == 1)
		{
			g = b = r;
		}
		else if (j == 4)
		{
			r = r * (v / 255.0);
			g = g * (v / 255.0);
			b = b * (v / 255.0);
		}

		// simulate qrad direct, ambient,and gamma adjustments, as well as engine scaling
		r = pow(r / 114.0, 0.6) * 264;
		g = pow(g / 114.0, 0.6) * 264;
		b = pow(b / 114.0, 0.6) * 264;

		sprintf(szColor, "%d", r);
		CVAR_SET_STRING("sv_skycolor_r", szColor);
		sprintf(szColor, "%d", g);
		CVAR_SET_STRING("sv_skycolor_g", szColor);
		sprintf(szColor, "%d", b);
		CVAR_SET_STRING("sv_skycolor_b", szColor);

		return true;
	}

	return CLight::KeyValue(pkvd);
}


void CEnvLight::Spawn()
{
	char szVector[64];
	UTIL_MakeAimVectors(pev->angles);

	sprintf(szVector, "%f", gpGlobals->v_forward.x);
	CVAR_SET_STRING("sv_skyvec_x", szVector);
	sprintf(szVector, "%f", gpGlobals->v_forward.y);
	CVAR_SET_STRING("sv_skyvec_y", szVector);
	sprintf(szVector, "%f", gpGlobals->v_forward.z);
	CVAR_SET_STRING("sv_skyvec_z", szVector);

	CLight::Spawn();
}

//**********************************************************
//LRC- the CLightDynamic entity - works like the flashlight.
//**********************************************************

#define SF_LIGHTDYNAMIC_START_OFF 1
#define SF_LIGHTDYNAMIC_FLARE 2

class CLightDynamic : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void SetEffects();
	STATE GetState() override;
};

LINK_ENTITY_TO_CLASS(light_glow, CLightDynamic);

void CLightDynamic::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "sprites/null.spr");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if ((pev->spawnflags & SF_LIGHTDYNAMIC_START_OFF) == 0)
	{
		pev->health = 1;
		SetEffects();
	}
}

void CLightDynamic::Precache()
{
	PRECACHE_MODEL("sprites/null.spr");
}

void CLightDynamic::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (ShouldToggle(useType, pev->health != 0.0f))
	{
		if (pev->health != 0.0f)
			pev->health = 0;
		else
			pev->health = 1;
		SetEffects();
	}
}

void CLightDynamic::SetEffects()
{
	if (pev->health != 0.0f)
	{
		if (pev->frags == 2)
			pev->effects |= EF_BRIGHTLIGHT;
		else if (pev->frags != 0.0f)
			pev->effects |= EF_DIMLIGHT;

		if ((pev->spawnflags & SF_LIGHTDYNAMIC_FLARE) != 0)
			pev->effects |= EF_LIGHT;
	}
	else
	{
		pev->effects &= ~(EF_DIMLIGHT | EF_BRIGHTLIGHT | EF_LIGHT);
	}
}

STATE CLightDynamic::GetState()
{
	if (pev->health != 0.0f)
		return STATE_ON;
	else
		return STATE_OFF;
}

//**********************************************************
//LRC- the CTriggerLightstyle entity - changes the style of a light temporarily.
//**********************************************************
class CLightFader : public CPointEntity
{
public:
	void EXPORT FadeThink();
	void EXPORT WaitThink();
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	CLight* m_pLight;
	char m_cFrom;
	char m_cTo;
	char m_szCurStyle[2];
	float m_fEndTime;
	int m_iszPattern;
	float m_fStep;
	int m_iWait;
};

LINK_ENTITY_TO_CLASS(lightfader, CLightFader);

TYPEDESCRIPTION CLightFader::m_SaveData[] =
	{
		DEFINE_FIELD(CLightFader, m_pLight, FIELD_CLASSPTR),
		DEFINE_FIELD(CLightFader, m_cFrom, FIELD_CHARACTER),
		DEFINE_FIELD(CLightFader, m_cTo, FIELD_CHARACTER),
		DEFINE_ARRAY(CLightFader, m_szCurStyle, FIELD_CHARACTER, 2),
		DEFINE_FIELD(CLightFader, m_fEndTime, FIELD_FLOAT),
		DEFINE_FIELD(CLightFader, m_iszPattern, FIELD_STRING),
		DEFINE_FIELD(CLightFader, m_fStep, FIELD_FLOAT),
		DEFINE_FIELD(CLightFader, m_iWait, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CLightFader, CPointEntity);

void CLightFader::FadeThink()
{
	if (m_fEndTime > gpGlobals->time)
	{
		m_szCurStyle[0] = m_cTo + (char)((m_cFrom - m_cTo) * (m_fEndTime - gpGlobals->time) * m_fStep);
		m_szCurStyle[1] = 0; // null terminator
							 //		ALERT(at_console, "FadeThink: %s %s\n", STRING(m_pLight->pev->classname), m_szCurStyle);
		m_pLight->SetStyle(MAKE_STRING(m_szCurStyle));
		SetNextThink(0.1);
	}
	else
	{
		// fade is finished
		m_pLight->SetStyle(m_iszPattern);
		if (m_iWait > -1)
		{
			// wait until it's time to switch off
			SetThink(&CLightFader::WaitThink);
			SetNextThink(m_iWait);
		}
		else
		{
			// we've finished, kill the fader
			SetThink(&CLightFader::SUB_Remove);
			SetNextThink(0.1);
		}
	}
}

// we've finished. revert the light and kill the fader.
void CLightFader::WaitThink()
{
	m_pLight->SetCorrectStyle();
	SetThink(&CLightFader::SUB_Remove);
	SetNextThink(0.1);
}



class CTriggerLightstyle : public CPointEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_iszPattern;
	int m_iFade;
	int m_iWait;
};

LINK_ENTITY_TO_CLASS(trigger_lightstyle, CTriggerLightstyle);

TYPEDESCRIPTION CTriggerLightstyle::m_SaveData[] =
	{
		DEFINE_FIELD(CTriggerLightstyle, m_iszPattern, FIELD_STRING),
		DEFINE_FIELD(CTriggerLightstyle, m_iFade, FIELD_INTEGER),
		DEFINE_FIELD(CTriggerLightstyle, m_iWait, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CTriggerLightstyle, CBaseEntity);

bool CTriggerLightstyle::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "pattern"))
	{
		m_iszPattern = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iFade"))
	{
		m_iFade = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iWait"))
	{
		m_iWait = atoi(pkvd->szValue);
		return true;
	}
	return CBaseEntity::KeyValue(pkvd);
}

void CTriggerLightstyle::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBaseEntity* pTarget = nullptr;
	if (pev->target == 0u)
		return;

	//ALERT( at_console, "Lightstyle change for: (%s)\n", STRING(pev->target) );

	for (;;)
	{
		pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(pev->target), pActivator);
		if (FNullEnt(pTarget))
			break;

		int iszPattern;
		if (m_iszPattern != 0)
			iszPattern = m_iszPattern;
		else
			iszPattern = GetStdLightStyle(m_iStyle);

		// not a light entity?
		if (!FClassnameIs(pTarget->pev, "light") && !FClassnameIs(pTarget->pev, "light_spot") && !FClassnameIs(pTarget->pev, "light_environment"))
		{
			if (pTarget->m_iStyle >= 32)
				LIGHT_STYLE(pTarget->m_iStyle, (char*)STRING(iszPattern));
		}
		else
		{
			CLight* pLight = (CLight*)pTarget;

			if (m_iFade != 0)
			{
				//				ALERT(at_console, "Making fader ent, step 1/%d = %f\n", m_iFade, 1/m_iFade);
				CLightFader* pFader = GetClassPtr((CLightFader*)nullptr);
				pFader->m_pLight = pLight;
				pFader->m_cFrom = ((char*)STRING(pLight->GetStyle()))[0];
				pFader->m_cTo = ((char*)STRING(iszPattern))[0];
				pFader->m_iszPattern = iszPattern;
				pFader->m_fEndTime = gpGlobals->time + m_iFade;
				pFader->m_fStep = ((float)1) / m_iFade;
				pFader->m_iWait = m_iWait;
				pFader->SetThink(&CLightFader::FadeThink);
				pFader->SetNextThink(0.1);
			}
			else
			{
				pLight->SetStyle(iszPattern);
				if (m_iWait != -1)
				{
					CLightFader* pFader = GetClassPtr((CLightFader*)nullptr);
					pFader->m_pLight = pLight;
					pFader->SetThink(&CLightFader::WaitThink);
					pFader->SetNextThink(m_iWait);
				}
			}
		}
	}
}
