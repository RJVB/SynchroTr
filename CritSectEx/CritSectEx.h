// kate: auto-insert-doxygen true; backspace-indents true; indent-width 5; keep-extra-spaces true; replace-tabs false; tab-indents true; tab-width 5;
/**
	@file CritSectEx.h
	A fast CriticalSection like class with timeout (taken from and) inspired by
	@n
	http://www.codeproject.com/KB/threads/CritSectEx.aspx
	released under the CPOL license (http://www.codeproject.com/info/cpol10.aspx)
	@n
	extended and ported to Mac OS X & linux by RJVB
 */

#ifdef SWIG

%module CritSectEx
%{
#	if !(defined(WIN32) || defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) || defined(SWIGWIN))
#		include "msemul.h"
#	endif
#	include "CritSectEx.h"
%}
%include <windows.i>
%feature("autodoc","3");

%init %{
	init_HRTime();
%}

#endif //SWIG

#ifndef _CRITSECTEX_H

#pragma once

#if defined(WIN32) || defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) || defined(SWIGWIN)
#	if !defined(WINVER) || WINVER < 0x0501
#		define WINVER 0x0501
#	endif
#	if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#		define _WIN32_WINNT 0x0501
#	endif
#	define	__windows__
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef CRITSECT
#	define CRITSECT	CritSectEx
#endif

#include "msemul.h"
//#if !defined(__windows__)
#	ifdef __cplusplus
#		include <cstdlib>
#		include <exception>
		typedef class cseAssertFailure : public std::exception{
		public:
			const char *msg;
			const int errcode;
			cseAssertFailure( const char *s, int errcode=errno )
				: errcode(errcode)
			{
				msg = s;
			}
			virtual const char* what() const throw()
			{
				return msg;
			}
			virtual const int code() const throw()
			{
				return errcode;
			}
		} cseAssertFailure;
#	endif
//#endif


#if /*defined(WIN32) || */ defined(_MSC_VER)
#	define InlDebugBreak()	{ __asm { int 3 }; }
#	pragma intrinsic(_WriteBarrier)
#	pragma intrinsic(_ReadWriteBarrier)
#elif (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#	define _WriteBarrier()		__sync_synchronize()
#	define _ReadWriteBarrier()	__sync_synchronize()
#endif

#if defined(DEBUG)
#	if defined(_MSC_VER)
#		ifndef ASSERT
//#			define ASSERT(x) do { if (!(x)) InlDebugBreak(); } while (false)
#			define ASSERT(x) cseAssertExInline((void*)(x),__FILE__,__LINE__,"ASSERT")
#		endif // ASSERT
#		ifndef VERIFY
#			define VERIFY(x) ASSERT(x)
#		endif
#	else
#		include <assert.h>
#		ifndef ASSERT
#			define ASSERT(x) assert(x)
#		endif // ASSERT
#		ifndef VERIFY
#			define VERIFY(x) ASSERT(x)
#		endif
#	endif
#else // DEBUG
#	ifndef ASSERT
#		define ASSERT(x)
#	endif // ASSERT
#	ifndef VERIFY
#		define VERIFY(x) (x)
#	endif
#endif // DEBUG

#ifndef STR
#	define STR(name)	# name
#endif
#ifndef STRING
#	define STRING(name)	STR(name)
#endif

#define EXCEPTION_FAILED_CRITSECTEX_SIGNAL	0xC20A018E

#ifdef __cplusplus
#	include <typeinfo>
#endif

#ifdef __cplusplus
	__forceinline static void cseAssertExInline(bool expected, const char *fileName, int linenr, const char *title="CritSectEx malfunction", const char *arg=NULL) throw(cseAssertFailure)
#else
	__forceinline static void cseAssertExInline(void *expected, const char *fileName, int linenr, const char *title, const char *arg)
#endif
	{
		if( !(expected) ){
#if defined(__windows__)
		  ULONG_PTR args[2];
		  int confirmation;
		  char msgBuf[1024];
#endif
		  const char *larg = (arg)? arg : "";;
			if( !title ){
				title = "CritSectEx malfunction";
			}
#if defined(__windows__)
			// error handling. Do whatever is necessary in your implementation.
#	ifdef _MSC_VER
			_snprintf_s( msgBuf, sizeof(msgBuf), sizeof(msgBuf),
					"assertion failure (cseAssertEx called from '%s' line %d%s%s) - continue execution?",
					fileName, linenr, (arg)? " with arg=" : "", larg
			);
#	else
			snprintf( msgBuf, sizeof(msgBuf),
					"assertion failure (cseAssertEx called from '%s' line %d%s%s) - continue execution?",
					fileName, linenr, (arg)? " with arg=" : "", larg
			);
#	endif
			msgBuf[sizeof(msgBuf)-1] = '\0';
			confirmation = MessageBox( NULL, msgBuf, title, MB_APPLMODAL|MB_ICONQUESTION|MB_YESNO );
			if( confirmation != IDOK && confirmation != IDYES ){
				args[0] = GetLastError();
				args[1] = (ULONG_PTR) linenr;
				RaiseException( EXCEPTION_FAILED_CRITSECTEX_SIGNAL, 0 /*EXCEPTION_NONCONTINUABLE_EXCEPTION*/, 2, args );
			}
#else
		  char msgBuf[1024];
			// error handling. Do whatever is necessary in your implementation.
			snprintf( msgBuf, sizeof(msgBuf),
					"fatal CRITSECT malfunction at '%s':%d%s%s)",
					fileName, linenr, (arg)? " with arg=" : "", larg
			);
			fprintf( stderr, "%s %s (errno=%d=%s)\n", msgBuf, title, errno, strerror(errno) );
			fflush(stderr);
#	ifdef __cplusplus
			throw cseAssertFailure(msgBuf);
#	endif
#endif
		}
	}
