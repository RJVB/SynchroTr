/*!
	@file Thread.h
	A generic thread class based on Arun N Kumar's CThread
	http://www.codeproject.com/Articles/1570/A-Generic-C-Thread-Class
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#if !defined(WIN32) && !defined(_MSC_VER) && !defined(__MINGW32__)
#	include "CritSectEx/msemul.h"
#else
#	include <Windows.h>
#	include <intrin.h>
#endif
#include "CritSectEx/CritSectEx.h"

class Thread {
	public:
		/*!
		 *	Info: Starts the thread.
		 *	
		 *	This function starts the thread pointed by m_pThreadFunc with default attributes
		 */
		DWORD Start( void* arg = NULL )
		{ DWORD ret = 0;
			if( !startLock.IsLocked() && !m_ThreadCtx.m_hThread ){
				m_ThreadCtx.m_pUserData = arg;
				if( (m_ThreadCtx.m_hThread = CreateThread( NULL, 0, m_pThreadFunc, this, CREATE_SUSPENDED,
											  &m_ThreadCtx.m_dwTID ))
				){
					m_ThreadCtx.m_dwExitCode = (DWORD)-1;
					m_ThreadCtx.m_pParent = this;
					ret = GetLastError();
					ResumeThread( m_ThreadCtx.m_hThread );
				}
				else{
					ret = GetLastError();
				}
			}

			return ret;
		}

		DWORD Wait()
		{ DWORD ret;
			if( m_ThreadCtx.m_hThread ){
				return WaitForSingleObject( m_ThreadCtx.m_hThread, INFINITE );
			}
			else{
				ret = WAIT_ABANDONED;
			}
			return ret;
		}
		DWORD Wait(DWORD dwMilliSeconds)
		{ DWORD ret;
			if( m_ThreadCtx.m_hThread ){
				return WaitForSingleObject( m_ThreadCtx.m_hThread, dwMilliSeconds );
			}
			else{
				ret = WAIT_ABANDONED;
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
		DWORD Stop( bool bForceKill = false, DWORD dwExitCode=(DWORD)-1 )
		{
			if( m_ThreadCtx.m_hThread ){
				GetExitCodeThread( m_ThreadCtx.m_hThread, &m_ThreadCtx.m_dwExitCode );

				if ( m_ThreadCtx.m_dwExitCode == STILL_ACTIVE ){
					if( bForceKill ){
#if !defined(WIN32) && !defined(_MSC_VER) && !defined(__MINGW32__)
						TerminateThread( m_ThreadCtx.m_hThread, dwExitCode );
#else
						// first try to do something like pthread_cancel
						// (cf. http://locklessinc.com/articles/pthreads_on_windows/)
						{
						  int i = 5;
//						  CONTEXT ctxt;
//							ctxt.ContextFlags = CONTEXT_CONTROL;
//							SuspendThread(m_ThreadCtx.m_hThread);
//							GetThreadContext( m_ThreadCtx.m_hThread, &ctxt );
//#	ifdef _M_X64
//							ctxt.Rip = (uintptr_t) InvokeCancel;
//#	else
//							ctxt.Eip = (uintptr_t) &Thread::InvokeCancel;
//#	endif
//							SetThreadContext( m_ThreadCtx.m_hThread, &ctxt);
//							_InterlockedIncrement(&m_lCancelling);
//							ResumeThread(m_ThreadCtx.m_hThread);
//							for( i = 0 ; i < 5 ; ){
//								if( WaitForSingleObject( m_ThreadCtx.m_hThread, 1000 ) == WAIT_OBJECT_0 ){
//									break;
//								}
//								else{
//									i += 1;
//								}
//							}
							if( i == 5 ){
								TerminateThread( m_ThreadCtx.m_hThread, dwExitCode );
							}
							else{
								m_ThreadCtx.m_dwExitCode = dwExitCode;
							}
						}
#endif
						CloseHandle(m_ThreadCtx.m_hThread);
						m_ThreadCtx.m_hThread = NULL;
						m_ThreadCtx.m_dwExitCode = dwExitCode;
					}
				}
				else{
					CloseHandle(m_ThreadCtx.m_hThread);
					m_ThreadCtx.m_hThread = NULL;
				}
			}

			return m_ThreadCtx.m_dwExitCode;
		}

		DWORD GetExitCode() const 
		{ 
			if ( m_ThreadCtx.m_hThread ){
				GetExitCodeThread( m_ThreadCtx.m_hThread, (LPDWORD)&m_ThreadCtx.m_dwExitCode );
			}
			return m_ThreadCtx.m_dwExitCode;
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
			Detach();
		}

		/*!
		 *	Info: Plug Constructor
		 *
		 *	Use this to migrate/port existing worker threads to objects immediately
		 *  Although you lose the benefits of ThreadCTOR and ThreadDTOR.
		 */
		Thread(LPTHREAD_START_ROUTINE lpExternalRoutine)
		{
			Attach(lpExternalRoutine);
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
				CloseHandle(m_ThreadCtx.m_hThread);
			}
		}

	protected:

		/*!
		 *	Info: DONT override this method.
		 *	
		 *	This function is like a standard template. 
		 *	Override if you are sure of what you are doing.
		 */
		static DWORD WINAPI EntryPoint( LPVOID pArg)
		{
			Thread *pParent = reinterpret_cast<Thread*>(pArg);

			pParent->Init();

			pParent->Run( pParent->m_ThreadCtx.m_pUserData );

			pParent->Cleanup();

			GetExitCodeThread( pParent->m_ThreadCtx.m_hThread, (LPDWORD)&pParent->m_ThreadCtx.m_dwExitCode );

			return STILL_ACTIVE;
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
		 *	Info: Initialisation function. 
		 *	
		 *	Will be called by EntryPoint before executing the thread body.
		 *  Override this function to provide your extra initialisation.
		 */
		virtual void Init()
		{
			return;
		}

		/*!
		 *	Info: Cleanup function. 
		 *	
		 *	Will be called by EntryPoint after executing the thread body.
		 *  Override this function to provide your extra destruction.
		 */
		virtual void Cleanup()
		{
			return;
		}
		
	private:
		volatile long m_lCancelling;
		CritSectEx startLock;
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
		};

		void WINAPI InvokeCancel()
		{

			_InterlockedDecrement(&m_lCancelling);
			((Thread*)m_ThreadCtx.m_pParent)->Cleanup();

			((Thread*)m_ThreadCtx.m_pParent)->m_ThreadCtx.m_dwExitCode = ~STILL_ACTIVE;

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
