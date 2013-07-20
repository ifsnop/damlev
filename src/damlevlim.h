/* Damerau-Levenshtein Distance UDF for MySQL
Supports upper bounding for fast searching and UTF-8 case
insensitive throught iconv
Updated 20130707
        by Diego Torres
        - Added more documentation and some info on errors
        when compiling cross-platform.

Updated 20120328
        by Diego Torres <diego dot torres at gmail.com>
        - Support UTF8 characters, using iconv
        (http://en.wikipedia.org/wiki/Iconv), and
        force case-insensitivenes.
        - Added debuging output to a temporary file
        - Support for x86_64 and 32bits architecture
        - Provided make.sh, documentation and examples

Updated 20090416
        by Sean Collins <sean at lolyco.com>
        - Tomas' bug (damlevlim("h","hello",2) i get 4)
        Adapted from Josh Drew's levenshtein code using pseudo
        code from
        http://en.wikipedia.org/wiki/Damerau–Levenshtein_distance
        - an optimal string alignment algorithm, as opposed to
        'edit distance' as per the notes in the wp article

Adapted 20080827
        by Sean Collins <sean at lolyco.com>

Originally 20031228
        Levenshtein Distance Algorithm implementation as MySQL UDF
        by Joshua Drew for SpinWeb Net Designs, Inc. on 2003-12-28.

Derived
        The levenshtein function is derived from the C implementation
        by Lorenzo Seidenari. More information about the Levenshtein
        Distance Algorithm can be found at http://www.merriampark.com/ld.htm

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

