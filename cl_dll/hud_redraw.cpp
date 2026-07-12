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
// hud_redraw.cpp
//
#include <cmath>

#include "hud.h"
#include "cl_util.h"
//#include "triangleapi.h"

#if USE_VGUI
#include "vgui_TeamFortressViewport.h"
#endif

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};

extern int g_iVisibleMouse;

float HUD_GetFOV( void );

extern cvar_t *sensitivity;

// Think
void CHud::Think( void )
{
#if USE_VGUI
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);
#endif

	int newfov;
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p->m_iFlags & HUD_ACTIVE )
			pList->p->Think();
		pList = pList->pNext;
	}

	newfov = HUD_GetFOV();
	if( newfov == 0 )
	{
		m_iFOV = default_fov->value;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud
	// Set a new sensitivity
	if( m_iFOV == default_fov->value )
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / Q_max( default_fov->value, 90 )) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	// think about default fov
	if( m_iFOV == 0 )
	{
		// only let players adjust up in fov,  and only if they are not overriden by something else
		m_iFOV = Q_max( default_fov->value, 90 );  
	}

	if( gEngfuncs.IsSpectateOnly() )
	{
		m_iFOV = gHUD.m_Spectator.GetFOV(); // default_fov->value;
	}
}

// Redraw
// step through the local data,  placing the appropriate graphics & text as appropriate
// returns 1 if they've changed, 0 otherwise
#ifdef __GAMECUBE__
/* File-scope sprite handles — avoid function-local COMDAT BSS packing on GC. */
static HSPRITE s_gcLeanPain = 0;
static HSPRITE s_gcLeanTrain = 0;
static HSPRITE s_gcLeanCross = 0;
static HSPRITE s_gcLeanHud2 = 0;

/* 320hud2 digit/cross rects from valve/sprites/hud.txt (320 block). */
static void GC_LeanDrawHudDigit( HSPRITE sheet, int digit, int x, int y, int r, int g, int b )
{
	wrect_t rc;

	if( digit < 0 || digit > 9 || !sheet )
		return;
	rc.left = digit * 12;
	rc.top = 0;
	rc.right = rc.left + 12;
	rc.bottom = 16;
	SPR_Set( sheet, r, g, b );
	SPR_DrawAdditive( 0, x, y, &rc );
}

static void GC_LeanDrawHudNumber( HSPRITE sheet, int value, int x, int y, int r, int g, int b )
{
	int hundreds, tens, ones;

	if( value < 0 )
		value = 0;
	if( value > 999 )
		value = 999;
	hundreds = value / 100;
	tens = ( value / 10 ) % 10;
	ones = value % 10;
	GC_LeanDrawHudDigit( sheet, hundreds, x, y, r, g, b );
	GC_LeanDrawHudDigit( sheet, tens, x + 12, y, r, g, b );
	GC_LeanDrawHudDigit( sheet, ones, x + 24, y, r, g, b );
}
#endif

