#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	uselocale(LC_GLOBAL_LOCALE);
	printf("%s\n", gettext("yesexpr"));
	return 0;
}