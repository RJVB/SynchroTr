/*!
	@file Thread.h
	A generic thread class based on Arun N Kumar's CThread
	http://www.codeproject.com/Articles/1570/A-Generic-C-Thread-Class
	adapted and extended by RJVB (C) 2012
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#define CRITSECTEX_ALLOWSHARED
#include "CritSectEx/CritSectEx.h"

#ifdef __windows__
// to "enable" placement new in MSVC
#	include <new.h>
#endif

typedef enum { THREAD_SUSPEND_NOT=0,		//!< The thread runs normally after the initial creation
	THREAD_SUSPEND_BEFORE_INIT=1<<0,		//!< The thread is not unsuspended after its creation
	THREAD_SUSPEND_AFTER_INIT=1<<1,		//!< The thread is allowed to perform Init() and is suspended
	THREAD_SUSPEND_BEFORE_CLEANUP=1<<2		//!< The thread is suspended before Cleanup()
} SuspenderThreadTypes;

extern DWORD thread2ThreadKey, thread2ThreadKeyClients;

/*!
	Thread class for easy creation of background worker threads. Standard overridable methods
	are Run() (the worker function), InitThread() and CleanupThread(). In the default version
	the worker thread is created and started with the Start() method, after which it runs to
	completion or until Stop() is called. It is possible to override the constructor to create
	a 'SuspenderThread' which will be created at once. In this case one can specify synchronisation
	points (before/after InitThread(), before CleanupThread()) at which the worker will wait
	until Continue() is called. This makes it possible to ensure all worker threads exist (and
	are initialised properly) before launching the actual workload.
 */
class Thread {
	public:
#pragma mark new Thread
		/*!
			new() operator that allocates from anonymous shared memory - necessary to be able
			to share semaphore handles among processes
		 */
		void *operator new(size_t size)
		{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
			return MSEreallocShared( NULL, size, 0 );
		}
		/*!
			delete operator that frees anonymous shared memory
		 */
		void operator delete(void *p)
		{ extern void MSEfreeShared(void *ptr);
			MSEfreeShared(p);
		}

		/*!
		 *	Info: Starts the thread.
		 *	
		 *	This function creates and starts the worker thread, passing arg to the worker.
		 *	When called on a SuspenderThread it will unblock the worker in case it is waiting
		 *	at a synchronisation point. (The initial invocation that creates the thread is done
		 *	through the constructor in this case.)
		 */
		DWORD Start( void* arg = NULL );

		/*!
			returns true if the worker has been started
		 */
		bool isStarted()
		{
			return hasBeenStarted;
		}

		/*!
			returns true if the worker exists and is waiting at a synchronisation point
			OR is suspended
		 */
		bool IsWaiting()
		{
			return hasBeenStarted && (startLock.IsLocked() || isSuspended);
		}

		/*!
			unblocks a worker that is suspended or waiting at a synchronisation point
		 */
		bool Continue();

		/*!
			suspends the worker thread. This can be done at any point
			in the worker cycle, contrary to blocking at synchronisation
			which the worker does itself at fixed points. The method returns
			the previous suspension state.
		 */
		bool Suspend();

		/*!
			join the worker. This is pthread terminology for waiting until
			the worker thread exits ... either because it is done or because
			it has received a signal to exit (which Join does NOT give).
			It is possible to specify a timeout in milliseconds.
		 */
		//DWORD Join()
		//{ DWORD ret;
		//	if( m_ThreadCtx.m_hThread ){
		//		return WaitForSingleObject( m_ThreadCtx.m_hThread, INFINITE );
		//	}
		//	else{
		//		ret = WAIT_FAILED;
		//	}
		//	return ret;
		//}
		DWORD Join(DWORD dwMilliSeconds=INFINITE);

		/*!
			Stop the worker thread. This call unlocks the worker if it is suspended or waiting
			at a synchronisation point. Currently this function does not actually stop a still
			running thread but only sets the threadShouldStop flag unless the ForceKill flag is
			true. In that case, the thread will be 1) cancelled (which will invoke CleanupThread()
			on MS Windows) and if that has no effect in 5 seconds the worker will be terminated.
			Thread cancelling is a concept from pthreads where the thread will be 'redirected'
			to a proper exit routine (possibly after executing any cleanup handlers) rather than
			killed outright.
			This is likely to change so that Stop(false) will cancel the thread which always ought to
			call the cleanup method) and Stop(true) will terminate the thread if cancelling has no
			effect.
		 */
		DWORD Stop( bool bForceKill=false, DWORD dwForceExitCode=(DWORD)-1 );

