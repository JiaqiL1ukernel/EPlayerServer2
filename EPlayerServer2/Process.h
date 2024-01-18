#pragma once
#include "Function.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>

class CProccess
{
public:
	CProccess() {
		m_func = NULL;
		memset(pipe, 0, sizeof(pipe));
	}
	template<typename _FUNC_, typename... _ARG_>
	int SetEntryFunction(_FUNC_ func, _ARG_... args)
	{
		m_func = new CFunction<_FUNC_, _ARG_...>(func, args...);
		return 0;
	}

	~CProccess()
	{
		delete m_func;
		m_func = NULL;
	}

	int CreateSubProcess()
	{
		if (m_func == NULL) return -1;
		int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipe);
		if (ret == -1)
			return -2;
		pid_t pid = fork();
		if (pid == -1)
			return -3;
		if (pid == 0)
		{
			close(pipe[1]);  //子进程关闭写端
			ret = (*m_func)();
			exit(0);
		}
		close(pipe[0]); //  主进程关闭读端
		m_pid = pid;
		return 0;
	}

	int SendFD(int fd)
	{
		struct msghdr msg;
		iovec io[2];
		char buf[2][10] = { "edoyun" ,"jueding" };
		io[0].iov_base = buf[0];
		io[0].iov_len = sizeof(buf[0]);
		io[1].iov_base = buf[1];
		io[1].iov_len = sizeof(buf[0]);
		msg.msg_iov = io;
		msg.msg_iovlen = 2;

		struct cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int*)CMSG_DATA(cmsg) = fd;

		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;
		int ret = (int)sendmsg(pipe[1], &msg, 0);
		free(cmsg);
		if (ret == -1) {
			return -2;
		}
		return 0;
	}

	int SendSock(int fd, sockaddr_in* addr)
	{
		struct msghdr msg;
		iovec io[2];
		char buf[2][10] = { "edoyun" ,"jueding" };
		io[0].iov_base = addr;
		io[0].iov_len = sizeof(sockaddr_in);
		io[1].iov_base = buf[1];
		io[1].iov_len = sizeof(buf[0]);
		msg.msg_iov = io;
		msg.msg_iovlen = 2;

		struct cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int*)CMSG_DATA(cmsg) = fd;

		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;
		int ret = (int)sendmsg(pipe[1], &msg, 0);
		free(cmsg);
		if (ret == -1) {
			return -2;
		}
		return 0;
	}

	int RecvFD(int& fd)
	{
		struct msghdr msg;
		iovec io[2];
		char buf[][10] = { "","" };
		io[0].iov_base = buf[0];
		io[0].iov_len = sizeof(buf[0]);
		io[1].iov_base = buf[1];
		io[1].iov_len = sizeof(buf[1]);

		msg.msg_iov = io;
		msg.msg_iovlen = 2;

		struct cmsghdr* cmsg = (struct cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;
		int ret = (int)recvmsg(pipe[0], &msg, 0);
		if (ret == -1) {
			free(cmsg);
			return -2;
		}
		fd = *(int*)CMSG_DATA(cmsg);
		free(cmsg);

		return 0;
	}

	int RecvSock(int& fd, sockaddr_in* addr)
	{
		struct msghdr msg;
		iovec io[2];
		char buf[][10] = { "","" };
		io[0].iov_base = addr;
		io[0].iov_len = sizeof(sockaddr_in);
		io[1].iov_base = buf[1];
		io[1].iov_len = sizeof(buf[1]);

		msg.msg_iov = io;
		msg.msg_iovlen = 2;

		struct cmsghdr* cmsg = (struct cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;
		int ret = (int)recvmsg(pipe[0], &msg, 0);
		if (ret == -1) {
			free(cmsg);
			return -2;
		}
		fd = *(int*)CMSG_DATA(cmsg);
		free(cmsg);

		return 0;
	}

	static int SwitchDeamon()
	{
		int fd = fork();
		if (fd > 0) exit(0);
		int sid = setsid();
		if (sid == -1) return -1;
		fd = fork();
		if (fd > 0) exit(0);
		int ret = chdir("/home/book");
		if (ret == -1)
			return -2;
		for (int i = 0; i < 3; i++) close(i);
		umask(0);
		signal(SIGCHLD, SIG_IGN);
		return 0;
	}

private:
	CFunctionBase* m_func;
	pid_t m_pid;
	int pipe[2];
};