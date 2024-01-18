#pragma once
#include "Epoll.h"
#include "Socket.h"
#include "Thread.h"
#include <sys/timeb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sstream>

enum LOGINFO {
	LOG_INFO,
	LOG_DEBUG,
	LOG_WARNING,
	LOG_ERROR,
	LOG_FATAL
};

//class CLoggerServer;

class LogInfo
{
public:
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int logLevel,
		const char* format, ...);
	

	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int logLevel);
	

	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int logLevel,
		void* pData, size_t nSize);
	

	~LogInfo();
	
	operator Buffer() const { return m_buffer; }

	template<typename T>
	LogInfo& operator<<(const T& data) {
		std::stringstream stream;
		stream << data;
		//printf("%s(%d):[%s][%s]\n", __FILE__, __LINE__, __FUNCTION__, stream.str().c_str());
		m_buffer += stream.str().c_str();
		//printf("%s(%d):[%s][%s]\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_buffer);
		return *this;
	}
private:
	Buffer m_buffer;
	bool m_bAuto;
};


class CLoggerServer
{
public:
	CLoggerServer():m_thread(&CLoggerServer::ThreadFunc,this) {
		m_server = NULL;
		m_path = "./log/" + GetTimeStr() + ".log";
		printf("%s(%d): [%s] path=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_path);
	}
	~CLoggerServer() {
		Close();
	}
public:
	CLoggerServer(const CLoggerServer&) = delete;
	CLoggerServer& operator=(const CLoggerServer&) = delete;
public:
	int Start() {
		if (m_server != NULL) return -1;
		m_server = new CSocket();

		if (access("log", W_OK | R_OK) != 0) {
			mkdir("log", S_IRUSR | S_IWUSR |S_IXUSR|
				S_IRGRP | S_IWGRP | S_IXGRP| 
				S_IROTH);
		}
		m_file = fopen(m_path, "w+");
		if (m_file == NULL) return -2;

		if (m_server == NULL)
			return -3;
		int ret = m_server->Init(CSockParam("./log/server.sock", SOCK_ISSERVER));
		if (ret != 0) {
#ifdef _DEBUG
			printf("%s(%d): [%s] ret:%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif // _DEBUG
			Close();
			return -4;
		}
		ret = m_epoll.Create(1);
		if (ret != 0) {
			Close();
			return -5;
		}

		ret = m_epoll.Add(*m_server, EpollData(m_server),EPOLLIN|EPOLLERR);
		if (ret != 0) {
			Close();
			return -5;
		}

		ret = m_thread.Start();
		if (ret != 0) {
#ifdef _DEBUG
			printf("%s(%d): [%s] ret:%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif // _DEBUG
			return -6;
		}
		return 0;
	}
	
	int Close() {
		if (m_server != NULL) {
			CSocketBase* temp = m_server;
			m_server = NULL;
			delete temp;
		}
		m_epoll.Close();
		m_thread.Stop();
		return 0;
	}
public:
	static void TRACE(const LogInfo& info) {
		static thread_local CSocket client;
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam("./log/server.sock", 0));
			if (ret != 0) {
#ifdef _DEBUG
				printf("%s(%d): [%s] ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif 
				return;
			}
			ret = client.Link();
			printf("%s(%d): [%s] errno:%d strerr:%s\n", __FILE__, __LINE__, __FUNCTION__, errno,strerror(errno));
		}
			ret = client.Send(info);
			printf("%s(%d): [%s] ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	}

	static Buffer GetTimeStr() {
		timeb tmb;
		ftime(&tmb);
		tm* tim = localtime(&tmb.time);
		Buffer data(128);
		int size = snprintf(data, data.size(),
			"%04d-%02d-%02d %02d:%02d:%02d %03d",
			tim->tm_year + 1900, tim->tm_mon + 1, tim->tm_mday,
			tim->tm_hour, tim->tm_min, tim->tm_sec, tmb.millitm);
		data.resize(size);
		return data;
	}
private:
	void WriteLog(const Buffer& data) {
		if (m_file != NULL) {	
			fwrite((const char*)data, 1, data.size(), m_file);
			fflush(m_file);
#ifdef _DEBUG
			printf("%s(%d): [%s] data=%s", __FILE__, __LINE__, __FUNCTION__, (char*)data);
#endif
		}
	}

	int ThreadFunc() {
		EPEvents evs(MAX_EVENTS);
		std::map<int, CSocketBase*> mapClients;
		while ((m_epoll != -1) && (m_server != NULL) && (m_thread.isValid())) {
			int ret = m_epoll.WaitEvent(evs, 1);
			if (ret < 0)break;
			if (ret > 0) {
				int i = 0;
				for (; i < ret; ++i) {
					if (evs[i].events & EPOLLERR) {			//EPOLLERR
						break;
					} 
					if (evs[i].events & EPOLLIN) {
						if (evs[i].data.ptr == m_server) {	//EPOLLIN服务端
							CSocketBase* pClient = NULL;
							int r = m_server->Link(&pClient);
							printf("%s(%d): [%s] r:%d\n", __FILE__, __LINE__, __FUNCTION__, r);

							if (r != 0) continue;
							r = m_epoll.Add(*pClient, EpollData(pClient), EPOLLIN | EPOLLERR);
							printf("%s(%d): [%s] r:%d\n", __FILE__, __LINE__, __FUNCTION__, r);

							if (r < 0) {
								delete pClient;
								continue; 
							}
							auto it = mapClients.find(*pClient);
							if (it != mapClients.end()) {
								if (it->second != NULL) {
									delete it->second;
								}
							}
							mapClients[*pClient] = pClient;

						}
						else {								//EPOLLIN客户端
							CSocketBase* pClient = (CSocketBase*)evs[i].data.ptr;
							if (pClient != NULL) {
								Buffer buf(1024 * 1024);
								int r = pClient->Recv(buf);
								printf("%s(%d): [%s] r:%d\n", __FILE__, __LINE__, __FUNCTION__, r);
								printf("%s(%d): [%s] buf:%s\n", __FILE__, __LINE__, __FUNCTION__, buf);

								if (r <= 0) {
									mapClients[*pClient] = NULL; 
									m_epoll.Del(*pClient);
									delete pClient;
								}
								else {
									WriteLog(buf);
								}
							}

						}
					}
				}
				if (i != ret)
					break;
			}
		}
		for (auto it = mapClients.begin(); it != mapClients.end(); ++it) {
			if (it->second != NULL)
				delete it->second;
		}
		mapClients.clear();

		return 0;
	}
private:
	CThread m_thread;
	CEpoll m_epoll;
	CSocketBase* m_server;
	Buffer m_path;
	FILE* m_file;
};

#ifndef TRACE
#define TRACEI(...) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_INFO,__VA_ARGS__))
#define TRACED(...) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_DEBUG,__VA_ARGS__))
#define TRACEW(...) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_WARNING,__VA_ARGS__))
#define TRACEE(...) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_ERROR,__VA_ARGS__))
#define TRACEF(...) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_FATAL,__VA_ARGS__))

//LogInfo<<"hello"<<"nihao"
#define LOGI LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_INFO)
#define LOGD LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_DEBUG)
#define LOGW LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_WARNING)
#define LOGE LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_ERROR)
#define LOGF LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_FATAL)

//内存导出
#define DUMPI(data,size) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_INFO,data,size)) 
#define DUMPD(data,size) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_DEBUG,data,size)) 
#define DUMPW(data,size) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_WARNING,data,size)) 
#define DUMPE(data,size) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_ERROR,data,size)) 
#define DUMPF(data,size) CLoggerServer::TRACE(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_FATAL,data,size)) 

#endif // !TRACE
