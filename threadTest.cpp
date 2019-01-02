/*
 *  threadTest.cpp
 *  cseTest
 *
 *  Created by René J.V. Bertin on 20120626.
 *  Copyright 2012 IFSTTAR — LEPSIS. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <iostream>

#include "Thread/Thread.hpp"
// we use timing functions in this module (and cannot be sure the header 
// has been included through Thread.hpp):
#include "CritSectEx/timing.h"

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
	va_end(ap);
	return n;
}
#	endif

#else
#	define winError(err)	strerror(err)
#endif

THREAD_RETURN function0Args()
{ long count = 0;
	while( count < 5 ){
		fprintf( stderr, "##%lu(%p) function0Args() t=%gs\n",
			   GetCurrentThreadId(), GetCurrentThread(), HRTime_toc() );
		// increment the shared counter (with exclusive access)
		count += 1;
		Sleep(1000);
	}
	fprintf( stderr, "##%lu(%p) returning 123 at t=%gs\n", GetCurrentThreadId(), GetCurrentThread(), HRTime_toc() );
	return (THREAD_RETURN) 123;
}

THREAD_RETURN function1BoolArg(bool arg)
{ long count = 0;
	fprintf( stderr, "##%lu(%p) function1BoolArg(%s) t=%gs\n",
		   GetCurrentThreadId(), GetCurrentThread(), (arg)? "true" : "false", HRTime_toc() );
	while( count < 2 ){
		fprintf( stderr, "##%lu(%p) function1BoolArg(%s) t=%gs\n",
			   GetCurrentThreadId(), GetCurrentThread(), (arg)? "true" : "false", HRTime_toc() );
		// increment the shared counter (with exclusive access)
		count += 1;
		Sleep(1000);
	}
	fprintf( stderr, "##%lu(%p) returning 123 at t=%gs\n", GetCurrentThreadId(), GetCurrentThread(), HRTime_toc() );
	return (THREAD_RETURN) 123;
}

double function1Arg(DWORD *count)
{
	while( *count < 5 ){
		fprintf( stderr, "##%lu(%p) function1Arg(%p) t=%gs\n",
			   GetCurrentThreadId(), GetCurrentThread(), count, HRTime_toc() );
		// increment the shared counter (with exclusive access)
		*count += 1;
		Sleep(1000);
	}
	fprintf( stderr, "##%lu(%p) returning 123.456 at t=%gs\n", GetCurrentThreadId(), GetCurrentThread(), HRTime_toc() );
	return 123.456;
}

class DemoThread : public Thread
{ protected:
	DWORD Run( LPVOID arg )
	{ SharedValue<DWORD> *shCount = (SharedValue<DWORD>*) arg;
		// reset the shared counter (with exclusive access)
		*shCount = 0;
		while( 1 ){
			fprintf( stderr, "##%lu(%p) DemoThread Object Code t=%gs\n",
				   GetCurrentThreadId(), GetThread(), HRTime_toc() );
			// increment the shared counter (with exclusive access)
			*shCount += 1;
			Sleep(1000);
		}
		fprintf( stderr, "##%lu(%p) returning 123 at t=%gs\n", GetCurrentThreadId(), GetThread(), HRTime_toc() );
		return 123;
	}
	virtual void CleanupThread()
	{
		fprintf( stderr, "##%lu(%p) DemoThread Object Cleanup Code (through thread cancelling), exitCode=%lu t=%gs\n",
			   GetCurrentThreadId(), GetThread(), GetExitCode(), HRTime_toc() );
        fprintf( stderr, "##Backtrace:\n%s\n", Thread::BackTrace(-1).c_str() );
	}
};

class Demo2Thread : public Thread
{
	private:
		CritSectEx *outputLock;
	public:
		bool ok;
		Demo2Thread( int when, void* arg = NULL )
			: Thread( (SuspenderThreadTypes)when, arg)
		{
			if( GetThread() ){
				ok = true;
				outputLock = new CritSectEx(4000);
				fprintf( stderr, "##%lu(%p) created/lauched Demo2Thread object t=%gs (using shared mem=%d)\n",
					   GetCurrentThreadId(), GetThread(), HRTime_toc(), MSEmul_UseSharedMemory() );
			}
		}
		~Demo2Thread()
		{
			delete outputLock;
		}
		CritSectEx *getOutputLock()
		{
			return outputLock;
		}

	protected:
		virtual DWORD Run( LPVOID arg )
		{ DWORD *count = (DWORD*) arg;
			*count = 0;
			while( ok ){
				{ CritSectEx::Scope scope(outputLock,500);
#ifndef __windows__
					fprintf( stderr, "##%lu(%p) Demo2Thread Object '%s' Code t=%gs\n",
						   GetCurrentThreadId(), GetThread(),
						   m_ThreadCtx.m_hThread->asString().c_str(), HRTime_toc() );
#else
					fprintf( stderr, "##%lu(%p) Demo2Thread Object %p Code t=%gs\n",
						   GetCurrentThreadId(), GetThread(),
						   m_ThreadCtx.m_hThread, HRTime_toc() );
#endif
				}
				*count += 1;
				Sleep(1000);
			}
			{ CritSectEx::Scope scope(outputLock,500);
				fprintf( stderr, "##%lu(%p) returning 123 at t=%gs\n", GetCurrentThreadId(), GetThread(), HRTime_toc() );
			}
			return 123;
		}
		virtual void InitThread()
		{ CritSectEx::Scope scope(outputLock,500);
			ok = true;

			fprintf( stderr, "##%lu(%p) Demo2Thread Object Init Code t=%gs (using shared mem=%d)\n",
				   GetCurrentThreadId(), GetThread(), HRTime_toc(), MSEmul_UseSharedMemory() );
            fprintf( stderr, "##Backtrace:\n%s\n", Thread::BackTrace(-1).c_str() );
		}
		virtual void CleanupThread()
		{ CritSectEx::Scope scope(outputLock,500);
			fprintf( stderr, "##%lu(%p) Demo2Thread Object Cleanup Code, exitCode=%lu t=%gs\n",
				   GetCurrentThreadId(), GetThread(), GetExitCode(), HRTime_toc() );
            fprintf( stderr, "##Backtrace:\n%s\n", Thread::BackTrace(-1).c_str() );
		}
};

typedef struct TestStruct {
	int i;
	double d;
	TestStruct( int ii, double dd )
	{
		i = ii;
		d = dd;
	}
	int operator=(const int ii)
	{
		return i = ii;
	}
	int operator=(const double dd)
	{
		return d = dd;
	}
} TestStruct;

int main( int argc, char *argv[] )
{ DWORD counter = 0, stopRet, startRet = GetLastError();
	if( startRet != 0 ){
		fprintf( stderr, "Error %d = %s\n", startRet, winError(startRet) );
	}
	SetLastError(0);

	MSEmul_UseSharedMemory(true);
	SetLastError(0);

	SharedValue<DWORD> *shCounter = new SharedValue<DWORD>(0);
	startRet = GetLastError();
	if( startRet != 0 ){
		fprintf( stderr, "Error %d = %s\n", startRet, winError(startRet) );
	}

	init_HRTime();
	HRTime_tic();

	fprintf( stderr, "Immediate spawning of function0Args in a background thread...\n" );
	HRTime_tic();
	BackgroundFunction<THREAD_RETURN,int> bgFun0A(function0Args);
	fprintf( stderr, "\twaiting for bgFun0A to finish ..." ); fflush(stderr);
	stopRet = bgFun0A.Join();
	fprintf( stderr, "(%gs) status=%lu result=%lu\n", HRTime_toc(), stopRet, bgFun0A.result() );

	fprintf( stderr, "Delayed spawning of function1BoolArg in a background thread...\n" );
	counter = 0;
	BackgroundFunction<THREAD_RETURN,bool> bgFun1BA(function1BoolArg, false);
	fprintf( stderr, "\twaiting for bgFun1BA to finish ..." ); fflush(stderr);
	HRTime_tic();
	bgFun1BA.Continue();
	stopRet = bgFun1BA.Join();
	fprintf( stderr, "(%gs) status=%lu result=%lu\n", HRTime_toc(), stopRet, bgFun1BA.result() );

	fprintf( stderr, "Delayed spawning of function1Arg in a background thread...\n" );
	counter = 0;
	BackgroundFunction<double,DWORD*> bgFun1A(function1Arg, &counter, false);
	fprintf( stderr, "\twaiting for (%s) bgFun1A to finish ...", (bgFun1A.IsWaiting())? "unsuspended" : "running" );
	fflush(stderr);
	HRTime_tic();
	bgFun1A.Continue();
	stopRet = bgFun1A.Join();
	fprintf( stderr, "(%gs) status=%lu result=%g, counter=%lu\n", HRTime_toc(), stopRet, bgFun1A.result(), counter );

	counter = 0;
	DemoThread *dmt = new DemoThread;
	startRet = GetLastError();
	if( startRet != 0 ){
		fprintf( stderr, "Error %d = %s\n", startRet, winError(startRet) );
	}
	try{
		Thread T(*dmt);
		fprintf( stderr, ">>%lu started %p == %lu at t=%gs, sleeping 5s\n",
			   GetCurrentThreadId(), dmt->GetThread(), (startRet = dmt->Start(shCounter)), HRTime_toc() );
		if( startRet != 0 ){
			fprintf( stderr, "Error %d = %s\n", startRet, winError(startRet) );
		}
		else{
			fprintf( stderr, "Started, creator=%p (%p)\n", dmt->Creator(), T.Creator() );
		}
	}
	catch( const char* e ){
		fprintf( stderr, "Exception doing Thread T(*dmt): %s\n", e );
	}
	std::cout << "shCounter=" << *shCounter << "\n";
	SharedValue<double> kkk;
	std::cout << "SharedValue<double>kkk=" << kkk << "\n";
	{ SharedValue<double> kk(10.2);
		std::cout << "SharedValue<double>kk(10.2)=" << kk << "\n";
		volatile double _kk = kk;
		kkk = kk;
	}
	std::cout << "SharedValue<double>kkk=kk = " << kkk << "\n";
	try{
		volatile double _kkk = kkk;
	}
	catch( SharedValue_Exception &e ){
		fprintf( stderr, "Exception doing double _kkk = kkk : %s\n", e.what() );
	}
	{ SharedValue<DWORD>::DirectAccess shv(shCounter);
		// shv will export a pointer to the shared variable for exclusive 'direct access', preempting
		// all other access to the variable during its lifetime (as can be seen by moving the closing
		// curly brace to between the 2 Sleep(2500) statements).
		*shv.variable = 10;
		// test recursive/multiple locking, and reading the shared value through dereferencing:
		DWORD val0 = *shCounter;
		fprintf( stderr, "Set shCounter=%lu\n", val0 );
	}
		{ TestStruct kk(10, 3.14115), *kkk;
		  SharedValue<TestStruct> *shTT = new SharedValue<TestStruct>(kk);
			kkk = &kk;
			*kkk = 2;
			kk = 2.718;
			*shTT = kk;
			shTT->Value(kk);
		}
		{ int kk[4] = {1,2,3,4}, i, j;
		  SharedArray<int> *shVal = new SharedArray<int>(kk,sizeof(kk)/sizeof(kk[0]));
		  SharedArray<int> ss = *shVal;
			i = shVal->ValueAtIndex(1);
			j = (*shVal)[0];
			// fetch a reference and increment it with 2 ... NON preempted!! (The lock will have been released)
			(*shVal)[0] += 2;
			fprintf( stderr, "kk[1]=%d; *kk=%d/%d\n", i, j, **shVal );
			std::cout << "shVal=" << *shVal << "\n";
			std::cout << "ss=*shVal = " << ss << "\n";
		}
		Sleep(2500);
	Sleep(2500);
	stopRet = dmt->Stop(false);
	fprintf( stderr, ">>%lu %p->Stop(FALSE) == %ld, ExitCode=%lu, t=%gs\n",
		   GetCurrentThreadId(), &dmt, stopRet, dmt->GetExitCode(), HRTime_toc() );
	stopRet = dmt->Stop(true);
	fprintf( stderr, ">>%lu %p->Stop(TRUE) == %ld, ExitCode=%lu, t=%gs running time=%gs\n",
		   GetCurrentThreadId(), &dmt, stopRet, dmt->GetExitCode(), HRTime_toc(), dmt->RunningTime() );
	fprintf( stderr, "counter=%lu; shCounter(counter)=%lu\n\n", counter, shCounter->Value() );
	{ SharedValue<DWORD>::DirectAccess shv(shCounter);
		fprintf( stderr, "direct access shCounter::DirectAccess == %p=%lu\n", shv.variable, *shv.variable );
	}
	delete dmt;
    delete shCounter;

	HRTime_tic();
	SetLastError(0);
	Demo2Thread dmt2( THREAD_SUSPEND_BEFORE_INIT|THREAD_SUSPEND_AFTER_INIT|THREAD_SUSPEND_BEFORE_CLEANUP,
				  (void*)&counter );
	dmt2.ThreadPriority( dmt2.Creator() );
#ifdef __windows__
	std::cout << "dmt2's creator = " << dmt2.Creator() << "\n";
#else
	std::cout << "dmt2's creator = " << *(dmt2.Creator()) << "\n";
#endif
	if( dmt2.IsWaiting() ){
		startRet = GetLastError();
		if( startRet != 0 ){
			fprintf( stderr, "Error %d = %s\n", startRet, winError(startRet) );
		}
		{ CritSectEx::Scope scope(dmt2.getOutputLock(),500);
			fprintf( stderr, ">>%lu started %p == %lu at , IsWaiting()=%d sleeping 1.5s then Continue() so that Init() can run\n",
				   GetCurrentThreadId(), dmt2.GetThread(), startRet, HRTime_toc(), dmt2.IsWaiting() );
		}
		Sleep(1500);
		bool cRet = dmt2.Continue();
		double now = HRTime_toc();
		{ CritSectEx::Scope scope(dmt2.getOutputLock(),500);
			fprintf( stderr, ">>%lu  %p.Continue() == %d at t=%gs, sleeping 1s then Continue() so that Run() can run\n",
				   GetCurrentThreadId(), dmt2.GetThread(), cRet, now );
		}
		Sleep(1000);
		cRet = dmt2.Continue();
		now = HRTime_toc();
		{ CritSectEx::Scope scope(dmt2.getOutputLock(),500);
			fprintf( stderr, ">>%lu  %p.Continue() == %d at t=%gs, sleeping 5s then set ok=false and sleep 1.5s for Run() to exit\n",
				   GetCurrentThreadId(), dmt2.GetThread(), cRet, now );
		}
		Sleep(5000);
		dmt2.ok = false;
		Sleep(2500);
		cRet = dmt2.Continue();
		now = HRTime_toc();
		{ CritSectEx::Scope scope(dmt2.getOutputLock(),500);
			fprintf( stderr, ">>%lu  %p.Continue() == %d at t=%gs so that Cleanup() can run\n",
				   GetCurrentThreadId(), dmt2.GetThread(), cRet, now );
		}
		startRet = dmt2.Join(5000);
		if( startRet == WAIT_OBJECT_0 ){
			fprintf( stderr, ">>%lu  %p.Join() == %d at t=%gs\n",
				   GetCurrentThreadId(), dmt2.GetThread(), startRet, HRTime_toc() );
			stopRet = dmt2.Stop(false);
			fprintf( stderr, ">>%lu %p->Stop(FALSE) == %ld, ExitCode=%lu, t=%gs running time=%gs\n",
				   GetCurrentThreadId(), &dmt2, stopRet, dmt2.GetExitCode(), HRTime_toc(), dmt2.RunningTime() );
		}
		else{
			stopRet = dmt2.Stop(true);
			fprintf( stderr, ">>%lu %p->Stop(TRUE) == %ld, ExitCode=%lu, t=%gs running time=%gs\n",
				   GetCurrentThreadId(), &dmt2, stopRet, dmt2.GetExitCode(), HRTime_toc(), dmt2.RunningTime() );
		}
	}
	return 0;
}