int CHud::Redraw( float flTime, int intermission )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)( m_flTime - m_fOldTime );
	static float m_flShotTime = 0;

	// Clock was reset, reset delta
	if( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;

#if USE_VGUI
	// Bring up the scoreboard during intermission
	if (gViewPort)
	{
		if( m_iIntermission && !intermission )
		{
			// Have to do this here so the scoreboard goes away
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideScoreBoard();
			gViewPort->UpdateSpectatorPanel();
		}
		else if( !m_iIntermission && intermission )
		{
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideVGUIMenu();
#if !USE_NOVGUI_SCOREBOARD
			gViewPort->ShowScoreBoard();
#endif
			gViewPort->UpdateSpectatorPanel();
			// Take a screenshot if the client's got the cvar set
			if( CVAR_GET_FLOAT( "hud_takesshots" ) != 0 )
				m_flShotTime = flTime + 1.0;	// Take a screenshot in a second
		}
	}
#else
	if( !m_iIntermission && intermission )
	{
		// Take a screenshot if the client's got the cvar set
		if( CVAR_GET_FLOAT( "hud_takesshots" ) != 0 )
			m_flShotTime = flTime + 1.0f;	// Take a screenshot in a second
	}
#endif
	if( m_flShotTime && m_flShotTime < flTime )
	{
		gEngfuncs.pfnClientCmd( "snapshot\n" );
		m_flShotTime = 0;
	}

	m_iIntermission = intermission;

#ifdef __GAMECUBE__
	/* Quality 0 skips hud.txt sheet load in VidInit; drawing the full HUD list
	 * would chase null m_rghSprites / DVD stubs and stall Host_Frame. Draw one
	 * direct-load 320 sheet as New Game HUD proof instead. */
	{
		cvar_t *quality = gEngfuncs.pfnGetCvarPointer( "gc_quality" );
		const int q = quality ? (int)quality->value : 1;

		if( q <= 0 )
		{
			/* Load number sheet first — same size as crosshairs; avoid last-slot OOM. */
			if( !s_gcLeanHud2 )
				s_gcLeanHud2 = SPR_Load( "sprites/gc_320hud2.spr" );
			if( !s_gcLeanPain )
				s_gcLeanPain = LoadSprite( "sprites/%d_pain.spr" );
			if( !s_gcLeanTrain )
				s_gcLeanTrain = LoadSprite( "sprites/%d_train.spr" );
			if( !s_gcLeanCross )
				s_gcLeanCross = SPR_Load( "sprites/crosshairs.spr" );
			if( s_gcLeanPain )
			{
				int y;

				SPR_Set( s_gcLeanPain, 255, 180, 180 );
				y = ScreenHeight - SPR_Height( s_gcLeanPain, 0 ) - 4;
				if( y < 0 )
					y = 0;
				SPR_DrawAdditive( 0, 4, y, NULL );
			}
			if( s_gcLeanTrain )
			{
				int y;

				SPR_Set( s_gcLeanTrain, 200, 220, 255 );
				y = ScreenHeight - SPR_Height( s_gcLeanTrain, 0 ) - 4;
				if( y < 0 )
					y = 0;
				SPR_DrawAdditive( 0, ScreenWidth - SPR_Width( s_gcLeanTrain, 0 ) - 4, y, NULL );
			}
			if( s_gcLeanCross )
			{
				const int w = SPR_Width( s_gcLeanCross, 0 );
				const int h = SPR_Height( s_gcLeanCross, 0 );
				int x = ( ScreenWidth - w ) / 2;
				int y = ( ScreenHeight - h ) / 2;

				if( x < 0 )
					x = 0;
				if( y < 0 )
					y = 0;
				SPR_Set( s_gcLeanCross, 0, 255, 0 );
				SPR_DrawHoles( 0, x, y, NULL );
			}
			if( s_gcLeanHud2 )
			{
				wrect_t cross, suit;
				int hp = m_Health.m_iHealth;
				int suit_val = m_Battery.m_iBat;
				int clip = m_Ammo.GetLeanClip();
				const int y = 4;

				if( hp <= 0 )
					hp = 100;
				if( suit_val < 0 )
					suit_val = 0;
				if( suit_val > 999 )
					suit_val = 999;
				/* health cross icon at hud.txt: cross 0,72 16x16 — top-left */
				cross.left = 0;
				cross.top = 72;
				cross.right = 16;
				cross.bottom = 88;
				SPR_Set( s_gcLeanHud2, 255, 160, 0 );
				SPR_DrawAdditive( 0, 4, y, &cross );
				GC_LeanDrawHudNumber( s_gcLeanHud2, hp, 22, y, 255, 160, 0 );

				/* suit_empty 20,52 20x20 — top-right with armor digits */
				suit.left = 20;
				suit.top = 52;
				suit.right = 40;
				suit.bottom = 72;
				{
					const int sx = ScreenWidth - 20 - 4 - 36;

					SPR_Set( s_gcLeanHud2, 0, 160, 255 );
					SPR_DrawAdditive( 0, sx, y, &suit );
					GC_LeanDrawHudNumber( s_gcLeanHud2, suit_val, sx + 22, y + 2, 0, 160, 255 );
				}

				/* Ammo/clip digits bottom-right when weapon has a clip. */
				if( clip >= 0 )
				{
					if( clip > 999 )
						clip = 999;
					GC_LeanDrawHudNumber( s_gcLeanHud2, clip,
						ScreenWidth - 4 - 36, ScreenHeight - 4 - 16, 255, 160, 0 );
				}
			}
			/* Once-flag lives on CHud — file/function statics re-spam on GC. */
			if( m_iGcLeanHudLog == 0 )
			{
				if( s_gcLeanPain || s_gcLeanTrain || s_gcLeanCross || s_gcLeanHud2 )
				{
					m_iGcLeanHudLog = 1;
					gEngfuncs.Con_Printf( "Xash3D GameCube: lean HUD sprites drawn pain=%d train=%d cross=%d nums=%d suit=%d ammo=%d\n",
						s_gcLeanPain ? 1 : 0, s_gcLeanTrain ? 1 : 0,
						s_gcLeanCross ? 1 : 0, s_gcLeanHud2 ? 1 : 0,
						s_gcLeanHud2 ? 1 : 0,
						( s_gcLeanHud2 && m_Ammo.GetLeanClip() >= 0 ) ? 1 : 0 );
				}
				else
				{
					m_iGcLeanHudLog = 2;
					gEngfuncs.Con_Printf( "Xash3D GameCube: lean HUD sprite load failed\n" );
				}
			}
			return 1;
		}
	}
#endif

	// if no redrawing is necessary
	// return 0;

	m_iHudNumbersYOffset = IsHL25() ? m_iFontHeight * 0.2 : 0;

	if( m_pCvarDraw->value )
	{
		HUDLIST *pList = m_pHudList;

		while( pList )
		{
			if( !intermission )
			{
				if ( ( pList->p->m_iFlags & HUD_ACTIVE ) && !( m_iHideHUDDisplay & HIDEHUD_ALL ) )
					pList->p->Draw( flTime );
			}
			else
			{
				// it's an intermission,  so only draw hud elements that are set to draw during intermissions
				if( pList->p->m_iFlags & HUD_INTERMISSION )
					pList->p->Draw( flTime );
			}

			pList = pList->pNext;
		}
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if( m_iLogo )
	{
		int x, y, i;

		if( m_hsprLogo == 0 )
			m_hsprLogo = LoadSprite( "sprites/%d_logo.spr" );

		SPR_Set( m_hsprLogo, 250, 250, 250 );

		x = SPR_Width( m_hsprLogo, 0 );
		x = ScreenWidth - x;
		y = SPR_Height( m_hsprLogo, 0 ) / 2;

		// Draw the logo at 20 fps
		int iFrame = (int)( flTime * 20 ) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive( i, x, y, NULL );
	}

	/*
	if( g_iVisibleMouse )
	{
		void IN_GetMousePos( int *mx, int *my );
		int mx, my;

		IN_GetMousePos( &mx, &my );

		if( m_hsprCursor == 0 )
		{
			m_hsprCursor = SPR_Load( "sprites/cursor.spr" );
		}

		SPR_Set( m_hsprCursor, 250, 250, 250 );

		// Draw the logo at 20 fps
		SPR_DrawAdditive( 0, mx, my, NULL );
	}
	*/

	return 1;
}

void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)( r * x );
	g = (int)( g * x );
	b = (int)( b * x );
}

