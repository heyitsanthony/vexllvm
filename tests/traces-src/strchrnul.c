#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <assert.h>

static const char	s1[] = {"abcdefghijklmnopqrstuvwxy"};
static const char	s2[] = {"abc"};

int main(int argc, char* argv[])
{
	int	i;

	for (i = 0; i < sizeof(s1); i++) {
		char	*s;
		int	c;
		c = 'a' + i;
		s = strchrnul(s1, c);
		assert (s == &s1[i]);
	}

	for (i = 0; i < sizeof(s2); i++) {
		char	*s;
		int	c;
		c = 'a' + i;
		s = strchrnul(s2, c);
		assert (s == &s2[i]);
	}


	return 0;
}
