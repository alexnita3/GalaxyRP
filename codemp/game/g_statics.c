/*
===========================================================================
DAJ_RP: [Static Models] the map's misc_model_static, as the server knows them

A misc_model_static is drawn by each client from its own copy of the map file and nothing else: the
server frees it as it spawns (SP_misc_model_static, g_misc.c), and it is not networked. But the server
reads the same entity string from the same map file, so while the map loads it can note each one -- its
model, origin, angles and scale, read exactly as the client reads them (SP_misc_model_static in
cg_spawn.c) -- before the entity goes. That list is what Entity Bounds (/settings 6) draws in red and
what /entcopystatic copies (g_entbounds.c, g_entgrab.c).

Only the map's own: the map loading (level.spawning, before RP_MarkMapEntities() sets rp_map_loaded),
and not inside a misc_bsp's sub-BSP (the client reads only the map's own entity string). An entity
file, /entadd or a per-map fix never adds one -- a client would never see it.

The numbers (#0, #1, ...) are the order the map lists them in, which is the order the client loads them
in too. The counts live in level, so a map load starts the list afresh; the arrays are kept here, out of
level, being large. A model's bounds are read from its md3 the first time they are needed, once per
model per map (RP_ReadModelBounds, g_utils.c); one the server does not have, or that is not an md3,
gets the editor's box of the class instead.

A client with another version of the map draws its own statics, and still sees the server's here.
===========================================================================
*/

#include "g_local.h"

#define RP_STATIC_NAME_POOL		65536

typedef struct {
	int			name;			// offset of its path in rp_static_names
	signed char	state;			// 0 not read yet, 1 read, -1 not readable (the default box is used)
	vec3_t		mins, maxs;		// md3 frame-0 bounds, unscaled
} rpStaticModel_t;

typedef struct {
	int			model;			// index in rp_static_models
	vec3_t		origin, angles, scale;
	matrix3_t	axis;			// AnglesToAxis( angles ), unscaled
} rpStatic_t;

static rpStatic_t		rp_statics[RP_MAX_STATICS];
static rpStaticModel_t	rp_static_models[RP_MAX_STATIC_MODELS];
static char				rp_static_names[RP_STATIC_NAME_POOL];

// zyk: the model's index in rp_static_models, added if new; -1 when a table is full
static int RP_StaticModelIntern( const char *path )
{
	int i, len;

	for ( i = 0; i < level.rp_num_static_models; i++ )
	{
		if ( Q_stricmp( rp_static_names + rp_static_models[i].name, path ) == 0 )
			return i;
	}

	len = strlen( path ) + 1;
	if ( level.rp_num_static_models >= RP_MAX_STATIC_MODELS || level.rp_static_names_used + len > RP_STATIC_NAME_POOL )
		return -1;

	i = level.rp_num_static_models++;
	rp_static_models[i].name = level.rp_static_names_used;
	rp_static_models[i].state = 0;
	Q_strncpyz( rp_static_names + level.rp_static_names_used, path, len );
	level.rp_static_names_used += len;

	return i;
}

/*
==================
RP_StaticsRecord

From SP_misc_model_static, before it frees the entity: notes the static model the spawn vars describe,
when it is one of the map's own.
==================
*/
void RP_StaticsRecord( void )
{
	rpStatic_t *s;
	char *model;
	float angle, scale;
	int m;

	if ( !level.spawning || level.rp_map_loaded || level.mBSPInstanceDepth > 0 )
		return;

	// zyk: as the client: no model, no static (the client refuses the map outright)
	if ( !G_SpawnString( "model", "", &model ) || !model || !model[0] )
		return;

	if ( level.rp_num_statics >= RP_MAX_STATICS )
	{
		level.rp_statics_dropped++;
		return;
	}

	m = RP_StaticModelIntern( model );
	if ( m < 0 )
	{
		level.rp_statics_dropped++;
		return;
	}

	s = &rp_statics[level.rp_num_statics++];
	memset( s, 0, sizeof( *s ) );
	s->model = m;

	// zyk: the keys exactly as cg_spawn.c reads them: angles, or else angle as the yaw; modelscale_vec, or
	// else modelscale on all three axes; 1 1 1 with neither
	G_SpawnVector( "origin", "0 0 0", s->origin );

	if ( !G_SpawnVector( "angles", "0 0 0", s->angles ) )
	{
		VectorClear( s->angles );
		if ( G_SpawnFloat( "angle", "0", &angle ) )
			s->angles[YAW] = angle;
	}

	if ( !G_SpawnVector( "modelscale_vec", "1 1 1", s->scale ) )
	{
		VectorSet( s->scale, 1.0f, 1.0f, 1.0f );
		if ( G_SpawnFloat( "modelscale", "1", &scale ) )
			VectorSet( s->scale, scale, scale, scale );
	}

	AnglesToAxis( s->angles, s->axis );
}

// zyk: the log line once the map has loaded, when some were not kept
void RP_StaticsReport( void )
{
	if ( level.rp_statics_dropped > 0 )
		G_LogPrintf( "Static models: %d of the map's misc_model_static were not noted (at most %d, %d different models)\n",
			level.rp_statics_dropped, RP_MAX_STATICS, RP_MAX_STATIC_MODELS );
}

int RP_StaticsCount( void )
{
	return level.rp_num_statics;
}

