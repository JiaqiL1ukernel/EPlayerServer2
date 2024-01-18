#pragma once
#include "ThreadPool.h"
#include "Thread.h"
#include "Epoll.h"
#include "Process.h"
#include "Socket.h"
#include "Logger.h"
#include "Function.h"

template<typename _FUNC_, typename... _ARG_>
class CConnectFunction :public CFunctionBase
{
public:
	CConnectFunction(_FUNC_ func, _ARG_... args) :m_binder(std::forward<_FUNC_>(func), std::forward<_ARG_>(args)...) {}

	virtual ~CConnectFunction() {}

	virtual int operator()(CSocketBase* pClient)
	{
		return m_binder(pClient);
	}

	typename std::_Bindres_helper<int, _FUNC_, _ARG_...>::type m_binder;

};

template<typename _FUNC_, typename... _ARG_>
class CRecvFunction :public CFunctionBase
{
public:
	CRecvFunction(_FUNC_ func, _ARG_... args) :m_binder(std::forward<_FUNC_>(func), std::forward<_ARG_>(args)...) {}

	virtual ~CRecvFunction() {}

	virtual int operator()(CSocketBase* pClient, const Buffer& data)
	{
		return m_binder(pClient, data);
	}

	typename std::_Bindres_helper<int, _FUNC_, _ARG_...>::type m_binder;

};

class CBusiness {
public:
	CBusiness() :m_connectCallBack(NULL), m_recvCallBack(NULL) {}

	virtual int BusinessProcess(CProccess* proc) = 0;

	template<typename _FUNC_, typename... _ARGS_>
	int SetConnectedCallBack(_FUNC_ func, _ARGS_... args) {
		m_connectCallBack = new CConnectFunction<_FUNC_, _ARGS_...>(func, args...);
		if (m_connectCallBack == NULL) return -1;
		return 0;
	}

	template<typename _FUNC_, typename... _ARGS_>
	int SetRecvCallBack(_FUNC_ func, _ARGS_... args) {
		m_recvCallBack = new CRecvFunction<_FUNC_,_ARGS_...>(func, args...);
		if (m_recvCallBack == NULL) return -1;
		return 0;
	}

protected:
	CFunctionBase* m_connectCallBack;
	CFunctionBase* m_recvCallBack;
};


class CServer
{
public:
	CServer() {
		m_server = NULL;
		m_business = NULL;
	}
	~CServer() {
		Close();
	}
public:
	int Init(CBusiness* business,const Buffer& ip = "127.0.0.1",short port=9999);
	int Start();
	int Close();
private:
	int ThreadFunc();
private:
	CThreadPool m_pool;
	CSocketBase* m_server;
	CEpoll m_epoll;
	CProccess m_process;
	CBusiness* m_business;
};

