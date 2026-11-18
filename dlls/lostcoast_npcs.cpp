/*
lostcoast_npcs.cpp - Half-Life 2: Lost Coast characters for the GameCube port

The Lost Coast bake (lostcoast-gc tools/bake_lostcoast_studio.py) turns the
map's npc_seagull / npc_fisherman / npc_combine_s into these entities, with
their Source models converted to GoldSrc studio files. Behaviour follows
Valve's own NPC code (Source SDK 2013 npc_crow.cpp for the gulls), cut down
to what the map asks of each character, and runs on a plain 0.1 s think:
no schedules, node graph or squad code, so it stays cheap on Gekko.

Only Lost Coast's stub BSP spawns these classnames.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "animation.h"

// npc_crow.h
#define CROW_TAKEOFF_SPEED	170.0f
#define CROW_AIRSPEED		220.0f
// seagull.mdl Takeoff: AE_CROW_TAKEOFF at cycle 0.373
#define CROW_TAKEOFF_CYCLE	0.373f

// Probe switch: a line in the disc's gamecube.cfg (the GameCube boot file
// only forwards the switches the engine knows, so read it here).
static BOOL LC_BootSwitch( const char *word )
{
	int len = 0;
	char *d = (char *)LOAD_FILE_FOR_ME( (char *)"gamecube.cfg", &len );
	BOOL found = FALSE;

	if( !d )
		return FALSE;
	for( char *p = d; p < d + len; )
	{
		char *eol = p;
		size_t wl = strlen( word );

		while( eol < d + len && *eol != '\n' )
			eol++;
		if((size_t)( eol - p ) >= wl && !strncmp( p, word, wl ) && ( eol - p == (int)wl || p[wl] == '\r' || p[wl] == ' ' ))
			found = TRUE;
		p = eol + 1;
	}
	FREE_FILE( d );
	return found;
}

static float LC_Clamp( float v, float lo, float hi )
{
	return v < lo ? lo : ( v > hi ? hi : v );
}

//=========================================================
// monster_lc_seagull: npc_seagull (CNPC_Crow with the gull
// model). The map fires FlyAway,<path_corner> at spawn on two
// of its four gulls; they take off and circle the path loop
// for the rest of the level, alternating Fly and Soar.
//=========================================================
class CLCSeagull : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT GullThink( void );

private:
	enum { GULL_SIT, GULL_TAKEOFF, GULL_FLY };

	void SetSeq( const char *label, BOOL restart );
	void ChooseFlySeq( void );
	void MoveFly( float interval );
	CBaseEntity *Goal( void );

	int	m_state;
	float	m_flLast;
	float	m_flSoarTime;
	BOOL	m_bSoar;
	BOOL	m_bLaunched;
	Vector	m_vFwd;
	Vector	m_vCurrentTarget;
	Vector	m_vDesiredTarget;
	string_t m_iGoal;
};

LINK_ENTITY_TO_CLASS( monster_lc_seagull, CLCSeagull )

void CLCSeagull::Precache( void )
{
	PRECACHE_MODEL( STRING( pev->model ));
}

void CLCSeagull::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), STRING( pev->model ));
	UTIL_SetSize( pev, Vector( -8, -8, 0 ), Vector( 8, 8, 16 ));
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	m_state = GULL_SIT;
	m_iGoal = pev->target; // FlyAway target fired by the map at spawn
	UTIL_MakeVectors( pev->angles );
	m_vFwd = gpGlobals->v_forward;
	m_vCurrentTarget = m_vFwd;
	m_vDesiredTarget = g_vecZero;
	SetSeq( "idle", TRUE );
	pev->frame = RANDOM_FLOAT( 0, 255 );
	SetThink( &CLCSeagull::GullThink );
	pev->nextthink = gpGlobals->time + 0.1f + RANDOM_FLOAT( 0.0f, 0.2f );
	m_flLast = gpGlobals->time;
}

void CLCSeagull::SetSeq( const char *label, BOOL restart )
{
	int seq = LookupSequence( label );

	if( seq < 0 || ( seq == pev->sequence && !restart ))
		return;
	pev->sequence = seq;
	pev->frame = 0;
	pev->framerate = 1.0f;
	ResetSequenceInfo();
}

CBaseEntity *CLCSeagull::Goal( void )
{
	if( FStringNull( m_iGoal ))
		return NULL;
	return UTIL_FindEntityByTargetname( NULL, STRING( m_iGoal ));
}

// NPC_TranslateActivity for ACT_FLY: soar half the time while descending.
void CLCSeagull::ChooseFlySeq( void )
{
	if( m_flSoarTime < gpGlobals->time )
	{
		if( RANDOM_LONG( 0, 100 ) <= 50 && !m_bSoar && pev->velocity.z < 0 )
		{
			m_bSoar = TRUE;
			m_flSoarTime = gpGlobals->time + RANDOM_FLOAT( 1, 4 );
		}
		else
		{
			m_bSoar = FALSE;
			m_flSoarTime = gpGlobals->time + RANDOM_FLOAT( 3, 5 );
		}
	}
	SetSeq( m_bSoar ? "soar" : "fly", FALSE );
}

// CNPC_Crow::MoveCrowFly, flying a path_corner chain.
void CLCSeagull::MoveFly( float interval )
{
	CBaseEntity *goal = Goal();
	Vector vecMoveGoal, vecMoveDir, vecRight;
	float flDistance, flLerpMod, flRoll;

	if( interval > 1.0f )
		interval = 1.0f;
	if( !goal )
	{
		pev->velocity = g_vecZero;
		return;
	}
	vecMoveGoal = goal->pev->origin;
	vecMoveDir = vecMoveGoal - pev->origin;
	flDistance = vecMoveDir.Length();

	if( vecMoveGoal != m_vDesiredTarget )
		m_vDesiredTarget = vecMoveGoal;
	else
		m_vCurrentTarget = ( m_vDesiredTarget - pev->origin ).Normalize();

	flLerpMod = 0.25f;
	if( flDistance <= 256.0f )
		flLerpMod = 1.0f - ( flDistance / 256.0f );
	m_vFwd = m_vFwd + ( m_vCurrentTarget - m_vFwd ) * flLerpMod;

	// Reached the corner: on to the next one (the gull loops never end).
	if( flDistance < CROW_AIRSPEED * interval && !FStringNull( goal->pev->target ))
		m_iGoal = goal->pev->target;

	m_vFwd = m_vFwd.Normalize();
	pev->velocity = m_vFwd * CROW_AIRSPEED;

	// Bank into the turn (Source: dot(right, unnormalised move dir) * 45).
	pev->angles = UTIL_VecToAngles( m_vFwd );
	{
		Vector a = pev->angles;

		a.z = 0;
		UTIL_MakeVectors( a );
	}
	vecRight = gpGlobals->v_right;
	flRoll = LC_Clamp( DotProduct( vecRight, vecMoveDir ) * 45.0f, -45.0f, 45.0f );
	pev->angles.z = flRoll;
}

void CLCSeagull::GullThink( void )
{
	float interval = gpGlobals->time - m_flLast;

	m_flLast = gpGlobals->time;
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1f;

	switch( m_state )
	{
	case GULL_SIT:
	{
		CBaseEntity *goal = Goal();
		TraceResult tr;

		if( !goal )
			return; // no FlyAway for this one: it stays put
		// InputFlyAway: only if the target is in sight.
		UTIL_TraceLine( pev->origin + Vector( 0, 0, 8 ), goal->pev->origin, ignore_monsters, ENT( pev ), &tr );
		if( tr.flFraction != 1.0f )
		{
			m_iGoal = 0;
			return;
		}
		m_state = GULL_TAKEOFF;
		m_bLaunched = FALSE;
		SetSeq( "takeoff", TRUE );
		pev->angles.y = UTIL_VecToYaw( goal->pev->origin - pev->origin );
		break;
	}
	case GULL_TAKEOFF:
	{
		CBaseEntity *goal = Goal();

		// Takeoff(): leave the ground straight at the goal on the event.
		if( !m_bLaunched && pev->frame >= CROW_TAKEOFF_CYCLE * 255.0f && goal )
		{
			Vector dir = ( goal->pev->origin - pev->origin ).Normalize();

			m_bLaunched = TRUE;
			pev->movetype = MOVETYPE_NOCLIP;
			pev->origin.z += 1.0f;
			UTIL_SetOrigin( pev, pev->origin );
			m_vFwd = dir;
			m_vCurrentTarget = dir;
			pev->angles = UTIL_VecToAngles( dir );
			pev->velocity = dir * CROW_TAKEOFF_SPEED;
		}
		if( m_fSequenceFinished )
		{
			m_state = GULL_FLY;
			m_bSoar = FALSE;
			m_flSoarTime = gpGlobals->time + RANDOM_FLOAT( 2, 5 );
			pev->movetype = MOVETYPE_NOCLIP;
			SetSeq( "fly", TRUE );
			pev->frame = RANDOM_FLOAT( 0.0f, 0.75f ) * 255.0f; // OnChangeActivity
		}
		break;
	}
	case GULL_FLY:
		MoveFly( interval );
		{
			static int logs;
			static float next;

			if( logs < 6 && gpGlobals->time >= next )
			{
				logs++;
				next = gpGlobals->time + 10.0f;
				ALERT( at_console, "Lost Coast: seagull %d flying %s at %.0f %.0f %.0f seq=%d\n",
					ENTINDEX( ENT( pev )), STRING( m_iGoal ), pev->origin.x, pev->origin.y,
					pev->origin.z, pev->sequence );
			}
		}
		if( m_fSequenceFinished )
		{
			m_fSequenceFinished = FALSE;
			ChooseFlySeq();
		}
		break;
	}
}

//=========================================================
// monster_lc_fisherman: npc_fisherman. The map's own logic,
// baked by lostcoast-gc tools/bake_lostcoast_actors.py:
//  - waits on the jetty until the player walks into the
//    trigger that starts lcs_fisherman_intro
//  - plays that scene from its .vcd timeline: speak, lookat,
//    face, moveto (gestures are listed; layered gestures wait
//    for engine support)
//  - then ai_goal_lead lead_to_gate: walks Source's AI node
//    graph (A*) to the gate marker, waits when the player falls
//    behind LeadDistance, comes back for him (Retrieve) and
//    plays the talker's LeadComingBack / LeadRetrieve scenes
//  - looks at the player through Source's head/spine/body
//    pose parameters, baked as GoldSrc bone controllers 0..3
//=========================================================
#define LC_MAX_EVENTS	40
#define LC_MAX_SCENES	8
#define LC_MAX_NODES	512
#define LC_MAX_LINKS	1024
#define LC_MAX_ROUTE	64

enum { LCE_SPEAK, LCE_LOOKAT, LCE_FACE, LCE_MOVETO, LCE_GESTURE };

typedef struct
{
	char	type;
	char	started;
	float	t0, t1;
	float	a, b;		// speak: volume, attenuation; moveto: distance
	char	arg[52];	// the longest voice path is 48 characters
	unsigned char	ntags;	// gesture: timing tags, (event, sequence) fractions * 255
	unsigned char	tags[8];
} lc_event_t;

typedef struct
{
	char		file[48];
	int		n;
	float		end;
	lc_event_t	ev[LC_MAX_EVENTS];
} lc_scene_t;

// One fisherman per map: keep the bulky data out of entity private memory.
static lc_scene_t	g_lcScenes[LC_MAX_SCENES];
static int		g_lcNumScenes;
static float		g_lcNodes[LC_MAX_NODES][3];
static unsigned short	g_lcLinks[LC_MAX_LINKS][2];
static int		g_lcNumNodes, g_lcNumLinks;

static float LC_Approach( float target, float value, float speed )
{
	float d = target - value;

	if( d > speed )
		return value + speed;
	if( d < -speed )
		return value - speed;
	return target;
}

static lc_scene_t *LC_LoadScene( const char *file )
{
	int i, len = 0;
	char *data, *p, line[160];
	lc_scene_t *sc;

	for( i = 0; i < g_lcNumScenes; i++ )
		if( !strcmp( g_lcScenes[i].file, file ))
			return &g_lcScenes[i];
	if( g_lcNumScenes >= LC_MAX_SCENES )
		return NULL;
	data = (char *)LOAD_FILE_FOR_ME( (char *)file, &len );
	if( !data )
	{
		ALERT( at_console, "Lost Coast: scene %s missing\n", file );
		return NULL;
	}
	sc = &g_lcScenes[g_lcNumScenes++];
	memset( sc, 0, sizeof( *sc ));
	strncpy( sc->file, file, sizeof( sc->file ) - 1 );
	for( p = data; p < data + len; )
	{
		char kind[16], arg[64];
		float t0 = 0, t1 = 0, a = 0, b = 0;
		int n = 0;
		lc_event_t *ev;

		while( p < data + len && ( *p == '\n' || *p == '\r' ))
			p++;
		while( p < data + len && *p != '\n' && *p != '\r' && n < (int)sizeof( line ) - 1 )
			line[n++] = *p++;
		while( p < data + len && *p != '\n' )
			p++;
		line[n] = 0;
		if( !n || line[0] == '/' )
			continue;
		arg[0] = 0;
		if( sscanf( line, "%15s", kind ) != 1 )
			continue;
		if( !strcmp( kind, "end" ))
		{
			sscanf( line, "%*s %f", &sc->end );
			continue;
		}
		if( sc->n >= LC_MAX_EVENTS )
			continue;
		ev = &sc->ev[sc->n];
		if( !strcmp( kind, "speak" ) && sscanf( line, "%*s %f %f %63s %f %f", &t0, &t1, arg, &a, &b ) >= 3 )
			ev->type = LCE_SPEAK;
		else if( !strcmp( kind, "lookat" ) && sscanf( line, "%*s %f %f %63s", &t0, &t1, arg ) >= 2 )
			ev->type = LCE_LOOKAT;
		else if( !strcmp( kind, "face" ) && sscanf( line, "%*s %f %f %63s", &t0, &t1, arg ) >= 2 )
			ev->type = LCE_FACE;
		else if( !strcmp( kind, "moveto" ) && sscanf( line, "%*s %f %f %63s %f", &t0, &t1, arg, &a ) >= 3 )
			ev->type = LCE_MOVETO;
		else if( !strcmp( kind, "gesture" ))
		{
			float tg[8];
			int got = sscanf( line, "%*s %f %f %63s %f %f %f %f %f %f %f %f", &t0, &t1, arg,
				&tg[0], &tg[1], &tg[2], &tg[3], &tg[4], &tg[5], &tg[6], &tg[7] );

			if( got < 3 )
				continue;
			ev->type = LCE_GESTURE;
			ev->ntags = ( got - 3 ) / 2;
			for( int k = 0; k < ev->ntags * 2; k++ )
				ev->tags[k] = (unsigned char)( LC_Clamp( tg[k], 0.0f, 1.0f ) * 255.0f + 0.5f );
		}
		else
			continue;
		ev->t0 = t0;
		ev->t1 = t1;
		ev->a = a;
		ev->b = b;
		strncpy( ev->arg, arg, sizeof( ev->arg ) - 1 );
		sc->n++;
		if( ev->type == LCE_SPEAK )
			PRECACHE_SOUND( (char *)STRING( ALLOC_STRING( ev->arg )));
	}
	FREE_FILE( data );
	return sc;
}

static void LC_LoadNodes( const char *file )
{
	int len = 0, i;
	byte *d;

	if( g_lcNumNodes )
		return;
	d = LOAD_FILE_FOR_ME( (char *)file, &len );
	if( !d )
		return;
	if( len >= 8 && !memcmp( d, "LCNG", 4 ))
	{
		int nn = ( d[4] << 8 ) | d[5], nl = ( d[6] << 8 ) | d[7];
		const byte *p = d + 8;

		if( nn > LC_MAX_NODES ) nn = LC_MAX_NODES;
		if( 8 + nn * 12 + nl * 4 <= len )
		{
			for( i = 0; i < nn; i++, p += 12 )
			{
				for( int k = 0; k < 3; k++ )
				{
					unsigned int u = ( p[k * 4] << 24 ) | ( p[k * 4 + 1] << 16 ) | ( p[k * 4 + 2] << 8 ) | p[k * 4 + 3];
					float f;

					memcpy( &f, &u, 4 );
					g_lcNodes[i][k] = f;
				}
			}
			p = d + 8 + ((( d[4] << 8 ) | d[5] ) * 12 );
			for( i = 0; i < nl && g_lcNumLinks < LC_MAX_LINKS; i++, p += 4 )
			{
				int a = ( p[0] << 8 ) | p[1], b = ( p[2] << 8 ) | p[3];

				if( a < nn && b < nn )
				{
					g_lcLinks[g_lcNumLinks][0] = a;
					g_lcLinks[g_lcNumLinks][1] = b;
					g_lcNumLinks++;
				}
			}
			g_lcNumNodes = nn;
		}
	}
	FREE_FILE( d );
	ALERT( at_console, "Lost Coast: AI node graph %s: %d nodes, %d links\n", file, g_lcNumNodes, g_lcNumLinks );
}

static int LC_NearestNode( const Vector &p )
{
	int best = -1;
	float bd = 1e30f;

	for( int i = 0; i < g_lcNumNodes; i++ )
	{
		Vector d( g_lcNodes[i][0] - p.x, g_lcNodes[i][1] - p.y, ( g_lcNodes[i][2] - p.z ) * 2.0f );
		float l = d.Length();

		if( l < bd )
		{
			bd = l;
			best = i;
		}
	}
	return best;
}

// A* over the node graph (CAI_Pathfinder's job). Returns waypoint count.
static int LC_Route( const Vector &from, const Vector &to, Vector *out, int maxout )
{
	static float g[LC_MAX_NODES], f[LC_MAX_NODES];
	static short prev[LC_MAX_NODES];
	static char state[LC_MAX_NODES]; // 0 new, 1 open, 2 closed
	int s = LC_NearestNode( from ), e = LC_NearestNode( to ), i, n = 0;
	short path[LC_MAX_NODES];

	if( s < 0 || e < 0 )
		return 0;
	for( i = 0; i < g_lcNumNodes; i++ )
	{
		g[i] = 1e30f;
		prev[i] = -1;
		state[i] = 0;
	}
	g[s] = 0;
	f[s] = ( Vector( g_lcNodes[s] ) - Vector( g_lcNodes[e] )).Length();
	state[s] = 1;
	for( ;; )
	{
		int cur = -1;
		float bf = 1e30f;

		for( i = 0; i < g_lcNumNodes; i++ )
			if( state[i] == 1 && f[i] < bf )
			{
				bf = f[i];
				cur = i;
			}
		if( cur < 0 || cur == e )
			break;
		state[cur] = 2;
		for( i = 0; i < g_lcNumLinks; i++ )
		{
			int o;
			float c;

			if( g_lcLinks[i][0] == cur )
				o = g_lcLinks[i][1];
			else if( g_lcLinks[i][1] == cur )
				o = g_lcLinks[i][0];
			else
				continue;
			if( state[o] == 2 )
				continue;
			c = g[cur] + ( Vector( g_lcNodes[cur] ) - Vector( g_lcNodes[o] )).Length();
			if( c < g[o] )
			{
				g[o] = c;
				prev[o] = cur;
				f[o] = c + ( Vector( g_lcNodes[o] ) - Vector( g_lcNodes[e] )).Length();
				state[o] = 1;
			}
		}
	}
	if( prev[e] < 0 && e != s )
		return 0;
	for( i = e; i >= 0 && n < LC_MAX_NODES; i = prev[i] )
		path[n++] = i;
	{
		int k = 0;

		for( i = n - 1; i >= 0 && k < maxout - 1; i-- )
			out[k++] = Vector( g_lcNodes[path[i]] );
		out[k++] = to;
		return k;
	}
}

class CLCFisherman : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT FishThink( void );

private:
	enum { FS_WAIT, FS_SCENE, FS_LEAD, FS_DONE };

	void SetSeq( const char *label );
	CBaseEntity *Player( void );
	Vector PlayerPos( void );
	void StartScene( lc_scene_t *sc );
	void PlayGesture( lc_event_t *ev, float sceneStart );
	void RunScene( float interval );
	void RunLead( float interval );
	BOOL WalkTo( const Vector &goal, float interval, float stopdist, BOOL follow_z = TRUE );
	void UpdateLook( float interval );
	void FaceYaw( float yaw, float interval );
	void Concept( const char *concept );

	int		m_state;
	float		m_flLast;
	lc_scene_t	*m_pScene;
	float		m_flSceneStart;
	BOOL		m_bLook, m_bFace, m_bMoving;
	float		m_look[4];	// current pose values: head yaw, head pitch, spine yaw, body yaw
	float		m_lookRange[4][4];
	int		m_nLook;
	float		m_trigger[6];
	BOOL		m_bHaveTrigger;
	string_t	m_iszScene, m_iszNodes, m_iszTalker;
	Vector		m_vLeadGoal;
	float		m_flLeadYaw, m_flLeadDist, m_flRetrieveDist;
	BOOL		m_bHaveLead, m_bRetrieving, m_bWaiting;
	Vector		m_route[LC_MAX_ROUTE];
	int		m_nRoute, m_iRoute;
	float		m_flNextTalk;
	float		m_flTest;
};

LINK_ENTITY_TO_CLASS( monster_lc_fisherman, CLCFisherman )

void CLCFisherman::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "lc_trigger" ))
	{
		m_bHaveTrigger = sscanf( pkvd->szValue, "%f %f %f %f %f %f", &m_trigger[0], &m_trigger[1],
			&m_trigger[2], &m_trigger[3], &m_trigger[4], &m_trigger[5] ) == 6;
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "lc_scene" ))
	{
		m_iszScene = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "lc_nodes" ))
	{
		m_iszNodes = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "lc_talker" ))
	{
		m_iszTalker = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "lc_lead_goal" ))
	{
		m_bHaveLead = sscanf( pkvd->szValue, "%f %f %f %f", &m_vLeadGoal.x, &m_vLeadGoal.y,
			&m_vLeadGoal.z, &m_flLeadYaw ) == 4;
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "lc_lead_dist" ))
	{
		m_flLeadDist = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "lc_retrieve_dist" ))
	{
		m_flRetrieveDist = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "lc_look" ))
	{
		float *r = &m_lookRange[0][0];

		m_nLook = sscanf( pkvd->szValue, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
			r, r + 1, r + 2, r + 3, r + 4, r + 5, r + 6, r + 7, r + 8, r + 9, r + 10, r + 11,
			r + 12, r + 13, r + 14, r + 15 ) / 4;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseAnimating::KeyValue( pkvd );
}

void CLCFisherman::Precache( void )
{
	PRECACHE_MODEL( STRING( pev->model ));
	if( !FStringNull( m_iszScene ))
		LC_LoadScene( STRING( m_iszScene ));
	if( !FStringNull( m_iszTalker ))
	{
		int len = 0;
		char *d = (char *)LOAD_FILE_FOR_ME( (char *)STRING( m_iszTalker ), &len );

		if( d )
		{
			// "<Concept> scene scene ..." lines: preload the lead concepts.
			char *p = d, word[64];
			int n;

			while( p < d + len && sscanf( p, "%63s%n", word, &n ) == 1 )
			{
				p += n;
				if( word[0] == '/' )
				{
					while( p < d + len && *p != '\n' )
						p++; // comment line
					continue;
				}
				if( strstr( word, "scripts/lc/" ))
					LC_LoadScene( word );
			}
			FREE_FILE( d );
		}
	}
}

void CLCFisherman::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), STRING( pev->model ));
	UTIL_SetSize( pev, Vector( -13, -13, 0 ), Vector( 13, 13, 72 ));
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->flags |= FL_MONSTER | FL_FLY; // kinematic: walks the node graph, no gravity
	pev->takedamage = DAMAGE_NO;
	pev->yaw_speed = 90;
	m_state = FS_WAIT;
	if( m_flLeadDist <= 0 )
		m_flLeadDist = 128;
	if( m_flRetrieveDist <= 0 )
		m_flRetrieveDist = 96;
	if( !FStringNull( m_iszNodes ))
		LC_LoadNodes( STRING( m_iszNodes ));
	// The map spawns him already on "idle", so SetSeq would see no change
	// and leave the frame rate at zero: he stood frozen on frame 0.
	pev->sequence = LookupSequence( "idle" );
	if( pev->sequence < 0 )
		pev->sequence = 0;
	pev->frame = 0;
	pev->framerate = 1.0f;
	ResetSequenceInfo();
	InitBoneControllers();
	m_flTest = LC_BootSwitch( "lcscenetest" ) ? gpGlobals->time + 3.0f : 0.0f;
	SetThink( &CLCFisherman::FishThink );
	pev->nextthink = gpGlobals->time + 0.1f;
	m_flLast = gpGlobals->time;
}

void CLCFisherman::SetSeq( const char *label )
{
	int seq = LookupSequence( label );

	if( seq < 0 || seq == pev->sequence )
		return;
	pev->sequence = seq;
	pev->frame = 0;
	pev->framerate = 1.0f;
	ResetSequenceInfo();
}

CBaseEntity *CLCFisherman::Player( void )
{
	return UTIL_PlayerByIndex( 1 );
}

// Where the player is. Under -lcscenetest (a probe with nobody at the
// controls) he is taken to stand where the scene's trigger would catch him.
Vector CLCFisherman::PlayerPos( void )
{
	CBaseEntity *pl = Player();

	if( m_flTest != 0.0f && m_bHaveTrigger )
	{
		// the edge of the trigger where he would walk in from where he is
		Vector from = pl ? pl->pev->origin : pev->origin;

		return Vector( LC_Clamp( from.x, m_trigger[0], m_trigger[3] ), LC_Clamp( from.y, m_trigger[1], m_trigger[4] ),
			m_trigger[2] + 36.0f );
	}
	return pl ? pl->pev->origin : pev->origin;
}

#if XASH_GAMECUBE
extern "C" void GC_ActorPose( int ent, const char *name, float cycle ) __attribute__(( weak ));
#endif

#if XASH_GAMECUBE
// The game code and the renderer are one binary on GameCube: gestures go
// straight to the engine's table (engine/platform/gamecube/actor_gesture.c),
// which the renderer reads when it poses him.
// Weak: an engine built before the table existed still links, without gestures.
extern "C" void GC_ActorGesture( int ent, const char *name, float start, float end, int ntags, const float *tags ) __attribute__(( weak ));
#endif

void CLCFisherman::PlayGesture( lc_event_t *ev, float sceneStart )
{
#if XASH_GAMECUBE
	float tags[8];

	for( int k = 0; k < ev->ntags * 2; k++ )
		tags[k] = ev->tags[k] / 255.0f;
	if( GC_ActorGesture )
		GC_ActorGesture( ENTINDEX( edict()), ev->arg, sceneStart + ev->t0, sceneStart + ev->t1, ev->ntags, tags );
#endif
}

void CLCFisherman::StartScene( lc_scene_t *sc )
{
	if( !sc )
		return;
	m_pScene = sc;
	m_flSceneStart = gpGlobals->time;
	for( int i = 0; i < sc->n; i++ )
		sc->ev[i].started = 0;
	m_state = FS_SCENE;
	ALERT( at_console, "Lost Coast: fisherman scene %s (%d events, %.1f s)\n", sc->file, sc->n, sc->end );
}

void CLCFisherman::Concept( const char *concept )
{
	// Pick one of the talker's scenes for this concept, at random.
	int len = 0, n, count = 0;
	char *d, *p, word[64], pick[64];

	if( FStringNull( m_iszTalker ) || !( d = (char *)LOAD_FILE_FOR_ME( (char *)STRING( m_iszTalker ), &len )))
		return;
	pick[0] = 0;
	for( p = d; p < d + len; )
	{
		char *eol = p;

		while( eol < d + len && *eol != '\n' )
			eol++;
		if( sscanf( p, "%63s%n", word, &n ) == 1 && !strcmp( word, concept ))
		{
			for( p += n; p < eol && sscanf( p, "%63s%n", word, &n ) == 1; p += n )
				if( RANDOM_LONG( 0, ++count - 1 ) == 0 )
					strcpy( pick, word );
		}
		p = eol + 1;
	}
	FREE_FILE( d );
	if( pick[0] )
	{
		lc_scene_t *sc = LC_LoadScene( pick );

		if( sc )
		{
			int st = m_state;

			StartScene( sc );
			m_state = st == FS_LEAD ? FS_LEAD : FS_SCENE;
		}
	}
}

void CLCFisherman::FaceYaw( float yaw, float interval )
{
	pev->angles.y = UTIL_ApproachAngle( yaw, pev->angles.y, pev->yaw_speed * interval );
}

BOOL CLCFisherman::WalkTo( const Vector &goal, float interval, float stopdist, BOOL follow_z )
{
	Vector d = goal - pev->origin;
	float dist2 = sqrtf( d.x * d.x + d.y * d.y );
	float speed = m_flGroundSpeed > 1.0f ? m_flGroundSpeed : 50.0f;
	float step;

	if( dist2 <= stopdist + 0.5f )
	{
		m_bMoving = FALSE;
		return TRUE;
	}
	m_bMoving = TRUE;
	SetSeq( "walk" );
	FaceYaw( UTIL_VecToYaw( d ), interval );
	step = speed * interval;
	if( step > dist2 - stopdist )
		step = dist2 - stopdist;
	pev->origin.x += d.x / dist2 * step;
	pev->origin.y += d.y / dist2 * step;
	if( follow_z ) // node-graph floor: follow the waypoint heights
		pev->origin.z += d.z * ( step / ( dist2 - stopdist + step ));
	UTIL_SetOrigin( pev, pev->origin );
	return FALSE;
}

// CAI_BaseActor::UpdateHeadControl, on four controllers: the head takes
// what it can of the turn, then the spine, then the body.
void CLCFisherman::UpdateLook( float interval )
{
	float want[4] = { 0, 0, 0, 0 };
	CBaseEntity *pl = Player();
	int i;

	if( m_bLook && ( pl || m_flTest != 0.0f ))
	{
		Vector eye = pev->origin + Vector( 0, 0, 64 );
		Vector to = PlayerPos() + Vector( 0, 0, 28 ) - eye;
		float yaw = UTIL_AngleDiff( UTIL_VecToYaw( to ), pev->angles.y );
		float pitch = UTIL_VecToAngles( to ).x;
		float rest = yaw, reach = 0;

		for( i = 0; i < m_nLook; i++ )
			if( i != 1 )
				reach += Q_max( fabs( m_lookRange[i][0] ), fabs( m_lookRange[i][1] ));
		// out of reach behind him: Source drops the look rather than twist
		if( fabs( yaw ) > reach )
			rest = pitch = 0;
		for( i = 0; i < 4 && i < m_nLook; i++ )
		{
			float lo = m_lookRange[i][0], hi = m_lookRange[i][1];

			if( i == 1 )
				want[i] = LC_Clamp( pitch, lo, hi );
			else
			{
				want[i] = LC_Clamp( rest, lo, hi );
				rest -= want[i];
			}
		}
	}
	for( i = 0; i < m_nLook; i++ )
	{
		float lo = m_lookRange[i][0], hi = m_lookRange[i][1];
		float dlo = m_lookRange[i][2], dhi = m_lookRange[i][3];

		m_look[i] = LC_Approach( want[i], m_look[i], 120.0f * interval );
		(void)dlo;
		(void)dhi;
#if XASH_GAMECUBE
		// Each look pose is a blend of whole delta poses (head, spine, coat).
		// A bone controller adds to one Euler angle and turned him about the
		// wrong axis, so the pose goes to the renderer's layer instead.
		static const char *const pose[4] = { "look_head_yaw", "look_head_pitch", "look_spine_yaw", "look_body_yaw" };

		if( hi > lo && i < 4 && GC_ActorPose )
		{
			// The yaw poses run from a left turn at their low end to a right
			// turn at their high end (measured on the baked poses); m_look
			// is the turn to the left. Pitch runs down to up as m_look does.
			float c = ( m_look[i] - lo ) / ( hi - lo );

			GC_ActorPose( ENTINDEX( edict()), pose[i], i == 1 ? c : 1.0f - c );
		}
#endif
	}
}

void CLCFisherman::RunScene( float interval )
{
	float t = gpGlobals->time - m_flSceneStart;
	CBaseEntity *pl = Player();
	BOOL moving = FALSE;
	lc_scene_t *sc = m_pScene;

	m_bLook = m_bFace = FALSE;
	for( int i = 0; i < sc->n; i++ )
	{
		lc_event_t *ev = &sc->ev[i];

		if( t < ev->t0 )
			continue;
		switch( ev->type )
		{
		case LCE_SPEAK:
			if( !ev->started )
				EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, ev->arg, ev->a > 0 ? ev->a : 1.0f,
					ev->b > 0 ? ev->b : ATTN_NORM, 0, PITCH_NORM );
			break;
		case LCE_LOOKAT:
			if( t <= ev->t1 )
				m_bLook = TRUE;
			break;
		case LCE_FACE:
			if( t <= ev->t1 )
				m_bFace = TRUE;
			break;
		case LCE_GESTURE:
			if( !ev->started )
				PlayGesture( ev, m_flSceneStart );
			break;
		case LCE_MOVETO:
			// walks on past the event's end until he gets there
			if(( pl || m_flTest != 0.0f ) && ev->started != 2 )
			{
				if( WalkTo( PlayerPos(), interval, ev->a, FALSE ))
					ev->started = 2;
				else
					moving = TRUE;
			}
			break;
		}
		if( !ev->started )
			ev->started = 1;
	}
	if( m_bFace && ( pl || m_flTest != 0.0f ) && !moving )
		FaceYaw( UTIL_VecToYaw( PlayerPos() - pev->origin ), interval );
	if( !moving )
		SetSeq( "idle" );
	if( t >= sc->end && !moving )
	{
		m_pScene = NULL;
		m_bLook = TRUE;
		if( m_state == FS_SCENE && m_bHaveLead )
		{
			m_state = FS_LEAD;
			m_nRoute = LC_Route( pev->origin, m_vLeadGoal, m_route, LC_MAX_ROUTE );
			m_iRoute = 0;
			m_flNextTalk = gpGlobals->time + 5.0f;
			ALERT( at_console, "Lost Coast: fisherman leads to %.0f %.0f %.0f over %d waypoints\n",
				m_vLeadGoal.x, m_vLeadGoal.y, m_vLeadGoal.z, m_nRoute );
		}
		else if( m_state == FS_SCENE )
			m_state = FS_DONE;
	}
}

// ai_goal_lead / CAI_LeadBehavior: lead while the player keeps within
// LeadDistance, wait for him otherwise, and with Retrieve set go back for
// him once he is lost, until he is within RetrieveDistance again.
void CLCFisherman::RunLead( float interval )
{
	CBaseEntity *pl = Player();
	float pd = pl ? ( pl->pev->origin - pev->origin ).Length() : 0.0f;
	static int test = -1;

	if( test < 0 )
		test = m_flTest != 0.0f ? 1 : 0;
	if( test )
		pd = 0.0f; // probe without a player at the controls: he walks on

	m_bLook = pl && pd < 512.0f;
	if( m_iRoute >= m_nRoute )
	{
		SetSeq( "idle" );
		FaceYaw( m_flLeadYaw, interval );
		if( m_state != FS_DONE )
			ALERT( at_console, "Lost Coast: fisherman reached the lead goal\n" );
		m_state = FS_DONE;
		return;
	}
	if( m_bRetrieving )
	{
		if( !pl || WalkTo( pl->pev->origin, interval, m_flRetrieveDist, FALSE ))
		{
			m_bRetrieving = FALSE;
			Concept( "LeadRetrieve" );
			m_nRoute = LC_Route( pev->origin, m_vLeadGoal, m_route, LC_MAX_ROUTE );
			m_iRoute = 0;
		}
		return;
	}
	if( pd > m_flLeadDist )
	{
		SetSeq( "idle" );
		if( pl )
			FaceYaw( UTIL_VecToYaw( pl->pev->origin - pev->origin ), interval );
		if( !m_bWaiting )
		{
			m_bWaiting = TRUE;
			m_flNextTalk = gpGlobals->time + 3.0f;
		}
		if( gpGlobals->time >= m_flNextTalk )
		{
			m_flNextTalk = gpGlobals->time + RANDOM_FLOAT( 8.0f, 12.0f );
			if( pd > m_flLeadDist * 2.0f )
			{
				m_bRetrieving = TRUE;
				Concept( "LeadComingBack" );
			}
		}
		return;
	}
	m_bWaiting = FALSE;
	if( WalkTo( m_route[m_iRoute], interval, m_iRoute == m_nRoute - 1 ? 4.0f : 16.0f ))
		m_iRoute++;
}

void CLCFisherman::FishThink( void )
{
	float interval = gpGlobals->time - m_flLast;
	CBaseEntity *pl = Player();

	m_flLast = gpGlobals->time;
	if( interval > 0.5f )
		interval = 0.5f;
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1f;

	switch( m_state )
	{
	case FS_WAIT:
	{
		BOOL go = FALSE;

		if( m_flTest > 0 && gpGlobals->time >= m_flTest )
			go = TRUE;
		if( pl && m_bHaveTrigger )
		{
			const Vector &o = pl->pev->origin;

			go |= o.x >= m_trigger[0] && o.y >= m_trigger[1] && o.z >= m_trigger[2] - 36
				&& o.x <= m_trigger[3] && o.y <= m_trigger[4] && o.z <= m_trigger[5] + 36;
		}
		if( go && !FStringNull( m_iszScene ))
			StartScene( LC_LoadScene( STRING( m_iszScene )));
		break;
	}
	case FS_SCENE:
		RunScene( interval );
		break;
	case FS_LEAD:
		if( m_pScene )
		{
			// a guide line plays over the lead without stopping it
			if( gpGlobals->time - m_flSceneStart >= m_pScene->end )
				m_pScene = NULL;
			else
			{
				float t = gpGlobals->time - m_flSceneStart;

				for( int i = 0; i < m_pScene->n; i++ )
				{
					lc_event_t *ev = &m_pScene->ev[i];

					if( t >= ev->t0 && !ev->started )
					{
						ev->started = 1;
						if( ev->type == LCE_GESTURE )
							PlayGesture( ev, m_flSceneStart );
						if( ev->type == LCE_SPEAK )
							EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, ev->arg, ev->a > 0 ? ev->a : 1.0f,
								ev->b > 0 ? ev->b : ATTN_NORM, 0, PITCH_NORM );
					}
				}
			}
		}
		RunLead( interval );
		break;
	case FS_DONE:
		SetSeq( "idle" );
		if( m_bHaveLead && m_iRoute >= m_nRoute && m_nRoute > 0 )
			FaceYaw( m_flLeadYaw, interval ); // the goal marker's facing
		if( pl )
			m_bLook = ( pl->pev->origin - pev->origin ).Length() < 256.0f;
		break;
	}
	UpdateLook( interval );
	{
		static int logs;
		static float next;

		if( logs < 40 && gpGlobals->time >= next && m_state != FS_WAIT )
		{
			logs++;
			next = gpGlobals->time + 2.0f;
			ALERT( at_console, "Lost Coast: fisherman state=%d at %.0f %.0f %.0f yaw=%.0f seq=%d look=%.0f/%.0f/%.0f/%.0f\n",
				m_state, pev->origin.x, pev->origin.y, pev->origin.z, pev->angles.y, pev->sequence,
				m_look[0], m_look[1], m_look[2], m_look[3] );
		}
	}
}
