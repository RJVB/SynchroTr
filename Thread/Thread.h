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
#	include <ostream>
#	include <sstream>
#endif
#include <iostream>

typedef enum { THREAD_SUSPEND_NOT=0,		//!< The thread runs normally after the initial creation
	THREAD_SUSPEND_BEFORE_INIT=1<<0,		//!< The thread is not unsuspended after its creation
	THREAD_SUSPEND_AFTER_INIT=1<<1,		//!< The thread is allowed to perform Init() and is suspended
	THREAD_SUSPEND_BEFORE_CLEANUP=1<<2		//!< The thread is suspended before Cleanup()
} SuspenderThreadTypes;
#define	THREAD_CREATE_SUSPENDED	THREAD_SUSPEND_BEFORE_INIT

extern DWORD thread2ThreadKey, thread2ThreadKeyClients;

#pragma mark -------------

#include <exception>

class Thread_Exception : public std::exception {
public:
	const char *errorMessage;
	Thread_Exception() throw()
	{
		errorMessage = "";
	}
	Thread_Exception(const char *msg) throw()
	{
		errorMessage = msg;
	}
	virtual const char* what() const throw()
	{
		return errorMessage;
	}
};

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
		void *operator new(size_t size) throw(std::bad_alloc)
		{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN );
		  void *p = MSEreallocShared( NULL, size, 0 );
			if( p ){
				return p;
			}
			else{
				throw std::bad_alloc();
			}
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
			Return the creator thread's HANDLE. This value is defined only after the thread
			has been started (i.e. immediately for SuspenderThreads, but after the call to
			Start() for regular Thread instances).
		 */
		HANDLE Creator()
		{
			return m_ThreadCtx.m_hCreator;
		}

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
			Returns the thread's current priority level
		 */
		int ThreadPriority()
		{
			return GetThreadPriority(m_ThreadCtx.m_hThread);
		}
		/*!
			Returns the thread's current priority level and sets a new level.
		 */
		int ThreadPriority(int nPriority)
		{ int ret = GetThreadPriority(m_ThreadCtx.m_hThread);
			SetThreadPriority( m_ThreadCtx.m_hThread, nPriority );
			return ret;
		}
		/*!
			Returns the thread's current priority level and sets a new level
			according to refThread's priority level
		 */
		int ThreadPriority(HANDLE refThread)
		{ int ret = GetThreadPriority(m_ThreadCtx.m_hThread);
			if( refThread ){
				SetThreadPriority( m_ThreadCtx.m_hThread, GetThreadPriority(refThread) );
			}
			return ret;
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
		Thread( Thread &t ) throw(char*)
		{
			throw "Thread instance copying is undefined";
		}
		Thread( const Thread &t ) throw(char*)
		{
			throw "Thread instance copying is undefined";
		}

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
#if DEBUG > 1
					fprintf( stderr, "@@%p/%p starting AFTER_INIT suspension\n", pParent, pParent->m_ThreadCtx.m_pParent );
#endif
					pParent->startLock.Wait();
				}
			}

			pParent->m_ThreadCtx.m_dwExitCode = pParent->Run( pParent->m_ThreadCtx.m_pUserData );
			pParent->m_ThreadCtx.m_bExitCodeSet = true;

			if( pParent->suspendOption ){
				if( (pParent->suspendOption & THREAD_SUSPEND_BEFORE_CLEANUP) ){
#if DEBUG > 1
					fprintf( stderr, "@@%p/%p starting BEFORE_CLEANUP suspension\n", pParent, pParent->m_ThreadCtx.m_pParent );
#endif
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
#pragma mark class Thread::ThreadContext
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
				ThreadContext();
//#ifdef __windows__
//					:ProgName(ProgName())
//#endif
//				{
//					memset(this, 0, sizeof(this));
//				}

				/*!
				 *	Attributes Section
				 */
			public:
				HANDLE m_hThread;					//!<	The Thread Handle
				DWORD  m_dwTID;					//!<	The Thread ID
				LPVOID m_pUserData;					//!<	The user data pointer
				LPVOID m_pParent;					//!<	A copy of the <this> pointer of the Thread object
#ifdef __windows__
				const char *ProgName;
#endif
				DWORD  m_dwExitCode;				//!<	The Exit Code of the thread
				bool	  m_bExitCodeSet;				//!< Whether the exit code has been set explicitly
				HANDLE m_hCreator;					//!< handle of the creator thread
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

#pragma mark -------------

/*!
	A simple class interface to spawn a function in a Thread. Rather than defining a specific Thread instance
	for each background process, this interface allows boost::thread like statements like
	@n
	BackgroundFunction<THREAD_RETURN,int> bgFun0A(functionWithoutArguments, true);
	@n or
	@n
	BackgroundFunction<double,DWORD*> bgFun1A(functionWith1Argument, &counter, false);
	@n (see threadTest.cpp)
 */
template <typename ReturnType, typename ArgType>
class BackgroundFunction : public Thread
{
	protected:
		const bool hasArgument;
		bool done;
		ReturnType functionResult;
		union f {
			ReturnType (*function0Args)();
			ReturnType (*function1Arg)(ArgType arg);
		} backgroundFunction;
	public:
		const ArgType functionArgument;

		BackgroundFunction(ReturnType (*function)(ArgType arg),ArgType arg, bool immediate=true)
			: hasArgument(true),
				functionArgument(arg),
				Thread( THREAD_CREATE_SUSPENDED, NULL )
		{
			backgroundFunction.function1Arg = function;
			if( immediate ){
				Continue();
			}
		}
		BackgroundFunction(ReturnType (*function)(), bool immediate=true)
			: hasArgument(false),
				functionArgument((ArgType)0),
				Thread( THREAD_CREATE_SUSPENDED, NULL )
		{
			backgroundFunction.function0Args = function;
			if( immediate ){
				Continue();
			}
		}
		/*!
			quick query of the function result - undefined when the function
			is still running
		 */
		ReturnType result()
		{
			return functionResult;
		}
		/*!
			Returns true and the function's result value when it is done
			executing, false otherwise (in which case result is not changed).
		 */
		bool getResult(ReturnType &result)
		{
			if( done ){
				result = functionResult;
			}
			return done;
		}
	protected:
		DWORD Run( LPVOID arg )
		{
			done = false;
			if( hasArgument ){
				functionResult = (*backgroundFunction.function1Arg)(functionArgument);
			}
			else{
				functionResult = (*backgroundFunction.function0Args)();
			}
			done = true;
			return 0;
		}
};

#pragma mark -------------

#include <list>

class SharedValue_Exception : public Thread_Exception {
public:
	SharedValue_Exception() throw()
	{
		errorMessage = "";
	}
	SharedValue_Exception(const char *msg) throw()
	{
		errorMessage = msg;
	}
};

template <typename SHType>
/*!
	a simple template class for creating shared objects. Access is preempted
	through inheritance of the CritSectEx critical section class.
 */
class SharedValue : protected CritSectEx {
	public:
		typedef SharedValue<SHType>	SharedValueType;
		typedef SharedValueType		*SharedValueTypePtr;
	private:
		//!< fields for constructing a linked list of class instances referring to the same SHType instance:
		SharedValueType *master;
		typedef std::list<SharedValueTypePtr> SharedValueCopies;
		SharedValueCopies *copies;
		void AddCopy(SharedValueTypePtr copy) throw(std::bad_alloc)
		{
			if( !copies ){
				if( !(copies = new SharedValueCopies) ){
					throw std::bad_alloc();
				}
			}
			copies->push_front(copy);
		}
	protected:
		SHType		*value;			//!< a pointer to shared memory containing the actual variable
		const bool	freeMemory;		//!< false when the value memory should not be released in the destructor
	// shared value
	public:
		/*!
			new operator that always uses shared memory
		 */
		void *operator new(size_t size) throw(std::bad_alloc)
		{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN, int forceShared );
		  void *p = MSEreallocShared( NULL, size, 0, true );
			if( p ){
				return p;
			}
			else{
				throw std::bad_alloc();
			}
		}
		void operator delete(void *p)
		{ extern void MSEfreeShared(void *ptr);
			MSEfreeShared(p);
		}
		SharedValue()
			:freeMemory(false)
		{
			value = NULL;
			master = NULL;
			copies = NULL;
		}
		/*!
			constructor; initialises a new instance with the specified initial value and
			spinMax. The value is stored in newly allocated shared memory.
		 */
		SharedValue(const SHType val, DWORD spinMax=4000)
			:freeMemory(true)
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
			master = NULL;
			copies = NULL;
		}
		/*!
			constructor; initialises a new instance with the specified memory buffer and
			spinMax. If transferOwnerShip==true the memory buffer will be released in our
			destructor, relieving the user of this task.
		 */
		SharedValue(SHType *buffer, bool transferOwnership, DWORD spinMax)
			:freeMemory(transferOwnership)
		{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN, int forceShared );
			// set the CritSectEx spinMax property. This value should apparently be positive to
			// avoid deadlocking
			SetSpinMax( (spinMax)? spinMax : 1 );
			// store the buffer
			value = buffer;
			master = NULL;
			copies = NULL;
		}
		/*!
			the copy constructor creates a new lock but copies the actual memory, which is shared after all
			of course that means that freeMemory must be False
		 */
		SharedValue(SharedValueType &p)
			:freeMemory(false)
		{
			SetSpinMax(p.CritSectEx::SpinMax());
			value = p.value;
			master = (p.master)? p.master : &p;
			master->AddCopy(this);
		}
		SharedValue(SharedValueType &p, DWORD spinMax)
			:freeMemory(false)
		{
			SetSpinMax(spinMax);
			value = p.value;
			master = (p.master)? p.master : &p;
			master->AddCopy(this);
		}
		~SharedValue()
		{ extern void MSEfreeShared(void *ptr);
			// placement delete:
			if( value && freeMemory ){
				value->~SHType();
				MSEfreeShared(value);
			}
			if( master && master->copies ){
				master->copies->remove(this);
			}
			if( copies ){
			  typename SharedValueCopies::iterator it;
				for( it = copies->begin() ; it != copies->end(); ++it ){
					if( *it && (*it)->value == value ){
						(*it)->value = NULL;
						(*it)->master = NULL;
					}
				}
				copies->clear();
				delete copies;
				copies = NULL;
			}
		}

		/*!
			a quick read-out that does not lock the object
		 */
		inline SHType Read() throw(SharedValue_Exception)
		{
			if( value ){
				return *value;
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			return the value after preempting access
		 */
		inline SHType Value(DWORD dwTimeout=INFINITE) throw(SharedValue_Exception)
		{
			if( value ){
			  Scope scope(this,dwTimeout);
				return *value;
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			set a new value and return the old value after preempting access
		 */
		inline SHType Value( const SHType &val, DWORD dwTimeout=INFINITE) throw(SharedValue_Exception)
		{
			if( value ){
			  Scope scope(this,dwTimeout);
			  SHType oldValue = *value;
				*value = val;
				return oldValue;
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		inline SHType Value( const SHType *val, DWORD dwTimeout=INFINITE) throw(SharedValue_Exception)
		{ 
			if( value ){
			  Scope scope(this,dwTimeout);
			  SHType oldValue = *value;
				*value = *val;
				return oldValue;
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
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
					variable = this->value;
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
		operator SHType()
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return *value;
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			operator for assigning a value to shared (scalar) values
		 */
		inline SHType operator=(const SHType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return *value = val;
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			assignment operating between shared (scalar) values. If the target
			is initialised copy the source value after locking both SharedValues,
			else, become a copy of the source.
		 */
		inline SHType operator=(SharedValueType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return *value = val.Value();
			}
			else{
				// we're uninitialised: become a copy of the source
				SetSpinMax(val.CritSectEx::SpinMax());
				value = val.value;
				master = (val.master)? val.master : &val;
				master->AddCopy(this);
				// noboday can be using us, so there's no need for locking:
				return *value;
			}
		}
		/*!
			equality operator for shared (scalar) values
		 */
		inline bool operator==(const SHType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return *value == val;
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			equality operator for shared (scalar) values
		 */
		inline bool operator==(SharedValueType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return *value == val.Value();
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			inequality operator for shared (scalar) values
		 */
		inline bool operator!=(const SHType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return *value != val;
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			inequality operator for shared (scalar) values
		 */
		inline bool operator!=(SharedValueType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return *value != val.Value();
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			increment operator for shared (scalar) values
		 */
		inline SHType operator+=(const SHType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return (*value += val);
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			decrement operator for shared (scalar) values
		 */
		inline SHType operator-=(const SHType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return (*value -= val);
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			in-place multiply operator for shared (scalar) values
		 */
		inline SHType operator*=(const SHType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return (*value *= val);
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		/*!
			in-place division operator for shared (scalar) values
		 */
		inline SHType operator/=(const SHType &val)
		{
			if( value ){
			  Scope scope(this,INFINITE);
				return (*value /= val);
			}
			else{
				throw SharedValue_Exception("Stale or uninitialised SharedValue");
			}
		}
		template <typename CharT, typename Traits>
			friend std::basic_ostream<CharT, Traits>& operator <<(std::basic_ostream <CharT, Traits>& os, SharedValueType& x)
			{
				if( os.good() ){
				  typename std::basic_ostream <CharT, Traits>::sentry opfx(os);
					if( opfx ){
					  std::basic_ostringstream<CharT, Traits> s;
						s.flags(os.flags());
						s.imbue(os.getloc());
						s.precision(os.precision());
						if( x.value ){
							s << "<SharedValue<" << typeid(SHType).name() << "> " << x.Read() << ">";
						}
						else{
							s << "<SharedValue<" << typeid(SHType).name() << "> - stale or uninitialised>";
						}
						if( s.fail() ){
							os.setstate(std::ios_base::failbit);
						}
						else{
							os << s.str();
						}
					}
				}
				return os;
			};
	// allow the SharedArray class to access our protected items
	template <typename SHAType> friend class SharedArray;
};

#pragma mark -------------

/*!
	A variant on the SharedValue that can hold shared arrays. It is nothing more than a wrapper
	around SharedValue that adds size and "last accessed element" constants which keep track of
	how many elements the SharedValue contains, exploiting the fact that a pointer to a single instance
	is also a C array of 1 such instance.
 */
template <typename SHAType>
class SharedArray {
	protected:
		SharedValue<SHAType>	*data;		//!< The SharedValue instance that will hold the actual data
		size_t				N, last;		//!< constants allowing to interpret the SharedValue for what it is
		DWORD				spinMax;
	public:
		/*!
			new operator that always uses shared memory
		 */
		void *operator new(size_t size) throw(std::bad_alloc)
		{ extern void *MSEreallocShared( void* ptr, size_t N, size_t oldN, int forceShared );
		  void *p = MSEreallocShared( NULL, size, 0, true );
			if( p ){
				return p;
			}
			else{
				throw std::bad_alloc();
			}
		}
		void operator delete(void *p)
		{ extern void MSEfreeShared(void *ptr);
			MSEfreeShared(p);
		}
		SharedArray(DWORD spinMax=4000)
		{
			spinMax = (spinMax)? spinMax : 1;
			last = N = 0;
			data = NULL;
		}
		/*!
			constructor; initialises a new instance with the specified initial values and
			spinMax
		 */
		SharedArray(const SHAType *arr, const size_t elems, DWORD spinMax=4000)
		{
			spinMax = (spinMax)? spinMax : 1;
			// the actual work is done by the method that can also be called by the user
			cseAssertExInline( SetValues( arr, elems ), __FILE__, __LINE__, "shared memory allocation error" );
		}
		SharedArray(SharedArray<SHAType> &p) throw(std::bad_alloc)
		{
			// the throw is made by new SharedValue, when required:
			data = new SharedValue<SHAType>( *p.data, p.spinMax );
			N = p.N, last = p.last, spinMax = p.spinMax;
		}
		virtual ~SharedArray()
		{
			if( data ){
				delete data;
			}
		}

		bool SetValues(const SHAType *arr, const size_t elems)
		{ extern void *MSEreallocShared( void*, size_t, size_t, int );
			// allocate the value store using placement new (invokes the SHAType constructor on
			// memory that's been allocated by our shared memory allocator).
			if( data && data->freeMemory ){
			  CritSectEx::Scope scope(data);
				// reallocate the shared variable's memory
				data->value = new ((SHAType*) MSEreallocShared( data->value, elems * sizeof(SHAType), N * sizeof(SHAType), true )) SHAType[elems];
				if( data->value ){
					N = elems;
					// store the initial values
					memmove( data->value, arr, N * sizeof(SHAType) );
					last = 0;
					return true;
				}
				else{
					last = N = 0;
					delete data;
					data = NULL;
					return false;
				}
			}
			else{
			  SHAType *buffer;
				if( data ){
					// this probably should never happen, but should we encounter a SharedValue
					// that does not own its value store, delete it rather than reallocating
					// its memory store.
					delete data;
					data = NULL;
				}
				// allocate the value store from shared memory
				buffer = new ((SHAType*) MSEreallocShared( NULL, elems * sizeof(SHAType), 0, true )) SHAType[N];
				if( buffer ){
					N = elems;
					// store the initial values
					memmove( buffer, arr, N * sizeof(SHAType) );
					// create the SharedValue instance with the buffer we just initialised, transferring
					// ownership so we do not have to release the memory ourselves.
					data = new SharedValue<SHAType>(buffer, true, spinMax);
					last = 0;
				}
				return data != NULL;
			}
			return false;
		}
		/*!
			a quick read-out that does not lock the object
		 */
		inline SHAType Read(const size_t idx)
		{
			return data->value[idx];
		}
		/*!
			return the element after preempting access
		 */
		inline SHAType ValueAtIndex(const size_t idx, DWORD dwTimeout=INFINITE)
		{ CritSectEx::Scope scope(data,dwTimeout);
			return data->value[last=idx];
		}
		/*!
			set a new value and return the old value after preempting access
		 */
		inline SHAType ValueAtIndex( const size_t idx, const SHAType &val, DWORD dwTimeout=INFINITE)
		{ CritSectEx::Scope scope(data,dwTimeout);
		  SHAType oldValue = data->value[last=idx];
			data->value[idx] = val;
			return oldValue;
		}
		inline SHAType ValueAtIndex( const size_t idx, const SHAType *val, DWORD dwTimeout=INFINITE)
		{ CritSectEx::Scope scope(data,dwTimeout);
		  SHAType oldValue = data->value[last=idx];
			data->value[idx] = *val;
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
			which preempts access to the store during its lifetime. This is NOT meant to
			change all elements; for that use SetValues().
		 */
		struct DirectAccess {
			protected:
				CritSectEx::Scope *scope;
			public:
				SHAType *variable;
				const size_t size;
				DirectAccess(DWORD dwTimeout=INFINITE)
					:size(this->N)
				{
					scope = new CritSectEx::Scope(this->data,dwTimeout);
					variable = this->data->value;
				}
				DirectAccess(SharedArray *shVal, DWORD dwTimeout=INFINITE)
					:size((shVal)?shVal->N : 0)
				{
					cseAssertExInline( shVal!=NULL, __FILE__, __LINE__, "NULL pointer passed to DirectAccess constructor" );
					scope = new CritSectEx::Scope(shVal,dwTimeout);
					variable = shVal->data->value;
				}
				DirectAccess(SharedArray &shVal, DWORD dwTimeout=INFINITE)
					:size(shVal.N)
				{
					scope = new CritSectEx::Scope(shVal,dwTimeout);
					variable = shVal.data->value;
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
		{ CritSectEx::Scope scope(data,INFINITE);
			return data->value[last];
		}
		/*!
			index operator for shared arrays
		 */
		inline SHAType& operator[](const size_t idx)
		{ CritSectEx::Scope scope(data,INFINITE);
			return data->value[last=idx];
		}

		template <typename CharT, typename Traits>
			friend std::basic_ostream<CharT, Traits>& operator <<(std::basic_ostream <CharT, Traits>& os, SharedArray<SHAType>& x)
			{
				if( os.good() ){
				  typename std::basic_ostream <CharT, Traits>::sentry opfx(os);
					if( opfx ){
					  std::basic_ostringstream<CharT, Traits> s;
					  size_t N = x.Size();
						s.clear();
						s.flags(os.flags());
						s.imbue(os.getloc());
						s.precision(os.precision());
						s << "<SharedArray<" << typeid(SHAType).name() << ">[" << N << "] {" << x[0];
						for( size_t i = 1 ; i < N ; i++ ){
							s << "," << x[i];
						}
						s << "}>";
						if( s.fail() ){
							os.setstate(std::ios_base::failbit);
						}
						else{
							os << s.str();
						}
					}
				}
				return os;
			};
};

#endif //__THREAD_H__
