// This file contains fixes to make Intellisense happy
#ifndef __INTELLIFIX_H__
#define __INTELLIFIX_H__

#ifndef __cplusplus
#define true 1
#define false 0
#endif

#define __extension__
#define __attribute__(x)

typedef void* __builtin_va_list;

#endif