const unsigned char colors[8][3] =
{
{127, 127, 127}, // additive cannot be black
{255,   0,   0},
{  0, 255,   0},
{255, 255,   0},
{  0,   0, 255},
{  0, 255, 255},
{255,   0, 255},
{240, 180,  24}
};

int CHud::DrawHudString( int xpos, int ypos, int iMaxX, const char *szIt, int r, int g, int b )
{
	if( hud_textmode->value == 2 )
	{
		gEngfuncs.pfnDrawSetTextColor( r / 255.0, g / 255.0, b / 255.0 );
		return gEngfuncs.pfnDrawConsoleString( xpos, ypos, (char*) szIt );
	}

	// xash3d: reset unicode state
	TextMessageDrawChar( 0, 0, 0, 0, 0, 0 );

	// draw the string until we hit the null character or a newline character
	for( ; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		int w = gHUD.m_scrinfo.charWidths['M'];
		if( xpos + w  > iMaxX )
			return xpos;
		if( ( *szIt == '^' ) && ( *( szIt + 1 ) >= '0') && ( *( szIt + 1 ) <= '9') )
		{
			szIt++;
			int index = (*szIt - '0') & 7;
			r = colors[index][0];
			g = colors[index][1];
			b = colors[index][2];
			if( !*(++szIt) )
				return xpos;
		}
		int c = (unsigned int)(unsigned char)*szIt;

		xpos += TextMessageDrawChar( xpos, ypos, c, r, g, b );
	}

	return xpos;
}

