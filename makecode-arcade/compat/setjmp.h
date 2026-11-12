// MIT. Normal fiber scheduling does not use setjmp. Fatal paths never resume.
#pragma once
#include <stdlib.h>
typedef int jmp_buf[1];
#define setjmp(env) ((void)(env), 0)
#define longjmp(env, value) abort()
