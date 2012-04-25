#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>

#include <assert.h>
#include <zmq.h>

#include "common.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "pgas.h"

void* zmq_context = NULL;              // here, we keep the context (seems necessary)

void* the_responder;   // for dsl nodes, it will be the responder socket; for app node, it will be NULL
void** dsl_node_addrs; // for app nodes, it will be an array containing sockets; for dsl nodes, it should be NULL

// barrier stuff
void* zmq_barrier_subscriber; /* socket of the barrier subscriber (to barrier_syncer) */
void* zmq_barrier_client;     /* socket of the barrier subscriber (from barrier_syncer) */

static PS_COMMAND *psc; /* buffer to hold the current command */

/*
 * For cluster conf, we need a different kind of command line parameters.
 * Since there is no way for a node to know it's identity, we need to pass it,
 * along with the total number of nodes.
 * To make sure we don't rely on any particular order, params should be passed
 * as: -id=ID -total=TOTAL_NODES
 */
nodeid_t MY_NODE_ID;
nodeid_t MY_TOTAL_NODES;

void 
sys_init_system(int* argc, char* argv[])
{
	if (*argc < 3) {
		fprintf(stderr, "Not enough parameters (%d)\n", *argc);
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s -id=ID -total=TOTAL_NODES ...\n", argv[0]);
		EXIT(1);
	}

	int p = 1;
	int found = 0;
	while (p < *argc) {
		if (strncmp("-id=", argv[p], strlen("-id=")) == 0) {
			char *cf = argv[p] + strlen("-id=");
			MY_NODE_ID = atoi(cf);
			argv[p] = NULL;
			found = found | 0x01;
		} else if (strncmp("-total=", argv[p], strlen("-total=")) == 0) {
			char *cf = argv[p] + strlen("-total=");
			MY_TOTAL_NODES = atoi(cf);
			argv[p] = NULL;
			found = found | 0x02;
		}
		p++;
	}
	if ((found & 0x03) != 0x03) {
		fprintf(stderr, "Did not pass all parameters\n");
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s -id=ID -total=TOTAL_NODES ...\n", argv[0]);
		EXIT(1);
	}
	p = 1;
	int cur = 1;
	while (p < *argc) {
		if (argv[p] == NULL) {
			p++;
			continue;
		}
		argv[cur] = argv[p];
		cur++;
		p++;
	}
	*argc = *argc - (p-cur);

	zmq_context = zmq_init(1);
}

void
term_system()
{
    zmq_term(zmq_context);
}

sys_t_vcharp
sys_shmalloc(size_t size)
{
	return fakemem_malloc(size);
}

void
sys_shfree(sys_t_vcharp ptr)
{
	fakemem_free(ptr);
}

nodeid_t
NODE_ID(void)
{
	return MY_NODE_ID;
}

nodeid_t
TOTAL_NODES(void)
{
	return MY_TOTAL_NODES;
}

void
sys_tm_init()
{
}

void
sys_ps_init_(void)
{
    if ((dsl_node_addrs = (void**)malloc(TOTAL_NODES() * sizeof(void*))) == NULL) {
        PRINT("malloc dsl_node_addrs");
        EXIT(-1);
    }

    unsigned int j;
    for (j = 0; j < NUM_UES; j++) {
        // initialize the sockets to all dsl nodes.
        // read the configuration, and get the address from there
        if (j % DSLNDPERNODES == 0) {
            char send_spec_path[256];
            snprintf(send_spec_path, 255, "nodes.[%d].send_spec", j); // extra check if the node is right
            const char* send_spec_string = NULL;
            if (config_lookup_string(the_config, send_spec_path, &send_spec_string) == CONFIG_FALSE) {
                PRINTD("Could not read the parameter from the config file: %s\n", send_spec_path);
                EXIT(2);
            }
            PRINTD("Connecting to %d: %s\n", j, send_spec_string);
            void *a_socket = zmq_socket(zmq_context, ZMQ_REQ);
            if (zmq_connect(a_socket, send_spec_string) != 0) {
                PRINTD("Failed to connect to %s\n", send_spec_string);
                EXIT(1);
            }
            dsl_node_addrs[j] = a_socket;
            int zero = 100;
            zmq_setsockopt(a_socket, ZMQ_LINGER, &zero, sizeof (zero));
        } else {
            dsl_node_addrs[j] = NULL;
        }
    }
}

