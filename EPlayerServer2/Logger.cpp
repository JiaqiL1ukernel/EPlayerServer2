#include "Logger.h"

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int logLevel, const char* format, ...)
{
	//头部 file(line):[level][time]<pid-tid>(func)
	const char sLevel[][8] = {
		"Info",
		"Debug",
		"Warning",
		"Error",
		"Fatal"
	};
	m_bAuto = false;
	char* buf = NULL;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[logLevel], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buffer = buf;
		free(buf);
	}
	else return;
	va_list ap;
	va_start(ap, format);
	count = vasprintf(&buf, format, ap);
	if (count > 0) {
		m_buffer += buf;
		free(buf);
	}
	else return;
	va_end(ap);
	m_buffer += "\n";
}

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int logLevel)
{
	m_bAuto = true;
	//头部 file(line):[level][time]<pid-tid>(func)
	const char sLevel[][8] = {
		"Info",
		"Debug",
		"Warning",
		"Error",
		"Fatal"
	};
	char* buf = NULL;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[logLevel], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buffer = buf;
		free(buf);
	}
	else return;
}

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int logLevel, void* pData, size_t nSize)
{
	m_bAuto = false;
	//头部 file(line):[level][time]<pid-tid>(func)
	const char sLevel[][8] = {
		"Info",
		"Debug",
		"Warning",
		"Error",
		"Fatal"
	};
	m_bAuto = false;
	char* buf = NULL;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)\n",
		file, line, sLevel[logLevel], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buffer = buf;
		free(buf);
	}
	else return;

	char* data = (char*)pData;
	size_t i = 0;
	for (; i < nSize; ++i) {
		char buf[16] = "";
		snprintf(buf, sizeof(buf), "%02X ", data[i] & 0xFF);
		m_buffer += buf;
		if ((i+1) % 16 == 0) {
			m_buffer += "\t; ";
			for (size_t j = i - 15; j <= i; ++j) {
				if (((data[j] & 0xff) > 31) && ((data[j] & 0xff) < 0x7f)) {
					m_buffer += data[j];
				}
				else
					m_buffer += ".";
			}
			m_buffer += "\n";
		}
	}
	size_t k = i % 16;
	if (k != 0) {
		size_t j = 0;
		for (; j < 16 - k; ++j) {
			m_buffer += "   ";
		}
		m_buffer += "\t; ";
		for (j = i - k; j < i; ++j) {
			if (((data[j] & 0xff) > 31) && ((data[j] & 0xff) < 0x7f)) {
				m_buffer += data[j];
			}
		}
		m_buffer += "\n";
	}

}

LogInfo::~LogInfo()
{
	if (m_bAuto) {
		m_buffer += "\n";
		CLoggerServer::TRACE(*this);
	}
}
