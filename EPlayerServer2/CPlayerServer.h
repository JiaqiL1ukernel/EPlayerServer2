#pragma once
#include "Server.h"
#include <map>
#include "HttpParser.h"
#include "MysqlClient.h"
#include "Crypto.h"
#include "jsoncpp/json.h"
#define CHECK_RETURN(ret,err) if(ret!=0){TRACEW("ret:%d errno:%d errmsg:%s\n", ret, errno, strerror(errno));return err;}
#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret:%d errno:%d errmsg:%s\n", ret, errno, strerror(errno));continue;}

DECLARE_TABLE_CLASS(edoyunLogin_user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")  //QQ号
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, DEFAULT, "VARCHAR", "(11)", "'18888888888'", "")  //手机
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")    //姓名
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")    //昵称
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat_id, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_address, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_province, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_country, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_age, DEFAULT | CHECK, "INTEGER", "", "18", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_male, DEFAULT, "BOOL", "", "1", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_flags, DEFAULT, "TEXT", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_experience, DEFAULT, "REAL", "", "0.0", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_level, DEFAULT | CHECK, "INTEGER", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_class_priority, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_time_per_viewer, DEFAULT, "REAL", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_career, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_password, NOT_NULL, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_birthday, NONE, "DATETIME", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_describe, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_education, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_register_time, DEFAULT, "DATETIME", "", "LOCALTIME()", "")
DECLARE_TABLE_CLASS_EDN()


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
		if (m_db) {
			CDatabaseClient* temp = m_db;
			m_db->Close();
			m_db = NULL;

			delete temp;
		}
	}
	virtual int BusinessProcess(CProccess* proc) {
		int ret = 0;
		m_db = new CMysqlClient();
		if (m_db == NULL) {
			TRACEE("no more memory!");
			return -1;
		}
		KeyValue args;
		args["host"] = "192.168.1.100";
		args["user"] = "root";
		args["password"] = "123456";
		args["port"] = 3306;
		args["db"] = "edoyun";
		ret = m_db->Connect(args);
		CHECK_RETURN(ret, -2);

		edoyunLogin_user_mysql user;
		ret = m_db->Exec(user.Create());
		CHECK_RETURN(ret, -3);

		ret = m_epoll.Create((unsigned)m_count);
		CHECK_RETURN(ret, -4);

		ret = SetConnectedCallBack(&CPlayerServer::Connected, this, std::placeholders::_1);
		CHECK_RETURN(ret,-5)

		ret = SetRecvCallBack(&CPlayerServer::Recieved, this, std::placeholders::_1,std::placeholders::_2);
		CHECK_RETURN(ret, -6)
		ret = m_pool.Start((unsigned)m_count);
		CHECK_RETURN(ret, -7);

		for (size_t i = 0; i < m_count; ++i) {
			ret = m_pool.AddTask(&CPlayerServer::ThreadFunc, this);
			CHECK_RETURN(ret, -8);
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
		sockaddr_in* addr = (sockaddr_in*)*pClient;
		TRACEI("client connect addr:%s port:%d", inet_ntoa(addr->sin_addr),ntohs(addr->sin_port));
		return 0;
	}

	int Recieved(CSocketBase* pClient, const Buffer& data) {
		
		int ret = 0;
		ret = HttpParser(data);
		//验证结果的反馈
		Buffer response = MakeResponse(ret);
		ret = pClient->Send(response);
		if (ret != 0) {
			TRACEE("http response failed!:%d rep-content:[%s]", ret,(char*)response);
		}
		else {
			TRACEI("http response success!");
		}
		

		return 0;
	}

	int HttpParser(const Buffer& data) {
		//HTTP解析
		CHttpParser httpParser;
		size_t size = httpParser.Parser(data);
		if (size == 0||httpParser.Errno()!=0) {
			TRACEE("size:%llu errno:%u",size,httpParser.Errno());
			return -1;
		}
		if (httpParser.Method() == HTTP_GET) {
			UrlParser url("192.168.3.9" + httpParser.Url());
			int ret = url.Parser();
			if (ret != 0) {
				TRACEE("ret:%d", ret);
				return -2;
			}
			Buffer uri = url.Uri();
			if (uri == "login") {
				Buffer user = url["user"];
				Buffer salt = url["salt"];
				Buffer time = url["time"];
				Buffer sign = url["sign"];
				TRACEI("time %s salt %s user %s sign %s",
					(char*)time, (char*)salt, (char*)user, (char*)sign);
				//数据库查询
				edoyunLogin_user_mysql userTable;
				Result res;
				Buffer sql = "username=\"" + user + "\"";
				Buffer query = userTable.Query(sql);
				ret = m_db->Exec(query, res, userTable);
				if (ret != 0) {
					TRACEE("sql:%s ret:%d", sql, ret);
					return -3;
				}
				if (res.size() == 0) {
					TRACEE("exec:%s result size:0", query);
					return -4;
				}
				else if (res.size() != 1) {
					TRACEE("exec:%s result size:%d>1", query,res.size());
					return -5;
				}
				auto user1 = res.front();
				Buffer password = *user1->Fields["user_password"]->Value.String;
				TRACEI("password:%s", (char*)password);
			
				//登录请求的验证
				const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
				Buffer correctSign = Crypto::MD5(time+MD5_KEY+password+salt);
				TRACEI("md5:%s", correctSign);
				if (sign == correctSign) {
					return 0;
				}
				else return -6;
			}
		}
		else if(httpParser.Method()==HTTP_POST)
		{
			 
		}
		return -7;
	}

	Buffer MakeResponse(int& ret) {
		Json::Value root;
		root["status"] = ret;
		if (ret != 0) {
			root["message"] = "登陆失败,可能是用户名或密码错误！";
		}
		else
		{
			root["message"] = "success";
		}
		Buffer json = root.toStyledString();
		Buffer Date = "Date: ";
		time_t t;
		time(&t);
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a %d %b %G %T GMT\r\n", ptm);
		Date += temp;

		Buffer Server = "Server: Edoyun/1.0\r\nContent-Type: text/html; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		sprintf(temp, "%d", json.size());
		Buffer Length = Buffer("Content-Length: ") + temp+"\r\n";
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";
		Buffer res = Date + Server + Length + Stub + json;
		TRACEI("response: %s", (char*)res);
		return res;
	}

private:
	CDatabaseClient* m_db;
	std::map<int, CSocketBase*> m_mapClients;
	CThreadPool m_pool;
	CEpoll m_epoll;
	size_t m_count;
};