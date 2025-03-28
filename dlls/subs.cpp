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

  Last Modifed 4 May 2004 By Andrew Hamilton (AJH)
  :- Added support for acceleration of doors etc

===== subs.cpp ========================================================

  frequently used global functions

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "nodes.h"
#include "doors.h"
#include "movewith.h"
#include "player.h"
#include "locus.h"
#include "UserMessages.h"

#define ACCELTIMEINCREMENT 0.1 //AJH for acceleration/deceleration time steps

extern bool FEntIsVisible(entvars_t* pev, entvars_t* pevTarget);

// Landmark class
void CPointEntity::Spawn()
{
	pev->solid = SOLID_NOT;
}

class CNullEntity : public CBaseEntity
{
public:
	void Spawn() override;
};


// Null Entity, remove on startup
void CNullEntity::Spawn()
{
	REMOVE_ENTITY(ENT(pev));
}
LINK_ENTITY_TO_CLASS(info_null, CNullEntity);
LINK_ENTITY_TO_CLASS(info_texlights, CNullEntity); // don't complain about Merl's new info entities
LINK_ENTITY_TO_CLASS(info_compile_parameters, CNullEntity);

class CBaseDMStart : public CPointEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	STATE GetState(CBaseEntity* pEntity) override;

private:
};

// These are the new entry points to entities.
LINK_ENTITY_TO_CLASS(info_player_deathmatch, CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_start, CPointEntity);
LINK_ENTITY_TO_CLASS(info_landmark, CPointEntity);

bool CBaseDMStart::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}

STATE CBaseDMStart::GetState(CBaseEntity* pEntity)
{
	if (UTIL_IsMasterTriggered(pev->netname, pEntity))
		return STATE_ON;
	else
		return STATE_OFF;
}

// This updates global tables that need to know about entities being removed
void CBaseEntity::UpdateOnRemove()
{
	int i;
	CBaseEntity* pTemp;

	if (g_pWorld == nullptr)
	{
		ALERT(at_debug, "UpdateOnRemove has no AssistList!\n");
		return;
	}

	//LRC - remove this from the AssistList.
	for (pTemp = g_pWorld; pTemp->m_pAssistLink != nullptr; pTemp = pTemp->m_pAssistLink)
	{
		if (this == pTemp->m_pAssistLink)
		{
			//			ALERT(at_console,"REMOVE: %s removed from the Assist List.\n", STRING(pev->classname));
			pTemp->m_pAssistLink = this->m_pAssistLink;
			this->m_pAssistLink = nullptr;
			break;
		}
	}

	//LRC
	if (m_pMoveWith != nullptr)
	{
		// if I'm moving with another entity, take me out of the list. (otherwise things crash!)
		pTemp = m_pMoveWith->m_pChildMoveWith;
		if (pTemp == this)
		{
			m_pMoveWith->m_pChildMoveWith = this->m_pSiblingMoveWith;
		}
		else
		{
			while (pTemp->m_pSiblingMoveWith != nullptr)
			{
				if (pTemp->m_pSiblingMoveWith == this)
				{
					pTemp->m_pSiblingMoveWith = this->m_pSiblingMoveWith;
					break;
				}
				pTemp = pTemp->m_pSiblingMoveWith;
			}
		}
		//		ALERT(at_console,"REMOVE: %s removed from the %s ChildMoveWith list.\n", STRING(pev->classname), STRING(m_pMoveWith->pev->targetname));
	}

	//LRC - do the same thing if another entity is moving with _me_.
	if (m_pChildMoveWith != nullptr)
	{
		CBaseEntity* pCur = m_pChildMoveWith;
		CBaseEntity* pNext;
		while (pCur != nullptr)
		{
			pNext = pCur->m_pSiblingMoveWith;
			// bring children to a stop
			UTIL_SetMoveWithVelocity(pCur, g_vecZero, 100);
			UTIL_SetMoveWithAvelocity(pCur, g_vecZero, 100);
			pCur->m_pMoveWith = nullptr;
			pCur->m_pSiblingMoveWith = nullptr;
			pCur = pNext;
		}
	}

	if (FBitSet(pev->flags, FL_GRAPHED))
	{
		// this entity was a LinkEnt in the world node graph, so we must remove it from
		// the graph since we are removing it from the world.
		for (i = 0; i < WorldGraph.m_cLinks; i++)
		{
			if (WorldGraph.m_pLinkPool[i].m_pLinkEnt == pev)
			{
				// if this link has a link ent which is the same ent that is removing itself, remove it!
				WorldGraph.m_pLinkPool[i].m_pLinkEnt = nullptr;
			}
		}
	}
	if (!FStringNull(pev->globalname))
		gGlobalState.EntitySetState(pev->globalname, GLOBAL_DEAD);
}