		/*!
			get the worker's current exit code. This will be STILL_ACTIVE if the
			thread is still running, or else the exit code specified by the worker.
		 */
		THREAD_RETURN GetExitCode();

		/*!
		 *	Info: Attaches a Thread Function
		 *	
		 *	Used primarily for porting but can serve in developing generic thread objects
		 */
		void Attach( LPTHREAD_START_ROUTINE lpThreadFunc ){
			m_pThreadFunc = lpThreadFunc;
		}

		/*!
		 *	Info: Detaches the Attached Thread Function
		 *	
		 *	Detaches the Attached Thread Function, If any.
		 *	by resetting the thread function pointer to EntryPoint
		 */
		void Detach( void ){
			m_pThreadFunc = /*Thread::*/EntryPoint; 
		}

		/*!
		 *	Info: Default Constructor
		 */
		Thread();

		/*!
		 *	Constructor to create a thread that is launched at once but
		 *	kept suspended either before or after execution of the InitThread() method.
		 */
		Thread( SuspenderThreadTypes when, void* arg = NULL );
		Thread( int when, void* arg = NULL );

		/*!
		 *	Info: Plug Constructor
		 *
		 *	Use this to migrate/port existing worker threads to objects immediately
		 *  Although you lose the benefits of ThreadCTOR and ThreadDTOR.
		 */
		Thread(LPTHREAD_START_ROUTINE lpExternalRoutine);

		/*!
			initialisation function to convert an already created Thread object
			into a SuspenderThread instance - BEFORE Start() has been called.
		 */
		DWORD SuspenderThread( SuspenderThreadTypes when, void* arg = NULL );
		/*!
			initialisation function to convert an already created Thread object
			into a SuspenderThread instance - BEFORE Start() has been called.
		 */
		DWORD SuspenderThread( int when, void* arg = NULL );
		/*!
			destructor. Stops the worker thread if it is still running and releases
			the thread2ThreadKey local storage object if no one is still using it.
		 */
		~Thread();

	protected:

		/*!
			set the worker exit code/status
		 */
		THREAD_RETURN SetExitCode(THREAD_RETURN dwExitCode);

		/*!
			the worker entry point which is responsible for all administrative
			actions that must be performed from inside the worker thread. It can
			but should not be overridden.
		 */
		static THREAD_RETURN WINAPI EntryPoint( LPVOID pArg)
		{ Thread *pParent = reinterpret_cast<Thread*>(pArg);

			// associate the thread class instance with the thread
			if( thread2ThreadKey ){
				TlsSetValue( thread2ThreadKey, pParent );
				thread2ThreadKeyClients += 1;
//				fprintf( stderr, "@@ TlsSetValue(%p,%p)\n", thread2ThreadKey, pParent );
			}

			pParent->InitThread();
			if( pParent->suspendOption ){
				if( pParent->suspendOption & THREAD_SUSPEND_AFTER_INIT ){
					fprintf( stderr, "@@%p starting AFTER_INIT suspension\n", pParent );
					pParent->startLock.Wait();
				}
			}

			pParent->m_ThreadCtx.m_dwExitCode = pParent->Run( pParent->m_ThreadCtx.m_pUserData );
			pParent->m_ThreadCtx.m_bExitCodeSet = true;

			if( pParent->suspendOption ){
				if( (pParent->suspendOption & THREAD_SUSPEND_BEFORE_CLEANUP) ){
					fprintf( stderr, "@@%p starting BEFORE_CLEANUP suspension\n", pParent );
					pParent->startLock.Wait();
				}
			}
			pParent->CleanupThread();

			return (THREAD_RETURN) pParent->m_ThreadCtx.m_dwExitCode;
			// why on earth would we wqnt to return STILL_ACTIVE when we exit???
			// return (THREAD_RETURN) STILL_ACTIVE;
		}

		/*!
		 *	Info: Initialisation function. 
		 *	
		 *	Will be called by EntryPoint before executing the thread body.
		 *  Override this function to provide your extra initialisation.
		 */
		virtual void InitThread()
		{
		}