#ifdef __cplusplus
	extern void cseAssertEx(bool, const char *, int, const char*, const char*);
	extern void cseAssertEx(bool, const char *, int, const char*);
	extern void cseAssertEx(bool, const char *, int);
#endif

#if defined(__GNUC__) && (defined(i386) || defined(__i386__) || defined(__x86_64__) || defined(_MSEMUL_H))

#	define CRITSECTGCC

#else // WIN32?
#	ifdef DEBUG
#		include "timing.h"
#	endif
#endif // CRITSECTGCC

#ifdef __cplusplus
static inline void _InterlockedSetTrue( volatile long &atomic )
{
	if /*while*/( !atomic ){
		if( !_InterlockedIncrement(&atomic) ){
			YieldProcessor();
		}
	}
}

static inline void _InterlockedSetFalse( volatile long &atomic )
{
	while( atomic ){
		if( atomic > 0 ){
			if( _InterlockedDecrement(&atomic) ){
				YieldProcessor();
			}
		}
		else{
			if( _InterlockedIncrement(&atomic) ){
				YieldProcessor();
			}
		}
	}
}
#endif //__cplusplus

#ifndef _MSEMUL_H
/**
	set the referenced state variable to True in an atomic operation
	(which avoids changing the state while another thread is reading it)
 */
static inline void _InterlockedSetTrue( volatile long *atomic )
{
	if /*while*/( !*atomic ){
		if( !_InterlockedIncrement(atomic) ){
			YieldProcessor();
		}
	}
}

/**
	set the referenced state variable to False in an atomic operation
	(which avoids changing the state while another thread is reading it)
 */
static inline void _InterlockedSetFalse( volatile long *atomic )
{
	if /*while*/( *atomic ){
		if( _InterlockedDecrement(atomic) ){
			YieldProcessor();
		}
	}
}
#endif //_MSEMUL_H

#if defined(__cplusplus) && (defined(__windows__) || defined(CRITSECTGCC))
/**
	A fast critical section class based on Vladislav Gelfer's implementation
	at http://www.codeproject.com/KB/threads/CritSectEx.aspx . It uses a
	spinlock and/or semaphore, reducing the chance that the thread will be
	suspended. The class also provides a scoped lock, allowing create critical
	(preempted) blocks of code that will be unlocked even if one exits through
	an interrupt.
 */
class CritSectEx {

	static DWORD s_dwProcessors;
	struct membervars {

		// Declare all variables volatile, so that the compiler won't
		// try to optimise something important away.
#ifdef CRITSECTGCC
		volatile long	nLocker;
#else
		volatile DWORD	nLocker;
#endif
#ifdef DEBUG0
		volatile DWORD nLockedBy;
#endif
		volatile DWORD		dwSpinMax;
		volatile HANDLE	hSemaphore;
		volatile long		nWaiters;
		volatile bool		bIsLocked, bTimedOut;
// #ifdef DEBUG
		volatile bool		bUnlocking;
// #endif
	} m;

	// disable copy constructor and assignment
	CritSectEx(const CritSectEx&);
	void operator = (const CritSectEx&);

	__forceinline bool PerfLockImmediate(DWORD dwThreadID)
	{
		if( !_InterlockedCompareExchange((long*) &m.nLocker, dwThreadID, 0) ){
			m.bIsLocked = true;
			return true;
		}
		else{
			return false;
		}
	}
	__forceinline void WaiterPlus()
	{
		_InterlockedIncrement(&m.nWaiters);
	}
	__forceinline void WaiterMinus()
	{
		_InterlockedDecrement(&m.nWaiters);
	}

	bool PerfLock(DWORD dwThreadID, DWORD dwTimeout);
	__forceinline bool PerfLockKernel(DWORD dwThreadID, DWORD dwTimeout);

	__forceinline void PerfUnlock()
	{
#if DEBUG > 1
		if( !m.bUnlocking ){
			fprintf( stderr, "Mutex of thread %lu will be unlocked by thread %lu at t=%gs\n",
				m.nLocker, GetCurrentThreadId(), HRTime_toc()
			);
		}
#endif
		_WriteBarrier(); // changes done to the shared resource are committed.

		_InterlockedCompareExchange((long*) &m.nLocker, 0, m.nLocker);
		m.nLocker = 0;

		_ReadWriteBarrier(); // The CS is released.

		if (m.nWaiters > 0) // AFTER it is released we check if there're waiters.
		{
			WaiterMinus();

			ASSERT(m.hSemaphore);
			cseAssertExInline(ReleaseSemaphore(m.hSemaphore, 1, NULL), __FILE__, __LINE__);
		}
		m.bIsLocked = false;
#ifdef DEBUG0
		m.nLockedBy = 0;
#endif
	}

public:
	/**
		CRITSECTEX_ALLOWSHARED: if defined, operators new and delete are added that use
		shared memory allocation depending on a thread-specific flag
	 */
#ifdef CRITSECTEX_ALLOWSHARED
	void *operator new(size_t size)
	{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
		return MSEreallocShared( NULL, size, 0 );
	}
	void operator delete(void *p)
	{ extern void MSEfreeShared(void *ptr);
		MSEfreeShared(p);
	}
#endif //CRITSECTEX_ALLOWSHARED

	// Constructor/Destructor
	CritSectEx(DWORD dwSpinMax=0)
	{
		s_dwProcessors = 0;
		m.nLocker = 0;
#ifdef DEBUG0
		m.nLockedBy = 0;
#endif
		m.dwSpinMax = 0;
		m.hSemaphore = NULL;
		m.nWaiters = 0;
		m.bIsLocked = false, m.bTimedOut = false;
#ifdef DEBUG
		m.bUnlocking = false;
#endif
		SetSpinMax(dwSpinMax);
	}
	virtual ~CritSectEx()
	{
		if( m.hSemaphore ){
			VERIFY(CloseHandle(m.hSemaphore));
		}
	}

