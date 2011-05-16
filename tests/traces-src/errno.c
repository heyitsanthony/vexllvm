#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char* argv[])
{
	printf("Error: %d\n", errno);
	return 0;
}