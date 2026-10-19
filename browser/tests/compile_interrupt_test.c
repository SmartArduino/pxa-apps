#include "quickjs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Budget { unsigned calls, stop; int one_shot; };
static int interrupt(JSRuntime *runtime,void *opaque) {
    (void)runtime;struct Budget *budget=opaque;
    ++budget->calls;
    return budget->one_shot?budget->calls==budget->stop:budget->calls>=budget->stop;
}
static unsigned check(const char *source,unsigned stop,int one_shot) {
    JSRuntime *rt=JS_NewRuntime();assert(rt);
    JS_SetMemoryLimit(rt,4*1024*1024);JS_SetMaxStackSize(rt,sizeof(void*)==4?16384:2*1024*1024);
    JSContext *ctx=JS_NewContextRaw(rt);assert(ctx);
    assert(JS_AddIntrinsicBaseObjects(ctx)==0 && JS_AddIntrinsicEval(ctx)==0);
    struct Budget budget={0,stop,one_shot};JS_SetInterruptHandler(rt,interrupt,&budget);
    JSValue result=JS_Eval(ctx,source,strlen(source),"budget.js",JS_EVAL_FLAG_COMPILE_ONLY);
    if(stop!=~0u) {
        assert(budget.calls>=stop && JS_IsException(result));
        JSValue error=JS_GetException(ctx);const char *message=JS_ToCString(ctx,error);
        assert(message && strstr(message,"interrupted"));
        JS_FreeCString(ctx,message);JS_FreeValue(ctx,error);
        JS_SetUncatchableException(ctx,0);
    } else if(JS_IsException(result)) {
        JSValue error=JS_GetException(ctx);const char *message=JS_ToCString(ctx,error);
        fprintf(stderr,"unexpected: %s\n",message?message:"");abort();
    }
    JS_FreeValue(ctx,result);JS_SetInterruptHandler(rt,NULL,NULL);
    result=JS_Eval(ctx,"1+2",3,"recover.js",0);
    int answer=0;assert(!JS_IsException(result) && JS_ToInt32(ctx,&answer,result)==0 && answer==3);
    JS_FreeValue(ctx,result);JS_FreeContext(ctx);JS_FreeRuntime(rt);
    return budget.calls;
}
int main(void) {
    char *source=malloc(32768);assert(source);char *at;
    unsigned checked=0;
    for(unsigned kind=0;kind<7;++kind) {
        at=source;
        if(kind==0) {
            at+=sprintf(at,"var a=");for(unsigned j=0;j<8;j++)*at++='(';
            *at++='[';for(unsigned j=0;j<200;j++)at+=sprintf(at,"%u,",j);*at++=']';
            for(unsigned j=0;j<8;j++)*at++=')';*at++=';';
        } else if(kind==1) {
            at+=sprintf(at,"var f=(");for(unsigned j=0;j<100;j++)at+=sprintf(at,"a%u%s",j,j==99?"":",");
            at+=sprintf(at,")=>a1+a2;");
        } else if(kind==2) {
            for(unsigned j=0;j<60;j++)at+=sprintf(at,"function f%u(a){var p='length';return a[p]+%u;}\n",j,j);
        } else if(kind==3) {
            at+=sprintf(at,"var [");for(unsigned j=0;j<100;j++)at+=sprintf(at,"a%u%s",j,j==99?"":",");
            at+=sprintf(at,"]=[];");
        } else if(kind==4) {
            at+=sprintf(at,"function f(a){switch(a){");
            for(unsigned j=0;j<100;j++)at+=sprintf(at,"case %u:return %u;",j,j);
            at+=sprintf(at,"}};");
        } else if(kind==5) {
            at+=sprintf(at,"function outer(x){let a='captured';return function inner(y){return function last(z){return eval('a+x+y+z')}}}class C{m(){return outer(1)}get p(){return 3}};");
        } else {
            for(unsigned j=0;j<24;j++)at+=sprintf(at,"function f%u(x){let a='captured%u';return function(y){return a+x+y+'suffix'}};",j,j);
        }
        *at=0;assert((size_t)(at-source)<32768);
        unsigned calls=check(source,~0u,0);
        for(unsigned stop=1;stop<=calls;++stop)for(int one_shot=0;one_shot<=1;one_shot++){
            check(source,stop,one_shot);++checked;
        }
        printf("kind=%u bytes=%zu cancellation_points=%u\n",kind,strlen(source),calls);
    }
    printf("compiler cancellation/recovery PASS %u\n",checked);free(source);
}