	// Lock/Unlock
	__forceinline bool Lock(bool& bUnlockFlag, DWORD dwTimeout = INFINITE)
	{
#ifdef CRITSECTGCC
		long dwThreadId = GetCurrentThreadId();
#else
		DWORD dwThreadId = GetCurrentThreadId();
#endif
		if (dwThreadId == m.nLocker)
			bUnlockFlag = false;
		else{
			m.bTimedOut = false;
			if ((!m.nLocker && PerfLockImmediate(dwThreadId)) ||
				(dwTimeout && PerfLock(dwThreadId, dwTimeout))
			){
				bUnlockFlag = true;
#ifdef DEBUG0
				m.nLockedBy = dwThreadId;
#endif
			}
			else{
				return false;
			}
		}

		return true;
	}
	__forceinline void Unlock(bool bUnlockFlag)
	{
		if (bUnlockFlag){
#ifdef DEBUG
			m.bUnlocking = true;
#endif
			PerfUnlock();
#ifdef DEBUG
			m.bUnlocking = false;
#endif
		}
	}

	__forceinline bool TimedOut() const { return m.bTimedOut; }
	__forceinline bool IsLocked() const { return m.bIsLocked; }
#ifdef DEBUG0
	__forceinline bool IsInconsistent()
	{
		if( !m.bIsLocked ){
			if( m.nLockedBy > 0 ){
				return true;
			}
		}
		return false;
	}
	__forceinline DWORD LockedBy()
	{
		return m.nLockedBy;
	}
#endif
	virtual operator bool () const { return m.bIsLocked; }

	// Some extra
	__forceinline DWORD SpinMax()	const { return m.dwSpinMax; }
	void SetSpinMax(DWORD dwSpinMax);
	void AllocateKernelSemaphore();

	/**
		Scope: the class that implements the scoped lock. Creating a scoped lock
		will lock the CritSectEx, unless a 0 timeout is requested in which case
		the lock remains open. If no explicit action is taking, the lock will
		persist as long as the scoped lock exists.
	 */
	class Scope {

		// disable copy constructor and assignment
		Scope(const Scope&);
		void operator = (const Scope&);

		CritSectEx* m_pCs;
		bool m_bLocked;
		bool m_bUnlockFlag;

		__forceinline void InternalUnlock()
		{
			if (m_bUnlockFlag)
			{
				ASSERT(m_pCs);
				m_pCs->PerfUnlock();
#ifdef DEBUG0
				if( m_pCs->m.nLockedBy > 0 ){
					m_pCs->IsInconsistent();
				}
#endif
			}
#ifdef DEBUG0
			else{
				if( m_pCs && (m_pCs->m.nLockedBy == GetCurrentThreadId()) ){
					m_pCs->IsInconsistent();
				}
			}
#endif
		}
		__forceinline void InternalLock(DWORD dwTimeout)
		{
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}
		__forceinline void InternalLock(CritSectEx& cs, DWORD dwTimeout)
		{
			m_bUnlockFlag = false;
			m_pCs = &cs;
//			InternalLock(dwTimeout);
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}
		__forceinline void InternalLock(CritSectEx *cs, DWORD dwTimeout)
		{
			m_bUnlockFlag = false;
			m_pCs = cs;
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}

	public:
		bool verbose;
		__forceinline Scope()
			:m_pCs(NULL)
			,m_bLocked(false)
			,m_bUnlockFlag(false)
			,verbose(false)
		{
		}
		__forceinline Scope(CritSectEx& cs, DWORD dwTimeout = INFINITE)
		{
			verbose = false;
			if( dwTimeout ){
			InternalLock(cs, dwTimeout);
		}
			else{
				m_pCs = &cs, m_bLocked = m_bUnlockFlag = false;
			}
		}
		__forceinline Scope(CritSectEx *cs, DWORD dwTimeout = INFINITE)
			:m_pCs(NULL)
			,m_bLocked(false)
			,m_bUnlockFlag(false)
			,verbose(false)
		{
			if( cs && dwTimeout ){
				InternalLock(cs, dwTimeout);
			}
			else{
				m_pCs = cs, m_bLocked = m_bUnlockFlag = false;
			}
		}
		__forceinline ~Scope()
		{
			InternalUnlock();
		}

		bool Lock(CritSectEx& cs, DWORD dwTimeout = INFINITE)
		{
			if (&cs == m_pCs)
				return Lock(dwTimeout);

			InternalUnlock();
			InternalLock(cs, dwTimeout);
			return m_bLocked;
		}
		bool Lock(DWORD dwTimeout = INFINITE)
		{
			ASSERT(m_pCs);
			if (!m_bLocked)
				InternalLock(dwTimeout);
			return m_bLocked;
		}
		void Unlock()
		{
			InternalUnlock();
			m_bUnlockFlag = false;
			m_bLocked = false;
		}

		__forceinline bool TimedOut()
		{
			if( m_pCs ){
				return m_pCs->TimedOut();
			}
			else{
				return false;
			}
		}
		__forceinline bool IsLocked() const { return m_bLocked; }
		operator bool () const { return m_bLocked; }
	};
	friend class Scope;
};

// Classical critical sections, with internal support for recursive locks counter.
// RJVB 20111128: added Lock/Unlock methods for exchangeability with the other classes.
class CritSectRec {
	CritSectEx m_cs;
	long m_nRecursion;

