/*
 *  cseTest.c
 *  sfCAN
 *
 *  Created by René J.V. Bertin on 20110705.
 *  Copyright 2011 IFSTTAR — LEPSIS. All rights reserved.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "CritSectEx/timing.h"

#ifndef CRITSECT
#	define CRITSECT	CritSectEx
#endif

#include "CritSectEx/CritSectEx.h"

#if defined(__windows__)
#	if WIN32_WINNT < 0x0502
#		define GetThreadId(h)	0
#	endif
#else
#	include <semaphore.h>
#endif


#define SLEEPTIMEBG	3.14159
#define SLEEPTIMEFG	2.71828
#undef BUSYSLEEPING

#define LOCKSCOPEBG
#define LOCKSCOPEFG

CSEHandle *csex;
#ifndef true
#	define true 1
#endif
#ifndef false
#	define false 0
#endif
long bgRun = true;

double tStart;

#if defined(__windows__)
	char *winError( DWORD err )
	{ static char errStr[512];
		FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM
				    | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
				    NULL, err, 0, errStr, sizeof(errStr), NULL );
		return errStr;
	}
#	if !defined(__MINGW32__) && !defined(__MINGW64__)
	static int snprintf( char *buffer, size_t count, const char *format, ... )
	{ int n;
		va_list ap;
		va_start( ap, format );
		n = _vsnprintf( buffer, count, format, ap );
		if( n < 0 ){
			buffer[count-1] = '\0';
		}
		return n;
	}
#	endif

	void MMSleep(double seconds)
	{
		timeBeginPeriod(1);
		WaitForSingleObjectEx( GetCurrentThread(), (DWORD)(seconds * 1000), TRUE );
		timeEndPeriod(1);
	}
#else
#	define winError(err)	strerror(err)
#	define MMSleep(s)		usleep((useconds_t)(s*1000000))
#endif

#ifdef __GNUC__

#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#endif

HANDLE nudgeEvent = NULL;

THREAD_RETURN WINAPI bgThreadSleeper( LPVOID dum )
{
	fprintf( stderr, "## GetCurrentThread() = 0x%p\n", GetCurrentThread() ); Sleep(5);
	fprintf( stderr, "## GetCurrentThread() = 0x%p\n", GetCurrentThread() ); Sleep(5);
	fprintf( stderr, "## GetCurrentThread() = 0x%p\n", GetCurrentThread() ); Sleep(5);
	fprintf( stderr, "##%lx bgThreadSleeper starting to sleep for 5s at t=%g\n", GetCurrentThreadId(), HRTime_Time() - tStart );
	Sleep(5000);
	fprintf( stderr, "##%lx bgThreadSleeper will exit at t=%g\n", GetCurrentThreadId(), HRTime_Time() - tStart );
	return (THREAD_RETURN)1;
}

THREAD_RETURN WINAPI bgThread2Nudge( LPVOID dum )
{ unsigned long ret;
  double tEnd;
	fprintf( stderr, "##%lx bgThread2Nudge starting to wait for nudge event at t=%g\n", GetCurrentThreadId(), HRTime_Time() - tStart );
	ret = WaitForSingleObject( nudgeEvent, INFINITE ); tEnd = HRTime_Time();
	fprintf( stderr, "##%lx WaitForSingleObject( nudgeEvent, INFINITE ) = %lu at t=%g; sleep(1ms) and then send return nudge\n", GetCurrentThreadId(), ret, tEnd - tStart );
	Sleep(1);
	fprintf( stderr, "##%lx t=%g SetEvent(nudgeEvent) = %d\n", GetCurrentThreadId(), HRTime_Time() - tStart, SetEvent(nudgeEvent) );
	return (THREAD_RETURN)2;
}

THREAD_RETURN WINAPI bgThread4SemTest( LPVOID dum )
{ unsigned long ret;
  double tEnd;
  HANDLE hh = OpenSemaphore( DELETE|SYNCHRONIZE|SEMAPHORE_MODIFY_STATE, false, (char*)"cseSem");
	if( hh ){
#if defined(__windows__)
		fprintf( stderr, "##%lx bgThread4SemTest starting to wait for semaphore (0x%p) release at t=%g\n",
			   GetCurrentThreadId(), hh, HRTime_Time() - tStart );
#else
		fprintf( stderr, "##%lx bgThread4SemTest starting to wait for semaphore (0x%p,%s,%p) release at t=%g\n",
			   GetCurrentThreadId(), hh, hh->d.s.name, hh->d.s.sem, HRTime_Time() - tStart );
#endif
		ret = WaitForSingleObject( hh, INFINITE ); tEnd = HRTime_Time();
		fprintf( stderr, "##%lx WaitForSingleObject( hh, INFINITE ) = %lu at t=%g; sleep(1ms) and ReleaseSemaphore(hh,1,NULL)\n",
			   GetCurrentThreadId(), ret, tEnd - tStart );
		Sleep(1);
		fprintf( stderr, "##%lx t=%g ReleaseSemaphore(hh,1,NULL) = %d\n",
			   GetCurrentThreadId(), HRTime_Time() - tStart, ReleaseSemaphore(hh, 1, NULL) );
		CloseHandle(hh);
  	}
	else{
		fprintf( stderr, "##%lx bgThread4SemTest() couldn't obtain semaphore '%s', will exit\n", GetCurrentThreadId(), "cseSem" );
	}
	return (THREAD_RETURN)3;
}

THREAD_RETURN WINAPI bgCSEXaccess( LPVOID dum )
{ static unsigned long n = 0;
	fprintf( stderr, "## entering bgCSEXaccess thread 0x%lx at t=%gs\n", GetCurrentThreadId(), HRTime_toc() );
	while( bgRun ){
	  double t0, t1;
		if( IsCSEHandleLocked(csex) ){
			fprintf( stderr, "\tbgCSEXaccess waiting for csex lock\n" );
		}
		t0 = HRTime_toc();
		{
#ifdef LOCKSCOPEBG
		  CSEScopedLock *scope = ObtainCSEScopedLock(csex);
#else
		  unsigned char unlock = LockCSEHandle(csex);
#endif
			t1 = HRTime_toc();
			n += 1;
			fprintf( stderr, "## got csex lock #%lu=%d at t=%g after %gs; starting %g s wait\n",
					n, IsCSEHandleLocked(csex), t1, t1-t0, SLEEPTIMEBG ); fflush(stderr);
#ifndef BUSYSLEEPING
			MMSleep(SLEEPTIMEBG);
#else
			do{
				t1 = HRTime_toc();
			} while (t1-t0 < SLEEPTIMEBG);
#endif
			fprintf( stderr, "\tbgCSEXaccess wait #%lu ended at t=%gs; csex lock=%d\n",
				n, HRTime_toc(), IsCSEHandleLocked(csex) ); fflush(stderr);
#ifndef LOCKSCOPEFG
			UnlockCSEHandle( csex, unlock );
#else
			scope = ReleaseCSEScopedLock(scope);
#endif
		}
		// just to give the other thread a chance to get a lock:
		Sleep(1);
	}
	fprintf( stderr, "## exiting bgCSEXaccess thread at t=%gs\n", HRTime_toc() );
	return (THREAD_RETURN)4;
}

typedef struct kk {
	int count;
} kk;

void u1handler(int sig)
{
	fprintf( stderr, "process %lu (thread %p) received signal %d\n",
		   GetCurrentProcessId(), GetCurrentThreadId(), sig );
}

int main( int argc, char *argv[] )
{ int i;
  HANDLE bgThread;
  DWORD exitCode;
  SECURITY_ATTRIBUTES semSec;

	MSEmul_UseSharedMemory(true);

#if 1
  char test[256], *head;
	test[0] = '\0';
	head = test;
	fprintf( stderr, "appending to test[%lu] with snprintf:\n", sizeof(test) );
	do{
	  int len = strlen(test), n;
	  size_t ni = strlen("0123456789");
		fprintf( stderr, "len(test[%lu])=%d, rem. %lu added", sizeof(test), len, (size_t)(sizeof(test) - len) ); fflush(stderr);
		n = snprintf( &test[len], (size_t)(sizeof(test) - len), "0123456789" );
		head += n;
		fprintf( stderr, " %d (head[%lu]=", n, head - test ); fflush(stderr);
		if( len + n < sizeof(test) ){
			fprintf( stderr, "%c) -> %lu\n", head[0], strlen(test) );
		}
		else{
			fprintf( stderr, "!$@#) -> %lu\n", strlen(test) );
		}
	} while( strlen(test) < sizeof(test)-1 );
	fprintf( stderr, "test = %s\n", test );
#endif
	init_HRTime();
	tStart = HRTime_Time();
	HRTime_tic();
	{ double t0;
	  DWORD ret;
	  HANDLE hh;
		if( (hh = CreateSemaphore( NULL, 1, 0x7FFFFFFF, NULL )) ){
			t0 = HRTime_Time();
			ret = WaitForSingleObject(hh, (DWORD)(SLEEPTIMEFG*1000));
			t0 = HRTime_Time() - t0;
			fprintf( stderr, "WaitForSingleObject(hh,%lu)==%lu took %g seconds\n",
				   (DWORD)(SLEEPTIMEFG*1000), ret, t0
			);
			t0 = HRTime_Time();
			ret = WaitForSingleObject(hh, (DWORD)(SLEEPTIMEFG*1000));
			t0 = HRTime_Time() - t0;
			fprintf( stderr, "WaitForSingleObject(hh,%lu)==%lu took %g seconds\n",
				   (DWORD)(SLEEPTIMEFG*1000), ret, t0
			);
			t0 = HRTime_Time();
			ret = WaitForSingleObject(hh, 500);
			t0 = HRTime_Time() - t0;
			fprintf( stderr, "WaitForSingleObject(hh,500)==%lu took %g seconds\n",
				   ret, t0
			);
			ReleaseSemaphore(hh, 1, NULL);
			t0 = HRTime_Time();
			ret = WaitForSingleObject(hh, 500);
			t0 = HRTime_Time() - t0;
			fprintf( stderr, "WaitForSingleObject(hh,500)==%lu took %g seconds after ReleaseSemaphore()\n",
				   ret, t0
			);
			CloseHandle(hh);
		}
		else{
			fprintf( stderr, "Error creating semaphore: %s\n", winError(GetLastError()) );
		}

		ret = 0;
		YieldProcessor();
		fprintf( stderr, "sizeof(long)=%lu\n", sizeof(long) );
		_WriteBarrier();
		{ long oval, lbool;
		  void *ptr = NULL, *optr;
			oval = _InterlockedCompareExchange( (long*) &ret, 10L, 0L );
			fprintf( stderr, "_InterlockedCompareExchange(&ret==0, 10, 0) == %lu, ret==%lu\n", oval, ret );
			optr = InterlockedCompareExchangePointer( &ptr, (void*) fprintf, NULL );
			fprintf( stderr, "InterlockedCompareExchangePointer(&ptr==NULL, fprintf==%p, NULL) == %p, ret==%p\n",
				   fprintf, optr, ptr );
			_InterlockedIncrement( (long*) &ret );
			fprintf( stderr, "_InterlockedIncrement(&ret) ret=%lu\n", ret );
			_InterlockedDecrement( (long*) &ret );
			fprintf( stderr, "_InterlockedDecrement(&ret) ret=%lu\n", ret );
			_ReadWriteBarrier();
			lbool = false;
			_InterlockedSetTrue(&lbool);
			fprintf( stderr, "lbool = %ld\n", lbool );
			_InterlockedSetTrue(&lbool);
			fprintf( stderr, "lbool = %ld\n", lbool );
			_InterlockedSetFalse(&lbool);
			fprintf( stderr, "lbool = %ld\n", lbool );
		}
	}
#ifdef DEBUG
	{ CSEScopedLock *scope = ObtainCSEScopedLock(NULL);
		fprintf( stderr, "NULL testscope %p:locked==%u\n", scope, IsCSEScopeLocked(scope) );
		scope = ReleaseCSEScopedLock(scope);
	}
#endif

	csex = CreateCSEHandle(4000);
	if( !csex ){
		fprintf( stderr, "Failure creating CSEHandle\n" );
		exit(1);
	}
	else{
		fprintf( stderr, "Created a '%s' CSEHandle with spinMax==%lu\n", CSEHandleInfo(csex), csex->spinMax );
	}

	fprintf( stderr, "GetCurrentThread() = 0x%p\n", GetCurrentThread() ); Sleep(5);
	fprintf( stderr, "GetCurrentThread() = 0x%p\n", GetCurrentThread() ); Sleep(5);
	fprintf( stderr, "GetCurrentThread() = 0x%p\n", GetCurrentThread() ); Sleep(5);

	fputs( "\n", stderr );
	if( (bgThread = CreateThread( NULL, 0, bgThreadSleeper, NULL, CREATE_SUSPENDED, NULL )) ){
	  unsigned long ret;
	  double tEnd;
		fprintf( stderr, ">%lx t=%g will start %lx and WaitForSingleObject on it (should take 5s)\n",
			   GetCurrentThreadId(), HRTime_Time() - tStart, GetThreadId(bgThread) );
		ResumeThread(bgThread);
//		Sleep(1);
		ret = WaitForSingleObject( bgThread, 1500 ); tEnd = HRTime_Time();
		GetExitCodeThread( bgThread, &exitCode );
		fprintf( stderr, ">%lx WaitForSingleObject(bgThread,1500)=%lu exitCode=%ld at t=%g\n",
			   GetCurrentThreadId(), ret, exitCode, tEnd - tStart );
		ret = WaitForSingleObject( bgThread, 10000 ); tEnd = HRTime_Time();
		GetExitCodeThread( bgThread, &exitCode );
		fprintf( stderr, ">%lx WaitForSingleObject(bgThread,10000)=%lu exitCode=%ld at t=%g\n",
			   GetCurrentThreadId(), ret, exitCode, tEnd - tStart );
		CloseHandle(bgThread);
	}
	fputs( "\n", stderr );
	if( (nudgeEvent = CreateEvent( NULL, false, false, NULL )) ){
		if( (bgThread = CreateThread( NULL, 0, bgThread2Nudge, NULL, 0, NULL )) ){
		  unsigned long ret;
		  double tEnd;
			Sleep(1000);
			fprintf( stderr, ">%lx t=%g SetEvent(nudgeEvent) = %d; sleep(1ms) and then wait for return nudge\n", GetCurrentThreadId(), HRTime_Time() - tStart, SetEvent(nudgeEvent) );
			Sleep(1);
			ret = WaitForSingleObject( nudgeEvent, INFINITE ); tEnd = HRTime_Time();
			fprintf( stderr, ">%lx WaitForSingleObject( nudgeEvent, INFINITE ) = %lu at t=%g\n",
				   GetCurrentThreadId(), ret, tEnd - tStart );
			ret = WaitForSingleObject( bgThread, 5000 ); tEnd = HRTime_Time();
			GetExitCodeThread( bgThread, &exitCode );
			fprintf( stderr, ">%lx WaitForSingleObject(bgThread,5000)=%lu exitCode=%ld at t=%g\n",
				   GetCurrentThreadId(), ret, exitCode, tEnd - tStart );
			CloseHandle(bgThread);
		}
		CloseHandle(nudgeEvent);
	}
	fputs( "\n", stderr );
	semSec.nLength = sizeof(SECURITY_ATTRIBUTES);
	semSec.lpSecurityDescriptor = NULL;
	semSec.bInheritHandle = true;
	// semSec does not actually need to be used:
	if( (nudgeEvent = CreateSemaphore( &semSec, 0, 0x7FFFFFFF, (char*) "cseSem" )) ){
		if( (bgThread = CreateThread( NULL, 0, bgThread4SemTest, NULL, 0, NULL )) ){
		  unsigned long ret;
		  double tEnd;
			Sleep(1000);
			fprintf( stderr, ">%lx t=%g ReleaseSemaphore(nudgeEvent,1,NULL) = %d; sleep(1ms) and then wait for return nudge\n",
				   GetCurrentThreadId(), HRTime_Time() - tStart, ReleaseSemaphore(nudgeEvent, 1, NULL) );
			Sleep(1);
			ret = WaitForSingleObject( nudgeEvent, INFINITE ); tEnd = HRTime_Time();
			fprintf( stderr, ">%lx WaitForSingleObject( nudgeEvent, INFINITE ) = %lu at t=%g\n",
				   GetCurrentThreadId(), ret, tEnd - tStart );
			ret = WaitForSingleObject( bgThread, 5000 ); tEnd = HRTime_Time();
			GetExitCodeThread( bgThread, &exitCode );
			fprintf( stderr, ">%lx WaitForSingleObject(bgThread,5000)=%lu exitCode=%ld at t=%g\n",
				   GetCurrentThreadId(), ret, exitCode, tEnd - tStart );
			CloseHandle(bgThread);
		}
		CloseHandle(nudgeEvent);
	}
	fputs( "\n", stderr );
	if( (bgThread = CreateThread( NULL, 0, bgCSEXaccess, NULL, CREATE_SUSPENDED, NULL )) ){
		fprintf( stderr, "csex is %slocked\n", (IsCSEHandleLocked(csex))? "" : "not " );
		SetThreadPriority( bgThread, GetThreadPriority(GetCurrentThread()) );
		fprintf( stderr, "GetThreadPriority(GetCurrentThread()) = %d\n", GetThreadPriority(GetCurrentThread()) );
		ResumeThread(bgThread);
		i = 0;
		fprintf( stderr, "entering main csex locking loop at t=%gs\n", HRTime_toc() );
		while( i < 5 ){
		  double t0, t1;

			if( IsCSEHandleLocked(csex) ){
				fprintf( stderr, "\tmain loop waiting for csex lock\n" );
			}
			t0 = HRTime_toc();
			{
#ifdef LOCKSCOPEFG
			  CSEScopedLock *scope = ObtainCSEScopedLock(csex);
#else
			  unsigned char unlock = LockCSEHandle(csex);
#endif
				t1 = HRTime_toc();
				i += 1;
				fprintf( stderr, "> got csex lock #%d=%d at t=%g after %gs; starting %g s wait\n",
						i, IsCSEHandleLocked(csex), t1, t1-t0, SLEEPTIMEFG ); fflush(stderr);
#ifdef BUSYSLEEPING
				MMSleep(SLEEPTIMEFG);
#else
				do{
					t1 = HRTime_toc();
				} while (t1-t0 < SLEEPTIMEFG);
#endif
				fprintf( stderr, "\tmain loop wait #%d ended at t=%gs; csex lock=%d\n",
					i, HRTime_toc(), IsCSEHandleLocked(csex) ); fflush(stderr);
#ifndef LOCKSCOPEFG
				UnlockCSEHandle( csex, unlock );
#else
				scope = ReleaseCSEScopedLock(scope);
#endif
			}
			// just to give the other thread a chance to get a lock:
			Sleep(1);
		}
		fprintf( stderr, "exiting main csex locking loop at t=%gs\n", HRTime_toc() );
		_InterlockedSetFalse(&bgRun);
		WaitForSingleObject( bgThread, 5000 );
		CloseHandle(bgThread);
		fprintf( stderr, "Background loop finished at t=%gs\n", HRTime_toc() );
	}
	else{
		fprintf( stderr, "Failure creating bgCSEXaccess thread\n" );
	}
	DeleteCSEHandle(csex);
	exit(0);
}
