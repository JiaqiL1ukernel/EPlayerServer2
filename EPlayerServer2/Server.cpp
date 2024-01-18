#include "Server.h"

int CServer::Init(CBusiness* business, const Buffer& ip, short port)
{
    if (business == NULL) return -1;
    m_business = business;
    int ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business,&m_process);
	if (ret != 0) return -2;
    ret = m_process.CreateSubProcess();
	if (ret != 0) return -3;

    m_server = new CSocket;
    if (m_server == NULL)
        return -4;
    ret = m_server->Init(CSockParam(ip, port, SOCK_ISIP | SOCK_ISSERVER));
	if (ret != 0) return -5;
    ret = m_pool.Start(2);
	if (ret != 0) return -6;
    ret = m_epoll.Create(2);
	if (ret != 0) return -7;
    ret = m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR);
	if (ret != 0) return -8;

    for (unsigned i = 0; i < m_pool.Size(); ++i) {
        ret = m_pool.AddTask(&CServer::ThreadFunc,this);
        if (ret != 0) return -9;
    }
    return 0;
}

int CServer::Start()
{
    while (m_server!=NULL) {
        usleep(10);
    }
    return 0;
}

int CServer::Close()
{
    if (m_server) {
        CSocketBase* temp = m_server;
        m_server = NULL;
        m_epoll.Del(*temp);
        delete temp;
    }
    m_epoll.Close();
    m_pool.Close();
    m_process.SendFD(-1);

    return 0;
}

int CServer::ThreadFunc()
{

    while ((m_server!=NULL)&&(m_epoll!=-1))
    {
        EPEvents events;
        int size = m_epoll.WaitEvent(events);
        int ret = 0;
        if(size < 0) break;
        if (size > 0) {
            for (int i = 0; i < size; ++i) {
                if(events[i].events&EPOLLERR)
                    break;
                if (events[i].events &EPOLLIN) {
                    CSocketBase* pClient = NULL;
                    ret = m_server->Link(&pClient);
                    if(ret!=0) continue;
                    if (pClient != NULL) {
                        ret = m_process.SendSock(*pClient,(sockaddr_in*)*pClient);
                        delete pClient;
                        if (ret != 0) {
                            TRACEE("send client %d failed!\n",(int)*pClient);
                            continue;
                        }
                    }
                }
            }
        }
    }

    return 0;
}
