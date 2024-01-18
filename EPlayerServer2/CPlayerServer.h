#pragma once
#include "Server.h"
#include <map>

#define CHECK_RETURN(ret,err) if(ret!=0){TRACEW("ret:%d errno:%d errmsg:%s\n", ret, errno, strerror(errno));return err;}
#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret:%d errno:%d errmsg:%s\n", ret, errno, strerror(errno));continue;}




class CPlayerServer :public CBusiness
{
public:
	CPlayerServer(size_t count) :CBusiness() {
		m_count = count;
	}

	~CPlayerServer() {
		m_epoll.Close();
		m_pool.Close();
		for (auto it : m_mapClients) {
			if (it.second)
				delete it.second;
		}
		m_mapClients.clear();
	}
	virtual int BusinessProcess(CProccess* proc) {
		int ret = m_epoll.Create((unsigned)m_count);
		CHECK_RETURN(ret, -1);

		ret = SetConnectedCallBack(&CPlayerServer::Connected, this, std::placeholders::_1);
		CHECK_RETURN(ret,-2)

		ret = SetRecvCallBack(&CPlayerServer::Recieved, this, std::placeholders::_1,std::placeholders::_2);
		CHECK_RETURN(ret, -3)
		ret = m_pool.Start((unsigned)m_count);
		CHECK_RETURN(ret, -4);

		for (size_t i = 0; i < m_count; ++i) {
			ret = m_pool.AddTask(&CPlayerServer::ThreadFunc, this);
			CHECK_RETURN(ret, -5);
		}
		while (1) {
			int fd = -1;
			sockaddr_in addr_in;
			ret = proc->RecvSock(fd,&addr_in);
			if(ret < 0 || fd==-1) break;
			CSocketBase* pClient = new CSocket(fd);
			if (pClient == NULL) continue;
			ret = pClient->Init(CSockParam(&addr_in,SOCK_ISIP));
			WARN_CONTINUE(ret);
			ret = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR);
			WARN_CONTINUE(ret);
			if (m_connectCallBack)
				(*m_connectCallBack)(pClient);
		}
		return 0;
	}


private:
	int ThreadFunc() {
		int ret = 0;
		size_t size = 0;
		while (m_epoll != -1) {
			EPEvents events;
			size = m_epoll.WaitEvent(events);
			if(size<0) break;
			if (size > 0) {
				for (size_t i = 0; i < size; ++i) {
					if (events[i].events & EPOLLERR)break;
					if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
						if (pClient) {
							Buffer data;
							ret = pClient->Recv(data);
							WARN_CONTINUE(ret);
							if (m_recvCallBack) {
								(*m_recvCallBack)(pClient,data);
							}
						}
					}
				}
			}
		
		}
		return 0;
	}

	int Connected(CSocketBase* pClient) {

		return 0;
	}

	int Recieved(CSocketBase* pClient, const Buffer& data) {
		return 0;
	}

private:
	std::map<int, CSocketBase*> m_mapClients;
	CThreadPool m_pool;
	CEpoll m_epoll;
	size_t m_count;
};