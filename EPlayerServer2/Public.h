#pragma once
#include <string.h>
#include <string>
class Buffer :public std::string
{
public:
	Buffer() :std::string() {}
	Buffer(size_t size) :std::string() { resize(size); }
	Buffer(const std::string& str) :std::string(str) {}
	Buffer(const char* cstr) :std::string(cstr) {}
	Buffer(const char* str, size_t len) :std::string() {
		resize(len);
		memcpy((char*)c_str(), str, len);
	}
	Buffer(const char* begin, const char* end) {
		long len = end - begin;
		resize(len);
		memcpy((char*)c_str(), begin, len);
	}
	operator char* () { return (char*)c_str(); }
	operator char* ()const { return (char*)c_str(); }
	operator const char* ()const { return (const char*)c_str(); }
	operator unsigned char* () { return (unsigned char*)c_str(); }
	operator const unsigned char* ()const { return (unsigned char*)c_str(); }
	operator const void* () const{ return (void*)c_str(); }
	operator void* () { return (void*)c_str(); }

};