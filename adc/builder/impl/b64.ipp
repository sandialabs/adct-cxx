#include <openssl/evp.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <limits>

// link with -lssl  -lcrypto

namespace adc {

// return the output data size to hold encoded data of size input_len
// including the terminal null byte
size_t b64_length(size_t input_len)
{
	return static_cast<int>(4*((input_len+2)/3) + 1);
}

// return the output data size to hold decoded data of size input_len
// including the terminal null byte
size_t binary_length(size_t input_len)
{
	return static_cast<int>(3*input_len/4 + 1);
}

// return the b64buf pointer populated with the b64 version
// of the input bytes, or NULL if not possible.
unsigned char *base64(const unsigned char *binary, size_t binary_len, unsigned char *b64buf, size_t b64buf_length)
{
	const size_t pl = b64_length(binary_len);
	if (!b64buf || b64buf_length < pl || b64buf_length >= std::numeric_limits<int>::max())
		return NULL;
	const int ol = EVP_EncodeBlock(b64buf, binary, binary_len);
	if (ol != static_cast<int>(pl-1)) {
		std::cerr << "Insufficient space to b64 encode" << std::endl;
		return NULL;
	}
	return b64buf;
}

// return the binary pointer populated with the binary version
// of the b64buf base64 bytes, or NULL if not possible.
unsigned char *decode64(const unsigned char *b64buf, size_t b64buf_length, unsigned char *binary, size_t binary_len)
{
	const size_t pl = binary_length(b64buf_length);
	if (!binary || binary_len < pl || binary_len > std::numeric_limits<int>::max())
		return NULL;
	const int ol = EVP_DecodeBlock(binary, b64buf, b64buf_length);
	if (static_cast<int>(pl-1) != ol) { 
		std::cerr << "Insufficient space to b64 decode;" << std::endl;
		return NULL;
	}
	return binary;
}

}
