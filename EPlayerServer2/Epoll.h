#pragma once
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <errno.h>
#include <sys/signal.h>
#include <memory.h>
#define MAX_EVENTS 128
class EpollData
{
public:
	EpollData() { m_data.u64 = 0; }
	EpollData(void* pdata) { m_data.ptr = pdata; }
	explicit EpollData(int fd) { m_data.fd = fd; }
	explicit EpollData(uint32_t u) { m_data.u32 = u; }
	explicit EpollData(uint64_t u) { m_data.u64 = u; }
	EpollData(const EpollData& data) { m_data.u64 = data.m_data.u64; }
	EpollData& operator=(const EpollData& data) {
		if (this != &data) {
			m_data.u64 = data.m_data.u64;
		}
		return *this;
	}
	EpollData& operator=(void* pdata) {
		m_data.ptr = pdata;
		return *this;
	}
	EpollData& operator=(int fd) {
		m_data.fd = fd;
		return *this;
	}
	EpollData& operator=(uint32_t u) {
		m_data.u32 = u;
		return *this;
	}
	EpollData& operator=(uint64_t u) {
		m_data.u64 = u;
		return *this;
	}
public:
	operator epoll_data_t() { return m_data; }
	operator epoll_data_t() const{ return m_data; }
	operator epoll_data_t*() { return &m_data; }
	operator const epoll_data_t* () const{ return &m_data; }

private:
	epoll_data_t m_data;
};

using EPEvents = std::vector<epoll_event>;

class CEpoll
{

public:
	CEpoll()
	{
		m_epoll = -1;
	}
	~CEpoll()
	{
		Close();
	}
public:
	CEpoll(const CEpoll&) = delete;
	CEpoll& operator= (const CEpoll &) = delete;
public:
	operator int()const { return m_epoll; }
public:
	int Create(unsigned count)
	{
		if (m_epoll != -1) return -1;
		m_epoll = epoll_create(count);
		if (m_epoll == -1)
			return -2;
		return 0;
	}
	int WaitEvent(EPEvents& event, int timeout = 10)
	{	
		if (m_epoll == -1)return -1;
		EPEvents evs(MAX_EVENTS);
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout);
		if (ret == -1)
		{
			if ((errno == EINTR) || (errno == EAGAIN))return 0;
			return -2;
		}
		if (ret > (int)event.size()) {
			event.resize(ret);
		}
		memcpy(event.data(), evs.data(), ret * sizeof(epoll_event));
		return ret;
	}
	int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN)
	{
		if (m_epoll == -1)return -1;
		epoll_event event;
		event.data = data;
		event.events = events;
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &event);
		if (ret == -1)return-2;
		return 0;
	}
	int Modify(int fd, uint32_t events, const EpollData& data = EpollData((void*)0))
	{
		if (m_epoll == -1)return -1;
		epoll_event event;
		event.data = data;
		event.events = events;
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &event);
		if (ret == -1)return-2;
		return 0;
	}
	int Del(int fd)
	{
		if (m_epoll == -1)return -1;
		
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL);
		if (ret == -1)return-2;
		return 0;
	}
	void Close()
	{
		if (m_epoll != -1) {
			int fd = m_epoll;
			m_epoll = -1;
			close(fd);
		}
	}
private:
	int m_epoll;
};
