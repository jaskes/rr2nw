
#ifndef	__DEBUGEXT_CPP__
	#include<stdlib.h>
	#include<stdio.h>
	#include<stdarg.h>
	#include<string.h>
	#define _NOAUTOINCLUDE
	#include"stdtypes.h"
	#include"debugext.h"
#endif

#ifdef __NT__
	#include<windows.h>
	#include<conio.h>
#endif

// --------------------------------------------------------------------------
static	bool	bDebugAssertionFlag = FALSE;
static	void	(*pDebugCleanupProc)() = NULL;
static	TPDebugFatalProc pDebugFatalProc = NULL;
static	dword	dwDebugFlags = DBGF_DEFAULT;
static	TCchar	*pszDebugLogFileName = "logfile";
static	FILE	*pDebugLogFile = NULL;
static	bool	bDebugLogFileAppend = FALSE;
static	char	szDebugDashes[] = "-------------------------------------------------------\n";
#ifndef __sgi
#ifdef _MSC_VER
static char **DebugArgv() { return *__p___argv(); }
#else
extern "C" {
	extern char **_argv;
}
static char **DebugArgv() { return _argv; }
#endif
#endif
// --------------------------------------------------------------------------
//#if	!defined(__NT__) || defined(_DEBUG)
static	FILE	*DebugOpenLogFile()
{
	if(!pDebugLogFile)
		pDebugLogFile = fopen(pszDebugLogFileName,
										bDebugLogFileAppend?"at":"wt");
	return	pDebugLogFile;
}
//#endif
// --------------------------------------------------------------------------
#ifdef	__WATCOMC__
	void Int3();
	#pragma aux Int3 = "int 3";
	void Int16_0();
	#pragma aux Int16_0 = \
		"mov ah,0" \
		"int 16h" \
		modify [eax]
#elif	defined(__BORLANDC__)
	#define	Int3()	__asm { int 3 }
	#define	Int16_0()	__asm { mov ah,0; int 0x16 }
#elif	defined(_MSC_VER)
	#include<intrin.h>
	#define	Int3()	__debugbreak()
	#define	Int16_0()	((void)0)
#elif	defined(__sgi)
#include<assert.h>
inline void Int3() { IF_DEBUG(assert(0)); }
inline void Int16_0() {}
#endif
// --------------------------------------------------------------------------
extern "C" int
	#ifdef	_DEBUG
		DebugAssertionFailedD(TCchar *pszAssertion,TCchar *pszFile,int nLine,
															TCchar *pszMsg)
	#else
		DebugAssertionFailedR(TCchar *pszMsg)
	#endif
{
	if( dwDebugFlags & DBGF_ERR_ABORT ) {
		if(bDebugAssertionFlag) return 0;
		bDebugAssertionFlag = TRUE;
		if(pDebugCleanupProc)	pDebugCleanupProc();
	}
	FILE *f = dwDebugFlags & DBGF_ERR_LOG ? DebugOpenLogFile() : stdout;
	#ifdef	__NT__
		if( f == stdout ) {
			HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD outputType = hOutput == INVALID_HANDLE_VALUE ?
				FILE_TYPE_UNKNOWN : GetFileType(hOutput);
			if( outputType != FILE_TYPE_PIPE && outputType != FILE_TYPE_DISK ) {
				AllocConsole();
				freopen("CONOUT$","wt",stdout);
			}
		}
	#endif
	if(dwDebugFlags & DBGF_ERR_LOG) fprintf(f,szDebugDashes);
	#ifndef __sgi
		if(dwDebugFlags & DBGF_ERR_EXENAME)
			fprintf(f,"Executable: %s\n",DebugArgv()[0]);
	#endif
	#ifdef _DEBUG
		if(dwDebugFlags & DBGF_ERR_SOURCE)
			fprintf(f,"Debug Assertion Failed\n"
					"File: %s\n"
					"Line: %d\n"
					"Assertion: %s\n", pszFile, nLine, pszAssertion );
		else
			fprintf(f,"Debug Assertion Failed: %s\n", pszAssertion );
		if(pszMsg)
			fprintf(f,"Message: %s\n", pszMsg );
		if(dwDebugFlags & DBGF_ERR_CALLS)
			CDebugCallStack::Dump(f,MAKEBOOL(dwDebugFlags & DBGF_ERR_LOG));
	#else
		if(!pszMsg)	pszMsg = "Unknown";
		fprintf(f,"Run-time error: %s\n", pszMsg );
	#endif
	if(dwDebugFlags & DBGF_ERR_LOG) {
		fprintf(f,szDebugDashes);
		if( dwDebugFlags & (DBGF_FLUSHLOG|DBGF_ERR_ABORT) ) fflush(f);
	}
	if( dwDebugFlags & DBGF_ERR_ABORT ) {
		if(dwDebugFlags & DBGF_ERR_LOG) {
			#ifdef _DEBUG
				printf("Debug Assertion Failed. See logfile for details\n");
			#else
				printf("Run-time error: %s\n", pszMsg );
			#endif
		}
		#ifdef _MSC_VER
			printf("Terminating after fatal error\n");
		#else
			printf("Press any key\n");
		#endif
		if( pDebugLogFile )	fclose(pDebugLogFile);
		else fflush(f);
		if( pDebugFatalProc ) {
			#ifdef _DEBUG
				pDebugFatalProc(pszAssertion,pszFile,nLine,pszMsg);
			#else
				pDebugFatalProc(NULL,NULL,0,pszMsg);
			#endif
		}
		#ifdef __NT__
			#if !defined(_MSC_VER)
				if( ! getch() ) getch();
				if( dwDebugFlags & DBGF_ERR_INT3 )	Int3();
			#endif
			ExitProcess(1);
		#else
			Int16_0();
			if( dwDebugFlags & DBGF_ERR_INT3 )	Int3();
			exit(1);
		#endif
	}
	return 0;
}