	// disable copy constructor and assignment
	CritSectRec(const CritSectRec&);
	void operator = (const CritSectRec&);

public:
#ifdef CRITSECTEX_ALLOWSHARED
	void *operator new(size_t size)
	{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
		return MSEreallocShared( NULL, size, 0 );
	}
	void operator delete(void *p)
	{ extern void MSEfreeShared(void *ptr);
		MSEfreeShared(p);
	}
#endif //CRITSECTEX_ALLOWSHARED
	CritSectRec()
		:m_nRecursion(0)
	{
// shouldn't be necessary and is potentially dangerous to m_cx
// 		ZeroMemory(this, sizeof(*this));
	}

	CritSectRec(DWORD dwSpinMax)
		:m_nRecursion(0)
	{
// 		ZeroMemory(this, sizeof(*this));
		SetSpinMax(dwSpinMax);
	}

	__forceinline bool Enter(DWORD dwTimeout = INFINITE)
	{
		bool bUnlockFlag;
		if (!m_cs.Lock(bUnlockFlag, dwTimeout))
			return false;

		ASSERT(bUnlockFlag ? !m_nRecursion : (m_nRecursion > 0));
		m_nRecursion++;
		return true;
	}
	__forceinline bool Lock(bool &unlockFlag, DWORD dwTimeout = INFINITE)
	{
		unlockFlag = Enter(dwTimeout);
		return true;
	}
	__forceinline bool TryEnter()
	{
		return Enter(0);
	}
	__forceinline void Leave()
	{
		ASSERT(m_nRecursion > 0);
		if (! --m_nRecursion)
			m_cs.Unlock(true);
	}
	__forceinline void Unlock(bool bUnlockFlag)
	{
		if (bUnlockFlag){
			Leave();
		}
	}

	__forceinline bool TimedOut() const { return m_cs.TimedOut(); }
	__forceinline bool IsLocked() const { return m_cs.IsLocked(); }
	operator bool () const { return m_cs.IsLocked(); }

	// Some extra
	__forceinline DWORD SpinMax()	const { return m_cs.SpinMax(); }
	void SetSpinMax(DWORD dwSpinMax){ m_cs.SetSpinMax(dwSpinMax); }
	void AllocateKernelSemaphore(){ m_cs.AllocateKernelSemaphore(); }

	struct Scope {

		// disable copy constructor and assignment
		Scope(const Scope&);
		void operator = (const Scope&);

		CritSectRec* m_pCs;
		bool m_bLocked;

		__forceinline void InternalLeave()
		{
			if (m_bLocked)
			{
				ASSERT(m_pCs);
				m_pCs->Leave();
			}
		}
		__forceinline void InternalEnter(DWORD dwTimeout)
		{
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Enter(dwTimeout);
		}
		__forceinline void InternalEnter(CritSectRec& cs, DWORD dwTimeout)
		{
			m_pCs = &cs;
//			InternalEnter(dwTimeout);
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Enter(dwTimeout);
		}
		__forceinline void InternalEnter(CritSectRec *cs, DWORD dwTimeout)
		{
			m_pCs = cs;
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Enter(dwTimeout);
		}

	public:
		bool verbose;
		__forceinline Scope()
			:m_pCs(NULL)
			,m_bLocked(false)
			,verbose(false)
		{
		}
		__forceinline Scope(CritSectRec& cs, DWORD dwTimeout = INFINITE)
		{
			verbose = false;
			if( dwTimeout ){
			InternalEnter(cs, dwTimeout);
		}
			else{
				m_pCs = &cs, m_bLocked = false;
			}
		}
		__forceinline Scope(CritSectRec *cs, DWORD dwTimeout = INFINITE)
			:m_pCs(NULL)
			,m_bLocked(false)
			,verbose(false)
		{
			if( cs && dwTimeout ){
				InternalEnter(cs, dwTimeout);
			}
			else{
				m_pCs = cs, m_bLocked = false;
			}
		}
		__forceinline ~Scope()
		{
			InternalLeave();
		}

		__forceinline bool Enter(CritSectRec& cs, DWORD dwTimeout = INFINITE)
		{
			if (&cs == m_pCs)
				return Enter(dwTimeout);

			InternalLeave();
			InternalEnter(cs, dwTimeout);
			return m_bLocked;
		}
		__forceinline bool Enter(DWORD dwTimeout = INFINITE)
		{
			ASSERT(m_pCs);
			if (!m_bLocked)
				InternalEnter(dwTimeout);
			return m_bLocked;
		}
		__forceinline bool Lock(DWORD dwTimeout = INFINITE)
		{
			return Enter(dwTimeout);
		}
		__forceinline void Leave()
		{
			InternalLeave();
			m_bLocked = false;
		}
		__forceinline void Unlock()
		{
			Leave();
		}

		__forceinline bool TimedOut()
		{
			if( m_pCs ){
				return m_pCs->TimedOut();
			}
			else{
				return false;
			}
		}
		__forceinline bool IsLocked() const { return m_bLocked; }
		operator bool () const { return m_bLocked; }
	};
};
#endif

#if defined(__cplusplus) && defined(__windows__)
class CritSect {

	// Declare all variables volatile, so that the compiler won't
	// try to optimise something important away.
	volatile CRITICAL_SECTION	m_hCriticalSection;
	volatile bool	m_bIsLocked, m_bTimedOut;
	volatile DWORD	m_dwSpinMax;
// #ifdef DEBUG
	volatile HANDLE m_hLockerThread;
	volatile bool m_bUnlocking;
// #endif

	// disable copy constructor and assignment
	CritSect(const CritSect&);
	void operator = (const CritSect&);

	__forceinline void PerfLock()
	{
		EnterCriticalSection((LPCRITICAL_SECTION) &m_hCriticalSection);
#ifdef DEBUG
		m_hLockerThread = GetCurrentThread();
#endif
		m_bIsLocked = TRUE;
	}

