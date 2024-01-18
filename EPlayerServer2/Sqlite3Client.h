#pragma once
#include "DatabaseHelper.h"
#include "Public.h"
#include "sqlite3/sqlite3.h"

class CSqlite3Client:public CDatabaseClient
{
public:
	CSqlite3Client(const CSqlite3Client&) = delete;
	CSqlite3Client& operator=(const CSqlite3Client&) = delete;
public:
	CSqlite3Client() {
		m_db = NULL;
		m_stmt = NULL;
	}
	virtual ~CSqlite3Client() {
		Close();
	}
public:
	//连接
	virtual int Connect(const KeyValue& args);
	//执行
	virtual int Exec(const Buffer& sql);
	//带结果的执行
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table);
	//开启事务
	virtual int StartTransaction();
	//提交事务
	virtual int CommitTransaction();
	//回滚事务
	virtual int RollbackTransaction();
	//关闭连接
	virtual int Close();
	//是否连接
	virtual bool IsConnected()const;
private:
	static int ExecCallback(void* arg, int count, char** names, char** values);
	int ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values);
private:
	sqlite3_stmt* m_stmt;
	sqlite3* m_db;
private:
	class ExecParam {
	public:
		ExecParam(CSqlite3Client* obj, Result& result, const _Table_& table)
			:obj(obj), result(result), table(table)
		{}
		CSqlite3Client* obj;
		Result& result;
		const _Table_& table;
	};
};

class _sqlite3_table_ :
	public _Table_
{
public:
	_sqlite3_table_() :_Table_() {}
	_sqlite3_table_(const _sqlite3_table_& table);
	virtual ~_sqlite3_table_() {}
	//返回创建的SQL语句
	virtual Buffer Create();
	//删除表
	virtual Buffer Drop();
	//增删改查
	//TODO:参数进行优化
	virtual Buffer Insert(const _Table_& values);
	virtual Buffer Delete(const _Table_& values);
	//TODO:参数进行优化
	virtual Buffer Modify(const _Table_& values);
	virtual Buffer Query();
	//创建一个基于表的对象
	virtual PTable Copy()const;
	virtual void ClearFieldUsed();

public:
	//获取表的全名
	virtual operator const Buffer() const;
};



class _sqlite3_field_ :
	public _Field_
{
public:
	virtual Buffer Create();
	virtual void LoadFromStr(const Buffer& str);
	//where 语句使用的
	virtual Buffer toEqualExp() const;
	virtual Buffer toSqlStr() const;
	//列的全名
	virtual operator const Buffer() const;
private:
	union Value {
		int Integer;
		double Double;
		Buffer String;
		bool Bool;
	};
	SqlType nType;
};

