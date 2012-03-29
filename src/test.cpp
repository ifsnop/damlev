#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <wctype.h>
#include <iconv.h>
#include <errno.h>

int main(int argc, char* argv[]) {

    wchar_t wp[2560*sizeof(wchar_t)];
    const char *narrow = "TST diëgo";
    // multibyte
    char input[] = { 0x62, 0xc3, 0xa1, 0xc3, 0xa7, 0x63, 0x61, 0x4a, 0x6b, 0xc3, 0x87, 0x00 };
    char *inputptr = input;
    char *outbuf;
    char *outbufptr;
    
    wchar_t alphaw[] = L"éíáóúç\00\00";
    char alpha[] = "eiaouc\00\00";
    wchar_t *buffer;
    int len;
    char *output;
    int i,j;
    size_t k=0,l=0;

    setlocale(LC_ALL, "es_ES.UTF-8");

    outbuf = (char*)calloc(sizeof(char), 256);
    outbufptr = outbuf;
    
    iconv_t ic = iconv_open("ascii//TRANSLIT", "UTF-8" );

    if (ic<0) {
	printf("ErROR iconv_open\n");
        exit(-1);
    }

    k = strlen(input); l=k*2;

    printf("original:>%s<\n", inputptr);


    int count = iconv(ic, &inputptr, &k, &outbuf, &l);

    if (count==-1) {
    switch (errno) {
        case EILSEQ:
        printf("Invalid multibyte sequence.\n");
        break;
        case EINVAL:
        printf("Incomplete multibyte sequence.\n");
      break;
        case E2BIG:
        printf("No more room.\n");
        break;
        default:
        printf("Error: %s.\n", strerror (errno));
     }
     }
                      
                      
    printf("iconvcount: %d\n", count);
    
    printf("transformada: >%s<\n", outbufptr);


    return 0;

    //int mbtowc(wchar_t *pwc, const char *s, size_t n);

    buffer = (wchar_t*)calloc(sizeof(wchar_t), 6);
    output = (char*)calloc(sizeof(char), 6);

    len = mbstowcs(buffer, input, sizeof(input));

    for(i=0;i<len;i++)
	buffer[i] = towlower(buffer[i]);
//     wcstombs(NULL,buffer,0)

    printf("abecedario de conversion de %d letras.\n",strlen(alpha));

    int alphalen = strlen(alpha);
    for(j=0;j<len;j++) {
	printf("j: %d [%02X]\n", j, buffer[j]);
	int match=0;
	for(i=0;i<alphalen;i++) {
	   // if (!wcsncasecmp(&buffer[j],&alphaw[i],1)) {
	    if (buffer[j]==alphaw[i]) {
		printf("j: %d [%02X] %c, coincide\n", j, buffer[j], alpha[i]);
		match=1;
		break;
	    }
	}
	if (match==0) {
	    output[j] = buffer[j];
	} else {
	    output[j] = alpha[i];
	}
    }

    fprintf(stdout, "mb>%s<\n", input);
    
    fprintf(stdout, " c>%s<\n", output);
    
    return 0;
/*    len = fwide(stdout,1);
    fprintf(stdout, "fwide ret:%d\n", len);
    fwprintf(stdout, L" w>%s<\n", specialw);
    return 0;
  */  
    

    memset(wp, '\0', 2560*sizeof(wchar_t));
    len = mbstowcs(wp, narrow, 255);

    fprintf(stdout, "%s len:%d strlen:%d mblen:%d mbstowcs:%d\n", narrow, len, strlen(narrow), mblen(narrow,255), mbstowcs(NULL,narrow,0));

    for (unsigned int i = 0; i<strlen(narrow); ++i)
	fprintf(stdout, "[%c]%02X ", narrow[i], (unsigned int) *((unsigned char *)narrow+i));
    fprintf(stdout, "\n");


//    for (int i = 0; i< (wcslen(wp)/* * sizeof(wchar_t)*/); ++i)
//	fprintf(stdout, "[%c]%02X ", wp[i], (unsigned int) *((wchar_t *)wp+i));
//	fprintf(stdout, "\n");
/*
    for (int i = 0; i<len; ++i)
	fwprintf(stdout, L"[%c]%02X ", wp[i], wp);
	//, (unsigned int) *((unsigned char *)wp[i]));
    fprintf(stdout, "\n");
  */
    return 0;

}