	__forceinline void PerfUnlock()
	{
//		if( m_bIsLocked ){
			LeaveCriticalSection((LPCRITICAL_SECTION) &m_hCriticalSection);
			m_bIsLocked = false;
#ifdef DEBUG
			if( !m_bUnlocking ){
				fprintf( stderr, "CriticalSection unlocked\n" );
			}
#endif
//		}
	}

public:
#ifdef CRITSECTEX_ALLOWSHARED
	void *operator new(size_t size)
	{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
		return MSEreallocShared( NULL, size, 0 );
	}
	void operator delete(void *p)
	{ extern void MSEfreeShared(void *ptr);
		MSEfreeShared(p);
	}
#endif //CRITSECTEX_ALLOWSHARED
	// Constructor/Destructor
	CritSect(DWORD dwSpinMax=0)
	{
		ZeroMemory(this, sizeof(*this));
		if( dwSpinMax > 0 ){
			if( !InitializeCriticalSectionAndSpinCount( (LPCRITICAL_SECTION) &m_hCriticalSection, dwSpinMax ) ){
				InitializeCriticalSection( (LPCRITICAL_SECTION) &m_hCriticalSection );
				SetSpinMax(dwSpinMax);
			}
		}
		else{
			InitializeCriticalSection( (LPCRITICAL_SECTION) &m_hCriticalSection );
			m_dwSpinMax = 0;
		}
	}
	~CritSect()
	{
		// should not be done when m_bIsLocked == TRUE ?!
		DeleteCriticalSection( (LPCRITICAL_SECTION) &m_hCriticalSection );
	}

	// Lock/Unlock
	__forceinline bool Lock(bool& bUnlockFlag, DWORD dwTimeout = INFINITE)
	{
		PerfLock();
		bUnlockFlag = true;
		return true;
	}

	__forceinline void Unlock(bool bUnlockFlag)
	{
		if( bUnlockFlag ){
#ifdef DEBUG
			m_bUnlocking = true;
#endif
			PerfUnlock();
#ifdef DEBUG
			m_bUnlocking = false;
#endif
		}
	}

	__forceinline bool TimedOut() const { return m_bTimedOut; }
	__forceinline bool IsLocked() const { return m_bIsLocked; }
	__forceinline DWORD SpinMax()	const { return m_dwSpinMax; }
	operator bool () const { return m_bIsLocked; }

	// Some extra
	void SetSpinMax(DWORD dwSpinMax)
	{
		SetCriticalSectionSpinCount( (LPCRITICAL_SECTION) &m_hCriticalSection, dwSpinMax );
		m_dwSpinMax = dwSpinMax;
	}
	void AllocateKernelSemaphore()
	{
	}

	// Scope
	class Scope {

		// disable copy constructor and assignment
		Scope(const Scope&);
		void operator = (const Scope&);

		CritSect* m_pCs;
		bool m_bLocked;
		bool m_bUnlockFlag;

		void InternalUnlock()
		{
			if( m_bUnlockFlag ){
				ASSERT(m_pCs);
				m_pCs->PerfUnlock();
			}
		}

		__forceinline void InternalLock(DWORD dwTimeout)
		{
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}

		__forceinline void InternalLock(CritSect& cs, DWORD dwTimeout)
		{
			m_bUnlockFlag = false;
			m_pCs = &cs;
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}

		__forceinline void InternalLock(CritSect *cs, DWORD dwTimeout)
		{
			m_bUnlockFlag = false;
			m_pCs = cs;
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}

	public:
		bool verbose;
		__forceinline Scope()
			:m_bLocked(false)
			,m_bUnlockFlag(false)
			,m_pCs(NULL)
			,verbose(false)
		{
		}
		__forceinline Scope(CritSect& cs, DWORD dwTimeout = INFINITE)
		{
			verbose = false;
			if( dwTimeout ){
			InternalLock(cs, dwTimeout);
		}
			else{
				m_pCs = &cs, m_bLocked = m_bUnlockFlag = false;
			}
		}
		__forceinline Scope(CritSect *cs, DWORD dwTimeout = INFINITE)
			:m_bLocked(false)
			,m_bUnlockFlag(false)
			,m_pCs(NULL)
			,verbose(false)
		{
			if( cs && dwTimeout ){
				InternalLock(cs, dwTimeout);
			}
			else{
				m_pCs = cs, m_bLocked = m_bUnlockFlag = false;
			}
		}
		__forceinline ~Scope()
		{
			InternalUnlock();
		}

		bool Lock(CritSect& cs, DWORD dwTimeout = INFINITE)
		{
			if (&cs == m_pCs)
				return Lock(dwTimeout);

			InternalUnlock();
			InternalLock(cs, dwTimeout);
			return m_bLocked;
		}
		bool Lock(DWORD dwTimeout = INFINITE)
		{
			ASSERT(m_pCs);
			if (!m_bLocked)
				InternalLock(dwTimeout);
			return m_bLocked;
		}
		void Unlock()
		{
			InternalUnlock();
			m_bUnlockFlag = false;
			m_bLocked = false;
		}

		__forceinline bool TimedOut()
		{
			if( m_pCs ){
				return m_pCs->TimedOut();
			}
			else{
				return false;
			}
		}
		__forceinline bool IsLocked() const { return m_bLocked; }
		operator bool () const { return m_bLocked; }
	};
	friend class Scope;
};
#endif // __windows__

#if defined(__cplusplus)
#if defined(MUTEXEX_CAN_TIMEOUT) && defined(__APPLE__)
#	define __MUTEXEX_CAN_TIMEOUT__
#endif

/**
	A critical section class API-compatible with Vladislav Gelfer's CritSectEx
	This class uses a simple platform-specific mutex except where native mutexes
	don't provide a timed wait. In that case (OS X), the msemul layer is ued to
	emulate CreateSemaphore/ReleaseSemaphore given that a mutex is a semaphore
	with starting value 1. Note however that this imposes the limits that come
	with pthread's sem_open et al (semaphores count to the limit of open files).
 */