void
sys_dsl_init(void)
{
    psc = (PS_COMMAND*)malloc(sizeof(PS_COMMAND));

    char conn_spec_path[256];
    snprintf(conn_spec_path, 255, "nodes.[%d].conn_spec", ID);
    const char* conn_spec_string = NULL;
    if (config_lookup_string(the_config, conn_spec_path, &conn_spec_string) == CONFIG_FALSE) {
        PRINTD("Could not read the parameter from the config file: %s\n", conn_spec_path);
        EXIT(2);
    }
    PRINTD("Listening on %s\n", conn_spec_string);
    the_responder = zmq_socket(zmq_context, ZMQ_REP);
    if (zmq_bind(the_responder, conn_spec_string) != 0) {
        PRINTD("Could not bind to %s as REQ/REP\n", conn_spec_string);
        EXIT(2);
    }
    int zero = 100;
    zmq_setsockopt(the_responder, ZMQ_LINGER, &zero, sizeof (zero));
}

void
sys_dsl_term(void)
{
	PRINTD("sys_dsl_term: called");
	if (ID % DSLNDPERNODES == 0) {
		// DSL node
		zmq_setsockopt(zmq_barrier_subscriber, ZMQ_UNSUBSCRIBE, "all", 0);
	} else {
		zmq_setsockopt(zmq_barrier_subscriber, ZMQ_UNSUBSCRIBE, "a", 0);
    }
    zmq_close(zmq_barrier_subscriber);
    zmq_close(zmq_barrier_client);
    
    zmq_close(the_responder);
	PRINTD("sys_dsl_term: ended");
}

void
sys_ps_term(void)
{
	PRINTD("sys_ps_term: called");
	if (ID % DSLNDPERNODES == 0) {
		// DSL node
		zmq_setsockopt(zmq_barrier_subscriber, ZMQ_UNSUBSCRIBE, "all", 0);
	} else {
		zmq_setsockopt(zmq_barrier_subscriber, ZMQ_UNSUBSCRIBE, "a", 0);
    }
    zmq_close(zmq_barrier_client);
    zmq_close(zmq_barrier_subscriber);

    nodeid_t j;
    for (j = 0; j < NUM_UES; j++) {
	    if (dsl_node_addrs[j] != NULL) {
			if (zmq_close(dsl_node_addrs[j]) != 0) {
				PRINT("Problem closing socket %p[%u]\n",
					  dsl_node_addrs[j], j);
			}
		}
		dsl_node_addrs[j] = NULL;
	}
	PRINTD("sys_ps_term: ended");
}

//  Receive 0MQ string from socket and convert into C string
char*
zmq_s_recv(void *socket)
{
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
int
zmq_s_send(void *socket, char *string)
{
    int rc;
    zmq_msg_t message;
    zmq_msg_init_size (&message, strlen (string));
    memcpy (zmq_msg_data (&message), string, strlen (string));
    rc = zmq_send(socket, &message, 0);
    assert (!rc);
    zmq_msg_close(&message);
    return (rc);
}

int
sys_sendcmd(void* data, size_t len, nodeid_t to)
{
	assert((to>=0)&&(to<TOTAL_NODES()));
	assert(dsl_node_addrs[to]!=NULL);

	PRINTD("sys_sendcmd: to = %u", to);

	zmq_msg_t request;
	zmq_msg_init_size(&request, len);

	memcpy(zmq_msg_data(&request), data, len);
	int rc = zmq_send(dsl_node_addrs[to], &request, 0);

	if (rc) {
		perror(NULL);
	}
	assert(!rc);
	zmq_msg_close(&request);

	return rc;
}

int
sys_sendcmd_all(void* data, size_t len)
{
	int rc = 0;

	nodeid_t to;
	for (to=TOTAL_NODES()-1; to-- > 0;) {
	/*for (to=0; to < TOTAL_NODES(); to++) {*/
		if (dsl_node_addrs[to] == NULL)
			continue;

		zmq_msg_t request;
		zmq_msg_init_size(&request, len);
		memcpy(zmq_msg_data(&request), data, len);

		rc = zmq_send(dsl_node_addrs[to], &request, 0);
		PRINTD("sys_sendcmd_all: to = %u, rc = %d", to, rc);
		assert(!rc);
		if (rc) {
			perror(NULL);
		}

		zmq_msg_close(&request);

		zmq_msg_t reply;
		zmq_msg_init(&reply);

		rc = zmq_recv(dsl_node_addrs[to], &reply, 0);
		PRINTD("sys_sendcmd_all: [from] to = %u, rc = %d", to, rc);
		assert(!rc);
		if (rc) {
			perror(NULL);
		}

		zmq_msg_close(&reply);
	}

	return rc;
}

int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
    zmq_msg_t message;
    zmq_msg_init(&message);

    PRINTD("sys_recvcmd: from = %u", from);
    int rc = zmq_recv(dsl_node_addrs[from], &message, 0);
    if (rc != 0) {
		fprintf(stderr, "sys_recvcmd: there was a problem receiveing msg:\n%s\n",
				strerror(errno));
		zmq_msg_close (&message);
		return -1;
    }

    int size = zmq_msg_size (&message);
    memcpy(data, zmq_msg_data(&message), size);
    zmq_msg_close (&message);

	return 0;
}

