//=================================================================================================
// cthread.cpp - Implements a class that wraps POSIX threads
//=================================================================================================
#include <unistd.h>
#include "cthread.h"

//=================================================================================================
// This is used to pass information from Spawn() to LaunchThread()
//=================================================================================================
struct CThreadSpawn
{
	CThread*	object;
	void		*P1, *P2, *P3;
	CThreadSpawn(CThread* p, void* p1, void* p2, void* p3)
	{
	    object = p; P1 = p1; P2 = p2; P3 = p3;
	}
};
//=================================================================================================


//=================================================================================================
// This is a count of how many CThread objects have been created
//=================================================================================================
int CThread::m_thread_count= 0;
//=================================================================================================


//=================================================================================================
// launch_cthread() - This is a helper function responsible for actually bringing the thread
//                    into existence.
//
// Passed: thread_ptr = Pointer to a CThreadSpawn object *ON THE HEAP*.
//                      When this routine is done, it will automatically
//                      free the memory used by this CThreadSpawn object
//
//=================================================================================================
void* launch_cthread(void* thread_ptr)
{
	// Turn the passed parameter into a pointer of the correct type
	CThreadSpawn *p = (CThreadSpawn*)thread_ptr;

	// Spin up "Main()" in our new thread.  When this returns, the thread
	// is done executing
	p->object->main(p->P1, p->P2, p->P3);

	// Free up the memory that was allocated during Spawn()
	delete p;

	// There's nothing to hand back to the caller
	return nullptr;
}
//=================================================================================================


//=================================================================================================
// Default constructor
//=================================================================================================
CThread::CThread()
{
	// The default Thread ID is the # of CThread objects that have been
	// constructed previous to this one. A different Thread ID can be set
	// manually by calling set_thread_id();
	m_id = m_thread_count++;
}
//=================================================================================================


//=================================================================================================
// Spawn() - Called in order to start this thread
//=================================================================================================
int CThread::spawn(void* P1, void* P2, void* P3)
{
	// Create the parameter object that controls LaunchThread()
	CThreadSpawn *params = new CThreadSpawn(this, P1, P2, P3);

	// Spin up the thread
	int ret = pthread_create(&m_thread, nullptr, launch_cthread, (void*) params);

	// Tell the caller if it worked
	return ret;
}
//=================================================================================================

//=================================================================================================
// Join() - Join (i.e., wait on) this thread
//=================================================================================================
void CThread::join()
{
	// Join this thread
	pthread_join(m_thread, NULL);
}
//=================================================================================================


//=================================================================================================
// exit() - Terminates this thread.  Should only be called internally to
//          this thread!!
//=================================================================================================
void CThread::exit()
{
	pthread_exit(NULL);
}
//=================================================================================================


//=================================================================================================
// cancel() - Terminates and tears down the thread.
//
// Passed: wait = if 'true', this routine will wait for the thread to complete the
//                teardown process.  If 'false', this routine will return immediately
//=================================================================================================
void CThread::cancel(bool wait_flag)
{
	pthread_cancel(m_thread);
	if (wait_flag) join();
}
//=================================================================================================


//=================================================================================================
// Constructor() - Initializes the mutex
//=================================================================================================
PCriticalSection::PCriticalSection()
{
	pthread_mutexattr_t attributes;

	// Initialize the mutex attributes
	pthread_mutexattr_init(&attributes);

	// We want a thread to be able to recursively lock this mutex
	pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);

	// Create a recursive mutex
	pthread_mutex_init(&m_mutex, &attributes);
}
//=================================================================================================



