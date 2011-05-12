#include <assert.h>
#include <stdint.h>
#include <string.h>

const char test_str[] = {"abcdefabcdef"};

int main(int argc, char* argv[])
{
	char	*s;
	s = strstr(test_str, "def");
	assert (((intptr_t)(s - test_str)) == 3);
	return 0;
}
