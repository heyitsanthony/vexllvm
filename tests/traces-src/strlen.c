#include <string.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	char	x[10];
	int	ret;
	x[0] = 't'; x[1] = 'e'; x[2] = 's'; x[3] = 't'; x[4] = '\0';
	ret = strlen(x);
	assert (ret == 4 && "BAD LEN");
	return ret;
}