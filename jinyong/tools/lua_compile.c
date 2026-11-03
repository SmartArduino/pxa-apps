/* SPDX-License-Identifier: MIT
 * Build-time compiler only. No script is executed on the workstation. */
#include <stdio.h>
#include <stdlib.h>
#include "lua.h"
#include "lauxlib.h"

static int write_chunk(lua_State *state,const void *data,size_t bytes,void *file) {
    (void)state;return fwrite(data,1,bytes,(FILE *)file)!=bytes;
}
int main(int count,char **args) {
    if(count!=4 || sizeof(int)!=4 || sizeof(lua_Number)!=8)return 2;
    FILE *input=fopen(args[1],"rb");if(!input)return 3;
    if(fseek(input,0,SEEK_END))return 4;
    long length=ftell(input);if(length<0 || fseek(input,0,SEEK_SET))return 4;
    char *data=malloc(length?(size_t)length:1);if(!data)return 5;
    if(fread(data,1,(size_t)length,input)!=(size_t)length || fclose(input))return 6;
    lua_State *state=luaL_newstate();if(!state)return 7;
    if(luaL_loadbuffer(state,data,(size_t)length,args[3])) {
        fprintf(stderr,"%s\n",lua_tostring(state,-1));return 8;
    }
    free(data);
    FILE *output=fopen(args[2],"wb");if(!output)return 9;
    int result=lua_dump(state,write_chunk,output);
    if(fclose(output))result=10;
    lua_close(state);return result;
}