// If value == NULL, we just return the address.
// Otherwise, we return the value.
static inline void 
sys_ps_command_reply(nodeid_t target,
                    PS_COMMAND_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response)
{
    psc->nodeId = ID;
    psc->type = command;
    psc->response = response;

    PRINTD("sys_ps_command_reply: src=%u target=%u", psc->nodeId, target);
#ifdef PGAS
    if (value != NULL) {
        psc->value = *value;
    	PRINTD("sys_ps_command_reply: read value %u\n", psc->value);
    } else {
        psc->address = (uintptr_t)address;
    }
#else
#error CLUSTER without PGAS does not make sense
#endif

    zmq_msg_t message;
    zmq_msg_init_size(&message, sizeof(PS_COMMAND));
    memcpy(zmq_msg_data(&message), psc, sizeof(PS_COMMAND));

    if (zmq_send(the_responder, &message, 0) != 0) {
	    PRINT("sys_ps_command_reply: problem sending message");
	    perror(NULL);
    }
    
    zmq_msg_close(&message);
}


void
dsl_communication()
{
    PS_COMMAND* ps_remote = (PS_COMMAND*)malloc(sizeof(PS_COMMAND));

    PRINTD("dsl_communication loop starting");
loop:
    while (1) {
	    zmq_msg_t request;
	    zmq_msg_init(&request);

        int rc = zmq_recv(the_responder, &request, 0);
        if (rc != 0) {
	        if (errno == ETERM) {
		        PRINT("dsl_communication: zmq context is killed");
		        EXIT(13);
	        }
	        perror(NULL);
        }
        size_t size = zmq_msg_size(&request);
        if (size < sizeof(PS_COMMAND))
            size = sizeof(PS_COMMAND);
        memcpy((char*)ps_remote, zmq_msg_data(&request), size);
#ifdef DEBUG_MSG
        {
            int i;
            char* p=zmq_msg_data(&request);
            fprintf(stderr, "psc -> [");
            for (i=0; i<sizeof(PS_COMMAND); i++) {
                fprintf(stderr, "%02hhx ", p[i]);
            }
            fprintf(stderr, "]\n");
        };
#endif
        zmq_msg_close(&request);
        nodeid_t sender = ps_remote->nodeId;
        PRINTD("dsl_communication: got message %d from %u", ps_remote->type, sender);

        switch (ps_remote->type) {
        case PS_SUBSCRIBE:
            sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
                    (tm_addr_t)ps_remote->address,
                    PGAS_read(ps_remote->address),
                    try_subscribe(sender, ps_remote->address));
            break;
        case PS_PUBLISH:
			{ // need curlies because of the declaration below
            CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);
            if (conflict == NO_CONFLICT) {
                write_set_pgas_insert(PGAS_write_sets[sender],
                                      ps_remote->write_value, 
                                      ps_remote->address);
            }

            sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE,
                    (tm_addr_t)ps_remote->address,
                    NULL,
                    conflict);
			}
            break;
		case PS_WRITE_INC:
			{
#ifdef DEBUG_UTILIZATION
			write_reqs_num++;
#endif
			CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);
			if (conflict == NO_CONFLICT) {
				/*
					PRINT("PS_WRITE_INC from %2d for %3d, old: %3d, new: %d", sender, ps_remote->address, PGAS_read(ps_remote->address),
					PGAS_read(ps_remote->address) + ps_remote->write_value);
					*/
				write_set_pgas_insert(PGAS_write_sets[sender],
				                      *PGAS_read(ps_remote->address) + ps_remote->write_value,
				                      (tm_intern_addr_t)ps_remote->address);
			}
			sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE,
			                     (tm_addr_t)ps_remote->address,
			                     NULL,
			                     conflict);
			}
			break;
        case PS_REMOVE_NODE:
			if (ps_remote->response == NO_CONFLICT) {
				write_set_pgas_persist(PGAS_write_sets[sender]);
			}
			PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);

			ps_hashtable_delete_node(ps_hashtable, sender);
			sys_ps_command_reply(sender, PS_DUMMY_REPLY,
			                     (tm_addr_t)ps_remote->address,
			                     NULL,
			                     NO_CONFLICT);
            break;
        case PS_UNSUBSCRIBE:
            ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
            sys_ps_command_reply(sender, PS_DUMMY_REPLY, 
                    (tm_addr_t)ps_remote->address,
                    0,
                    NO_CONFLICT);
            break;
        case PS_PUBLISH_FINISH:
            ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
            sys_ps_command_reply(sender, PS_DUMMY_REPLY, 
                    (tm_addr_t)ps_remote->address,
                    0,
                    NO_CONFLICT);
            break;
        case PS_STATS:
            stats_aborts += ps_remote->aborts;
            stats_aborts_raw += ps_remote->aborts_raw;
            stats_aborts_war += ps_remote->aborts_war;
            stats_aborts_waw += ps_remote->aborts_waw;
            stats_commits += ps_remote->commits;
            stats_duration += ps_remote->tx_duration;
            stats_max_retries = stats_max_retries < ps_remote->max_retries ? ps_remote->max_retries : stats_max_retries;
            stats_total += ps_remote->commits + ps_remote->aborts;

            PRINTD("Stats received so far: %u/%u", stats_received+1,
            	   NUM_APP_NODES);
            // zmq thingy, needs a reply
            sys_ps_command_reply(sender, PS_DUMMY_REPLY, 
                    (tm_addr_t)ps_remote->address,
                    0,
                    NO_CONFLICT);

            if (++stats_received >= NUM_APP_NODES) {
                if (NODE_ID() == 0) {
                    print_global_stats();
                    print_hashtable_usage();
                }
				sleep(1);
				return;
            }
            break;
        case PS_LOAD_NONTX:
            sys_ps_command_reply(sender, PS_LOAD_NONTX_RESPONSE, 
                    (tm_addr_t)ps_remote->address,
                    PGAS_read(ps_remote->address),
                    NO_CONFLICT);
            break;
        case PS_STORE_NONTX:
            PGAS_write(ps_remote->address, ps_remote->value);
            sys_ps_command_reply(sender, PS_DUMMY_REPLY,
                    (tm_addr_t)ps_remote->address,
                    NULL,
                    NO_CONFLICT);
            break;
        default:
            PRINTD("REMOTE MSG: ??");
        }
    }
}

