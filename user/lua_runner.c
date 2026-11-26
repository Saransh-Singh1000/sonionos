#include "lua_runner.h"
#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

int lua_run_file(const char *path) {
    lua_State *L = luaL_newstate();
    if (!L) {
        printf("lua: out of memory\n");
        return -1;
    }
    luaL_openlibs(L);
    int status = luaL_dofile(L, path);
    if (status != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        printf("lua error: %s\n", err ? err : "(unknown error)");
        lua_pop(L, 1);
        lua_close(L);
        return -1;
    }
    lua_close(L);
    return 0;
}