// Convenient way to delay removing oneself
void CBaseEntity::SUB_Remove()
{
	UpdateOnRemove();
	if (pev->health > 0)
	{
		// this situation can screw up monsters who can't tell their entity pointers are invalid.
		pev->health = 0;
		ALERT(at_aiconsole, "SUB_Remove called on entity with health > 0\n");
	}

//RENDERERS START
	if (gmsgFreeEnt != 0)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgFreeEnt);
		WRITE_SHORT(entindex());
		MESSAGE_END();
	}
//RENDERERS END

	REMOVE_ENTITY(ENT(pev));
}


// Convenient way to explicitly do nothing (passed to functions that require a method)
void CBaseEntity::SUB_DoNothing()
{
	//	if (pev->ltime)
	//		ALERT(at_console, "Doing Nothing %f\n", pev->ltime);
	//	else
	//		ALERT(at_console, "Doing Nothing %f\n", gpGlobals->time);
}


// Global Savedata for Delay
TYPEDESCRIPTION CBaseDelay::m_SaveData[] =
	{
		DEFINE_FIELD(CBaseDelay, m_flDelay, FIELD_FLOAT),
		DEFINE_FIELD(CBaseDelay, m_iszKillTarget, FIELD_STRING),
		DEFINE_FIELD(CBaseDelay, m_hActivator, FIELD_EHANDLE), //LRC
};

IMPLEMENT_SAVERESTORE(CBaseDelay, CBaseEntity);

