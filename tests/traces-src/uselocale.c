#include <locale.h>

int main(int argc, char* argv[])
{
	const char*	cur_locale;
	uselocale(LC_GLOBAL_LOCALE);
	cur_locale = setlocale(0, NULL);
	printf("%s\n", cur_locale);
	gettext("xyz");
	return 0;
}