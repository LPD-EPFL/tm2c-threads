#include "common.h"

#include <libconfig.h>
#include <string.h>

config_t* the_config;

/*
 * The init_configuration assumes that there is a parameter in the form
 * -config=configurationFileName is in the list of params.
 * It will consume it, and read that particular configuration file.
 * Other parameters will remain intact.
 */
void init_configuration(int* argc, char** argv[]) {
	the_config = (config_t*)malloc(sizeof(config_t));
	if (the_config == NULL) {
		fprintf(stderr, "Could not create memory for the configuration\n");
		EXIT(1);
	}
	config_init(the_config);

	if (*argc < 2) {
		fprintf(stderr, "Not enough parameters (%d)\n", *argc);
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s <-config=path_to_config_file> ...\n", (*argv)[0]);
		return;
	}

	int p = 1;
	int clen = strlen("-config=");
	while (p < *argc
			&& strncmp("-config=",(*argv)[p], clen) != 0) {
		p++;
	}

	if (p < *argc) {
		char *cf = (*argv)[p]+strlen("-config="); // to skip "-config="

		if (config_read_file(the_config, cf) == CONFIG_FALSE) {
			fprintf(stderr, "There was an error with reading the configuration from %s:\nline %d: %s\n",
				cf, config_error_line(the_config), config_error_text(the_config));
			config_destroy(the_config);
			EXIT(1);
		}
		while (p < *argc-1) {
			(*argv)[p] = (*argv)[p+1];
			p++;
		}
		*argc -= 1;
	}
}
