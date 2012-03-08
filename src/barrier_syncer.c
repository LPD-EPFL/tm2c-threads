//
//  Synchronized publisher
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libconfig.h>
#include <zmq.h>
#include <assert.h>

//  Receive 0MQ string from socket and convert into C string
static char *
s_recv (void *socket) {
    zmq_msg_t message;
    zmq_msg_init (&message);
    zmq_recv (socket, &message, 0);
    int size = zmq_msg_size (&message);
    char *string = malloc (size + 1);
    memcpy (string, zmq_msg_data (&message), size);
    zmq_msg_close (&message);
    string [size] = 0;
    return (string);
}

//  Convert C string to 0MQ string and send to socket
static int
s_send (void *socket, char *string) {
    int rc;
    zmq_msg_t message;
    zmq_msg_init_size (&message, strlen (string));
    memcpy (zmq_msg_data (&message), string, strlen (string));
    rc = zmq_send (socket, &message, 0);
    assert (!rc);
    zmq_msg_close (&message);
    return (rc);
}

static config_t *the_config;

enum barrier_type_t {
	BARRIER_UNKNOWN,
	BARRIER_APP,
	BARRIER_ALL,
};

int main (int argc, char* argv[]) {

	if (argc != 3 || !strcmp(argv[1], "-h")) {
		fprintf(stderr, "Not enough parameters (%d)", argc);
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s NUM_NODES path_to_config_file\n", argv[0]);
		exit(1);
	}
	// initialize the configuration, read from a config file
	// the name is passed via the command line
	the_config = (config_t*)malloc(sizeof(config_t));
	if (the_config == NULL) {
		fprintf(stderr, "Could not create memory for the configuration\n");
		exit(1);
	}
    config_init(the_config);

	if (config_read_file(the_config, argv[2]) == CONFIG_FALSE) {
		fprintf(stderr, "There was an error with reading the configuration from %s:\nline %d: %s\n",
				argv[2], config_error_line(the_config), config_error_text(the_config));
		config_destroy(the_config);
		exit(1);
	}

	const char* syncer_publisher_spec;
	const char* syncer_request_spec;

	int NUM_UES = atoi(argv[1]);
	int DSLNDPERNODES= 0;

	if (config_lookup_string(the_config, "syncer.publisher_spec", &syncer_publisher_spec) == CONFIG_FALSE) {
		fprintf(stderr, "Could not read the parameter '%s' from the config file\n", "syncer.publisher_spec");
		exit(1);
	}
	if (config_lookup_string(the_config, "syncer.request_spec", &syncer_request_spec) == CONFIG_FALSE) {
		fprintf(stderr, "Could not read the parameter '%s' from the config file\n", "syncer.request_spec");
		exit(1);
	}
	if (config_lookup_int(the_config, "dslndpernode", &DSLNDPERNODES) == CONFIG_FALSE) {
		fprintf(stderr, "Could not read the parameter 'dslndpernode' for the config file\n");
		exit(1);
	}

	int NUM_DSL_NODES = (int) ((NUM_UES / DSLNDPERNODES)) + (NUM_UES % DSLNDPERNODES ? 1 : 0);
	int NUM_APP_NODES = NUM_UES - NUM_DSL_NODES;

    void *context = zmq_init (1);

    //  Socket to talk to clients
    void *publisher = zmq_socket (context, ZMQ_PUB);
    if (zmq_bind (publisher, syncer_publisher_spec) != 0) {
		fprintf(stderr, "Could not bind to %s as publisher\n", syncer_publisher_spec);
		perror("zmq");
		exit(1);
    }

    //  Socket to receive signals
    void *syncservice = zmq_socket (context, ZMQ_REP);
    if (zmq_bind (syncservice, syncer_request_spec) != 0) {
		fprintf(stderr, "Could not bind to %s as REQ/REP\n", syncer_request_spec);
		perror("zmq");
		exit(1);
	}

    int barrier_num = 0;
    // loop until the experiment is done
    while (1) {
		//  Get synchronization from subscribers
		int subscribers = 0;
		int expected_subscribers = NUM_UES;

		// the logic is the following:
		// nodes send the type of the barrier (app/all)
		// and we change the number of subscribers based on that
		// also, based on this info, we send the corresponding message in the pub msg
		enum barrier_type_t barrier_type = BARRIER_UNKNOWN;
		while (subscribers < expected_subscribers) {
			//  - wait for synchronization request
			char *string = s_recv (syncservice);
			if (barrier_type == BARRIER_UNKNOWN) {
				if (!strcmp(string, "app")) {
					barrier_type = BARRIER_APP;
					expected_subscribers = NUM_APP_NODES;
				} else if (!strcmp(string, "all")) {
					barrier_type = BARRIER_ALL;
					expected_subscribers = NUM_UES;
				} else {
					fprintf(stderr, "Unknown subscription '%6s'\n", string);
					continue;
				}
			}
			fprintf(stderr, "Got one client, barrier type %d\n", barrier_type);
			free (string);
			//  - send synchronization reply
			switch(barrier_type) {
				case BARRIER_APP: s_send(syncservice, "app"); break;
				case BARRIER_ALL: s_send(syncservice, "all"); break;
				default: s_send(syncservice, ""); break;
			}
			subscribers++;
			fprintf(stderr, "subscribers: %d/%d\n", subscribers, expected_subscribers);
		}
		//  Now broadcast the pub message
        char publisher_string[256];
        switch (barrier_type) {
			case BARRIER_APP:
				snprintf(publisher_string, 4, "app ");
				break;
			case BARRIER_ALL:
				snprintf(publisher_string, 4, "all ");
				break;
			default:
				snprintf(publisher_string, 4, "xxx ");
				break;
        }
        snprintf(publisher_string+4, 256-1-4, "BARRIER%d", barrier_num);
        s_send (publisher, publisher_string);
        fprintf(stderr, "Sent a notification '%s' to %d subscribers\n", publisher_string, subscribers);
        barrier_num++;
    }

    zmq_close (publisher);
    zmq_close (syncservice);
    zmq_term (context);
    return 0;
}
