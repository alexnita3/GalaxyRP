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

One change, marked `GLX_U` in `lstrlib.c`: pattern matching calls
`glx_lstrlib_check` every 4096 matching steps, so the game's time and
instruction limits also cover patterns, which run in C where Lua's count hook
never fires. Everything else is byte-for-byte the release.