class MutexEx {
	// Declare all variables volatile, so that the compiler won't
	// try to optimise something important away.
	struct membervars {
#if defined(__windows__) || defined(__MUTEXEX_CAN_TIMEOUT__)
		volatile HANDLE	hMutex;
		volatile DWORD		bIsLocked;
#else
		pthread_mutex_t	mMutex, *hMutex;
		int				iMutexLockError;
		volatile DWORD		bIsLocked;
#endif
// #ifdef DEBUG
		volatile long	hLockerThreadId;
		volatile bool	bUnlocking;
// #endif
		volatile bool	bTimedOut;
public:
		volatile unsigned long lockCounter;
		size_t scopesUnlocked, scopesLocked;
	} m;

private:
	// disable copy constructor and assignment
	MutexEx(const MutexEx&);
	void operator = (const MutexEx&);

	__forceinline void PerfLock(DWORD dwTimeout)
	{
#ifdef DEBUG
		if( m.bIsLocked ){
			fprintf( stderr, "Thread %lu attempting to lock mutex of thread %ld\n",
				GetCurrentThreadId(), m.hLockerThreadId
			);
		}
#endif
#if defined(__windows__) || defined(__MUTEXEX_CAN_TIMEOUT__)
		switch( WaitForSingleObject( m.hMutex, dwTimeout ) ){
			case WAIT_ABANDONED:
			case WAIT_FAILED:
				cseAssertExInline(false, __FILE__, __LINE__);
				break;
			case WAIT_TIMEOUT:
				m.bTimedOut = true;
				break;
			default:
#	ifdef DEBUG
				m.hLockerThreadId = (long) GetCurrentThreadId();
#	endif
				m.bIsLocked += 1;
				break;
		}
#elif defined(MUTEXEX_CAN_TIMEOUT)
		{ struct timespec timeout;
			clock_gettime( CLOCK_REALTIME, &timeout );
			{ time_t sec = (time_t) (dwMilliseconds/1000);
				timeout.tv_sec += sec;
				timeout.tv_nsec += (long) ( (dwMilliseconds- sec*1000)* 1000000 );
				while( timeout.tv_nsec > 999999999 ){
					timeout.tv_sec += 1;
					timeout.tv_nsec -= 1000000000;
				}
			}
			errno = 0;
			if( (m.iMutexLockError = pthread_mutex_timedlock( m.hMutex, &timeout )) ){
				if( errno == ETIMEDOUT ){
					m.bTimedOut = true;
				}
				else{
					cseAssertExInline(false, __FILE__, __LINE__, "pthread_mutex_timedlock failed");
				}
			}
#	ifdef DEBUG
			m.hLockerThreadId = (long) GetCurrentThreadId();
#	endif
			m.bIsLocked += 1;
		}
#else
		// attempt to lock m.hMutex;
		if( (m.iMutexLockError = pthread_mutex_lock(m.hMutex)) ){
			if( errno == ETIMEDOUT ){
				m.bTimedOut = true;
			}
			else{
				cseAssertExInline(false, __FILE__, __LINE__, "pthread_mutex_lock failed");
			}
		}
#	ifdef DEBUG
		fprintf( stderr, "Mutex of thread %ld locked (recurs.lock=%ld) by thread %lu at t=%gs\n",
			m.hLockerThreadId, m.bIsLocked+1, GetCurrentThreadId(), HRTime_tic()
		);
		m.hLockerThreadId = (long) GetCurrentThreadId();
#	endif
		m.bIsLocked += 1;
#endif
	}

	__forceinline void PerfUnlock()
	{
//		if( m.bIsLocked ){
#if defined(__windows__)
		ReleaseMutex(m.hMutex);
#elif defined(__MUTEXEX_CAN_TIMEOUT__)
		ReleaseSemaphore(m.hMutex, 1, NULL);
#else
		// release m.hMutex
		m.iMutexLockError = pthread_mutex_unlock(m.hMutex);
#endif
		if( m.bIsLocked > 0 ){
			m.bIsLocked -= 1;
#ifdef DEBUG
			if( !m.bUnlocking ){
				fprintf( stderr, "Mutex of thread %ld unlocked (recurs.lock=%ld) by thread %lu at t=%gs\n",
					m.hLockerThreadId, m.bIsLocked, GetCurrentThreadId(), HRTime_toc()
				);
			}
#endif
		}
#ifdef DEBUG
		m.hLockerThreadId = -1;
#endif
//		}
	}

public:
#ifdef CRITSECTEX_ALLOWSHARED
	void *operator new(size_t size)
	{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
		return MSEreallocShared( NULL, size, 0 );
	}
	void operator delete(void *p)
	{ extern void MSEfreeShared(void *ptr);
		MSEfreeShared(p);
	}
#endif //CRITSECTEX_ALLOWSHARED
	// Constructor/Destructor
	MutexEx(DWORD dwSpinMax=0)
	{
		memset(&this->m, 0, sizeof(this->m));
#if defined(__windows__)
		m.hMutex = CreateMutex( NULL, FALSE, NULL );
		cseAssertExInline( (m.hMutex!=NULL), __FILE__, __LINE__);
#elif defined(__MUTEXEX_CAN_TIMEOUT__)
		m.hMutex = CreateSemaphore( NULL, 1, -1, NULL );
		cseAssertExInline( (m.hMutex!=NULL), __FILE__, __LINE__);
#else
		// create a pthread_mutex_t
		cseAssertExInline( (pthread_mutex_init(&m.mMutex, NULL) == 0), __FILE__, __LINE__);
		m.hMutex = &m.mMutex;
		m.scopesUnlocked = m.scopesLocked = 0;
#endif
#ifdef DEBUG
		m.hLockerThreadId = -1;
		init_HRTime();
#endif
	}

