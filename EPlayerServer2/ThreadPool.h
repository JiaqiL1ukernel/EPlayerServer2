#pragma once
#include "Thread.h"
#include "Socket.h"
#include "Epoll.h"
#include "Function.h"
#include <vector>


class CThreadPool
{
public:
	CThreadPool() {
		m_server = NULL;
		char *buf = NULL;
		timespec tp;
		clock_gettime(CLOCK_REALTIME, &tp);

		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000);
		if (buf != NULL) {
			m_path = buf;
			free(buf);
		}
		usleep(1);
	}
	~CThreadPool() {}
public:
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;

public:
	size_t Size()const { return m_threads.size(); }

	int Start(unsigned count) {
		if (m_server != NULL) return -1;
		if (m_path == NULL) return -2;
		m_server = new CSocket();
		if (m_server == NULL) return -3;
		int ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER));
		if (ret != 0)  return -4;
		ret = m_epoll.Create(count);
		if (ret != 0)return -5;
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR);
		if (ret != 0) return -6;
		
		m_threads.resize(count);
		for (unsigned i = 0; i < count; ++i) {
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch,this);
			if (m_threads[i] == NULL) return -7;
			ret = m_threads[i]->Start();
			if (ret != 0) return -8;
		}
		return 0;
	}
	void Close() {
		m_epoll.Close();
		if (m_server) {
			CSocketBase* temp = m_server;
			m_server = NULL;
			delete temp;
		}
		for (auto thread : m_threads) {
			if (thread) delete thread;
		}
		m_threads.clear();
		unlink(m_path);
	}
	template<typename _FUNC_,typename... _ARGS_>
	int AddTask(_FUNC_ func, _ARGS_... args) {
		static thread_local CSocket client;
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam(m_path, 0));
			if (ret != 0) return -1;
			ret = client.Link();
			if (ret != 0) return -2;
		}
		CFunctionBase* base = new CFunction<_FUNC_, _ARGS_...>(func, args...);
		if (base == NULL) return -3;
		Buffer data(sizeof(base));
		memcpy((char*)data, &base, sizeof(base));
		if (data == NULL) return -4;
		ret = client.Send(data);
		printf("%s<%d>:%s ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret != 0) {
			delete base;
			return -5;
		}
		return 0;
	}

private:
	int TaskDispatch() {
		while (m_epoll != -1) {
			EPEvents events;
			int ret = 0;
			size_t epSize = 0;
			epSize = m_epoll.WaitEvent(events);
			if (epSize > 0) {
				for (size_t i = 0; i < epSize; ++i) {
					if (events[i].events == EPOLLIN) {
						if (events[i].data.ptr == m_server) {	//客户端请求连接
							CSocketBase* client = NULL;
							ret = m_server->Link(&client);
							if (ret != 0) continue;
							ret = m_epoll.Add(*client, EpollData((void*)client), EPOLLIN | EPOLLERR);
							if (ret != 0) {
								delete client;
								continue;
							}
						}
						else { //客户端有数据发送
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient) {
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data);
								if (ret <= 0) {
									m_epoll.Del(*pClient);
									delete pClient;
									continue;
								}
								memcpy(&base, (char*)data, sizeof(base));
								if (base != NULL) {
									(*base)();
									delete base;
								}
							}
						}
					}
				}
			}
		}
		return 0;
	}
private:
	CEpoll m_epoll;
	std::vector<CThread*> m_threads;
	CSocketBase* m_server;
	Buffer m_path;
};