		/*!
			the actual worker function; override this method.
		 */
		virtual DWORD Run( LPVOID /* arg */ )
		{
			return m_ThreadCtx.m_dwExitCode;
		}

		/*!
		 *	Info: Cleanup function. 
		 *	
		 *	Will be called by EntryPoint after executing the worker function.
		 *  Override this function to provide your extra destruction.
		 */
		virtual void CleanupThread()
		{
		}


	private:
		volatile long m_lCancelling;		//!< flag that is set when the thread is being cancelled
		/*!
			private class that implements the lock used for blocking the worker thread at
			synchronisation points.
		 */
		class StartLocks {
			HANDLE lockEvent;			//!< the event HANDLE that is the actual lock
			long isLocked, isNotified;
			public:
				/*!
					new() operator that allocates from anonymous shared memory - necessary to be able
					to share semaphore handles among processes
				 */
				void *operator new(size_t size)
				{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
					return MSEreallocShared( NULL, size, 0 );
				}
				/*!
					delete operator that frees anonymous shared memory
				 */
				void operator delete(void *p)
				{ extern void MSEfreeShared(void *ptr);
					MSEfreeShared(p);
				}
				StartLocks();
				~StartLocks();
				__forceinline bool IsLocked()
				{
					return isLocked;
				}
				/*!
					notify the waiter, i.e. set the event to signalled. isNotified
					will remain set until the first waiter unlocks.
				 */
				__forceinline bool Notify()
				{
					// notified is used just in case something interrupts WaitForSingleObject
					// (i.e. a non-timeout return) not preceded by a notification.
					_InterlockedSetTrue(isNotified);
					return SetEvent(lockEvent);
				}
				/*!
					block waiting for the event to be notified. During the wait, isNotified==false
					and isLocked==true. The function waits on lockEvent for periods up to 0.1s
					checking isNotified after each wait.
				 */
				__forceinline bool Wait()
				{ DWORD ret;
					if( !isLocked ){
						_InterlockedSetTrue(isLocked);
						while( (ret = WaitForSingleObject( lockEvent, 100 )) == WAIT_TIMEOUT
							 /*&& ret != WAIT_ABANDONED*/ && !isNotified
						){
							if( ret == WAIT_TIMEOUT ){
								// just in case:
								_InterlockedSetTrue(isLocked);
								YieldProcessor();
							}
						}
#if defined(DEBUG)
						if( ret != WAIT_OBJECT_0 && ret != WAIT_TIMEOUT ){
							fprintf( stderr, "@@@ %p->Wait() on %p returned %lu; notified=%ld\n",
								this, lockEvent, ret, isNotified );
						}
#endif // DEBUG
						// unset isLocked
						_InterlockedSetFalse(isLocked);
						// reset the notified state
						_InterlockedSetFalse(isNotified);
					}
					else{
						ret = WAIT_FAILED;
					}
					return (ret != WAIT_ABANDONED && ret != WAIT_FAILED);
				}
		};
	private:
		StartLocks startLock;
		SuspenderThreadTypes suspendOption;	//!< mask specifying if and how the worker thread should wait at synchronisation points
		long isSuspended;
		bool hasBeenStarted;

		/*!
			lowlevel, internal initialisation
		 */
		void __init__();

		/*!
		 *	Info: Thread Context Inner Class
		 *	
		 *	Every thread object needs to be associated with a set of values.
		 *	like UserData Pointer, Handle, Thread ID etc.
		 *  
		 *  NOTE: This class can be enhanced to varying functionalities
		 *		  eg.,
		 *				* Members to hold StackSize
		 *				* SECURITY_ATTRIBUTES member.
		 */
		class ThreadContext
		{
			public:
				/*!
					new() operator that allocates from anonymous shared memory - necessary to be able
					to share semaphore handles among processes
				 */
				void *operator new(size_t size)
				{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
					return MSEreallocShared( NULL, size, 0 );
				}
				/*!
					delete operator that frees anonymous shared memory
				 */
				void operator delete(void *p)
				{ extern void MSEfreeShared(void *ptr);
					MSEfreeShared(p);
				}
				ThreadContext()
				{
					memset(this, 0, sizeof(this));
				}

