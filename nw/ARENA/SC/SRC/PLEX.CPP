#define lex_Include lex_Include_BrokenOwnership
#include "lex.c"
#undef lex_Include

/*
 * Keep this recovery shim outside Lex.c so its original DOS-era encoding
 * remains byte-for-byte intact.  The archived implementation allocates the
 * new scanner and points it at the compiler-owned scanner.  EOF then restores
 * into the allocation and frees the compiler-owned address.  Save the parent
 * state instead and continue scanning the include in the stable owner.
 */
TScanner *lex_Include( TScanner *scanner, const char *fileName )
 {
    TScanner   *previousScan;
    const char *includePath;
    char        nameBuf[1024];
    size_t      pathLength;
    size_t      fileLength;
    FILE       *f;

    if( scanner == NULL || fileName == NULL )
         return NULL;

    includePath = scanner->textPath != NULL ? scanner->textPath : "";
    pathLength = strlen( includePath );
    fileLength = strlen( fileName );
    if( pathLength + (pathLength != 0 ? 1 : 0) + fileLength >=
        sizeof(nameBuf) )
    {
         ERROR( ER_BUFFER_OVERFLOW );
         return NULL;
    }

    strcpy( nameBuf, includePath );
    if( pathLength != 0 && nameBuf[pathLength-1] != '\\' &&
        nameBuf[pathLength-1] != '/' )
         strcat( nameBuf, "\\" );
    strcat( nameBuf, fileName );
    f = fopen( nameBuf, "rb" );

    lex_Get( scanner );
    if( f == NULL )
    {
         ERROR( ER_INCLUDE_FILE_NOT_FOUND );
         return NULL;
    }

    previousScan = (TScanner *)(malloc(sizeof(TScanner)));
    if( previousScan == NULL )
    {
         fclose( f );
         ERROR( ER_BUFFER_OVERFLOW );
         return NULL;
    }

    memcpy( previousScan, scanner, sizeof(TScanner) );
    lex_InitScannerFromFile( scanner, f );
    scanner->textPath = previousScan->textPath;
    strncpy( scanner->fileName, fileName, MAXINCLUDEFILELEN-1 );
    scanner->fileName[MAXINCLUDEFILELEN-1] = 0;
    scanner->prevScanner = previousScan;
    return scanner;
 }