int DrawUtfString( int xpos, int ypos, int iMaxX, const char *szIt, int r, int g, int b )
{
	if (IsXashFWGS())
	{
		// xash3d: reset unicode state
		gEngfuncs.pfnVGUI2DrawCharacterAdditive( 0, 0, 0, 0, 0, 0, 0 );

		// draw the string until we hit the null character or a newline character
		for( ; *szIt != 0 && *szIt != '\n'; szIt++ )
		{
			int w = gHUD.m_scrinfo.charWidths['M'];
			if( xpos + w  > iMaxX )
				return xpos;
			if( ( *szIt == '^' ) && ( *( szIt + 1 ) >= '0') && ( *( szIt + 1 ) <= '9') )
			{
				szIt++;
				int index = (*szIt - '0') & 7;
				r = colors[index][0];
				g = colors[index][1];
				b = colors[index][2];
				if( !*(++szIt) )
					return xpos;
			}
			int c = (unsigned int)(unsigned char)*szIt;
			xpos += gEngfuncs.pfnVGUI2DrawCharacterAdditive( xpos, ypos, c, r, g, b, 0 );
		}
		return xpos;
	}
	else
	{
		return gHUD.DrawHudString(xpos, ypos, iMaxX, szIt, r, g, b);
	}
}

int CHud::DrawHudStringLen( const char *szIt )
{
	int l = 0;
	for( ; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		l += gHUD.m_scrinfo.charWidths[(unsigned char)*szIt];
	}
	return l;
}

int CHud::DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];
	sprintf( szString, "%d", iNumber );
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );
}

// draws a string from right to left (right-aligned)
int CHud::DrawHudStringReverse( int xpos, int ypos, int iMinX, const char *szString, int r, int g, int b )
{
	// find the end of the string
	for( const char *szIt = szString; *szIt != 0; szIt++ )
		xpos -= gHUD.m_scrinfo.charWidths[(unsigned char)*szIt];
	if( xpos < iMinX )
		xpos = iMinX;
	DrawHudString( xpos, ypos, gHUD.m_scrinfo.iWidth, szString, r, g, b );
	return xpos;
}

int CHud::DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b )
{
	int iWidth = GetSpriteRect( m_HUD_number_0 ).right - GetSpriteRect( m_HUD_number_0 ).left;
	int k;
	
	if( iNumber > 0 )
	{
		// SPR_Draw 100's
		if( iNumber >= 100 )
		{
			k = iNumber / 100;
			SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect( m_HUD_number_0 + k ) );
			x += iWidth;
		}
		else if( iFlags & ( DHN_3DIGITS ) )
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 10's
		if( iNumber >= 10 )
		{
			k = ( iNumber % 100 ) / 10;
			SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect( m_HUD_number_0 + k ) );
			x += iWidth;
		}
		else if( iFlags & ( DHN_3DIGITS | DHN_2DIGITS ) )
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect( m_HUD_number_0 + k ) );
		x += iWidth;
	}
	else if( iFlags & DHN_DRAWZERO )
	{
		SPR_Set( GetSprite( m_HUD_number_0 ), r, g, b );

		// SPR_Draw 100's
		if( iFlags & ( DHN_3DIGITS ) )
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		if( iFlags & ( DHN_3DIGITS | DHN_2DIGITS ) )
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect( m_HUD_number_0 ) );
		x += iWidth;
	}

	return x;
}

int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if( iFlags & ( DHN_3DIGITS ) )
		return 3;

	if( iFlags & ( DHN_2DIGITS ) )
		return 2;

	if( iNumber <= 0 )
	{
		if( iFlags & ( DHN_DRAWZERO ) )
			return 1;
		else
			return 0;
	}

	if( iNumber < 10 )
		return 1;

	if( iNumber < 100 )
		return 2;

	return 3;
}	

void CHud::DrawDarkRectangle( int x, int y, int wide, int tall )
{
	//gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pfnFillRGBABlend( x, y, wide, tall, 0, 0, 0, 255 * 0.6 );
	FillRGBA( x + 1, y, wide - 1, 1, 255, 140, 0, 255 );
	FillRGBA( x, y, 1, tall - 1, 255, 140, 0, 255 );
	FillRGBA( x + wide - 1, y + 1, 1, tall - 1, 255, 140, 0, 255 );
	FillRGBA( x, y + tall - 1, wide - 1, 1, 255, 140, 0, 255 );
}
