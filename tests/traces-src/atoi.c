#include <assert.h>
#include <stdlib.h>

const char* s[] = {"1", "-1", "-100", "100", "999999999" };
int v[] = {1, -1, -100, 100, 999999999 };

int main(int argc, char* argv[])
{
	int	i;
	for (i = 0; i < sizeof(v)/sizeof(int); i++) {
		assert (atoi(s[i]) == v[i]);
	}
	return 0;
}