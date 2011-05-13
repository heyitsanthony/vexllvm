#include <stdio.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	assert (argc == 1 && "But called with no args!");
	return argc;
}