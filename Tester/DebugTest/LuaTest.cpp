#include "lua/lua.hpp"
#include "assert.h"

static int myadd(lua_State* L) {
	int op1 = luaL_checkinteger(L,1);
	int op2 = luaL_checkinteger(L,2);
	lua_pushinteger(L, op1 + op2);
	return 1;  // 1 result
}

int main() {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	lua_register(L, "myadd", myadd);
	int ret;
	ret = luaL_dostring(L, 
		"print(\"hello world!\");"
		"print(\"second line...\");"
		"print(\"myadd(3+6) = \"..myadd(3,6));"
	);
	printf("ret = %d\n", ret);
	return 0;
}