bool CBaseDelay::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "delay"))
	{
		m_flDelay = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget"))
	{
		m_iszKillTarget = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}


/*
==============================
SUB_UseTargets

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Removes all entities with a targetname that match self.killtarget,
and removes them, so some events can remove other triggers.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function (if they have one)

==============================
*/
void CBaseEntity::SUB_UseTargets(CBaseEntity* pActivator, USE_TYPE useType, float value)
{
	//
	// fire targets
	//
	if (!FStringNull(pev->target))
	{
		FireTargets(STRING(pev->target), pActivator, this, useType, value);
	}
}


void FireTargets(const char* targetName, CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	const char* inputTargetName = targetName;
	CBaseEntity* inputActivator = pActivator;
	CBaseEntity* pTarget = nullptr;
	int i, j, found = 0;
	char szBuf[80];

	if (targetName == nullptr)
		return;
	if (useType == USE_NOT)
		return;

	//LRC - allow changing of usetype
	if (targetName[0] == '+')
	{
		targetName++;
		useType = USE_ON;
	}
	else if (targetName[0] == '-')
	{
		targetName++;
		useType = USE_OFF;
	}
	else if (targetName[0] == '!') //G-cont
	{
		targetName++;
		useType = USE_KILL;
	}
	else if (targetName[0] == '>') //G-cont
	{
		targetName++;
		useType = USE_SAME;
	}

	else if (targetName[0] == '&') //AJH Use_Spawn
	{
		targetName++;
		useType = USE_SPAWN;
	}

	ALERT(at_aiconsole, "Firing: (%s)\n", targetName);

	pTarget = UTIL_FindEntityByTargetname(pTarget, targetName, pActivator);
	if (pTarget == nullptr)
	{
		// it's not an entity name; check for a locus specifier, e.g: "fadein(mywall)"
		for (i = 0; targetName[i] != 0; i++)
		{
			if (targetName[i] == '(')
			{
				i++;
				for (j = i; targetName[j] != 0; j++)
				{
					if (targetName[j] == ')')
					{
						strncpy(szBuf, targetName + i, j - i);
						szBuf[j - i] = 0;
						pActivator = CalcLocusParameter(inputActivator, szBuf);
						//						pActivator = UTIL_FindEntityByTargetname(NULL, szBuf, inputActivator);
						if (pActivator == nullptr)
						{
							//ALERT(at_console, "Missing activator \"%s\"\n", szBuf);
							return; // it's a locus specifier, but the locus is invalid.
						}
						//ALERT(at_console, "Found activator \"%s\"\n", STRING(pActivator->pev->targetname));
						found = 1;
						break;
					}
				}
				if (found == 0)
					ALERT(at_error, "Missing ')' in target value \"%s\"", inputTargetName);
				break;
			}
		}
		if (found == 0)
			return; // no, it's not a locus specifier.

		strncpy(szBuf, targetName, i - 1);
		szBuf[i - 1] = 0;
		targetName = szBuf;
		pTarget = UTIL_FindEntityByTargetname(nullptr, targetName, inputActivator);

		if (pTarget == nullptr)
			return; // it's a locus specifier all right, but the target's invalid.
	}

	do // start firing targets
	{
		if ((pTarget->pev->flags & FL_KILLME) == 0) // Don't use dying ents
		{
			if (useType == USE_KILL)
			{
				ALERT(at_aiconsole, "Use_kill on %s\n", STRING(pTarget->pev->classname));
				UTIL_Remove(pTarget);
			}
			else
			{
				ALERT(at_aiconsole, "Found: %s, firing (%s)\n", STRING(pTarget->pev->classname), targetName);
				pTarget->Use(pActivator, pCaller, useType, value);
			}
		}
		pTarget = UTIL_FindEntityByTargetname(pTarget, targetName, inputActivator);
	} while (pTarget != nullptr);

	//LRC- Firing has finished, aliases can now reflect their new values.
	UTIL_FlushAliases();
}

LINK_ENTITY_TO_CLASS(DelayedUse, CBaseDelay);


void CBaseDelay::SUB_UseTargets(CBaseEntity* pActivator, USE_TYPE useType, float value)
{
	//
	// exit immediatly if we don't have a target or kill target
	//
	if (FStringNull(pev->target) && FStringNull(m_iszKillTarget))
		return;

	//
	// check for a delay
	//
	if (m_flDelay != 0)
	{
		// create a temp object to fire at a later time
		CBaseDelay* pTemp = GetClassPtr((CBaseDelay*)nullptr);
		pTemp->pev->classname = MAKE_STRING("DelayedUse");

		pTemp->SetNextThink(m_flDelay);

		pTemp->SetThink(&CBaseDelay::DelayThink);

		// Save the useType
		pTemp->pev->button = (int)useType;
		pTemp->m_iszKillTarget = m_iszKillTarget;
		pTemp->m_flDelay = 0; // prevent "recursion"
		pTemp->pev->target = pev->target;

		//LRC - Valve had a hacked thing here to avoid breaking
		// save/restore. In Spirit that's not a problem.
		// I've moved m_hActivator into this class, for the "elegant" fix.
		pTemp->m_hActivator = pActivator;

		return;
	}

	//
	// kill the killtargets
	//

	if (!FStringNull(m_iszKillTarget))
	{
		edict_t* pentKillTarget = nullptr;

		ALERT(at_aiconsole, "KillTarget: %s\n", STRING(m_iszKillTarget));
		//LRC- now just USE_KILLs its killtarget, for consistency.
		FireTargets(STRING(m_iszKillTarget), pActivator, this, USE_KILL, 0);
	}

	//
	// fire targets
	//
	if (!FStringNull(pev->target))
	{
		FireTargets(STRING(pev->target), pActivator, this, useType, value);
	}
}


/*
void CBaseDelay::SUB_UseTargetsEntMethod()
{
	SUB_UseTargets(pev);
}
*/

/*
QuakeEd only writes a single float for angles (bad idea), so up and down are
just constant angles.
*/
void SetMovedir(entvars_t* pev)
{
	pev->movedir = GetMovedir(pev->angles);
	pev->angles = g_vecZero;
}

Vector GetMovedir(Vector vecAngles)
{
	if (vecAngles == Vector(0, -1, 0))
	{
		return Vector(0, 0, 1);
	}
	else if (vecAngles == Vector(0, -2, 0))
	{
		return Vector(0, 0, -1);
	}
	else
	{
		UTIL_MakeVectors(vecAngles);
		return gpGlobals->v_forward;
	}
}



void CBaseDelay::DelayThink()
{
	CBaseEntity* pActivator = nullptr;

	// The use type is cached (and stashed) in pev->button
	//LRC - now using m_hActivator.
	SUB_UseTargets(m_hActivator, (USE_TYPE)pev->button, 0);
	REMOVE_ENTITY(ENT(pev));
}


// Global Savedata for Toggle
TYPEDESCRIPTION CBaseToggle::m_SaveData[] =
	{
		DEFINE_FIELD(CBaseToggle, m_toggle_state, FIELD_INTEGER),
		DEFINE_FIELD(CBaseToggle, m_flActivateFinished, FIELD_TIME),
		DEFINE_FIELD(CBaseToggle, m_flMoveDistance, FIELD_FLOAT),
		DEFINE_FIELD(CBaseToggle, m_flWait, FIELD_FLOAT),
		DEFINE_FIELD(CBaseToggle, m_flLip, FIELD_FLOAT),
		DEFINE_FIELD(CBaseToggle, m_flTWidth, FIELD_FLOAT),
		DEFINE_FIELD(CBaseToggle, m_flTLength, FIELD_FLOAT),
		DEFINE_FIELD(CBaseToggle, m_vecPosition1, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CBaseToggle, m_vecPosition2, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CBaseToggle, m_vecAngle1, FIELD_VECTOR), // UNDONE: Position could go through transition, but also angle?
		DEFINE_FIELD(CBaseToggle, m_vecAngle2, FIELD_VECTOR), // UNDONE: Position could go through transition, but also angle?
		DEFINE_FIELD(CBaseToggle, m_cTriggersLeft, FIELD_INTEGER),
		DEFINE_FIELD(CBaseToggle, m_flHeight, FIELD_FLOAT),
		DEFINE_FIELD(CBaseToggle, m_pfnCallWhenMoveDone, FIELD_FUNCTION),
		DEFINE_FIELD(CBaseToggle, m_vecFinalDest, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CBaseToggle, m_flLinearMoveSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CBaseToggle, m_flAngularMoveSpeed, FIELD_FLOAT), //LRC
		DEFINE_FIELD(CBaseToggle, m_vecFinalAngle, FIELD_VECTOR),
		DEFINE_FIELD(CBaseToggle, m_sMaster, FIELD_STRING),
		DEFINE_FIELD(CBaseToggle, m_bitsDamageInflict, FIELD_INTEGER), // damage type inflicted

		DEFINE_FIELD(CBaseToggle, m_flLinearAccel, FIELD_FLOAT), //
		DEFINE_FIELD(CBaseToggle, m_flLinearDecel, FIELD_FLOAT), //
		DEFINE_FIELD(CBaseToggle, m_flAccelTime, FIELD_FLOAT),	 // AJH Needed for acceleration/deceleration
		DEFINE_FIELD(CBaseToggle, m_flDecelTime, FIELD_FLOAT),	 //
		DEFINE_FIELD(CBaseToggle, m_flCurrentTime, FIELD_FLOAT), //
		DEFINE_FIELD(CBaseToggle, m_bDecelerate, FIELD_BOOLEAN), //

};
IMPLEMENT_SAVERESTORE(CBaseToggle, CBaseAnimating);


bool CBaseToggle::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_sMaster = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = atof(pkvd->szValue);
		return true;
	}

	return CBaseDelay::KeyValue(pkvd);
}