	virtual ~MutexEx()
	{
#if defined(__windows__) || defined(__MUTEXEX_CAN_TIMEOUT__)
		// should not be done when m.bIsLocked == TRUE ?!
		CloseHandle(m.hMutex);
#else
		// delete the m.hMutex
		m.iMutexLockError = pthread_mutex_destroy(m.hMutex);
		m.hMutex = NULL;
		if( m.scopesLocked || m.scopesUnlocked ){
			fprintf( stderr, "MutexEx: %lu scopes were destroyed still locked, %lu were already unlocked\n", 
				m.scopesLocked, m.scopesUnlocked );
		}
#endif
	}

	// Lock/Unlock
	__forceinline bool Lock(bool& bUnlockFlag, DWORD dwTimeout = INFINITE)
	{
		PerfLock(dwTimeout);
		bUnlockFlag = !m.bTimedOut;
		return true;
	}

	__forceinline void Unlock(bool bUnlockFlag)
	{
		if( bUnlockFlag ){
#ifdef DEBUG
			m.bUnlocking = true;
#endif
			PerfUnlock();
#ifdef DEBUG
			m.bUnlocking = false;
#endif
		}
	}

	__forceinline bool TimedOut() const { return m.bTimedOut; }
	__forceinline bool IsLocked() const { return (bool) m.bIsLocked; }
	__forceinline DWORD SpinMax()	const { return 0; }
	operator bool () const { return (bool) m.bIsLocked; }

	// Some extra
	void SetSpinMax(DWORD /*dwSpinMax*/)
	{
	}
	void AllocateKernelSemaphore()
	{
	}

	// Scope
	class Scope {

		// disable copy constructor and assignment
		Scope(const Scope&);
		void operator = (const Scope&);

		MutexEx* m_pCs;
		bool m_bLocked;
		bool m_bUnlockFlag;

		void InternalUnlock()
		{
			if( m_bUnlockFlag ){
				ASSERT(m_pCs);
				m_pCs->PerfUnlock();
			}
		}

		__forceinline void InternalLock(DWORD dwTimeout)
		{
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}

		__forceinline void InternalLock(MutexEx& cs, DWORD dwTimeout)
		{
			m_bUnlockFlag = false;
			m_pCs = &cs;
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}

		__forceinline void InternalLock(MutexEx *cs, DWORD dwTimeout)
		{
			m_bUnlockFlag = false;
			m_pCs = cs;
			ASSERT(m_pCs);
			m_bLocked = m_pCs->Lock(m_bUnlockFlag, dwTimeout);
		}

	public:
		bool verbose;
		__forceinline Scope()
			:m_pCs(NULL)
			,m_bLocked(false)
			,m_bUnlockFlag(false)
			,verbose(false)
		{
		}
		__forceinline Scope(MutexEx& cs, DWORD dwTimeout = INFINITE)
		{
			verbose = false;
			if( dwTimeout ){
				InternalLock(cs, dwTimeout);
			}
			else{
				m_pCs = &cs, m_bLocked = m_bUnlockFlag = false;
			}
		}
		__forceinline Scope(MutexEx *cs, DWORD dwTimeout = INFINITE)
			:m_pCs(NULL)
			,m_bLocked(false)
			,m_bUnlockFlag(false)
			,verbose(false)
		{
			if( cs && dwTimeout ){
				InternalLock(cs, dwTimeout);
			}
			else{
				m_pCs = cs, m_bLocked = m_bUnlockFlag = false;
			}
		}
		__forceinline ~Scope()
		{
			if( m_pCs && verbose ){
				if( m_bLocked ){
					m_pCs->m.scopesLocked += 1;
				}
				else{
					m_pCs->m.scopesUnlocked += 1;
				}
			}
			InternalUnlock();
		}

		bool Lock(MutexEx& cs, DWORD dwTimeout = INFINITE)
		{
			if (&cs == m_pCs)
				return Lock(dwTimeout);

#ifdef DEBUG
			fprintf( stderr, "InternalUnlock before InternalLock!\n" );
#endif
			InternalUnlock();
			InternalLock(cs, dwTimeout);
			return m_bLocked;
		}
		bool Lock(DWORD dwTimeout = INFINITE)
		{
			ASSERT(m_pCs);
			if (!m_bLocked)
				InternalLock(dwTimeout);
			return m_bLocked;
		}
		void Unlock()
		{
			InternalUnlock();
			m_bUnlockFlag = false;
			m_bLocked = false;
		}

		__forceinline bool TimedOut()
		{
			if( m_pCs ){
				return m_pCs->TimedOut();
			}
			else{
				return false;
			}
		}
		__forceinline bool IsLocked() const { return m_bLocked; }
		operator bool () const { return m_bLocked; }
	};
	friend class Scope;
};
#undef __MUTEXEX_CAN_TIMEOUT__
#endif

#pragma mark ---- C interface glue ----

