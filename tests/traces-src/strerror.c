#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char* argv[])
{
	printf("Error: %s\n", strerror(10));
	return 0;
}