//void CBaseToggle::LinearMove( Vector	vecInput, float flSpeed)
//{
//	LinearMove(vecInput, flSpeed);
//}

/*
=============
LinearMove

calculate pev->velocity and pev->nextthink to reach vecDest from
pev->origin traveling at flSpeed
===============
*/
void CBaseToggle::LinearMove(Vector vecInput, float flSpeed) //, bool bNow )
{
	//	ALERT(at_console, "LMove %s: %f %f %f, speed %f\n", STRING(pev->targetname), vecInput.x, vecInput.y, vecInput.z, flSpeed);
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");
	//	ASSERTSZ(m_pfnCallWhenMoveDone != NULL, "LinearMove: no post-move function defined");

	m_flLinearMoveSpeed = flSpeed;
	m_vecFinalDest = vecInput;
	m_flLinearAccel = -1.0; // AJH Not using Acceleration
	m_flLinearDecel = -1.0; //

	//	if ((m_pMoveWith || m_pChildMoveWith))// && !bNow)
	//	{
	//		ALERT(at_console,"Setting LinearMoveNow to happen after %f\n",gpGlobals->time);
	SetThink(&CBaseToggle::LinearMoveNow);
	UTIL_DesiredThink(this);
	//pev->nextthink = pev->ltime + 0.01;
	//	}
	//	else
	//	{
	//		LinearMoveNow(); // starring Martin Sheen and Marlon Brando
	//	}
}

