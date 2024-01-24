#include "Crypto.h"
#include <openssl/md5.h>
Buffer Crypto::MD5(const Buffer& text)
{
	Buffer res;
	Buffer data(16);
	MD5_CTX md5;
	MD5_Init(&md5);
	MD5_Update(&md5, text, text.size());
	MD5_Final(data, &md5);
	char temp[3] = "";
	for (size_t i = 0; i < data.size(); ++i) {
		snprintf(temp, 3, "%02X", data[i] & 0xFF);
		res += temp;
	}
	return res;
}