				/*!
				 *	Attributes Section
				 */
			public:
				HANDLE m_hThread;					//!<	The Thread Handle
				DWORD  m_dwTID;					//!<	The Thread ID
				LPVOID m_pUserData;					//!<	The user data pointer
				LPVOID m_pParent;					//!<	The this pointer of the parent Thread object
				DWORD  m_dwExitCode;				//!<	The Exit Code of the thread
				bool	  m_bExitCodeSet;				//!< Whether the exit code has been set explicitly
		};

		/*!
			the cancel callback responsible for calling CleanupThread when the worker
			is being cancelled
		 */
		static void WINAPI HandleCancel();

		/*!
			cancel the worker thread, i.e. coerce it through an 'official' exit point
			rather than killing it outright. Currently implemented on MS Win only.
		 */
		bool Cancel();

		/*!
		 *	Attributes Section
		 */
	protected:
		/*!
		 *	Info: Members of Thread
		 */
		ThreadContext			m_ThreadCtx;			//!<	The Thread Context member
		LPTHREAD_START_ROUTINE	m_pThreadFunc;			//!<	The Worker Thread Function Pointer
		long					threadShouldExit;		//!< flag set be Stop() to signal the worker that it should exit.
	public:
		HANDLE GetThread()
		{
			return m_ThreadCtx.m_hThread;
		}
};

template <typename SHType>
/*!
	a simple template class for creating shared objects. Access is preempted
	through inheritance of the CritSectEx critical section class.
 */
class SharedValue : protected CritSectEx {
	protected:
		SHType *value;					//!< a pointer to shared memory containing the actual variable
	// shared value
	public:
		/*!
			new operator that always uses shared memory
		 */
		void *operator new(size_t size)
		{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN, int forceShared );
			return MSEreallocShared( NULL, size, 0, true );
		}
		void operator delete(void *p)
		{ extern void MSEfreeShared(void *ptr);
			MSEfreeShared(p);
		}
		/*!
			constructor; initialises a new instance with the specified initial value and
			spinMax
		 */
		SharedValue(SHType val, DWORD spinMax=4000)
		{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN, int forceShared );
			// set the CritSectEx spinMax property. This value should apparently be positive to
			// avoid deadlocking
			SetSpinMax( (spinMax)? spinMax : 1 );
			// allocate the value store using placement new (invokes the SHType constructor on
			// memory that's been allocated by our shared memory allocator).
			value = new ((SHType*) MSEreallocShared( NULL, sizeof(SHType), 0, true )) SHType(val);
			cseAssertExInline( value!=NULL, __FILE__, __LINE__, "shared memory allocation error" );
			// store the initial value
			*value = val;
		}
		~SharedValue()
		{ extern void MSEfreeShared(void *ptr);
			// placement delete:
			if( value ){
				value->~SHType();
				MSEfreeShared(value);
			}
		}

		/*!
			a quick read-out that does not lock the object
		 */
		inline SHType Read()
		{
			return *value;
		}
		/*!
			return the value after preempting access
		 */
		inline SHType Value(DWORD dwTimeout=INFINITE)
		{ Scope scope(this,dwTimeout);
			return *value;
		}
		/*!
			set a new value and return the old value after preempting access
		 */
		inline SHType Value( const SHType &val, DWORD dwTimeout=INFINITE)
		{ Scope scope(this,dwTimeout);
		  SHType oldValue = *value;
			*value = val;
			return oldValue;
		}
		inline SHType Value( const SHType *val, DWORD dwTimeout=INFINITE)
		{ Scope scope(this,dwTimeout);
		  SHType oldValue = *value;
			*value = *val;
			return oldValue;
		}
		/*!
			a structure that exposes the value store pointer through an object instance
			which preempts access to the store during its lifetime
		 */
		struct DirectAccess {
			protected:
				Scope *scope;
			public:
				SHType *variable;
				DirectAccess(DWORD dwTimeout=INFINITE)
				{
					scope = new Scope(this,dwTimeout);
					variable = value;
				}
				DirectAccess(SharedValue *shVal, DWORD dwTimeout=INFINITE)
				{
					cseAssertExInline( shVal!=NULL, __FILE__, __LINE__, "NULL pointer passed to DirectAccess constructor" );
					scope = new Scope(shVal,dwTimeout);
					variable = shVal->value;
				}
				DirectAccess(SharedValue &shVal, DWORD dwTimeout=INFINITE)
				{
					scope = new Scope(shVal,dwTimeout);
					variable = shVal.value;
				}
				~DirectAccess()
				{
					delete scope;
				}
		};

		/*!
			return the value after preempting access
		 */
		inline SHType operator*()
		{ Scope scope(this,INFINITE);
			return *value;
		}
		/*!
			assignment operator for shared (scalar) values
		 */
		inline SHType operator=(const SHType &val)
		{ Scope scope(this,INFINITE);
			return *value = val;
		}
		/*!
			assignment operator for shared (scalar) values
		 */
		inline SHType operator=(const SharedValue<SHType> &val)
		{ Scope scope(this,INFINITE);
			return *value = val.Value();
		}
		/*!
			equality operator for shared (scalar) values
		 */
		inline bool operator==(const SHType &val)
		{ Scope scope(this,INFINITE);
			return *value == val;
		}
		/*!
			equality operator for shared (scalar) values
		 */
		inline bool operator==(const SharedValue<SHType> &val)
		{ Scope scope(this,INFINITE);
			return *value == val.Value();
		}
		/*!
			inequality operator for shared (scalar) values
		 */
		inline bool operator!=(const SHType &val)
		{ Scope scope(this,INFINITE);
			return *value != val;
		}
		/*!
			inequality operator for shared (scalar) values
		 */
		inline bool operator!=(const SharedValue<SHType> &val)
		{ Scope scope(this,INFINITE);
			return *value != val.Value();
		}
		/*!
			increment operator for shared (scalar) values
		 */
		inline SHType operator+=(const SHType &val)
		{ Scope scope(this,INFINITE);
			return (*value += val);
		}
		/*!
			decrement operator for shared (scalar) values
		 */
		inline SHType operator-=(const SHType &val)
		{ Scope scope(this,INFINITE);
			return (*value -= val);
		}
		/*!
			in-place multiply operator for shared (scalar) values
		 */
		inline SHType operator*=(const SHType &val)
		{ Scope scope(this,INFINITE);
			return (*value *= val);
		}
		/*!
			in-place division operator for shared (scalar) values
		 */
		inline SHType operator/=(const SHType &val)
		{ Scope scope(this,INFINITE);
			return (*value /= val);
		}
};

