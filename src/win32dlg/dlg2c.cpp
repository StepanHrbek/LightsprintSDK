#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../LightsprintCore/RRReporter/reporterWindow.h"

void Error(char *msg)
{
    fprintf(stderr, "dlg2c error: %s, GetLastError(): %u\n", msg, (unsigned) GetLastError());
    exit(EXIT_FAILURE);
}

void ConvertDialog(const char *tableName, const char *resourceName)
{
    HRSRC res;
    HGLOBAL h;
    unsigned char *c;
    unsigned length;
    unsigned n;
    HMODULE module = GetModuleHandle(NULL);        
    printf("unsigned char %s[] = {\n", tableName);
    if ( (res = FindResourceA(module, resourceName, MAKEINTRESOURCEA(5))) == NULL ) Error("Couldn't find resource");
    if ( (h = LoadResource(module, res)) == NULL ) Error("Couldn't load resource");
    if ( (length = SizeofResource(module, res)) == 0 ) Error("Couldn't get resource length");
    c = (unsigned char*) h;
    n = 0;
    while ( length )
    {
        printf("%d", (unsigned) *(c++));
        length--;
        n++;        
        if ( length )
            printf(",");
        if ( n == 25 )
        {
            printf("\n");
            n = 0;
        }
    }
    FreeResource(h);
    printf("\n};\n\n");
}

int main(void)
{
    ConvertDialog("mConfDialog", (const char*) IDD_DIALOG1);
    return 0;
}
