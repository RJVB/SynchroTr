/*!
	@file Thread.h
	A generic thread class based on Arun N Kumar's CThread
	http://www.codeproject.com/Articles/1570/A-Generic-C-Thread-Class
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#include "CritSectEx/CritSectEx.h"

typedef enum { THREAD_SUSPEND_NOT=0,		//!< The thread runs normally after the initial creation
	THREAD_SUSPEND_BEFORE_INIT=1<<0,		//!< The thread is not unsuspended after its creation
	THREAD_SUSPEND_AFTER_INIT=1<<1,		//!< The thread is allowed to perform Init() and is suspended
	THREAD_SUSPEND_BEFORE_CLEANUP=1<<2		//!< The thread is suspended before Cleanup()
} SuspenderThreadTypes;

static DWORD thread2ThreadKey = NULL, thread2ThreadKeyClients = 0;

class Thread {
	public:
		/*!
		 *	Info: Starts the thread.
		 *	
		 *	This function starts the thread pointed by m_pThreadFunc with default attributes
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

		bool isStarted()
		{
			return hasBeenStarted;
		}
		bool IsWaiting()
		{
			return hasBeenStarted && (startLock.IsLocked() || isSuspended);
		}
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
		bool Suspend()
		{ bool prev = isSuspended;
			if( hasBeenStarted && !isSuspended ){
				if( SuspendThread(m_ThreadCtx.m_hThread) ){
					_InterlockedSetTrue(isSuspended);
				}
			}
			return prev;
		}

		DWORD Join()
		{ DWORD ret;
			if( m_ThreadCtx.m_hThread ){
				return WaitForSingleObject( m_ThreadCtx.m_hThread, INFINITE );
			}
			else{
				ret = WAIT_FAILED;
			}
			return ret;
		}
		DWORD Join(DWORD dwMilliSeconds)
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
		 *	Info: Stops the thread.
		 *	
		 *	This function stops the current thread. 
		 *	We can force kill a thread which results in a TerminateThread, which is
		 *	a last-resource-only approach.
		 */
		DWORD Stop( bool bForceKill = false, DWORD dwForceExitCode=(DWORD)-1 )
		{
			if( m_ThreadCtx.m_hThread ){
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
						// (cf. http://locklessinc.com/articles/pthreads_on_windows/)
						{
						  int i = 5;
						  CONTEXT ctxt;
							ctxt.ContextFlags = CONTEXT_CONTROL;
							SuspendThread(m_ThreadCtx.m_hThread);
							GetThreadContext( m_ThreadCtx.m_hThread, &ctxt );
#	ifdef _M_X64
							ctxt.Rip = (uintptr_t) &Thread::InvokeCancel;
#	else
							ctxt.Eip = (uintptr_t) &Thread::InvokeCancel;
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
								TerminateThread( m_ThreadCtx.m_hThread, dwForceExitCode );
							}
							else{
								m_ThreadCtx.m_dwExitCode = dwForceExitCode;
							}
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
			memset( this, 0, sizeof(*this) );
			suspendOption = THREAD_SUSPEND_NOT;
			Detach();
		}

		/*!
		 *	Info: Constructor to create a thread that is launched at once but
		 *	kept suspended either before or after execution of the Init() method.
		 */
		Thread( SuspenderThreadTypes when, void* arg = NULL )
		{
			memset( this, 0, sizeof(*this) );
			SuspenderThread( when, arg );
		}
		Thread( int when, void* arg = NULL )
		{
			memset( this, 0, sizeof(*this) );
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
			memset( this, 0, sizeof(*this) );
			suspendOption = THREAD_SUSPEND_NOT;
			Attach(lpExternalRoutine);
		}

		DWORD SuspenderThread( SuspenderThreadTypes when, void* arg = NULL )
		{
			suspendOption = when;
			Detach();
			return Start(arg);
		}
		DWORD SuspenderThread( int when, void* arg = NULL )
		{
			suspendOption = (SuspenderThreadTypes) when;
			Detach();
			return Start(arg);
		}
		/*!
		 *	Info: Default Destructor
		 *
		 *	I think it is wise to destroy the thread even if it is running,
		 *  when the main thread reaches here.
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

		THREAD_RETURN SetExitCode(THREAD_RETURN dwExitCode)
		{ THREAD_RETURN ret = (THREAD_RETURN) m_ThreadCtx.m_dwExitCode;
			m_ThreadCtx.m_dwExitCode = (DWORD) dwExitCode;
			m_ThreadCtx.m_bExitCodeSet = true;
			return ret;
		}

		/*!
		 *	Info: DONT override this method.
		 *	
		 *	This function is like a standard template. 
		 *	Override if you are sure of what you are doing.
		 */
		static THREAD_RETURN WINAPI EntryPoint( LPVOID pArg)
		{
			Thread *pParent = reinterpret_cast<Thread*>(pArg);

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
		 *	Info: Override this method.
		 *	
		 *	This function should contain the body/code of your thread.
		 *	Notice the signature is similar to that of any worker thread function
		 *  except for the calling convention.
		 */
		virtual DWORD Run( LPVOID /* arg */ )
		{
			return m_ThreadCtx.m_dwExitCode;
		}

		/*!
		 *	Info: Cleanup function. 
		 *	
		 *	Will be called by EntryPoint after executing the thread body.
		 *  Override this function to provide your extra destruction.
		 */
		virtual void CleanupThread()
		{
		}


	private:
		volatile long m_lCancelling;
		class StartLocks {
			HANDLE event;
			long locked, notified;
			public:
				StartLocks()
				{
					cseAssertEx( (event = CreateEvent( NULL, false, false, NULL ))!=NULL, __FILE__, __LINE__ );
					locked = false;
					notified = false;
				}
				~StartLocks()
				{
					if( event ){
						CloseHandle(event);
					}
					locked = false;
				}
				__forceinline bool IsLocked()
				{
					return locked;
				}
				__forceinline bool Notify()
				{
					// notified is used just in case something interrupts WaitForSingleObject
					// (i.e. a non-timeout return) not preceded by a notification.
					_InterlockedSetTrue(notified);
					return SetEvent(event);
				}
				__forceinline bool Wait()
				{ DWORD ret;
					if( !locked ){
						_InterlockedSetTrue(locked);
						while( (ret = WaitForSingleObject( event, 100 )) == WAIT_TIMEOUT
							 /*&& ret != WAIT_ABANDONED*/ && !notified
						){
							if( ret == WAIT_TIMEOUT ){
								// just in case:
								_InterlockedSetTrue(locked);
								YieldProcessor();
							}
						}
						_InterlockedSetFalse(locked);
						_InterlockedSetFalse(notified);
					}
					else{
						ret = WAIT_FAILED;
					}
					return (ret != WAIT_ABANDONED && ret != WAIT_FAILED);
				}
		};
	private:
		StartLocks startLock;
		SuspenderThreadTypes suspendOption;
		long isSuspended;
		bool hasBeenStarted;

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

		static void WINAPI InvokeCancel()
		{ Thread *self = (Thread*)TlsGetValue(thread2ThreadKey);
//			fprintf( stderr, "@@ InvokeCancel(%p) ...", self ); fflush(stderr);
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
		 *	Attributes Section
		 */
	protected:
		/*!
		 *	Info: Members of Thread
		 */
		ThreadContext			m_ThreadCtx;			//!<	The Thread Context member
		LPTHREAD_START_ROUTINE	m_pThreadFunc;			//!<	The Worker Thread Function Pointer
	public:
		HANDLE GetThread()
		{
			return m_ThreadCtx.m_hThread;
		}
};

#endif //__THREAD_H__