template <typename SHAType>
class SharedArray {
	protected:
		CritSectEx	*cse;
		SHAType*		data;				//!< a pointer to shared memory containing the actual variable
		size_t		N, last;
		//typedef typename std::allocator<SHAType>					_Alloc;
		//typedef typename _Alloc::template rebind<SHAType>::other	_Tp_alloc_type;
		//typedef typename _Tp_alloc_type::reference				reference;
	// shared value
	public:
		/*!
			new operator that always uses shared memory
		 */
		void *operator new(size_t size)
		{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN, int forceShared );
			return MSEreallocShared( NULL, size, 0, true );
		}
		void operator delete(void *p)
		{ extern void MSEfreeShared(void *ptr);
			MSEfreeShared(p);
		}
		SharedArray(DWORD spinMax=4000)
		{ extern void *MSEreallocShared( void*, size_t, size_t, int );
		  int prevShMemFlag;
			// set the CritSectEx spinMax property. This value should apparently be positive to
			// avoid deadlocking. Placement new doesn't work on CritSectEx for some reason,
			// so we do a temporary change of the thread-wide shared-memory allocation flag.
			prevShMemFlag = MSEmul_UseSharedMemory(true);
			cse = new CritSectEx( (spinMax)? spinMax : 1 );
			MSEmul_UseSharedMemory(prevShMemFlag);
			cseAssertExInline( cse!=NULL, __FILE__, __LINE__, "shared memory allocation error" );
			last = N = 0;
			data = NULL;
		}
		/*!
			constructor; initialises a new instance with the specified initial value and
			spinMax
		 */
		SharedArray(SHAType *arr, const size_t elems, DWORD spinMax=4000)
		{ extern void *MSEreallocShared( void*, size_t, size_t, int );
		  int prevShMemFlag;
			// set the CritSectEx spinMax property. This value should apparently be positive to
			// avoid deadlocking. Placement new doesn't work on CritSectEx for some reason,
			// so we do a temporary change of the thread-wide shared-memory allocation flag.
			prevShMemFlag = MSEmul_UseSharedMemory(true);
			cse = new CritSectEx( (spinMax)? spinMax : 1 );
			MSEmul_UseSharedMemory(prevShMemFlag);
			cseAssertExInline( cse!=NULL, __FILE__, __LINE__, "shared memory allocation error" );
			// allocate the value store using placement new (invokes the SHAType constructor on
			// memory that's been allocated by our shared memory allocator).
			N = elems;
			data = new ((SHAType*) MSEreallocShared( NULL, N * sizeof(SHAType), 0, true )) SHAType[N];
			cseAssertExInline( data!=NULL, __FILE__, __LINE__, "shared memory allocation error" );
			// store the initial value
			memmove( data, arr, N * sizeof(SHAType) );
			last = 0;
		}
		virtual ~SharedArray()
		{ extern void MSEfreeShared(void *ptr);
			// placement delete:
			if( data ){
				data->~SHAType();
				MSEfreeShared(data);
			}
			delete cse;
		}

		bool SetValues(SHAType *arr, const size_t elems)
		{ extern void *MSEreallocShared( void*, size_t, size_t, int );
			// allocate the value store using placement new (invokes the SHAType constructor on
			// memory that's been allocated by our shared memory allocator).
			data = new ((SHAType*) MSEreallocShared( data, elems * sizeof(SHAType), N * sizeof(SHAType), true )) SHAType[elems];
			if( data ){
				N = elems;
				// store the initial value
				memmove( data, arr, N * sizeof(SHAType) );
				last = 0;
				return true;
			}
			else{
				last = N = 0;
				return false;
			}
		}
		/*!
			a quick read-out that does not lock the object
		 */
		inline SHAType Read(const size_t idx)
		{
			return data[idx];
		}
		/*!
			return the element after preempting access
		 */
		inline SHAType ValueAtIndex(const size_t idx, DWORD dwTimeout=INFINITE)
		{ CritSectEx::Scope scope(cse,dwTimeout);
			return data[last=idx];
		}
		/*!
			set a new value and return the old value after preempting access
		 */
		inline SHAType ValueAtIndex( const size_t idx, const SHAType &val, DWORD dwTimeout=INFINITE)
		{ CritSectEx::Scope scope(cse,dwTimeout);
		  SHAType oldValue = data[last=idx];
			data[idx] = val;
			return oldValue;
		}
		inline SHAType ValueAtIndex( const size_t idx, const SHAType *val, DWORD dwTimeout=INFINITE)
		{ CritSectEx::Scope scope(cse,dwTimeout);
		  SHAType oldValue = data[last=idx];
			data[idx] = *val;
			return oldValue;
		}
		inline size_t CurrentIndex()
		{
			return last;
		}
		inline size_t Size()
		{
			return N;
		}
		/*!
			a structure that exposes the value store pointer through an object instance
			which preempts access to the store during its lifetime
		 */
		struct DirectAccess {
			protected:
				CritSectEx::Scope *scope;
			public:
				SHAType *variable;
				DirectAccess(DWORD dwTimeout=INFINITE)
				{
					scope = new CritSectEx::Scope(cse,dwTimeout);
					variable = data;
				}
				DirectAccess(SharedArray *shVal, DWORD dwTimeout=INFINITE)
				{
					cseAssertExInline( shVal!=NULL, __FILE__, __LINE__, "NULL pointer passed to DirectAccess constructor" );
					scope = new CritSectEx::Scope(shVal->cse,dwTimeout);
					variable = shVal->data;
				}
				DirectAccess(SharedArray &shVal, DWORD dwTimeout=INFINITE)
				{
					scope = new CritSectEx::Scope(shVal.cse,dwTimeout);
					variable = shVal.data;
				}
				~DirectAccess()
				{
					delete scope;
				}
		};

		/*!
			return the value after preempting access
		 */
		inline SHAType operator*()
		{ CritSectEx::Scope scope(cse,INFINITE);
			return data[last];
		}
		/*!
			FIXME
			index operator for shared arrays
		 */
		SHAType& operator[](const size_t idx)
		{ CritSectEx::Scope scope(cse,INFINITE);
			return data[last=idx];
		}
};

#endif //__THREAD_H__
