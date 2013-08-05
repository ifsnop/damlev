/*

Damerau-Levenshtein Distance UDF for MySQL
Supports upper bounding for fast searching and UTF-8 case
insensitive throught iconv.

Copyright (C) 2013 Diego Torres <diego dot torres at gmail dot com>

Implementing
    https://github.com/torvalds/linux/blob/8a72f3820c4d14b27ad5336aed00063a7a7f1bef/tools/perf/util/levenshtein.c

Redistribute as you wish, but leave this information intact.

*/

#ifdef STANDARD
#include <string.h>
#ifdef __WIN__
typedef unsigned __int64 ulonglong;
typedef __int64 longlong;
#else
typedef unsigned long long ulonglong;
typedef long long longlong;
#endif /*__WIN__*/
#else
#include <my_global.h>
#include <my_sys.h>
#endif
#include <mysql.h>
#include <m_ctype.h>
#include <m_string.h>

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdarg.h>
#include <iconv.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

#ifndef DEBUG
#define NDEBUG
#endif