void CBaseToggle ::LinearMove(Vector vecInput, float flSpeed, float flAccel, float flDecel) //, BOOL bNow )  // AJH Call this to use acceleration
{
	//	ALERT(at_console, "LMove %s: %f %f %f, speed %f, accel %f \n", STRING(pev->targetname), vecInput.x, vecInput.y, vecInput.z, flSpeed, flAccel);
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");
	//	ASSERTSZ(m_pfnCallWhenMoveDone != NULL, "LinearMove: no post-move function defined");

	m_flLinearMoveSpeed = flSpeed;

	m_flLinearAccel = flAccel;
	m_flLinearDecel = flDecel;
	m_flCurrentTime = 0;

	if (m_flLinearAccel > 0)
	{
		m_flAccelTime = m_flLinearMoveSpeed / m_flLinearAccel;
	}
	else
	{
		m_flLinearAccel = -1;
		m_flAccelTime = 0;
	}
	if (m_flLinearDecel > 0)
	{
		m_flDecelTime = m_flLinearMoveSpeed / m_flLinearDecel;
	}
	else
	{
		m_flLinearDecel = -1;
		m_flDecelTime = 0;
	}

	m_vecFinalDest = vecInput;

	//	if ((m_pMoveWith || m_pChildMoveWith))// && !bNow)
	//	{
	//		ALERT(at_console,"Setting LinearMoveNow to happen after %f\n",gpGlobals->time);
	SetThink(&CBaseToggle::LinearMoveNow);
	UTIL_DesiredThink(this);
	//pev->nextthink = pev->ltime + 0.01;
	//	}
	//	else
	//	{
	//		LinearMoveNow(); // starring Martin Sheen and Marlon Brando
	//	}
}

