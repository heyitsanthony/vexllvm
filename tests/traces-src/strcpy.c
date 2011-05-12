#include <stdio.h>
#include <assert.h>
#include <string.h>

const char	s1[] = "HELLO COPY THIS STRING";

int main(int argc, char* argv[])
{
	char	s2[sizeof(s1)];
	int	i;

	strcpy(s2, s1);
	for (i = 0; s1[i]; i++) {
		assert (s2[i] == s1[i] && "CPY MISMATCH");
	}

	return 0;
}