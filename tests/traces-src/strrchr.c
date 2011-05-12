#include <string.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	char	x[10];
	char	*p;
	int	idx;
	x[0] = 't'; x[1] = 'e'; x[2] = 's'; x[3] = 't'; x[4] = '\0';
	p = strrchr(x, 's');
	idx = p - x;
	assert (idx == 2);
	return idx;
}