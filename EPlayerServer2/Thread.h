#pragma once
#include <unistd.h>
#include <pthread.h>
#include "Function.h"
#include <fcntl.h>
#include <signal.h>
#include <map>

class CThread
{
public:
	CThread() {
		m_func = NULL;
		m_thread = 0;
		m_bPause = false;
	}

	template<typename _FUNC_ , typename..._ARGS_>
	CThread(_FUNC_ func, _ARGS_... args) {
		m_func = new CFunction<_FUNC_,_ARGS_...>(func, args...);
		m_thread = 0;
		m_bPause = false;
	}


	~CThread() {
		if (m_func != NULL) {
			CFunctionBase* temp = m_func;
			m_func = NULL;
			delete(temp);
		}
	}

	template<typename _FUNC_, typename..._ARGS_>
	int SetThreadFunc(_FUNC_ func, _ARGS_... args) {
		m_func = new CFunction<_FUNC_, _ARGS_...>(func, args...);
		return 0;
	}

	int Start() {
		pthread_attr_t attr;
		int ret = pthread_attr_init(&attr);
		if (ret != 0) return -1;
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		if (ret != 0) return -2;
		/*ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
		if (ret != 0) return -3;*/
		ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this);
		if (ret != 0) return -4;
		m_mapThread[m_thread] = this;
		ret = pthread_attr_destroy(&attr);
		if (ret != 0) return -5; 
		return 0;
	}

	int Stop() {
		if (m_thread != 0) {  
			pthread_t thread = m_thread; 
			m_thread = 0;
			timespec tv;
			tv.tv_sec = 0;
			tv.tv_nsec = 100 * 1000000;	//100ms
			 
			int ret = pthread_timedjoin_np(thread, NULL, &tv);
			if (ret == ETIMEDOUT) {
				ret = pthread_detach(thread);
				printf("%s<%d>:%s ret:%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
				ret = pthread_kill(thread, SIGUSR2);
				printf("%s<%d>:%s ret:%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			}
			//if (m_thread != 0) m_thread = 0;
		}
		return 0;
	}

	int Pause() {
		if (m_thread == 0) return -1;
		if (m_bPause) {
			m_bPause = false;
			return 0;
		}
		m_bPause = true;
		int ret = pthread_kill(m_thread, SIGUSR1);
		if (ret != 0) {
			m_bPause = false;
			return -2;
		}
		return 0;
	}

	bool isValid()const { return m_thread != 0; }

public:
	CThread(const CThread&) = delete;
	CThread& operator=(const CThread&) = delete;

private:
	static void* ThreadEntry(void* arg) {
		CThread* thiz = (CThread*)arg;

		struct sigaction act;
		bzero(&act, sizeof(act));
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = &CThread::Sigaction;
		sigaction(SIGUSR1, &act, NULL);
		sigaction(SIGUSR2, &act, NULL);

		thiz->EnterThread();
		if(thiz->m_thread!=0)thiz->m_thread = 0;
		pthread_t thread = pthread_self();
		auto it = m_mapThread.find(thread);
		if (it != m_mapThread.end()) {
			m_mapThread[thread] = NULL;
		}

		pthread_detach(thread);
		pthread_exit(NULL);
	}

	void EnterThread() {
		if (m_func != NULL) {
			int ret = (*m_func)();
			if (ret != 0)
				printf("%s(%d):[%s] ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
		}
	}

	static void Sigaction(int signum) {
		auto it = m_mapThread.find(pthread_self());
		if (signum == SIGUSR1) {
			if (it != m_mapThread.end()) {
				if (it->second != NULL) {
					while (it->second->m_bPause) {
						if (it->second->m_thread == 0)
							pthread_exit(NULL);
						usleep(1000);	//ÐÝÃß1ms
					}
				}
			}
		}
		else if(signum==SIGUSR2)
		{
			if (it != m_mapThread.end())
				m_mapThread[pthread_self()] = NULL;
			pthread_exit(NULL);
		}
	}

private:
	CFunctionBase* m_func;
	pthread_t m_thread;
	bool m_bPause;
	static std::map<pthread_t, CThread*> m_mapThread;
};