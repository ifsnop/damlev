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
#ifdef HAVE_DLOPEN

//static pthread_mutex_t LOCK;

/******************************************************************************
** debug function declarations
******************************************************************************/

FILE *file = NULL;
#ifdef DEBUG
char buf[80];
char * getTS() {
    time_t now;
    struct tm ts;
    bzero(buf, 80);
    time(&now);
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ts);
    return buf;
}


void writeDebugHEX(FILE *file, const char *pre, const char *s) {

    fprintf(file, "%s - %s strlen(%zu) ", getTS(), (pre == NULL ? "" : pre), strlen(s) );
    for (size_t i = 0; i<strlen(s); i++)
        fprintf(file, "[%c]%02X ", s[i], (unsigned int) *((unsigned char *)s+i));
    fprintf(file, "\n");
    fflush(file);
    return;
}

void writeDebugHEXSize(FILE *file, const char *pre, const char *s, unsigned int l) {

    fprintf(file, "%s - %s strlen(%zu) ", getTS(), (pre == NULL ? "" : pre), strlen(s) );
    for (size_t i = 0; i<l; i++)
        fprintf(file, "[%c]%02X ", s[i], (unsigned int) *((unsigned char *)s+i));
    fprintf(file, "\n");
    fflush(file);
    return;
}
#endif

/******************************************************************************
** function declarations
******************************************************************************/

extern "C" {
        my_bool         damlevlim256u_init(UDF_INIT *initid, UDF_ARGS *args,
                            char *message);
        void            damlevlim256u_deinit(UDF_INIT *initid);
        longlong        damlevlim256u(UDF_INIT *initid, UDF_ARGS *args,
                            char *is_null, char *error);
}

/******************************************************************************
** function definitions
******************************************************************************/

/******************************************************************************
** purpose:     called once for each SQL statement which invokes DAMLEVLIM();
**              checks arguments, sets restrictions, allocates memory that
**              will be used during the main DAMLEVLIM() function (the same
**              memory will be reused for each row returned by the query)
** receives:    pointer to UDF_INIT struct which is to be shared with all
**              other functions (damlevlim256() and damlevlim256_deinit()) -
**              the components of this struct are described in the MySQL manual;
**              pointer to UDF_ARGS struct which contains information about
**              the number, size, and type of args the query will be providing
**              to each invocation of damlevlim256(); pointer to a char
**              array of size MYSQL_ERRMSG_SIZE in which an error message
**              can be stored if necessary
** returns:     1 => failure; 0 => successful initialization
******************************************************************************/
my_bool damlevlim256u_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    int *workspace;

    /* make sure user has provided three arguments */
    if (args->arg_count != 3) {
        strcpy(message, "DAMLEVLIM() requires three arguments");
        return 1;
    } else {
    /* make sure both arguments are strings - they could be cast to strings,
    ** but that doesn't seem useful right now */
        if (args->arg_type[0] != STRING_RESULT ||
            args->arg_type[1] != STRING_RESULT ||
            args->arg_type[2] != INT_RESULT) {
            strcpy(message, "DAMLEVLIM() requires arguments (string, string, int)");
            return 1;
        }
    }

    /* set the maximum number of digits MySQL should expect as the return
    ** value of the DAMLEVLIM() function */
    initid->max_length = 3;

    /* damlevlim256() will not be returning null */
    initid->maybe_null = 0;

    /* attempt to allocate memory in which to calculate distance */
    // workspace = new int[(args->lengths[0] + 1) * (args->lengths[1] + 1)];
    workspace = new int[256 * 256];

    if (workspace == NULL) {
        strcpy(message, "Failed to allocate memory for damlevlim256 function");
        return 1;
    }

    /* initialize first row to 0..n */
    int k;
    for (k = 0; k < 256; k++)
        workspace[k] = k;

    /* initialize first column to 0..m */
    k = 256;
    for (int i = 1; i < 256; i++) {
        workspace[k] = i;
        k += 256;
    }
    /* initid->ptr is a char* which MySQL provides to share allocated memory
    ** among the xxx_init(), xxx_deinit(), and xxx() functions */
    initid->ptr = (char*) workspace;

//    (void) pthread_mutex_init(&LOCK,MY_MUTEX_INIT_SLOW);

    return 0;
}