void CBaseToggle ::LinearMoveNow() // AJH Now supports acceleration
{
	//	ALERT(at_console, "LMNow %s\n", STRING(pev->targetname));

	Vector vecDest;
	//	if (m_pMoveWith || m_pChildMoveWith )
	//		ALERT(at_console,"THINK: LinearMoveNow happens at %f, speed %f\n",gpGlobals->time, m_flLinearMoveSpeed);

	if (m_pMoveWith != nullptr)
	{
		vecDest = m_vecFinalDest + m_pMoveWith->pev->origin;
	}
	else
		vecDest = m_vecFinalDest;

	// Already there?
	if (vecDest == pev->origin)
	{
		ALERT(at_console, "%s Already There!!\n", STRING(pev->targetname));
		LinearMoveDone();
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - pev->origin;

	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / m_flLinearMoveSpeed;
	if (m_flLinearAccel > 0 || m_flLinearDecel > 0)
	{ //AJH Are we using acceleration/deceleration?

		if (m_bDecelerate == true)
		{ // Are we slowing down?
			m_flCurrentTime -= ACCELTIMEINCREMENT;
			if (m_flCurrentTime <= 0)
			{
				//	ALERT(at_debug, "%s has finished moving\n", STRING(pev->targetname));
				LinearMoveDone(); //Finished slowing.

				m_flCurrentTime = 0;   //
				m_bDecelerate = false; // reset
			}
			else
			{


				UTIL_SetVelocity(this, vecDestDelta.Normalize() * (m_flLinearDecel * m_flCurrentTime)); //Slow down
				//	ALERT(at_debug, "%s is decelerating, time: %f speed: %f vector: %f %f %f\n", STRING(pev->targetname),m_flCurrentTime,(m_flLinearDecel*m_flCurrentTime),vecDestDelta.Normalize().x,vecDestDelta.Normalize().y,vecDestDelta.Normalize().z);

				// Continually calls LinearMoveNow every ACCELTIMEINCREMENT (seconds?) till stopped
				if (m_flCurrentTime < ACCELTIMEINCREMENT)
				{
					SetNextThink(m_flCurrentTime, true);
					m_flCurrentTime = 0;
				}
				else
				{
					SetNextThink(ACCELTIMEINCREMENT, true);
				}
				SetThink(&CBaseToggle ::LinearMoveNow);
			}
		}
		else
		{

			if (m_flCurrentTime < m_flAccelTime)
			{ // We are Accelerating.

				//	ALERT(at_console, "%s is accelerating\n", STRING(pev->targetname));
				UTIL_SetVelocity(this, vecDestDelta.Normalize() * (m_flLinearAccel * m_flCurrentTime));

				// Continually calls LinearMoveNow every 0.1 (seconds?) till up to speed
				SetNextThink(ACCELTIMEINCREMENT, true);
				SetThink(&CBaseToggle ::LinearMoveNow);
				m_flCurrentTime += ACCELTIMEINCREMENT;

				//BUGBUG this will mean that we will be going faster than maxspeed on the last call to 'accelerate'
			}
			else
			{ // We are now at full speed.

				//	m_flAccelTime = m_flCurrentTime;
				//	ALERT(at_console, "%s is traveling at constant speed\n", STRING(pev->targetname));

				UTIL_SetVelocity(this, vecDestDelta.Normalize() * (m_flLinearMoveSpeed)); //we are probably going slightly faster.

				// set nextthink to trigger a recall to LinearMoveNow when we need to slow down.
				SetNextThink((vecDestDelta.Length() - (m_flLinearMoveSpeed * m_flDecelTime / 2)) / (m_flLinearMoveSpeed), true);
				SetThink(&CBaseToggle ::LinearMoveNow);
				//	m_flCurrentTime = (flTravelTime);
				m_bDecelerate = true;								//Set boolean so next call we know we are decelerating.
				m_flDecelTime += (m_flCurrentTime - m_flAccelTime); //Hack to fix time increment bug
				m_flCurrentTime = m_flDecelTime;
			}
		}
	}
	else
	{ // We are not using acceleration.

		// set nextthink to trigger a call to LinearMoveDone when dest is reached
		SetNextThink(flTravelTime, true);
		SetThink(&CBaseToggle::LinearMoveDone);

		// scale the destdelta vector by the time spent traveling to get velocity
		//	pev->velocity = vecDestDelta / flTravelTime;
		UTIL_SetVelocity(this, vecDestDelta / flTravelTime);

		//	ALERT(at_console, "LMNow \"%s\": Vel %f %f %f, think %f\n", STRING(pev->targetname), pev->velocity.x, pev->velocity.y, pev->velocity.z, pev->nextthink);
	}
}

/*
============
After moving, set origin to exact final destination, call "move done" function
============
*/
/*void CBaseToggle::LinearMoveDone()
{
	Vector vecDiff;
	if (m_pMoveWith)
		vecDiff = (m_vecFinalDest + m_pMoveWith->pev->origin) - pev->origin;
	else
		vecDiff = m_vecFinalDest - pev->origin;
	if (vecDiff.Length() > 0.05) //pev->velocity.Length())
	{
		// HACK: not there yet, try waiting one more frame.
		ALERT(at_console,"Rejecting difference %f\n",vecDiff.Length());
		SetThink(LinearMoveFinalDone);
		pev->nextthink = gpGlobals->time + 0.01;
	}
	else
	{
		LinearMoveFinalDone();
	}
}*/

void CBaseToggle::LinearMoveDone()
{
	SetThink(&CBaseToggle::LinearMoveDoneNow);
	//	ALERT(at_console, "LMD: desiredThink %s\n", STRING(pev->targetname));
	UTIL_DesiredThink(this);
}

void CBaseToggle::LinearMoveDoneNow()
{
	Vector vecDest;

	//	ALERT(at_console, "LMDone %s\n", STRING(pev->targetname));

	UTIL_SetVelocity(this, g_vecZero); //, true);
									   //	pev->velocity = g_vecZero;
	if (m_pMoveWith != nullptr)
	{
		vecDest = m_vecFinalDest + m_pMoveWith->pev->origin;
		//		ALERT(at_console, "LMDone %s: p.origin = %f %f %f, origin = %f %f %f. Set it to %f %f %f\n", STRING(pev->targetname), m_pMoveWith->pev->origin.x,  m_pMoveWith->pev->origin.y,  m_pMoveWith->pev->origin.z, pev->origin.x, pev->origin.y, pev->origin.z, vecDest.x, vecDest.y, vecDest.z);
	}
	else
	{
		vecDest = m_vecFinalDest;
		//		ALERT(at_console, "LMDone %s: origin = %f %f %f. Set it to %f %f %f\n", STRING(pev->targetname), pev->origin.x, pev->origin.y, pev->origin.z, vecDest.x, vecDest.y, vecDest.z);
	}
	UTIL_AssignOrigin(this, vecDest);
	DontThink(); //LRC
	//pev->nextthink = -1;
	if (m_pfnCallWhenMoveDone != nullptr)
		(this->*m_pfnCallWhenMoveDone)();
}

bool CBaseToggle::IsLockedByMaster()
{
	return !FStringNull(m_sMaster) && !UTIL_IsMasterTriggered(m_sMaster, m_hActivator);
}

//LRC- mapping toggle-states to global states
STATE CBaseToggle::GetState()
{
	switch (m_toggle_state)
	{
	case TS_AT_TOP:
		return STATE_ON;
	case TS_AT_BOTTOM:
		return STATE_OFF;
	case TS_GOING_UP:
		return STATE_TURN_ON;
	case TS_GOING_DOWN:
		return STATE_TURN_OFF;
	default:
		return STATE_OFF; // This should never happen.
	}
};

void CBaseToggle::PlaySentence(const char* pszSentence, float duration, float volume, float attenuation)
{
	ASSERT(pszSentence != nullptr);

	if ((pszSentence == nullptr) || !IsAllowedToSpeak())
	{
		return;
	}

	PlaySentenceCore(pszSentence, duration, volume, attenuation);
}

void CBaseToggle::PlaySentenceCore(const char* pszSentence, float duration, float volume, float attenuation)
{
	if (pszSentence[0] == '!')
		EMIT_SOUND_DYN(edict(), CHAN_VOICE, pszSentence, volume, attenuation, 0, PITCH_NORM);
	else
		SENTENCEG_PlayRndSz(edict(), pszSentence, volume, attenuation, 0, PITCH_NORM);
}

void CBaseToggle::PlayScriptedSentence(const char* pszSentence, float duration, float volume, float attenuation, bool bConcurrent, CBaseEntity* pListener)
{
	PlaySentence(pszSentence, duration, volume, attenuation);
}


void CBaseToggle::SentenceStop()
{
	EMIT_SOUND(edict(), CHAN_VOICE, "common/null.wav", 1.0, ATTN_IDLE);
}

/*
=============
AngularMove

calculate pev->velocity and pev->nextthink to reach vecDest from
pev->origin traveling at flSpeed
Just like LinearMove, but rotational.
===============
*/
void CBaseToggle::AngularMove(Vector vecDestAngle, float flSpeed)
{
	ASSERTSZ(flSpeed != 0, "AngularMove:  no speed is defined!");
	//	ASSERTSZ(m_pfnCallWhenMoveDone != NULL, "AngularMove: no post-move function defined");

	m_vecFinalAngle = vecDestAngle;
	m_flAngularMoveSpeed = flSpeed;

	//	if ((m_pMoveWith || m_pChildMoveWith))// && !bNow)
	//	{
	//		ALERT(at_console,"Setting AngularMoveNow to happen after %f\n",gpGlobals->time);
	SetThink(&CBaseToggle::AngularMoveNow);
	UTIL_DesiredThink(this);
	//	ExternalThink( 0.01 );
	//		pev->nextthink = pev->ltime + 0.01;
	//	}
	//	else
	//	{
	//		AngularMoveNow(); // starring Martin Sheen and Marlon Brando
	//	}
}

void CBaseToggle::AngularMoveNow()
{
	//	ALERT(at_console, "AngularMoveNow %f\n", pev->ltime);
	Vector vecDestAngle;

	if (m_pMoveWith != nullptr)
		vecDestAngle = m_vecFinalAngle + m_pMoveWith->pev->angles;
	else
		vecDestAngle = m_vecFinalAngle;

	// Already there?
	if (vecDestAngle == pev->angles)
	{
		AngularMoveDone();
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDestAngle - pev->angles;

	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / m_flAngularMoveSpeed;

	// set nextthink to trigger a call to AngularMoveDone when dest is reached
	SetNextThink(flTravelTime, true);
	SetThink(&CBaseToggle::AngularMoveDone);

	// scale the destdelta vector by the time spent traveling to get velocity
	UTIL_SetAvelocity(this, vecDestDelta / flTravelTime);
}

void CBaseToggle::AngularMoveDone()
{
	SetThink(&CBaseToggle::AngularMoveDoneNow);
	//	ALERT(at_console, "LMD: desiredThink %s\n", STRING(pev->targetname));
	UTIL_DesiredThink(this);
}

/*
============
After rotating, set angle to exact final angle, call "move done" function
============
*/
void CBaseToggle::AngularMoveDoneNow()
{
	//	ALERT(at_console, "AngularMoveDone %f\n", pev->ltime);
	UTIL_SetAvelocity(this, g_vecZero);
	if (m_pMoveWith != nullptr)
	{
		UTIL_SetAngles(this, m_vecFinalAngle + m_pMoveWith->pev->angles);
	}
	else
	{
		UTIL_SetAngles(this, m_vecFinalAngle);
	}
	DontThink();
	if (m_pfnCallWhenMoveDone != nullptr)
		(this->*m_pfnCallWhenMoveDone)();
}

// this isn't currently used. Otherwise I'd fix it to use movedir the way it should...
float CBaseToggle::AxisValue(int flags, const Vector& angles)
{
	if (FBitSet(flags, SF_DOOR_ROTATE_Z))
		return angles.z;
	if (FBitSet(flags, SF_DOOR_ROTATE_X))
		return angles.x;

	return angles.y;
}


void CBaseToggle::AxisDir(entvars_t* pev)
{
	if (pev->movedir != g_vecZero) //LRC
		return;

	if (FBitSet(pev->spawnflags, SF_DOOR_ROTATE_Z))
		pev->movedir = Vector(0, 0, 1); // around z-axis
	else if (FBitSet(pev->spawnflags, SF_DOOR_ROTATE_X))
		pev->movedir = Vector(1, 0, 0); // around x-axis
	else
		pev->movedir = Vector(0, 1, 0); // around y-axis
}


float CBaseToggle::AxisDelta(int flags, const Vector& angle1, const Vector& angle2)
{
	if (FBitSet(flags, SF_DOOR_ROTATE_Z))
		return angle1.z - angle2.z;

	if (FBitSet(flags, SF_DOOR_ROTATE_X))
		return angle1.x - angle2.x;

	return angle1.y - angle2.y;
}


/*
=============
FEntIsVisible

returns true if the passed entity is visible to caller, even if not infront ()
=============
*/
bool FEntIsVisible(
	entvars_t* pev,
	entvars_t* pevTarget)
{
	Vector vecSpot1 = pev->origin + pev->view_ofs;
	Vector vecSpot2 = pevTarget->origin + pevTarget->view_ofs;
	TraceResult tr;

	UTIL_TraceLine(vecSpot1, vecSpot2, ignore_monsters, ENT(pev), &tr);

	if (0 != tr.fInOpen && 0 != tr.fInWater)
		return false; // sight line crossed contents

	if (tr.flFraction == 1)
		return true;

	return false;
}


//=========================================================
// LRC - info_movewith, the first entity I've made which
//       truly doesn't fit ANY preexisting category.
//=========================================================
#define SF_IMW_INACTIVE 1
#define SF_IMW_BLOCKABLE 2

class CInfoMoveWith : public CBaseEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	STATE GetState() override { return ((pev->spawnflags & SF_IMW_INACTIVE) != 0) ? STATE_OFF : STATE_ON; }
};

LINK_ENTITY_TO_CLASS(info_movewith, CInfoMoveWith);

void CInfoMoveWith::Spawn()
{
	if ((pev->spawnflags & SF_IMW_INACTIVE) != 0)
		m_MoveWith = pev->netname;
	else
		m_MoveWith = pev->target;

	if ((pev->spawnflags & SF_IMW_BLOCKABLE) != 0)
	{
		pev->solid = SOLID_SLIDEBOX;
		pev->movetype = MOVETYPE_FLY;
	}
	// and allow InitMoveWith to set things up as usual.
}

void CInfoMoveWith::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBaseEntity* pSibling;

	if (!ShouldToggle(useType))
		return;

	if (m_pMoveWith != nullptr)
	{
		// remove this from the old parent's list of children
		pSibling = m_pMoveWith->m_pChildMoveWith;
		if (pSibling == this)
			m_pMoveWith->m_pChildMoveWith = this->m_pSiblingMoveWith;
		else
		{
			while ((pSibling->m_pSiblingMoveWith != nullptr) && pSibling->m_pSiblingMoveWith != this)
			{
				pSibling = pSibling->m_pSiblingMoveWith;
			}

			if (pSibling->m_pSiblingMoveWith == this)
			{
				pSibling->m_pSiblingMoveWith = this->m_pSiblingMoveWith;
			}
			else
			{
				// failed to find myself in the list, complain
				ALERT(at_error, "info_movewith can't find itself\n");
				return;
			}
		}
		m_pMoveWith = nullptr;
		m_pSiblingMoveWith = nullptr;
	}

	if ((pev->spawnflags & SF_IMW_INACTIVE) != 0)
	{
		pev->spawnflags &= ~SF_IMW_INACTIVE;
		m_MoveWith = pev->target;
	}
	else
	{
		pev->spawnflags |= SF_IMW_INACTIVE;
		m_MoveWith = pev->netname;
	}

	// set things up for the new m_MoveWith value
	if (m_MoveWith == 0)
	{
		UTIL_SetVelocity(this, g_vecZero); // come to a stop
		return;
	}

	m_pMoveWith = UTIL_FindEntityByTargetname(nullptr, STRING(m_MoveWith));
	if (m_pMoveWith == nullptr)
	{
		ALERT(at_debug, "Missing movewith entity %s\n", STRING(m_MoveWith));
		return;
	}

	pSibling = m_pMoveWith->m_pChildMoveWith;
	while (pSibling != nullptr) // check that this entity isn't already in the list of children
	{
		if (pSibling == this)
			return;
		pSibling = pSibling->m_pSiblingMoveWith;
	}

	// add this entity to the list of children
	m_pSiblingMoveWith = m_pMoveWith->m_pChildMoveWith; // may be null: that's fine by me.
	m_pMoveWith->m_pChildMoveWith = this;
	m_vecMoveWithOffset = pev->origin - m_pMoveWith->pev->origin;
	UTIL_SetVelocity(this, g_vecZero); // match speed with the new entity
}
