#!/usr/bin/env python3
"""Runs a Lua script with the system Lua 5.4 library through ctypes (no lua binary needed).
usage: run_lua.py script.lua [more.lua ...]   (scripts run in order in one state; exit 1 on error)"""
import ctypes, sys, glob

def load_lua():
    for name in ("liblua5.4.so.0", "liblua5.4.so", "liblua5.3.so.0", "liblua5.1.so.0", "libluajit-5.1.so.2"):
        try:
            return ctypes.CDLL(name)
        except OSError:
            continue
    raise SystemExit("no Lua shared library found")

lua = load_lua()
lua.luaL_newstate.restype = ctypes.c_void_p
lua.luaL_openlibs.argtypes = [ctypes.c_void_p]
lua.luaL_loadfilex.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
lua.lua_pcallk.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
lua.lua_tolstring.restype = ctypes.c_char_p
lua.lua_tolstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
lua.lua_settop.argtypes = [ctypes.c_void_p, ctypes.c_int]

L = lua.luaL_newstate()
lua.luaL_openlibs(L)
for path in sys.argv[1:]:
    rc = lua.luaL_loadfilex(L, path.encode(), None)
    if rc == 0:
        rc = lua.lua_pcallk(L, 0, 0, 0, None, None)
    if rc != 0:
        msg = lua.lua_tolstring(L, -1, None)
        print("LUA ERROR in %s: %s" % (path, msg.decode() if msg else "?"))
        sys.exit(1)
print("lua ok")