extern "C" int
	#ifdef	_DEBUG
		DebugAssertionFailedDP(TCchar *pszAssertion,TCchar *pszFile,int nLine,
														TCchar *pszMsg,...)
	#else
		DebugAssertionFailedRP(TCchar *pszMsg,...)
	#endif
{
	va_list ap;
	va_start(ap,pszMsg);
	char szFormattedMsg[1024] = "";
	if( pszMsg ) {
		#ifdef _MSC_VER
			_vsnprintf_s(szFormattedMsg,sizeof(szFormattedMsg),_TRUNCATE,
						pszMsg,ap);
		#else
			vsnprintf(szFormattedMsg,sizeof(szFormattedMsg),pszMsg,ap);
			szFormattedMsg[sizeof(szFormattedMsg)-1] = 0;
		#endif
	}
	va_end(ap);
	if( dwDebugFlags & DBGF_ERR_ABORT ) {
		if(bDebugAssertionFlag) return 0;
		bDebugAssertionFlag = TRUE;
		if(pDebugCleanupProc)	pDebugCleanupProc();
	}
	FILE *f = dwDebugFlags & DBGF_ERR_LOG ? DebugOpenLogFile() : stdout;
	#ifdef	__NT__
		if( f == stdout ) {
			HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD outputType = hOutput == INVALID_HANDLE_VALUE ?
				FILE_TYPE_UNKNOWN : GetFileType(hOutput);
			if( outputType != FILE_TYPE_PIPE && outputType != FILE_TYPE_DISK ) {
				AllocConsole();
				freopen("CONOUT$","wt",stdout);
			}
		}
	#endif
	if(dwDebugFlags & DBGF_ERR_LOG) fprintf(f,szDebugDashes);
	#ifndef	__sgi
		if(dwDebugFlags & DBGF_ERR_EXENAME)
			fprintf(f,"Executable: %s\n",DebugArgv()[0]);
	#endif
	#ifdef _DEBUG
		if(dwDebugFlags & DBGF_ERR_SOURCE)
			fprintf(f,"Debug Assertion Failed\n"
					"File: %s\n"
					"Line: %d\n"
					"Assertion: %s\n", pszFile, nLine, pszAssertion );
		else
			fprintf(f,"Debug Assertion Failed: %s\n", pszAssertion );
		if(pszMsg) {
			fprintf(f,"Message: ");
			fprintf(f,"%s",szFormattedMsg);
			fprintf(f,"\n");
		}
		if(dwDebugFlags & DBGF_ERR_CALLS)
			CDebugCallStack::Dump(f,MAKEBOOL(dwDebugFlags & DBGF_ERR_LOG));
	#else
		if(!pszMsg)	pszMsg = "Unknown";
		fprintf(f,"Run-time error: ");
		fprintf(f,"%s",szFormattedMsg[0] ? szFormattedMsg : pszMsg);
		fprintf(f,"\n");
	#endif
	if(dwDebugFlags & DBGF_ERR_LOG) {
		fprintf(f,szDebugDashes);
		if( dwDebugFlags & (DBGF_FLUSHLOG|DBGF_ERR_ABORT) ) fflush(f);
	}
	if( dwDebugFlags & DBGF_ERR_ABORT ) {
		if(dwDebugFlags & DBGF_ERR_LOG) {
			#ifdef _DEBUG
				printf("Debug Assertion Failed. See logfile for details\n");
			#else
				printf("Run-time error: ");
				printf("%s",szFormattedMsg[0] ? szFormattedMsg : pszMsg);
				printf("\n");
			#endif
		}
		#ifdef _MSC_VER
			printf("Terminating after fatal error\n");
		#else
			printf("Press any key\n");
		#endif
		if( pDebugLogFile )	fclose(pDebugLogFile);
		else fflush(f);
		if( pDebugFatalProc ) {
			TCchar *pszFatalMsg = szFormattedMsg[0] ? szFormattedMsg : pszMsg;
			#ifdef _DEBUG
				pDebugFatalProc(pszAssertion,pszFile,nLine,pszFatalMsg);
			#else
				pDebugFatalProc(NULL,NULL,0,pszFatalMsg);
			#endif
		}
		#ifdef __NT__
			#if !defined(_MSC_VER)
				if( ! getch() ) getch();
				if( dwDebugFlags & DBGF_ERR_INT3 )	Int3();
			#endif
			ExitProcess(1);
		#else
			Int16_0();
			if( dwDebugFlags & DBGF_ERR_INT3 )	Int3();
			exit(1);
		#endif
	}
	return 0;
}
// --------------------------------------------------------------------------
#ifdef	_DEBUG
extern "C"
void	DebugWriteLog(TCchar *pszFile,int nLine,TCchar *pszMsg)
{
	(void)pszFile; (void)nLine;
	if(!(dwDebugFlags & DBGF_LOG_MSG))	return;
	FILE *f = DebugOpenLogFile();
	ASSERT(pszMsg);
	fprintf(f,"%s",pszMsg);
	if( dwDebugFlags & DBGF_FLUSHLOG )	fflush(f);
}
extern "C"
void	DebugWriteLogP(TCchar *pszFile,int nLine,TCchar *pszMsg,...)
{
	(void)pszFile; (void)nLine;
	if(!(dwDebugFlags & DBGF_LOG_MSG))	return;
	FILE *f = DebugOpenLogFile();
	ASSERT(pszMsg);
	va_list ap;
	va_start(ap,pszMsg);
	vfprintf(f,pszMsg,ap);
	va_end(ap);
	if( dwDebugFlags & DBGF_FLUSHLOG )	fflush(f);
}
#endif
// --------------------------------------------------------------------------
extern "C"
void (*SetDebugCleanupProc(void (*pCleanupProc)(void)))(void)
{
	typedef	void (*TPCleanUpProc)(void);
	//void	(*pPreviousProc)(void) = pDebugCleanupProc;
	TPCleanUpProc	pPreviousProc = pDebugCleanupProc;
	pDebugCleanupProc = pCleanupProc;
	return	pPreviousProc;
}
// --------------------------------------------------------------------------
extern "C" TPDebugFatalProc SetDebugFatalProc(TPDebugFatalProc pFatalProc)
{
	TPDebugFatalProc pPreviousProc = pDebugFatalProc;
	pDebugFatalProc = pFatalProc;
	return pPreviousProc;
}
// --------------------------------------------------------------------------
extern "C" void DebugTriggerFatalForTesting()
{
	#ifdef _DEBUG
		DebugAssertionFailedDP("controlled legacy fatal", "legacy-fatal-smoke",
						77, "controlled legacy RTCHECK %d", 17);
	#else
		DebugAssertionFailedRP("controlled legacy RTCHECK %d", 17);
	#endif
}
// --------------------------------------------------------------------------
extern "C" dword SetDebugFlags(dword dwFlags)
{
	dword	dwOldFlags = dwDebugFlags;
	dwDebugFlags = dwFlags;
	return dwOldFlags;
}
extern "C" dword GetDebugFlags()
{
	return dwDebugFlags;
}
// --------------------------------------------------------------------------
extern "C" TCchar *SetDebugLogFileName(TCchar *pszName,bool bAppend)
{
	TCchar	*pszOldName = pszDebugLogFileName;
	pszDebugLogFileName = pszName;
	bDebugLogFileAppend = bAppend;
	if(pDebugLogFile) {
		fclose(pDebugLogFile);
		pDebugLogFile = NULL;
	}
	return pszOldName;
}
extern "C" TCchar *GetDebugLogFileName()
{
	return pszDebugLogFileName;
}
extern "C" void FlushDebugLogFile()
{
	if(pDebugLogFile)	fflush(pDebugLogFile);
}
// --------------------------------------------------------------------------
#ifdef _DEBUG
TCchar	*CDebugCallStack::m_procName[128];
int		CDebugCallStack::m_procNameCnt = 0;
int		CDebugCallStack::m_procNamePos = 0;

void CDebugCallStack::Dump(FILE *f,bool bNewLines)
{
	if(m_procNameCnt==0) {
		fprintf(f,"Call stack empty\n");
	}
	else {
		fprintf(f,bNewLines?"Call stack:\n":"Call stack: ");
		for(int i=0; i<m_procNameCnt;++i) {
			fprintf(f,bNewLines?"  >%s\n":"%s ",
				m_procName[(m_procNamePos-i+128)&127]
			);
		}
		if(!bNewLines) fprintf(f,"\n");
	}

}
#endif
