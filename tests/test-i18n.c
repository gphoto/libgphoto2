#include "config.h"

#include <stdio.h>

#include <locale.h>

#include "libgphoto2/i18n.h"


static
const char *const default_locale_list[] = {
	"C",
	"",
	"en_US",
	"en_US.UTF-8",
	"en@quot",
	"en@quot_US",
	"en@quot_US.UTF-8",
	"en_US@quot",
	"en_US.UTF-8@quot",
	"en_US@quot.UTF-8",
	"de",
	"de_DE",
	"de_DE.UTF-8",
	"fr",
	"fr_FR",
	"fr_FR.UTF-8"
};


static
const size_t default_locale_list_count =
	sizeof(default_locale_list) / sizeof(default_locale_list[0]);


static
void indented_print(char *indentation, char *message)
{
	if (message[0] == '\0') {
		printf("%s<no translation>\n", indentation);
		return;
	}
	printf("%s", indentation);
	for (char *p=message; *p != '\0'; ++p) {
		if (p[0] == '\n') {
			putchar(p[0]);
			if (p[1] != '\0') {
				printf("%s", indentation);
			}
		} else {
			putchar(p[0]);
		}
	}
}


static
void print_locale_list(const size_t list_count,
		       const char *const *list)
{
	for (size_t i=0; i<list_count; ++i) {
		char *locale = setlocale(LC_ALL, list[i]);
		if (locale) {
			printf("Translated empty string to '%s' (%s):\n",
			       list[i], locale);
		        indented_print("    ",
				       dgettext(GETTEXT_PACKAGE_LIBGPHOTO2,
						""));
			setlocale(LC_ALL, "C");
		} else {
			printf("Skipping locale: '%s'\n", list[i]);
		}
	}
}


int main(const int argc, const char *const *argv)
{
	bindtextdomain(GETTEXT_PACKAGE_LIBGPHOTO2, LOCALEDIR);

	if (argc > 1) {
		const size_t list_counter = (size_t) (argc - 1);
		const char *const *list   = &(argv[1]);
		print_locale_list(list_counter, list);
	} else {
		print_locale_list(default_locale_list_count,
				  default_locale_list);
	}

	return 0;
}


/*
 * Local variables:
 * mode:c
 * c-basic-offset:8
 * End:
 */