/******************************************************************************
** purpose:     deallocate memory allocated by damlevlim256_init(); this func
**              is called once for each query which invokes DAMLEVLIM(),
**              it is called after all of the calls to damlevlim256() are done
** receives:    pointer to UDF_INIT struct (the same which was used by
**              damlevlim256_init() and damlevlim256())
** returns:     nothing
******************************************************************************/
void damlevlim256u_deinit(UDF_INIT *initid)
{
    if (initid->ptr != NULL)
        delete [] initid->ptr;

//    (void) pthread_mutex_destroy(&LOCK);
}

/******************************************************************************
** purpose:     initialize iconv library
** receives:    nothing
** returns:     iconv_t handler or -1 if error
******************************************************************************/

iconv_t iconv_init() {
    iconv_t ic;
    ic = iconv_open("ascii//TRANSLIT", "UTF-8");
#ifdef DEBUG
    if (ic == (iconv_t) -1) {
        fprintf(file, "%s - Initialization failure: %s\n", getTS(), strerror(errno)); fflush(file);
        //if (errno == EINVAL)
        //    fprintf(file, "Conversion from '%s' to '%s' is not supported.\n", "UTF-8", "ascii//TRANSLITE");
    }
#endif
    return ic;
}

/******************************************************************************
** purpose:     close iconv library
** receives:    iconv_t handler
** returns:     true if success, else false
******************************************************************************/

bool iconv_end(iconv_t ic) {
    int rc = iconv_close(ic);
    if ( rc != 0) {
#ifdef DEBUG
        fprintf(file, "%s - iconv_close failed: %s\n", getTS(), strerror(errno)); fflush(file);
#endif
        return false;
    }
    return true;
}


/******************************************************************************
** purpose:     helper that translates and utf8 string to ascii with extended
**              error codes
** receives:    FILE handler, iconv_t hander, string to translate and length
** returns:     ascii translated string
******************************************************************************/

char * utf8toascii (FILE *file, iconv_t ic, char *s, int l) { //euc)
    size_t iconv_value;
    char *buffer, *ret; //utf8;
    size_t len = l;
    size_t retlen = 2*l;

    ret = (char*)calloc (retlen,1);
    buffer = ret;

    //fprintf(file, "%s - s(%08X) len(%08X) ret(%08X) retlen(%08X)\n", getTS(),
    //  (unsigned int) s, (unsigned int) len, (unsigned int) ret, (unsigned int) retlen);
    //  fflush(file);

    iconv_value = iconv(ic, &s, &len, &ret, &retlen);
    /* Handle failures. */
    if ( iconv_value == (size_t) -1 ) {
#ifdef DEBUG
        switch (errno) {
            // See "man 3 iconv" for an explanation. 
            case EILSEQ:
                fprintf(file, "%s - Invalid multibyte sequence.(%s)\n", getTS(), s);
                break;
            case EINVAL:
                fprintf(file, "%s - Incomplete multibyte sequence.(%s)\n", getTS(), s);
                break;
            case E2BIG:
                fprintf(file, "%s - No more room.\n", getTS());
                break;
            default:
                fprintf(file, "%s - iconv failed: %s\n", getTS(), strerror(errno));
        }
        fflush(file);
#endif
        return NULL;
    }
#ifdef DEBUG
    fprintf(file, "%s - inside iconv_value(%zd) retlen(%zd)\n", getTS(), iconv_value, retlen);
    writeDebugHEXSize(file, "inside", buffer, retlen);
#endif
    return buffer;
}


