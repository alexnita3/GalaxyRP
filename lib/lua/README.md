# Lua 5.4.9 (vendored)

This is the `src/` folder of the official Lua 5.4.9 release from lua.org
(`lua-5.4.9.tar.gz`, 2026-08-10, SHA-256
`2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6`), used by
Galaxy RP Ultimate's server-side scripting (`galaxyrp/game/rp_lua*.c`). It is
built into the server game module (jampgame) only. Licence: MIT, see `LICENSE`.

## Left out on purpose

These files are not copied here, so they cannot be built into the game by
mistake:

| File | Why |
|---|---|
| `lua.c`, `luac.c` | Stand-alone interpreter and compiler programs |
| `liolib.c` | `io` library: files and processes |
| `loslib.c` | `os` library: `os.execute`, `os.remove`, `os.exit`... |
| `loadlib.c` | `package`/`require`: loading native libraries |
| `ldblib.c` | `debug` library: reaches past every sandbox rule |
| `linit.c` | `luaL_openlibs`, which would open the libraries above; the game opens the safe ones itself |

`lualib.h` still declares those libraries; nothing calls them.

## Changed

Every change is marked `GLX_U`. Lua's count hook only fires between VM
instructions, so the game's time and instruction limits need help where work
happens in C. `glx_lua.h` (new) holds what the changes share.

| File | Change |
|---|---|
| `lstrlib.c` | Pattern matching calls `glx_lstrlib_check` every 4096 matching steps; the `string.rep` loop every 1024 copies; `string.rep` with nothing to repeat returns `""` at once |
| `ltablib.c` | `table.insert`, `remove`, `move`, `concat`, `unpack` and `sort` call it every 1024 steps (`GLX_CWORK`) |
| `lutf8lib.c` | `utf8.len` and `utf8.offset` call it every 1024 steps |
| `lstring.c`, `lauxlib.c` | No string, and no string buffer, grows past `GLX_MAXSTRING` (4 MB): that bounds what one instruction can do between two hook calls |
| `lstate.c` | `luaE_resetthread` turns hooks back on before running a dead coroutine's pending `__close` metamethods (a coroutine stopped by an error from the hook still had them off) |

Everything else is byte-for-byte the release.
