#include "Sqlite3Client.h"
#include "Logger.h"

int CSqlite3Client::Connect(const KeyValue& args)
{
    auto it = args.find("host");
    if (it == args.end())  return -1;
    if (m_db != NULL) return -2;
    int ret = sqlite3_open(it->second, &m_db);
    if (ret != 0) {
        TRACEE("connect failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}

int CSqlite3Client::Exec(const Buffer& sql)
{
    if (m_db == NULL) return -1;
    int ret = sqlite3_exec(m_db, sql, NULL, this, NULL);
    if (ret != SQLITE_OK) {
        printf("sql:{%s}\n", (char*)sql);
		printf("exec failed:%d [%s]\n", ret, sqlite3_errmsg(m_db));
		return -2;
    }

    return 0;
}

int CSqlite3Client::Exec(const Buffer& sql, Result& result, const _Table_& table)
{
    char* errMsg = NULL;
    if (m_db == NULL) return -1;
    ExecParam param(this, result, table);
    int ret = sqlite3_exec(m_db, sql, &CSqlite3Client::ExecCallback,(void*)&param, &errMsg);
	if (ret != SQLITE_OK) {
		printf("sql:{%s}\n", (char*)sql);
		printf("exec failed:%d [%s]\n", ret, errMsg);
        if (errMsg) free(errMsg);
		return -2;
	}
	if (errMsg) free(errMsg);
    return 0;
}

int CSqlite3Client::StartTransaction()
{
	if (m_db == NULL) return -1;
    int ret = sqlite3_exec(m_db, "START TRANSACTION", NULL, NULL, NULL);
    if (ret != SQLITE_OK) {
		TRACEE("sql:{START TRANSACTION}");
		TRACEE("START TRANSACTION failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}

int CSqlite3Client::CommitTransaction()
{
	if (m_db == NULL) return -1;
	int ret = sqlite3_exec(m_db, "COMMIT TRANSACTION", NULL, NULL, NULL);
	if (ret != SQLITE_OK) {
		TRACEE("sql:{COMMIT TRANSACTION}");
		TRACEE("COMMIT TRANSACTION failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
    return 0;
}

int CSqlite3Client::RollbackTransaction()
{
	if (m_db == NULL) return -1;
	int ret = sqlite3_exec(m_db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
	if (ret != SQLITE_OK) {
		TRACEE("sql:{ROLLBACK TRANSACTION}");
		TRACEE("ROLLBACK TRANSACTION failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
    return 0;
}

int CSqlite3Client::Close()
{
	if (m_db == NULL) return -1;
    int ret = sqlite3_close(m_db);
	if (ret != SQLITE_OK) {
		TRACEE("close failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
    m_db = NULL;
    return 0;
}

bool CSqlite3Client::IsConnected() const
{
    return m_db!=NULL;
}


int CSqlite3Client::ExecCallback(void* arg, int count, char** values, char** names)
{
    ExecParam* param = (ExecParam*)arg;
    return param->obj->ExecCallback(param->result, param->table,
        count, values, names);
    
}

int CSqlite3Client::ExecCallback(Result& result, const _Table_& table, int count, char** values, char** names)
{
    PTable pTable = table.Copy();
    if (pTable == nullptr) {
        TRACEE("table %s error!", (const char*)(Buffer)table);
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        auto it = pTable->Fields.find(names[i]);
        if (it == pTable->Fields.end()) {
            TRACEE("table %s error!", (const char*)(Buffer)table);
            return -2;
        }
        if(values[i]!=NULL)
            it->second->LoadFromStr(values[i]);
    }
    result.push_back(pTable);
    return 0;
}

_sqlite3_table_::_sqlite3_table_(const _sqlite3_table_& table)
{
    Database = table.Database;
    Name = table.Name;
    for (size_t i = 0; i < table.FieldDefine.size(); ++i) {
        PField pField = PField(
            new _sqlite3_field_(*(_sqlite3_field_*)table.FieldDefine[i].get())
        );
        FieldDefine.push_back(pField);
        Fields[pField->Name] = pField;
        
    }
}

Buffer _sqlite3_table_::Create()
{
    Buffer sql = "CREATE TABLE IF NOT EXISTS " + (Buffer)*this + " (\r\n";
    for (size_t i = 0; i < FieldDefine.size(); ++i) {
        if (i > 0) sql += ",";
        sql += FieldDefine[i]->Create();
    }
    sql += ");";
    TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Drop()
{
    Buffer sql = "DROP TABLE " + (Buffer)*this+";";
	TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Insert(const _Table_& values)
{
    Buffer sql = "INSERT INTO " + (Buffer)*this + " (";
    Buffer sqlval = "VALUES(";
    bool isFirts = true;
    for (size_t i = 0; i < values.FieldDefine.size(); ++i) {
        if (values.FieldDefine[i]->Condition&SQL_INSERT) {
            if (!isFirts) {
                sql += ",";
                sqlval += ",";
            }
            else isFirts = false;
            sql += (Buffer)*values.FieldDefine[i];
            sqlval += values.FieldDefine[i]->toSqlStr();
        }
    }
    sql += ") ";
    sqlval += ");";
    sql += sqlval;
	TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Delete(const _Table_& values)
{
    Buffer sql = "DELETE FROM "+(Buffer)*this+" ";
    Buffer Where = "";
	bool isFirts = true;
	for (size_t i = 0; i < values.FieldDefine.size(); ++i) {
		if (values.FieldDefine[i]->Condition & SQL_CONDITION) {
			if (!isFirts) {
                Where += " AND ";
			}
			else isFirts = false;
			Where += (Buffer)*values.FieldDefine[i] +" = ";
			Where += values.FieldDefine[i]->toSqlStr();
		}
	}
    if (Where.size() > 0) {
        sql += "WHERE " + Where;
    }
    sql += ";";
	TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Modify(const _Table_& values)
{
	Buffer sql = "UPDATE " + (Buffer)*this + " SET ";
	Buffer Condition = "";
	bool isFirts = true;
	for (size_t i = 0; i < values.FieldDefine.size(); ++i) {
		if (values.FieldDefine[i]->Condition & SQL_MODIFY) {
			if (!isFirts) {
                Condition += ",";
			}
			else isFirts = false;
            Condition += (Buffer)*values.FieldDefine[i] + "=";
            Condition += values.FieldDefine[i]->toSqlStr();
		}
	}
    sql += Condition;


	Buffer Where = "";
	isFirts = true;
	for (size_t i = 0; i < values.FieldDefine.size(); ++i) {
		if (values.FieldDefine[i]->Condition & SQL_CONDITION) {
			if (!isFirts) {
				Where += " AND ";
			}
			else isFirts = false;
			Where += (Buffer)*values.FieldDefine[i] + " = ";
			Where += values.FieldDefine[i]->toSqlStr();
		}
	}
	if (Where.size() > 0) {
		sql += " WHERE " + Where;
	}
	sql += ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Query(const Buffer& condition)
{
    Buffer sql = "SELECT ";
    for (size_t i = 0; i < FieldDefine.size(); ++i) {
		if (i > 0) sql += ",";
		sql += (Buffer)*FieldDefine[i]+" ";
    }
    sql += " FROM " + (Buffer)*this;
	if (condition.size() > 0) {
		sql += " WHERE " + condition + ";";
	}
	TRACEI("sql = %s", (char*)sql);
    return sql;
}

PTable _sqlite3_table_::Copy() const
{
    return PTable(new _sqlite3_table_(*this));
}

void _sqlite3_table_::ClearFieldUsed()
{
    for (size_t i = 0; i < FieldDefine.size(); ++i) {
        FieldDefine[i]->Condition = 0;
    }
}

_sqlite3_table_::operator const Buffer() const
{
    Buffer Head;
    if (Database.size()>0) {
        Head = '"' + Database + "\".";
    }
    return Head + '"' + Name + '"';

}

_sqlite3_field_::_sqlite3_field_()
	:_Field_() {
	nType = TYPE_NULL;
	Value.Double = 0.0;
}

_sqlite3_field_::_sqlite3_field_(SqlType ntype, const Buffer& name, unsigned attr, const Buffer& type, const Buffer& size, const Buffer& default_, const Buffer& check)
{
	nType = ntype;
	switch (ntype)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.String = new Buffer();
		break;
	}

	Name = name;
	Attr = attr;
	Type = type;
	Size = size;
	Default = default_;
	Check = check;
}

_sqlite3_field_::_sqlite3_field_(const _sqlite3_field_& field)
{
	Name = field.Name;
	Type = field.Type;
	Size = field.Size;
	Attr = field.Attr;
	Default = field.Default;
	Check = field.Check;
	Condition = field.Condition;
	nType = field.nType;
	switch (nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.String = new Buffer();
		*Value.String = *field.Value.String;
		break;
	}
}

_sqlite3_field_::~_sqlite3_field_()
{
	switch (nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		if (Value.String) {
			Buffer* p = Value.String;
			Value.String = NULL;
			delete p;
		}
		break;
	}
}

Buffer _sqlite3_field_::Create()
{
	Buffer sql = '"' + Name + "\" " + Type + " ";
	if (Attr & NOT_NULL) {
		sql += " NOT NULL ";
	}
	if (Attr & DEFAULT) {
		sql += " DEFAULT " + Default + " ";
	}
	if (Attr & UNIQUE) {
		sql += " UNIQUE ";
	}
	if (Attr & PRIMARY_KEY) {
		sql += " PRIMARY KEY ";
	}
	if (Attr & CHECK) {
		sql += " CHECK( " + Check + ") ";
	}
	if (Attr & AUTOINCREMENT) {
		sql += " AUTOINCREMENT ";
	}
	return sql;
}

void _sqlite3_field_::LoadFromStr(const Buffer& str)
{
	switch (nType)
	{
	case TYPE_NULL:
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		Value.Integer = atoi(str);
		break;
	case TYPE_REAL:
		Value.Double = atof(str);
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
		*Value.String = str;
		break;
	case TYPE_BLOB:
		*Value.String = Str2Hex(str);
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
}

Buffer _sqlite3_field_::toEqualExp() const
{
	Buffer sql = (Buffer)*this + " = ";
	std::stringstream ss;
	switch (nType)
	{
	case TYPE_NULL:
		sql += " NULL ";
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		ss << Value.Integer;
		sql += ss.str() + " ";
		break;
	case TYPE_REAL:
		ss << Value.Double;
		sql += ss.str() + " ";
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		sql += '"' + *Value.String + "\" ";
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
	return sql;
}

Buffer _sqlite3_field_::toSqlStr() const
{
	Buffer sql = "";
	std::stringstream ss;
	switch (nType)
	{
	case TYPE_NULL:
		sql += " NULL ";
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		ss << Value.Integer;
		sql += ss.str() + " ";
		break;
	case TYPE_REAL:
		ss << Value.Double;
		sql += ss.str() + " ";
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		sql += '"' + *Value.String + "\" ";
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
	return sql;
}

_sqlite3_field_::operator const Buffer() const
{
	return '"' + Name + '"';
}

Buffer _sqlite3_field_::Str2Hex(const Buffer& data)
{
	const char* hex = "0123456789ABCDEF";
	std::stringstream ss;
	for (auto ch : data)
		ss << hex[(unsigned char)ch >> 4] << hex[(unsigned char)ch & 0xF];
	return ss.str();
}
