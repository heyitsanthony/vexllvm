#include <stdio.h>

void hello(void) { printf("hello\n"); }
void goodbye(void) { printf("goodbye\n"); }

int main(int argc, char* argv[])
{
	hello();
	goodbye();
	return 0;
}