tm_intern_addr_t
to_intern_addr(tm_addr_t addr)
{
	return fakemem_offset((void*)addr);
}

tm_addr_t
to_addr(tm_intern_addr_t i_addr)
{
	return (tm_addr_t)fakemem_addr_from_offset(i_addr);
}

/*
 * Seeding the rand()
 */
void
srand_core()
{
	srand(time(NULL));
}

double
wtime(void)
{
    struct timeval time_struct;
    gettimeofday(&time_struct, NULL);
    double retval = time_struct.tv_sec;
    retval += time_struct.tv_usec/1000000.0;
    return retval;
}

void 
udelay(unsigned int micros)
{
	usleep(micros);
}

void
init_barrier()
{
    // barrier stuff setup
    ME; PRINT("Initializing BARRIERs\n");
    const char* syncer_publisher_spec;
    const char* syncer_request_spec;
    if (config_lookup_string(the_config, "syncer.publisher_spec", &syncer_publisher_spec) == CONFIG_FALSE) {
        PRINTD("Could not read the parameter '%s' from the config file\n", "syncer.publisher_spec");
        EXIT(1);
    }
    if (config_lookup_string(the_config, "syncer.request_spec", &syncer_request_spec) == CONFIG_FALSE) {
        PRINTD("Could not read the parameter '%s' from the config file\n", "syncer.request_spec");
        EXIT(1);
    }

    //  First, connect our subscriber socket
    zmq_barrier_subscriber = zmq_socket(zmq_context, ZMQ_SUB);
    if (zmq_connect(zmq_barrier_subscriber, syncer_publisher_spec) != 0) {
        PRINTD("Failed to connect to %s as subscriber\n", syncer_publisher_spec);
        EXIT(1);
    }
    if (ID % DSLNDPERNODES == 0) {
        // DSL node
        zmq_setsockopt(zmq_barrier_subscriber, ZMQ_SUBSCRIBE, "all", 0);
    } else {
        zmq_setsockopt(zmq_barrier_subscriber, ZMQ_SUBSCRIBE, "a", 0);
    }
    PRINTD("Connected to %s as SUBSCRIBER\n", syncer_publisher_spec);

    //  Second, synchronize with publisher
    zmq_barrier_client = zmq_socket(zmq_context, ZMQ_REQ);
    if (zmq_connect(zmq_barrier_client, syncer_request_spec) != 0) {
        PRINTD("Failed to connect to %s as REQ/REP\n", syncer_request_spec);
        EXIT(1);
    }
    PRINTD("Connected to %s as REQ/REP for barrier subscriptions\n", syncer_request_spec);
    int zero = 100;
    zmq_setsockopt(zmq_barrier_subscriber, ZMQ_LINGER, &zero, sizeof (zero));
    zmq_setsockopt(zmq_barrier_client, ZMQ_LINGER, &zero, sizeof (zero));
}