/******************************************************************************
** purpose:     compute the Levenshtein distance (edit distance) between two
**              strings
** receives:    pointer to UDF_INIT struct which contains pre-allocated memory
**              in which work can be done; pointer to UDF_ARGS struct which
**              contains the functions arguments and data about them; pointer
**              to mem which can be set to 1 if the result is NULL; pointer
**              to mem which can be set to 1 if the calculation resulted in an
**              error
** returns:     the Levenshtein distance between the two provided strings
******************************************************************************/
longlong damlevlim256u(UDF_INIT *initid, UDF_ARGS *args, char *is_null,
                     char *error) {
    /* s is the first user-supplied argument; t is the second
    ** the levenshtein distance between s and t is to be computed */
    /*const*/ char *s = args->args[0]; char *s_c = NULL; char *pre_s = NULL;
    /*const*/ char *t = args->args[1]; char *t_c = NULL; char *pre_t = NULL;
    long long limit_arg = *((long long*)args->args[2]);
    /* get a pointer to the memory allocated in damlevlim256_init() */
    int *d = (int*) initid->ptr;
    longlong n, m;
    int b,c,f,g,h,i,j,k = 0,min, l1, l2, cost, tr, limit = limit_arg, best = 0;
    iconv_t ic;
    char *ret;

    /***************************************************************************
    ** damlevlim256 step one
    ***************************************************************************/

    /* if s or t is a NULL pointer, then the argument to which it points
    ** is a MySQL NULL value; when testing a statement like:
    ** >SELECT DAMLEVLIM(NULL, 'test');
    ** the first argument has length zero, but if some row in a table contains
    ** a NULL value which is sent to DAMLEVLIM() (or some other UDF),
    ** that NULL argument has the maximum length of the attribute (as defined
    ** in the CREATE TABLE statement); therefore, the argument length is not
    ** a reliable indicator of the argument's existence... checking for
    ** a NULL pointer is the proper course of action
    */

    n = (s == NULL) ? 0 : args->lengths[0];
    m = (t == NULL) ? 0 : args->lengths[1];

#ifdef DEBUG
    file = fopen("/tmp/damlevlim256u.log", "a+");
    fprintf(file, "\n%s - init damlevlim256u\n", getTS());
    fprintf(file, "%s - step.1 n(%lld) s(%s) m(%lld) t(%s) limit_arg(%lld)\n", getTS(), n, s, m, t, limit_arg); fflush(file);
#endif

    if(n != 0 && m != 0) {
        int nU = -1, mU = -1;
        ret = setlocale(LC_ALL, "es_ES.UTF-8");
        if (ret == NULL) {
#ifdef DEBUG
            fprintf(file, "%s - error setting locale %s", getTS(), strerror(errno)); fflush(file); fclose(file);
#endif
            return -1;
        }

        pre_s = (char*)calloc(n+2, 1);
        strncpy(pre_s, s, n);
        pre_s[n] = '\0'; pre_s[n+1] = '\0';

        pre_t = (char*)calloc(m+2, 1);
        strncpy(pre_t, t, m);
        pre_t[m] = '\0'; pre_t[m+1] = '\0';

        if ( (nU = mbstowcs(NULL,pre_s,0)) == -1 ) { // error
#ifdef DEBUG
            fprintf(file, "%s - some invalid characters found in mb string n(%lld) pre_s(%s) (%s)\n", getTS(), n, pre_s, strerror(errno)); fflush(file); fclose(file);
#endif
            return -1;
        }

        if ( (mU = mbstowcs(NULL,pre_t,0)) == -1 ) { // error
#ifdef DEBUG
            fprintf(file, "%s - some invalid characters found in mb string m(%lld) pre_t(%s) (%s)\n", getTS(), m, pre_t, strerror(errno)); fflush(file); fclose(file);
#endif
            return -1;
        }

        if ( (ic = iconv_init()) == (iconv_t) -1) {
#ifdef DEBUG
            fprintf(file, "%s - iconv_init returned -1\n", getTS()); fflush(file); fclose(file);
#endif
            return -1;
        }

        if (n!=nU) {
#ifdef DEBUG
            fprintf(file, "%s - pre_s(%s) has utf8 chars n(%lld) nU(%d)\n", getTS(), pre_s, n, nU);
            writeDebugHEX(file, "pre", pre_s);
            //fprintf(file, "%s - s(%16LX) n(%16LX)\n", getTS(), (uintptr_t)s, (uintptr_t) n); fflush(file);
#endif
            if ( (s_c = utf8toascii(file, ic, pre_s, n)) == NULL )
                return -1;
            n = strlen(s_c);
//            strncpy(s, str, n);
//            s[n] = '\0';
#ifdef DEBUG
            writeDebugHEX(file, "post", s_c);
//            free(str);
#endif
        } else {
            s_c = (char*)calloc(n+1, 1);
            strncpy(s_c, pre_s, n);
        }

        if (m!=mU) {
#ifdef DEBUG
            fprintf(file, "%s - pre_t(%s) has utf8 chars m(%lld) mU(%d)\n", getTS(), pre_t, m, mU);
            writeDebugHEX(file, "pre", pre_t);
#endif
            if ( (t_c = utf8toascii(file, ic, pre_t, m)) == NULL )
                return -1;
            m = strlen(t_c);
//            strncpy(t, str, m);
//            t[m] = '\0';
#ifdef DEBUG
            writeDebugHEX(file, "post", t_c);
//            free(str);
#endif
        } else {
            t_c = (char*)calloc(m+1, 1);
            strncpy(t_c, pre_t, m);
        }

        if (n > 255) {
            n = 255; s_c[n] = '\0';
        }

        if (m > 255) {
            m = 255; t_c[m] = '\0';
        }

        free(pre_s); free(pre_t);

        iconv_end(ic);
#ifdef DEBUG
        fprintf(file, "%s - step.2 n(%lld) s(%s) m(%lld) t(%s) limit_arg(%lld)\n", getTS(), n, s_c, m, t_c, limit_arg); fflush(file);
#endif
        /************************************************************************
        ** damlevlim256 step two
        ************************************************************************/

        l1 = n;
        l2 = m;
        n++;
        m++;

        /************************************************************************
        ** damlevlim256 step three
        ************************************************************************/

        /* throughout these loops, g will be equal to i minus one */
        g = 0;
        for (i = 1; i < n; i++) {
            /*********************************************************************
            ** damlevlim256 step four
            *********************************************************************/
#ifdef DEBUG
            //fprintf(file, "%s - step.3 i(%d)<n(%lld)\n", getTS(), i, n); fflush(file);
#endif
            k = i;

            /* throughout the for j loop, f will equal j minus one */
            f = 0;
            best = limit;
            for (j = 1; j < m; j++) {
                /******************************************************************
                ** damlevlim256 step five, six, seven
                ******************************************************************/
#ifdef DEBUG
                //fprintf(file, "%s - step.4 j(%d)<m(%lld)\n", getTS(), j, m); fflush(file);
#endif

                h = k;
                k += 256;

                min = d[h] + 1;
                b = d[k-1] + 1;
                if ( !strncasecmp(&s_c[g], &t_c[f], 1) ) { //(s[g] == t[f])
                    cost = 0;
                } else {
                    cost = 1;
                    /* transposition */
                    if (i < l1 && j < l2) {
                        if (strncasecmp(&s_c[i], &t_c[f], 1)==0 && //s[i] == t[f] && s[g] == t[j]) -
                            strncasecmp(&s_c[g], &t_c[j], 1)==0) {
                            tr = d[(h) - 1];
                            if (tr < min)
                                min = tr;
                        }
                    }
                }
                c = d[h - 1] + cost;

                if (b < min)
                    min = b;

                if (c < min) {
                    d[k] = c;
                    if (c < best)
                        best = c;
                } else {
                    d[k] = min;
                    if (min < best)
                        best = min;
                }
                f = j;
            }

            if (best >= limit) {
#ifdef DEBUG
                fprintf(file, "%s - return.1 limit_arg(%lld)\n", getTS(), limit_arg);
                fflush(file); fclose(file);
#endif
                free(s_c); free(t_c);
                return limit_arg;
            }
            /* g will equal i minus one for the next iteration */
            g = i;
        }
#ifdef DEBUG
        fprintf(file, "%s - step.5 (prefree) n(%lld) s(%s) m(%lld) t(%s) limit_arg(%lld)\n", getTS(), n, s_c, m, t_c, limit_arg); fflush(file);
#endif
        free(s_c); free(t_c);

        if (d[k] >= limit) {
#ifdef DEBUG
            fprintf(file, "%s - return.2 limit_arg(%lld)\n", getTS(), limit_arg);
            fflush(file); fclose(file);
#endif
            return limit_arg;
        } else {
#ifdef DEBUG
            fprintf(file, "%s - return.3 d[k](%lld)\n", getTS(), (longlong) d[k]);
            fflush(file); fclose(file);
#endif
            return (longlong) d[k];
        }
    } else {
        if (n == 0) {
            if (m < limit_arg) {
#ifdef DEBUG
                fprintf(file, "%s - return.4 m(%lld)\n", getTS(), m);
                fflush(file); fclose(file);
#endif
                return m;
            } else {
#ifdef DEBUG
                fprintf(file, "%s - return.5 limit_arg(%lld)\n", getTS(), limit_arg);
                fflush(file); fclose(file);
#endif
                return limit_arg;
            }
        } else {
            if (n < limit_arg) {
#ifdef DEBUG
                fprintf(file, "%s - return.6 n(%lld)\n", getTS(), n);
                fflush(file); fclose(file);
#endif
                return n;
            } else {
#ifdef DEBUG
                fprintf(file, "%s - return.7 limit_arg(%lld)\n", getTS(), limit_arg);
                fflush(file); fclose(file);
#endif
                return limit_arg;
            }
        }
    }
}

#endif /* HAVE_DLOPEN */