/*
==================
RP_StaticGet

What static model n is: its model's path, origin, angles and scale (any may be NULL). qfalse for a number
that is not one.
==================
*/
qboolean RP_StaticGet( int n, const char **model, vec3_t origin, vec3_t angles, vec3_t scale )
{
	const rpStatic_t *s;

	if ( n < 0 || n >= level.rp_num_statics )
		return qfalse;

	s = &rp_statics[n];

	if ( model )
		*model = rp_static_names + rp_static_models[s->model].name;
	if ( origin )
		VectorCopy( s->origin, origin );
	if ( angles )
		VectorCopy( s->angles, angles );
	if ( scale )
		VectorCopy( s->scale, scale );

	return qtrue;
}

/*
==================
RP_StaticBox

Static model n's box in its own axes, the way the client draws the model: its md3's frame-0 bounds, each
axis multiplied by its scale (turned by the angles, at the origin, when drawn). qfalse when the bounds
could not be read -- the box is then the editor's (-16 -16 0) (16 16 16), scaled as well.
==================
*/
qboolean RP_StaticBox( int n, vec3_t mins, vec3_t maxs )
{
	const rpStatic_t *s;
	rpStaticModel_t *m;
	int k;

	if ( n < 0 || n >= level.rp_num_statics )
	{
		VectorClear( mins );
		VectorClear( maxs );
		return qfalse;
	}

	s = &rp_statics[n];
	m = &rp_static_models[s->model];

	if ( m->state == 0 )
	{
		m->state = RP_ReadModelBounds( rp_static_names + m->name, m->mins, m->maxs ) ? 1 : -1;

		if ( m->state != 1 )
		{
			VectorSet( m->mins, -16, -16, 0 );
			VectorSet( m->maxs, 16, 16, 16 );
		}
	}

	for ( k = 0; k < 3; k++ )
	{
		float a = m->mins[k] * s->scale[k], b = m->maxs[k] * s->scale[k];

		mins[k] = ( a < b ) ? a : b;
		maxs[k] = ( a < b ) ? b : a;
	}

	return ( m->state == 1 ) ? qtrue : qfalse;
}

/*
==================
RP_StaticAim

The static model the ray from eye along dir (a unit vector) enters first, nearer than maxDist, or -1; its
distance in *dist. Each box is tested in the model's own axes, one unit wider each way, as Entity Bounds
tests its soft volumes; one the eye is inside of is skipped, as those are.
==================
*/
int RP_StaticAim( const vec3_t eye, const vec3_t dir, float maxDist, float *dist )
{
	int best = -1, n, k;
	float bestDist = maxDist;

	for ( n = 0; n < level.rp_num_statics; n++ )
	{
		const rpStatic_t *s = &rp_statics[n];
		vec3_t mins, maxs, rel, p, d;
		float tEnter = -1.0e9f, tExit = 1.0e9f, radius;
		qboolean miss = qfalse;

		RP_StaticBox( n, mins, maxs );

		// zyk: nowhere near: the farthest corner from the origin bounds the whole box
		radius = 0.0f;
		for ( k = 0; k < 3; k++ )
		{
			float r = ( fabs( mins[k] ) > fabs( maxs[k] ) ) ? fabs( mins[k] ) : fabs( maxs[k] );
			radius += r * r;
		}
		radius = sqrt( radius ) + 2.0f;	// and the padding, which reaches sqrt(3) further at a corner

		VectorSubtract( s->origin, eye, rel );
		{
			float along = DotProduct( rel, dir );
			float across2 = DotProduct( rel, rel ) - along * along;

			if ( along + radius < 0.0f || along - radius > bestDist || across2 > radius * radius )
				continue;
		}

		// zyk: the eye and the direction in the model's own axes
		VectorScale( rel, -1.0f, rel );
		for ( k = 0; k < 3; k++ )
		{
			p[k] = DotProduct( rel, s->axis[k] );
			d[k] = DotProduct( dir, s->axis[k] );
		}

		for ( k = 0; k < 3; k++ )
		{
			float lo = mins[k] - 1.0f, hi = maxs[k] + 1.0f;

			if ( d[k] > -0.0001f && d[k] < 0.0001f )
			{
				if ( p[k] < lo || p[k] > hi )
				{
					miss = qtrue;
					break;
				}
			}
			else
			{
				float t1 = ( lo - p[k] ) / d[k];
				float t2 = ( hi - p[k] ) / d[k];

				if ( t1 > t2 )
				{
					float t = t1; t1 = t2; t2 = t;
				}
				if ( t1 > tEnter ) tEnter = t1;
				if ( t2 < tExit ) tExit = t2;
			}
		}

		if ( miss || tEnter > tExit || tEnter <= 0.0f || tEnter >= bestDist )
			continue;

		bestDist = tEnter;
		best = n;
	}

	if ( dist )
		*dist = bestDist;

	return best;
}

/*
==================
RP_StaticLabel

"^1static model #12^7 (scale 1.5)" -- red, so a static model's number is never read as an entity's;
the scale only when it is not 1, all three when they differ.
==================
*/
const char *RP_StaticLabel( int n )
{
	vec3_t scale;

	if ( !RP_StaticGet( n, NULL, NULL, NULL, scale ) )
		return "^1static model ?^7";

	if ( scale[0] == 1.0f && scale[1] == 1.0f && scale[2] == 1.0f )
		return va( "^1static model #%d^7", n );

	if ( scale[0] == scale[1] && scale[1] == scale[2] )
		return va( "^1static model #%d^7 (scale %g)", n, scale[0] );

	return va( "^1static model #%d^7 (scale %g %g %g)", n, scale[0], scale[1], scale[2] );
}
