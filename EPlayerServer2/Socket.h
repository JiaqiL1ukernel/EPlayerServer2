#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fcntl.h>
#include "Public.h"

enum SockAddr
{
	SOCK_ISSERVER=1,//是否是服务器 1表示是 0表示客户端
	SOCK_ISNONBLOCK=2, //是否阻塞 1表示不阻塞 0表示阻塞
	SOCK_ISUDP=4,	//是否为UDP 1表示UDP 0表示TCP
	SOCK_ISIP=8		//是否为网络套接字 1表示网络套接字 0表示本地套接字
};

class CSockParam
{
public:
	CSockParam() {
		bzero(&addr_in, sizeof(addr_in));
		bzero(&addr_un, sizeof(addr_un));
		port = -1;
		attr = 0;
	}
	CSockParam(const Buffer& ip, short port, int attr)
	{
		this->IP = ip;
		this->port = port;
		this->attr = attr;
		bzero(&addr_in, sizeof(addr_in));
		addr_in.sin_family = AF_INET;
		addr_in.sin_addr.s_addr = inet_addr(IP);
		addr_in.sin_port = htons(port);
	}
	CSockParam(const sockaddr_in* addr, int attr)
	{
		this->IP = inet_ntoa(addr->sin_addr);
		this->port = ntohs(addr->sin_port);
		this->attr = attr;
		memcpy(&addr_in, addr, sizeof(addr_in));
	}

	CSockParam(const Buffer& path, int attr) {
		this->IP = path;
		this->attr = attr;
		strcpy(addr_un.sun_path, path);
		addr_un.sun_family = AF_LOCAL;
	}
	~CSockParam() {}
	CSockParam(const CSockParam& param) {
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
		memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
		port = param.port;
		IP = param.IP;
		attr = param.attr;
	}
	CSockParam& operator=(const CSockParam& param) {
		if (this != &param) {
			memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
			memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
			port = param.port;
			IP = param.IP;
			attr = param.attr;
		}
		return *this;
	}

	sockaddr* addrin()const{ return (sockaddr*)&addr_in; }
	sockaddr* addrun()const{ return (sockaddr*)&addr_un; }
	
public:
	//地址
	sockaddr_in addr_in;
	sockaddr_un addr_un;
	//IP
	Buffer IP;
	//端口
	short port;
	//属性 参考SockAddr
	int attr;
};

class CSocketBase
{
public:
	virtual ~CSocketBase() {
		Close();
	}
	CSocketBase() {
		m_socket = -1;
		m_status = 0;
	}
public:
	//初始化
	virtual int Init(const CSockParam& param) = 0;
	//连接
	virtual int Link(CSocketBase** pClient = NULL) = 0;
	//发送数据
	virtual int Send(const Buffer& buf) = 0;
	//接收数据
	virtual int Recv(Buffer& buf) = 0;
	//关闭
	virtual int Close()
	{
		m_status = 3;
		if (m_socket != -1) {
			int fd = m_socket;
			m_socket = -1;
			close(fd);
			if((m_param.attr==SOCK_ISSERVER)&&((m_param.attr&SOCK_ISIP)==0))
				unlink(m_param.IP);
		}
		return 0;
	}

	virtual operator int() {
		return m_socket;
	}

	virtual operator int()const {
		return m_socket;
	}

	virtual operator sockaddr_in* (){
		return &m_param.addr_in;
	}

	virtual operator const sockaddr_in* ()const {
		return &m_param.addr_in;
	}

protected:
	
	//套接字描述符 默认是-1
	int m_socket;	
	//状态 0未初始化 1初始化完成 2连接完成 3已经关闭 
	int m_status;
	CSockParam m_param;

};

class CSocket :public CSocketBase
{
public:
	CSocket() :CSocketBase(){}

