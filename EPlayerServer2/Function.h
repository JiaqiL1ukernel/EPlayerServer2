#pragma once
#include <functional>
#include <unistd.h>
#include <memory.h>

class CSocketBase;
class Buffer;

class CFunctionBase
{
public:
	CFunctionBase() {}
	virtual ~CFunctionBase() {}
	virtual int operator()() { return -1; }
	virtual int operator()(CSocketBase*) { return -1; }
	virtual int operator()(CSocketBase*, const Buffer&) { return -1; }
};

template<typename _FUNC_, typename... _ARG_>
class CFunction :public CFunctionBase
{
public:
	CFunction(_FUNC_ func, _ARG_... args) :m_binder(std::forward<_FUNC_>(func), std::forward<_ARG_>(args)...) {}

	virtual ~CFunction() {}

	virtual int operator() ()
	{
		return m_binder();
	}



	typename std::_Bindres_helper<int, _FUNC_, _ARG_...>::type m_binder;

};