typedef struct CSEHandle {
#if !defined(__cplusplus)
	void *cse;
	unsigned char IsLocked;
	DWORD spinMax;
	const char *info;
#else
	CRITSECT *cse;
	unsigned char IsLocked;
	DWORD spinMax;
private:
	const char *info;
public:
#ifdef CRITSECTEX_ALLOWSHARED
	void *operator new(size_t size)
	{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
		return MSEreallocShared( NULL, size, 0 );
	}
	void operator delete(void *p)
	{ extern void MSEfreeShared(void *ptr);
		MSEfreeShared(p);
	}
#endif //CRITSECTEX_ALLOWSHARED
	const char *CSEHandleInfo()
	{
		return info;
	}
	__forceinline CSEHandle(DWORD dwSpinMax=0): info(typeid(CRITSECT).name()) // info(STRING(CRITSECT))
	{
		cse = new CRITSECT(dwSpinMax);
		if( cse ){
			cse->AllocateKernelSemaphore();
			spinMax = dwSpinMax;
			IsLocked = cse->IsLocked();
			// cf: http://stackoverflow.com/questions/1024648/retrieving-a-c-class-name-programatically
//			strncpy( info, typeid(cse).name(), sizeof(info) );
//			info[sizeof(info)-1] = '\0';
//			fprintf( stderr, "CSE info \"%s\"\n", info );
		}
		else{
//			info[0] = '\0';
		}
	}
	__forceinline ~CSEHandle()
	{
		delete cse;
		cse = NULL;
	}
#endif
} CSEHandle;

typedef struct CSEScopedLock {
#if !defined(__cplusplus)
	void *cse, *scopedlock;
	unsigned char IsLocked;
	DWORD timeOut;
#else
	CRITSECT *cse;
	CRITSECT::Scope *scope;
	unsigned char IsLocked;
	DWORD timeOut;
#ifdef CRITSECTEX_ALLOWSHARED
	void *operator new(size_t size)
	{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
		return MSEreallocShared( NULL, size, 0 );
	}
	void operator delete(void *p)
	{ extern void MSEfreeShared(void *ptr);
		MSEfreeShared(p);
	}
#endif //CRITSECTEX_ALLOWSHARED
	__forceinline CSEScopedLock(CRITSECT *_cse, DWORD dwTimeout = INFINITE)
	{
		cse = _cse;
		if( (scope = new CRITSECT::Scope(cse, dwTimeout)) ){
			timeOut = dwTimeout;
			IsLocked = scope->IsLocked();
		}
	}
	__forceinline CSEScopedLock(DWORD dwTimeout = INFINITE)
	{
		CSEScopedLock((CRITSECT*) NULL, dwTimeout);
	}
	__forceinline CSEScopedLock(CSEHandle *cseH, DWORD dwTimeout = INFINITE)
	{
		cse = cseH->cse;
		if( (scope = new CRITSECT::Scope(cse, dwTimeout)) ){
			timeOut = dwTimeout;
			cseH->IsLocked = IsLocked = scope->IsLocked();
		}
	}
	__forceinline ~CSEScopedLock()
	{
		cse = NULL;
		delete scope;
		scope = NULL;
	}
#endif
} CSEScopedLock;

#ifdef __cplusplus
extern "C" {
#endif

extern CSEHandle *CreateCSEHandle(DWORD dwSpinMax);
extern CSEHandle *DeleteCSEHandle(CSEHandle *cseH);
extern const char *CSEHandleInfo(CSEHandle *cseH);
extern unsigned char LockCSEHandle(CSEHandle *cseH);
extern unsigned char LockCSEHandleWithTimeout(CSEHandle *cseH, DWORD dwTimeout);
extern unsigned char UnlockCSEHandle(CSEHandle *cseH, unsigned char unlockFlag);

extern CSEScopedLock *ObtainCSEScopedLock(CSEHandle *cseH);
extern CSEScopedLock *ObtainCSEScopedLockWithTimeout(CSEHandle *cseH, DWORD dwTimeout);
extern unsigned char LockCSEScopedLock(CSEScopedLock *scopeL);
extern unsigned char LockCSEScopedLockWithTimeout(CSEScopedLock *scopeL, DWORD dwTimeout);
extern unsigned char UnlockCSEScopedLock(CSEScopedLock *scopeL);
extern CSEScopedLock *ReleaseCSEScopedLock(CSEScopedLock *scopeL);

extern unsigned char IsCSEHandleLocked(CSEHandle *cseH);
extern unsigned char DidCSEHandleTimeout(CSEHandle *cseH);
extern unsigned char IsCSEScopeLocked(CSEScopedLock *scopeL);
extern unsigned char DidCSEScopeTimeout(CSEScopedLock *scopeL);

#ifdef __cplusplus
}
#endif

#ifdef __OBJC__

#	if !defined __cplusplus
	typedef struct CritSectEx		CritSectEx;
	typedef struct CritSectExScope	CritSectExScope;
#else
	typedef CritSectEx::Scope		CritSectExScope;
#	endif
#	if __has_feature(blocks)
	typedef void					(^LockedBlock)();
#	endif
	@class NSCriticalSection;
	@class NSCriticalSectionScope;

	@interface NSCriticalSection : NSObject
	{
		CritSectEx *cse;
		NSString *info;
		DWORD spinMax;
		@public DWORD scopedLocks;
	}
	+ (id) createWithSpinMax:(DWORD)sm;
	- (id) initWithSpinMax:(DWORD)sM;
	- (NSCriticalSectionScope*) getLockScope;
	- (NSCriticalSectionScope*) getLockScopeWithTimeOut:(DWORD)timeOut;
#	if __has_feature(blocks)
	- (void) callLockedBlock:(LockedBlock)block;
	- (void) callLockedBlock:(LockedBlock)block withTimeOut:(DWORD)timeOut;
#	endif
	- (BOOL) hasTimedOut;
	- (BOOL) isLocked;
	- (NSString*) description;

	@property DWORD spinMax;
	@property (retain) NSString *info;
	@property CritSectEx *cse;
	@end

	@interface NSCriticalSectionScope : NSObject
	{
		NSCriticalSection *criticalSection;
		CritSectExScope *scope;
	}
	- (CritSectExScope*) setScopeForCS:(NSCriticalSection*)cs withTimeOut:(DWORD)timeOut;
	- (BOOL) hasTimedOut;
	- (BOOL) isLocked;
	@end

#endif // __OBJC__

#define _CRITSECTEX_H
#endif // !_CRITSECTEX_H
