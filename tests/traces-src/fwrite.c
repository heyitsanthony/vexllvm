#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	FILE	*f;
	
	f = fopen("bogus_file", "w");
	assert (f != NULL);
	fwrite("abc", 3, 1, f);
	fclose(f);
	unlink("bogus_file");

	return 0;
}