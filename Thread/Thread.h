/*!
	@file Thread.h
	A generic thread class based on Arun N Kumar's CThread
	http://www.codeproject.com/Articles/1570/A-Generic-C-Thread-Class
	adapted and extended by RJVB (C) 2012
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#include "CritSectEx/CritSectEx.h"

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
		/*!
		 *	Info: Starts the thread.
		 *	
		 *	This function creates and starts the worker thread, passing arg to the worker.
		 *	When called on a SuspenderThread it will unblock the worker in case it is waiting
		 *	at a synchronisation point. (The initial invocation that creates the thread is done
		 *	through the constructor in this case.)
		 */
		DWORD Start( void* arg = NULL )
		{ DWORD ret = 0;
			if( !thread2ThreadKey ){
				thread2ThreadKey = TlsAlloc();
			}
			if( !startLock.IsLocked() ){
				cseAssertEx( m_ThreadCtx.m_hThread == NULL, __FILE__, __LINE__ );
				m_ThreadCtx.m_pUserData = arg;
				if( (m_ThreadCtx.m_hThread = CreateThread( NULL, 0, m_pThreadFunc, this, CREATE_SUSPENDED,
											  &m_ThreadCtx.m_dwTID ))
				){
					m_ThreadCtx.m_dwExitCode = (DWORD)-1;
					m_ThreadCtx.m_pParent = this;
					hasBeenStarted = true;
					ret = GetLastError();
					if( (suspendOption & THREAD_SUSPEND_BEFORE_INIT) == 0 ){
						ResumeThread( m_ThreadCtx.m_hThread );
						_InterlockedSetFalse(isSuspended);
					}
					else{
						_InterlockedSetTrue(isSuspended);
					}
				}
				else{
					ret = GetLastError();
				}
			}
			else{
				cseAssertEx( m_ThreadCtx.m_hThread != NULL, __FILE__, __LINE__ );
				startLock.Notify();
			}

			return ret;
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
			unblocks a worker that is suspended or waiting at a synchronisation point
		 */
		bool Continue()
		{
			if( hasBeenStarted ){
				if( isSuspended ){
					ResumeThread(m_ThreadCtx.m_hThread);
					_InterlockedSetFalse(isSuspended);
				}
				if( startLock.IsLocked() ){
					return startLock.Notify();
				}
			}
			return false;
		}
	
		/*!
			suspends the worker thread. This can be done at any point
			in the worker cycle, contrary to blocking at synchronisation
			which the worker does itself at fixed points. The method returns
			the previous suspension state.
		 */
		bool Suspend()
		{ bool prev = isSuspended;
			if( hasBeenStarted && !isSuspended ){
				if( SuspendThread(m_ThreadCtx.m_hThread) ){
					_InterlockedSetTrue(isSuspended);
				}
			}
			return prev;
		}

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
		DWORD Join(DWORD dwMilliSeconds=INFINITE)
		{ DWORD ret;
			if( m_ThreadCtx.m_hThread ){
				return WaitForSingleObject( m_ThreadCtx.m_hThread, dwMilliSeconds );
			}
			else{
				ret = WAIT_FAILED;
			}
			return ret;
		}

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
		DWORD Stop( bool bForceKill=false, DWORD dwForceExitCode=(DWORD)-1 )
		{
			if( m_ThreadCtx.m_hThread ){
				// set the shouldExit signal flag as the first thing
				_InterlockedSetTrue(threadShouldExit);
				if( isSuspended ){
					Continue();
				}
				DWORD temp = STILL_ACTIVE;
				if( GetExitCodeThread( m_ThreadCtx.m_hThread, &temp ) &&!m_ThreadCtx.m_bExitCodeSet ){
					m_ThreadCtx.m_dwExitCode = temp;
				}

				if( temp == STILL_ACTIVE ){
					if( IsWaiting() ){
						suspendOption = THREAD_SUSPEND_NOT;
						Continue();
					}
					if( bForceKill ){
#if !defined(WIN32) && !defined(_MSC_VER) && !defined(__MINGW32__)
						TerminateThread( m_ThreadCtx.m_hThread, dwForceExitCode );
#else
						// first try to do something like pthread_cancel
						if( !Cancel() ){
							TerminateThread( m_ThreadCtx.m_hThread, dwForceExitCode );
						}
						else{
							m_ThreadCtx.m_dwExitCode = dwForceExitCode;
						}
#endif
						CloseHandle(m_ThreadCtx.m_hThread);
						m_ThreadCtx.m_hThread = NULL;
						m_ThreadCtx.m_dwExitCode = dwForceExitCode;
					}
				}
				else{
					CloseHandle(m_ThreadCtx.m_hThread);
					m_ThreadCtx.m_hThread = NULL;
				}
			}

			return m_ThreadCtx.m_dwExitCode;
		}

		/*!
			get the worker's current exit code. This will be STILL_ACTIVE if the
			thread is still running, or else the exit code specified by the worker.
		 */
		THREAD_RETURN GetExitCode()
		{ 
			if( m_ThreadCtx.m_hThread && !m_ThreadCtx.m_bExitCodeSet ){
			  DWORD temp;
				if( GetExitCodeThread( m_ThreadCtx.m_hThread, &temp ) ){
					m_ThreadCtx.m_dwExitCode = temp;
				}
			}
			return (THREAD_RETURN) m_ThreadCtx.m_dwExitCode;
		}

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
		Thread()
		{
			__init__();
			Detach();
		}

		/*!
		 *	Constructor to create a thread that is launched at once but
		 *	kept suspended either before or after execution of the InitThread() method.
		 */
		Thread( SuspenderThreadTypes when, void* arg = NULL )
		{
			__init__();
			SuspenderThread( when, arg );
		}
		Thread( int when, void* arg = NULL )
		{
			__init__();
			SuspenderThread( (SuspenderThreadTypes)when, arg );
		}

		/*!
		 *	Info: Plug Constructor
		 *
		 *	Use this to migrate/port existing worker threads to objects immediately
		 *  Although you lose the benefits of ThreadCTOR and ThreadDTOR.
		 */
		Thread(LPTHREAD_START_ROUTINE lpExternalRoutine)
		{
			__init__();
			Attach(lpExternalRoutine);
		}

		/*!
			initialisation function to convert an already created Thread object
			into a SuspenderThread instance - BEFORE Start() has been called.
		 */
		DWORD SuspenderThread( SuspenderThreadTypes when, void* arg = NULL )
		{
			suspendOption = when;
			Detach();
			return Start(arg);
		}
		/*!
			initialisation function to convert an already created Thread object
			into a SuspenderThread instance - BEFORE Start() has been called.
		 */
		DWORD SuspenderThread( int when, void* arg = NULL )
		{
			suspendOption = (SuspenderThreadTypes) when;
			Detach();
			return Start(arg);
		}
		/*!
			destructor. Stops the worker thread if it is still running and releases
			the thread2ThreadKey local storage object if no one is still using it.
		 */
		~Thread()
		{
			if( m_ThreadCtx.m_hThread ){
				Stop(true);
				if( m_ThreadCtx.m_hThread ){
					CloseHandle(m_ThreadCtx.m_hThread);
				}
			}
			if( thread2ThreadKeyClients > 1 ){
				thread2ThreadKeyClients -= 1;
			}
			else if( thread2ThreadKeyClients == 1 ){
				thread2ThreadKeyClients = 0;
				TlsFree(thread2ThreadKey);
				thread2ThreadKey = NULL;
			}
		}

	protected:

		/*!
			set the worker exit code/status
		 */
		THREAD_RETURN SetExitCode(THREAD_RETURN dwExitCode)
		{ THREAD_RETURN ret = (THREAD_RETURN) m_ThreadCtx.m_dwExitCode;
			m_ThreadCtx.m_dwExitCode = (DWORD) dwExitCode;
			m_ThreadCtx.m_bExitCodeSet = true;
			return ret;
		}

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
				StartLocks()
				{
					cseAssertEx( (lockEvent = CreateEvent( NULL, false, false, NULL ))!=NULL, __FILE__, __LINE__ );
					isLocked = false;
					isNotified = false;
				}
				~StartLocks()
				{
					if( lockEvent ){
						CloseHandle(lockEvent);
					}
					isLocked = false;
				}
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
		void __init__()
		{
			suspendOption = THREAD_SUSPEND_NOT;
			isSuspended = m_lCancelling = threadShouldExit = 0;
			hasBeenStarted = false;
		}

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
		static void WINAPI HandleCancel()
		{ Thread *self = (Thread*)TlsGetValue(thread2ThreadKey);
//			fprintf( stderr, "@@ HandleCancel(%p) ...", self ); fflush(stderr);
			if( self ){
				self->CleanupThread();
				self->m_ThreadCtx.m_dwExitCode = ~STILL_ACTIVE;
				_InterlockedDecrement(&self->m_lCancelling);
			}
//			fprintf( stderr, " returning\n" ); fflush(stderr);
			ExitThread((THREAD_RETURN)~STILL_ACTIVE);
			return;
		}
		/*!
			cancel the worker thread, i.e. coerce it through an 'official' exit point
			rather than killing it outright. Currently implemented on MS Win only.
		 */
		bool Cancel()
		{ bool ret;
#if !defined(WIN32) && !defined(_MSC_VER) && !defined(__MINGW32__)
			// to be implemented
			ret = false; 
#else
			// (cf. http://locklessinc.com/articles/pthreads_on_windows/)
		  int i = 5;
		  CONTEXT ctxt;
			ctxt.ContextFlags = CONTEXT_CONTROL;
			SuspendThread(m_ThreadCtx.m_hThread);
			GetThreadContext( m_ThreadCtx.m_hThread, &ctxt );
#	ifdef _M_X64
			ctxt.Rip = (uintptr_t) &Thread::HandleCancel;
#	else
			ctxt.Eip = (uintptr_t) &Thread::HandleCancel;
#	endif
			SetThreadContext( m_ThreadCtx.m_hThread, &ctxt);
			_InterlockedIncrement(&m_lCancelling);
			ResumeThread(m_ThreadCtx.m_hThread);
			for( i = 0 ; i < 5 ; ){
				if( WaitForSingleObject( m_ThreadCtx.m_hThread, 1000 ) == WAIT_OBJECT_0 ){
					break;
				}
				else{
					i += 1;
				}
			}
			if( i == 5 ){
#ifdef DEBUG
				fprintf( stderr, "@@ %p->Cancel() thread %p didn't cancel in %ds\n",
					this, i );
#endif //DEBUG
				ret = false;
			}
			else{
				ret = true;
			}
#endif // !windows
			return ret;
		}

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

#endif //__THREAD_H__
