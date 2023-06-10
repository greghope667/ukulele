#pragma once

void panic (const char* fmt, ...) __attribute((noreturn));

#define assert(cond,msg)					\
	if (!(cond))							\
		panic("Assert failed %s:%i, %s\n",	\
			  __FILE__, __LINE__, msg)

