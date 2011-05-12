#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	int	d;
	char	c;

	sscanf("10 ", "%d", &d);
	assert (d == 10);
	sscanf("c ", "%c", &c);
	assert (c == 'c');

	return 0;
}