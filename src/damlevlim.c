/*

Damerau-Levenshtein Distance UDF for MySQL
Supports upper bounding for fast searching and UTF-8 case
insensitive throught iconv.

Copyright (C) 2013 Diego Torres <diego dot torres at gmail dot com>

Implementing
    https://github.com/torvalds/linux/blob/8a72f3820c4d14b27ad5336aed00063a7a7f1bef/tools/perf/util/levenshtein.c

Redistribute as you wish, but leave this information intact.

*/

#include "damlevlim.h"

#define LENGTH_MAX 255

#define debug_print(fmt, ...) \
    do { if (DEBUG_MYSQL) fprintf(stderr, "%s:%d> " fmt "\n", \
         __func__, __LINE__, __VA_ARGS__); fflush(stderr); } while (0)

//! structure to allocate memory in init and use it in core functions
struct workspace_t {
    char *str1;         //!< internal buffer to store 1st string
    char *str2;         //!< internal buffer to store 2nd string
    int *row0;          //!< round buffer for levenshtein_core function
    int *row1;
    int *row2;
    mbstate_t *mbstate; //!< buffer for mbsnrtowcs
    iconv_t ic;         //!< buffer for iconv
};

my_bool damlevlim_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void damlevlim_deinit(UDF_INIT *initid);
longlong damlevlim(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
int damlevlim_core(struct workspace_t *ws,
    const char *str1, int len1,
    const char *str2, int len2,
    int w, int s, int a, int d, int limit);
char * utf8toascii (const char *str_src, longlong *len_src,
    iconv_t ic, mbstate_t *mbstate, char *str_dst);

#ifdef HAVE_DLOPEN

//static pthread_mutex_t LOCK;

#ifdef DEBUG
#define LENGTH_1 7 // acéhce  len(7) + NULL
#define LENGTH_2 7 // aceché  len(7) + NULL
#define LENGTH_LIMIT 7

//! main function, only used in testing
int main(int argc, char *argv[]) {

    UDF_INIT *init = (UDF_INIT *) malloc(sizeof(UDF_INIT));
    UDF_ARGS *args = (UDF_ARGS *) malloc(sizeof(UDF_ARGS));
    args->arg_type = (enum Item_result *) malloc(sizeof(enum Item_result)*3);
    args->lengths = (long unsigned int *) malloc(sizeof(long unsigned int)*2);
    char *message = (char *) malloc(sizeof(char)*MYSQL_ERRMSG_SIZE);
    char *error = (char *) malloc(sizeof(char));
    char *is_null = (char *) malloc(sizeof(char));
    long long limit_arg = LENGTH_LIMIT;
    long long *limit_arg_ptr = &limit_arg;

    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    args->arg_type[2] = INT_RESULT;
    args->lengths[0] = LENGTH_1;
    args->lengths[1] = LENGTH_2;
    args->arg_count = 3;
    error[0] = '\0';

    is_null[0] = '\0';

    args->args = (char **) malloc(sizeof(char *)*3);
    args->args[0] = (char *) malloc(sizeof(char)*(LENGTH_1+2));
    args->args[1] = (char *) malloc(sizeof(char)*(LENGTH_2+2));
    args->args[2] = (char *) limit_arg_ptr;

    strncpy(args->args[0], "ac""\xc3\xa9""cha", LENGTH_1+1);
    strncpy(args->args[1], "acech""\xc3\xa9", LENGTH_2+1);
    args->args[0][LENGTH_1] = '\0'; args->args[0][LENGTH_1 + 1] = '\0';
    args->args[1][LENGTH_2] = '\0'; args->args[1][LENGTH_2 + 1] = '\0';

    printf("0>cad1(%s) cad2(%s) (%01X) (%01X)\n", args->args[0], args->args[1], (char) error[0], (char) is_null[0]);

    damlevlim_init(init, args, message);

    longlong ret = damlevlim(init, args, is_null, error);

    assert(ret == 1);

    damlevlim_deinit(init);

    printf("1>cad1(%s) cad2(%s) ret(%lld)\n", args->args[0], args->args[1], ret);

    free(args->args[1]);
    free(args->args[0]);
    free(args->args);
    free(is_null);
    free(error);
    free(message);
    free(args->arg_type);
    free(args->lengths);
    free(args);
    free(init);

    return 0;

}
#endif // DEBUG


//! check parameters and allocate memory for MySql
my_bool damlevlim_init(UDF_INIT *init, UDF_ARGS *args, char *message) {

    struct workspace_t *ws;

    // make sure user has provided three arguments
    if (args->arg_count != 3) {
        strncpy(message, "DAMLEVLIM() requires three arguments", 80);
        return 1;
    } else {
        // make sure both arguments are right
        if ( args->arg_type[0] != STRING_RESULT ||
            args->arg_type[1] != STRING_RESULT ||
            args->arg_type[2] != INT_RESULT ) {
            strncpy(message, "DAMLEVLIM() requires arguments (string, string, int<256)", 80);
            return 1;
        }
    }

    // this shouldn't be needed, we are returning an int.
    // set the maximum number of digits MySQL should expect as the return
    // value of the DAMLEVLIM() function
    // init->max_length = 21;

    // DAMLEVLIM() will not be returning null
    init->maybe_null = 0;

    // attempt to allocate memory in which to calculate distance
    ws = (struct workspace_t *) malloc( sizeof(struct workspace_t) );
    ws->mbstate = (mbstate_t *) malloc( sizeof(mbstate_t) );
    ws->str1 = (char *) malloc( sizeof(char)*(LENGTH_MAX+1) ); // max allocated for UTF-8 complex string
    ws->str2 = (char *) malloc( sizeof(char)*(LENGTH_MAX+1) );
    ws->row0 = (int *) malloc( sizeof(int)*(LENGTH_MAX+1) );
    ws->row1 = (int *) malloc( sizeof(int)*(LENGTH_MAX+1) );
    ws->row2 = (int *) malloc( sizeof(int)*(LENGTH_MAX+1) );


    if ( ws == NULL || ws->mbstate == NULL ||
        ws->str1 == NULL || ws->str2 == NULL ||
        ws->row0 == NULL || ws->row1 == NULL || ws->row2 == NULL ) {
        free(ws->row2); free(ws->row1); free(ws->row0);
        free(ws->str2); free(ws->str1);
        free(ws->mbstate); free(ws);
        strncpy(message, "DAMLEVLIM() failed to allocate memory", 80);
        return 1;
    }

    if ( setlocale(LC_ALL, "C.UTF-8") == NULL ) {
        free(ws->row2); free(ws->row1); free(ws->row0);
        free(ws->str2); free(ws->str1);
        free(ws->mbstate); free(ws);
        strncpy(message, "DAMLEVLIM() failed to change locale", 80);
        return 1;
    }

    if ( (ws->ic = iconv_open("ascii//TRANSLIT", "UTF-8")) == (iconv_t) -1 ) {
        free(ws->row2); free(ws->row1); free(ws->row0);
        free(ws->str2); free(ws->str1);
        free(ws->mbstate); free(ws);
        strncpy(message, "DAMLEVLIM() failed to initialize iconv", 80);
        return 1;
    }

    init->ptr = (char *) ws;
    debug_print("%s", "init successful");

    // (void) pthread_mutex_init(&LOCK,MY_MUTEX_INIT_SLOW);
    return 0;
}

//! check parameters akkd allocate memory for MySql
longlong damlevlim(UDF_INIT *init, UDF_ARGS *args, char *is_null, char *error) {

    // s is the first user-supplied argument; t is the second
    const char *str1 = args->args[0], *str2 = args->args[1];
    // upper bound for calculations
    long long limit = *((long long*)args->args[2]);
    // clean, ascii version of user supplied utf8 strings
    char *ascii_str1 = NULL, *ascii_str2 = NULL;

    // get a pointer to memory previously allocated
    struct workspace_t * ws = (struct workspace_t *) init->ptr;

    if (limit >= LENGTH_MAX) {
        *error = 1; return -1;
    }

    longlong len1 = (str1 == NULL) ? 0 : args->lengths[0];
    longlong len2 = (str2 == NULL) ? 0 : args->lengths[1];

    debug_print("BEFORE]cad1(%s) lencad1(%lld) cad2(%s) lencad2(%lld)", str1, len1, str2, len2);

    if ( len1 == 0 || len2 == 0 ) {
        if ( len1 == 0 ) {
            if ( len2 < limit ) {
                return len2;
            } else {
                return limit;
            }
        } else if ( len2 == 0 ) {
            if ( len1 < limit ) {
                return len1;
            } else {
                return limit;
            }
        } else {
            *error = 1; return -1;
        }
    }

    if ( (ascii_str1 = utf8toascii(str1, &len1, ws->ic, ws->mbstate, ws->str1)) == NULL ) {
        *error = 1; return -1;
    }
    if ( (ascii_str2 = utf8toascii(str2, &len2, ws->ic, ws->mbstate, ws->str2)) == NULL ) {
        *error = 1; return -1;
    }

    debug_print("AFTER]cad1(%s) lencad1(%lld) cad2(%s) lencad2(%lld)", ascii_str1, len1, ascii_str2, len2);

    debug_print("%s", "about to run levenshtein func");

    return damlevlim_core(
        ws,
        ascii_str1, len1,
        ascii_str2, len2,
        /* swap */              1,
        /* substitution */      1,
        /* insertion */         1,
        /* deletion */          1,
        /* limit */             limit
    );

}

//! deallocate memory, clean and close
void damlevlim_deinit(UDF_INIT *init) {

    if (init->ptr != NULL) {
        struct workspace_t * ws = (struct workspace_t *) init->ptr;
        iconv_close(ws->ic);
        free(ws->row2); free(ws->row1); free(ws->row0);
        free(ws->str2); free(ws->str1);
        free(ws->mbstate); free(ws);
    }

    debug_print("%s", "deinit successful");
    // (void) pthread_mutex_destroy(&LOCK);
}

//! core damlevlim_core function
int damlevlim_core(struct workspace_t *ws,
    const char *str1, int len1,
    const char *str2, int len2,
    int w, int s, int a, int d, int limit) {

    int *row0 = ws->row0; //(int*)malloc(sizeof(int) * (len2 + 1));
    int *row1 = ws->row1; //(int*)malloc(sizeof(int) * (len2 + 1));
    int *row2 = ws->row2; //(int*)malloc(sizeof(int) * (len2 + 1));
    int i, j;

    for (j = 0; j <= len2; j++)
        row1[j] = j * a;
    for (i = 0; i < len1; i++) {
        int *dummy;

        row2[0] = (i + 1) * d;
        for (j = 0; j < len2; j++) {
            /* substitution */
            row2[j + 1] = row1[j] + s * (str1[i] != str2[j]);
            /* swap */
            if (i > 0 && j > 0 && str1[i - 1] == str2[j] &&
                str1[i] == str2[j - 1] &&
                row2[j + 1] > row0[j - 1] + w) {
                row2[j + 1] = row0[j - 1] + w;
            }
            /* deletion */
            if (row2[j + 1] > row1[j + 1] + d) {
                row2[j + 1] = row1[j + 1] + d;
            }
            /* insertion */
            if (row2[j + 1] > row2[j] + a) {
                row2[j + 1] = row2[j] + a;
            }
        }

        dummy = row0;
        row0 = row1;
        row1 = row2;
        row2 = dummy;

        if (row1[len2] >= limit)
            return row1[len2];
    }

    return row1[len2];
}

//! helper that translates an utf8 string to ascii with some error return codes
char * utf8toascii(const char *str_src, longlong *len_src,
    iconv_t ic, mbstate_t *mbstate, char *str_dst) {

    size_t len_mbsrtowcs;
    char *ret = str_dst, *in_s = (char *)str_src; //utf8;
    size_t retlen = LENGTH_MAX+1;

    memset((void *)mbstate, '\0', sizeof(mbstate_t));
    if ( (len_mbsrtowcs = mbsnrtowcs(NULL, &str_src, *len_src, 0, mbstate)) == (size_t) -1 ) {
        debug_print("str_src(%s): %s", str_src, strerror(errno));
        return NULL;
    }

    if ( len_mbsrtowcs == *len_src ) {
        strncpy(str_dst, str_src, len_mbsrtowcs);
        //printf("2>%s\n", ws->str_s);
        str_dst[len_mbsrtowcs] = '\0';
        str_dst[len_mbsrtowcs + 1] = '\0';
        return str_dst;
    }

    if ( iconv(ic, &in_s, (size_t *) len_src, &ret, &retlen) == (size_t) -1 ) {
        debug_print("str_src(%s): %s", str_src, strerror(errno));
        return NULL;
    }

    *len_src = len_mbsrtowcs; // adjust converted length
    str_dst[len_mbsrtowcs] = '\0';
    str_dst[len_mbsrtowcs + 1] = '\0';

    if ( iconv(ic, NULL, NULL, NULL, NULL) == (size_t) -1 ) {
        return NULL;
    }

    return str_dst;
}

#endif // HAVE_DLOPEN
