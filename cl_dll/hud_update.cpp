/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
//
//  hud_update.cpp
//

#include <cmath>
#include "hud.h"
#include "cl_util.h"
#include <stdlib.h>

int CL_ButtonBits( int );
void CL_ResetButtonBits( int bits );

extern float v_idlescale;
float in_fov;
extern void HUD_SetCmdBits( int bits );

int CHud::UpdateClientData( client_data_t *cdata, float time )
{
#ifdef __GAMECUBE__
	{
		cvar_t *quality = gEngfuncs.pfnGetCvarPointer( "gc_quality" );
		cvar_t *probe_skip = gEngfuncs.pfnGetCvarPointer( "gc_hud_probe_skip" );
		const int q = quality ? (int)quality->value : 1;
		const int skip = probe_skip ? (int)probe_skip->value : 0;

		/* Lean low-memory HUD uses direct sprite draws from Redraw().
		 * Keep the post-map probe path away from the legacy HUD button/state
		 * bookkeeping until the full client HUD is safe to advance again. */
		if( q <= 0 && skip )
		{
			memcpy( m_vecOrigin, cdata->origin, sizeof(vec3_t) );
			memcpy( m_vecAngles, cdata->viewangles, sizeof(vec3_t) );
			in_fov = cdata->fov;
			m_iFOV = cdata->fov;
			v_idlescale = 0.0f;
			return 1;
		}
	}
#endif

	memcpy( m_vecOrigin, cdata->origin, sizeof(vec3_t) );
	memcpy( m_vecAngles, cdata->viewangles, sizeof(vec3_t) );

	m_iKeyBits = CL_ButtonBits( 0 );
	m_iWeaponBits = cdata->iWeaponBits;

	in_fov = cdata->fov;

	Think();

	cdata->fov = m_iFOV;

	v_idlescale = m_iConcussionEffect;

	CL_ResetButtonBits( m_iKeyBits );

	// return 1 if in anything in the client_data struct has been changed, 0 otherwise
	return 1;
}
