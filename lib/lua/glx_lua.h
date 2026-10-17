/*
** GLX_U: Galaxy RP Ultimate's additions to this vendored Lua (see README.md).
** Scripts run under a time and instruction budget that Lua's count hook enforces,
** but the hook only fires between VM instructions: a library function looping in
** C, or one instruction working on a huge string, is never interrupted by it.
*/

#ifndef glx_lua_h
#define glx_lua_h

#include "lua.h"


/*
** The host's check for work done in C (set by galaxyrp/game/rp_lua_core.c):
** charges 'steps' to the script's budget and raises a Lua error once the
** budget has run out. NULL means no check, which is stock Lua behaviour.
** Defined in lstrlib.c.
*/
extern void (*glx_lstrlib_check) (lua_State *L, unsigned int steps);

/* GLX_CWORK(L, n): count one more iteration of a C loop in 'n' (an unsigned
** int) and call the check every GLX_CWORK_STEPS of them. Each call charges
** GLX_CWORK_COST instructions: about one per 16 iterations, so a big but
** honest table.sort or table.move is not cut short by the instruction budget;
** the time budget, checked on every call, is what stops a loop that never ends. */
#define GLX_CWORK_STEPS	1024
#define GLX_CWORK_COST	64
#define GLX_CWORK(L, n) \
  { if (l_unlikely(((++(n)) & (GLX_CWORK_STEPS - 1)) == 0) && glx_lstrlib_check) \
      glx_lstrlib_check(L, GLX_CWORK_COST); }

/* The instructions 'L' ran since its count hook last fired, with the count started
** again (ldebug.c): RP_Lua_Call charges each call exactly its own around a nested one. */
LUA_API int glx_hookcount_take (lua_State *L);

/* The longest string the game's Lua makes (lstring.c, lauxlib.c): it bounds
** what one instruction or library call can do between two checks. */
#define GLX_MAXSTRING	((size_t)4 * 1024 * 1024)

#endif