	CSocket(int fd) {
		m_socket = fd;
		m_status = 2;
	}
	virtual ~CSocket() {
		Close();
	}

public:
	//初始化
	virtual int Init(const CSockParam& param) {
		
		if (m_status != 0 && m_socket==-1)return -1;
		m_param = param;
		int type = (m_param.attr & SOCK_ISUDP) ?  SOCK_DGRAM: SOCK_STREAM;
		if (m_socket == -1) {
			if(m_param.attr&SOCK_ISIP)
				m_socket = socket(PF_INET, type, 0);
			else
				m_socket = socket(PF_LOCAL, type, 0);
		}
		else
			m_status = 2;
		if (m_socket == -1)return -2;
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) {
			if ((m_param.attr & SOCK_ISIP) == 0)
				ret = bind(m_socket, m_param.addrun(), sizeof(m_param.addr_un));
			else
				ret = bind(m_socket, m_param.addrin(), sizeof(m_param.addr_in));
			if (ret == -1) {
				if (errno == EAGAIN)
					return -3;
			}
			ret = listen(m_socket, 32);
			if (ret == -1) {
				printf("%s<%d>:[%s] pid=%d errno:%d msg:%s ret:%d\n", __FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
				return -4;
			} 
		}
		if (m_param.attr & SOCK_ISNONBLOCK) {
			int option = fcntl(m_socket, F_GETFL);
			if (option == -1) return -5;
			option |= O_NONBLOCK;
			ret = fcntl(m_socket, F_SETFL, option);
			if (ret) return -6;
		}
		if(m_status==0)
			m_status = 1;
		return 0;
	}
	//连接
	virtual int Link(CSocketBase** pClient = NULL)
	{
		if (m_status <= 0 || m_socket == -1) return -1;
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) {
			//服务器
			CSockParam param;
			if (pClient == NULL) return -2;
			socklen_t len = 0;
			int fd = -1;
			if (m_param.attr & SOCK_ISIP) 
			{
				param.attr |= SOCK_ISIP;
				len = sizeof(sockaddr_in);
				bzero(&param.addr_in, sizeof(param.addr_in));
				fd = accept(m_socket, param.addrin(), &len);
			}
			else {
				len = sizeof(sockaddr_un);
				bzero(&param.addr_un, sizeof(param.addr_un));
				fd = accept(m_socket, param.addrun(), &len);
			}
			
			if (fd == -1) return -3;
			*pClient = new CSocket(fd);
			if (*pClient == NULL)return -4;
			
			ret = (*pClient)->Init(param);
			if (ret != 0) {
				printf("%s(%d): [%s] ret:%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
				delete(*pClient);
				*pClient = NULL;
				return -5;	
			}
		}
		else
		{//客户端
			if (m_param.attr & SOCK_ISIP) 
				ret = connect(m_socket, m_param.addrin(), sizeof(sockaddr_in));
			else{
				ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un));
				printf("%s(%d): [%s] ret:%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			}
			if (ret == -1)return -6;
		}
		m_status = 2;
		return 0;
	}
	//发送数据
	virtual int Send(const Buffer& buf)
	{
		if (m_status <= 1 || m_socket == -1) {

			printf("%s<%d>:%s m_status:%d m_socket:%d\n", __FILE__, __LINE__, __FUNCTION__, m_status,m_socket);
			return -1;
		}
		size_t size = buf.size();
		size_t index = 0;
		while (index < size) {
			size_t len = write(m_socket, (char*)buf + index, size - index);
			if (len == 0)return -2;
			if (len < 0)return -3;
			index += len;
		}
		return 0;
	}
	//接收数据
	virtual int Recv(Buffer& buf) 
	{
		if (m_status <= 1 || m_socket == -1) return -1;
		size_t size = buf.size();
		size_t len = read(m_socket, (char*)buf, size);
		if (len > 0) {
			buf.resize(len);
			return (int)len;
		}
		if (len < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				buf.clear();
				return 0;
			}
			return -2;
		}
		if (len == 0) {
			printf("%s(%d): [%s] errno:%d strerr:%s\n", __FILE__, __LINE__, __FUNCTION__, errno,strerror(errno));

			return -3;
		}
		
		return 0;
	}
	//关闭
	virtual int Close()
	{
		return CSocketBase::Close();
	}
};



