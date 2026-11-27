#include <stdio.h>
#include <string.h>
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

int main(void) {
    lua_State *L = luaL_newstate();
    if (!L) { printf("[LUA.SON] Failed to allocate Lua VM!\n"); return 1; }
    luaL_openlibs(L);

    printf("=== Standalone Lua 5.5.1 (.SON Ring 3 Process) ===\n");
    printf("Type a .lua file (e.g. hello.lua), Lua code, or 'exit':\n");

    char line[256];
    while (1) {
        printf("lua> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (n == 0) continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;

        int err = (n > 4 && strcmp(line + n - 4, ".lua") == 0)
                  ? luaL_dofile(L, line)
                  : luaL_dostring(L, line);
        if (err != LUA_OK) {
            printf("lua error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_close(L);
    return 0;
}
