#include "common.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "pgas.h"

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>

#include <assert.h>
#include <zmq.h>

#include <khash.h>

void* zmq_context = NULL;              // here, we keep the context (seems necessary)

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
	init_barrier();
}

void
term_system()
{
    if (the_responder != NULL) {
    	zmq_close(the_responder);
    	the_responder = NULL;
    }
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

void* the_responder;   // for dsl nodes, it will be the responder socket; for app node, it will be NULL
void** dsl_node_addrs; // for app nodes, it will be an array containing sockets; for dsl nodes, it should be NULL

// barrier stuff
void* zmq_barrier_subscriber; /* socket of the barrier subscriber (to barrier_syncer) */
void* zmq_barrier_client;     /* socket of the barrier subscriber (from barrier_syncer) */

static PS_COMMAND *psc; /* buffer to hold the current command */

void
sys_tm_init()
{
}

void
sys_ps_init_(void)
{
    if ((dsl_node_addrs = (void**)malloc(NUM_DSL_NODES * sizeof(void*))) == NULL) {
        PRINT("malloc dsl_node_addrs");
        EXIT(-1);
    }

    unsigned int j, dsln = 0;
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
fprintf(stderr, "sys_ps_init_: socket dsl_node_addrs[%u] = %p\n",
        dsln, a_socket);
            dsl_node_addrs[dsln++] = a_socket;
        }
    }
}

// in this hash table we keep all values for all addresses
// memory_hashtable is indexed by the nodeId
// and the field content is a hash of addr->value
KHASH_MAP_INIT_INT(32, uint32_t);
khash_t(32)* memory_hashtable[MAX_NODES];

void
sys_dsl_init(void)
{
    nodeid_t i = 0;
    for (i=0; i<NUM_UES; i++) {
        memory_hashtable[i] = kh_init(32);
    }
    PRINT("[DSL NODE] Initialized the memory mapping hashtable...");

    psc = (PS_COMMAND*)malloc(sizeof(PS_COMMAND));

    char conn_spec_path[256];
    snprintf(conn_spec_path, 255, "nodes.[%d].conn_spec", ID);
    const char* conn_spec_string = NULL;
    if (config_lookup_string(the_config, conn_spec_path, &conn_spec_string) == CONFIG_FALSE) {
        PRINTD("Could not read the parameter from the config file: %s\n", conn_spec_path);
        EXIT(2);
    }
    PRINTD("Listenning on %s\n", conn_spec_string);
    the_responder = zmq_socket(zmq_context, ZMQ_REP);
    if (zmq_bind(the_responder, conn_spec_string) != 0) {
        PRINTD("Could not bind to %s as REQ/REP\n", conn_spec_string);
        EXIT(2);
    }
}

void
sys_dsl_term(void)
{
	zmq_close(the_responder);
}

void
sys_ps_term(void)
{
    zmq_close(zmq_barrier_client);
    zmq_close(zmq_barrier_subscriber);
    nodeid_t j, dsln = 0;
    for (j = 0; j < NUM_UES; j++) {
        if (j % DSLNDPERNODES == 0) {
            if (dsl_node_addrs[dsln] != NULL) {
                if (zmq_close(dsl_node_addrs[dsln]) != 0) {
                    PRINTD("Problem closing socket %p[%u]\n",
                           dsl_node_addrs[dsln], dsln);
                }
            }
            dsl_node_addrs[dsln] = NULL;
            dsln++;
        }
    }
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
	assert((to>=0)&&(to<NUM_DSL_NODES));
	zmq_msg_t request;
	zmq_msg_init_size(&request, len);

fprintf(stderr, "sys_sendcmd: will send to dsl_node_addrs[%u] = %p\n", to,
		dsl_node_addrs[to]);
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

	zmq_msg_t request;
	zmq_msg_init_size(&request, len);
	memcpy(zmq_msg_data(&request), data, len);

	nodeid_t to;
	for (to=0; to < NUM_DSL_NODES; to++) {
		rc = rc
			|| zmq_send(dsl_node_addrs[to], &request, 0);
		assert(!rc);
		zmq_recv(dsl_node_addrs[to], &request, 0);
	}
	zmq_msg_close(&request);

	return rc;
}

int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
    zmq_msg_t message;
    zmq_msg_init(&message);

    int rc = zmq_recv(dsl_node_addrs[from], &message, 0);
    if (rc != 0) {
		fprintf(stderr, "sys_recvcmd: there was a problem receiveing msg:\n%s\n",
				strerror(errno));
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
sys_ps_command_reply(unsigned short int target,
                    PS_COMMAND_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response)
{
    psc->nodeId = ID;
    psc->type = command;
    psc->response = response;
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
    zmq_send(the_responder, &message, 0);
    zmq_msg_close(&message);
}


void
dsl_communication()
{
    PS_COMMAND* ps_remote = (PS_COMMAND*)malloc(sizeof(PS_COMMAND));

    while (1) {
fprintf(stderr, "Waiting for a mesaz...\n");
        zmq_msg_t request;
        zmq_msg_init(&request);
        zmq_recv(the_responder, &request, 0);
        size_t size = zmq_msg_size(&request);
        if (size < sizeof(PS_COMMAND))
            size = sizeof(PS_COMMAND);
        memcpy((char*)ps_remote, zmq_msg_data(&request), size);
fprintf(stderr, "Got something...\n");
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
        PRINT("dsl_communication: got message %d from %u\n", ps_remote->type, sender);

        int kh_ret;
        khiter_t kh_iterator;

        switch (ps_remote->type) {
        case PS_SUBSCRIBE:

fprintf(stderr, "PS_SUBSCRIBE...\n");
            sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
                    (tm_addr_t)ps_remote->address,
                    PGAS_read(ps_remote->address),
                    try_subscribe(sender, ps_remote->address));
            break;
        case PS_PUBLISH:
            // we store the value in a separate hash
fprintf(stderr, "PS_PUBLISH...\n");
            kh_iterator = kh_put(32, memory_hashtable[sender], ps_remote->address, &kh_ret);
            if (!kh_ret) kh_del(32, memory_hashtable[sender], kh_iterator);
            kh_value(memory_hashtable[sender], kh_iterator) = ps_remote->value;

            sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE,
                    (tm_addr_t)ps_remote->address,
                    NULL,
                    try_publish(sender, ps_remote->address));
            break;
#ifdef PGAS
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
				                      PGAS_read(ps_remote->address) + ps_remote->write_value,
				                      ps_remote->address);
			}
			sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE,
			                     ps_remote->address,
			                     NULL,
			                     conflict);
			}
			break;
#endif
        case PS_REMOVE_NODE:
fprintf(stderr, "PS_REMOVE_NODE...\n");
            // now, see what is the action, and either persist writes or remove them all...
            if (ps_remote->response == NO_CONFLICT) {
                khash_t(32)* h = memory_hashtable[sender];
                for (kh_iterator = kh_begin(h); kh_iterator != kh_end(h); ++kh_iterator) {
                    if (kh_exist(h, kh_iterator)) {
                        /* fetch the key, which is tm_intern_addr_t, and 
                         * val, which is uint32_t */
                        uint32_t  val = kh_value(h, kh_iterator);
                        tm_intern_addr_t key = (tm_intern_addr_t)kh_key(h, kh_iterator);
                        PGAS_write(key, val);
                        PRINTD("Set %"PRIxPTR" to %u\n", key, val);
                    }
                }
            }
            // clean up...
            kh_clear(32, memory_hashtable[sender]);

#ifdef PGAS
			// XXX: maybe not needed?
			if (ps_remote->response == NO_CONFLICT) {
				write_set_pgas_persist(PGAS_write_sets[sender]);
			}
			PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
#endif

			ps_hashtable_delete_node(ps_hashtable, sender);
			sys_ps_command_reply(sender, PS_DUMMY_REPLY,
			                     (tm_addr_t)ps_remote->address,
			                     NULL,
			                     NO_CONFLICT);
            break;
        case PS_UNSUBSCRIBE:
fprintf(stderr, "PS_UNSUBSCRIBE...\n");
            ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
            sys_ps_command_reply(sender, PS_DUMMY_REPLY, 
                    (tm_addr_t)ps_remote->address,
                    0,
                    NO_CONFLICT);
            break;
        case PS_PUBLISH_FINISH:
fprintf(stderr, "PS_PUBLISH_FINISH...\n");
            ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
            sys_ps_command_reply(sender, PS_DUMMY_REPLY, 
                    (tm_addr_t)ps_remote->address,
                    0,
                    NO_CONFLICT);
            break;
        case PS_STATS:
fprintf(stderr, "PS_STATS...\n");
            stats_aborts += ps_remote->aborts;
            stats_aborts_raw += ps_remote->aborts_raw;
            stats_aborts_war += ps_remote->aborts_war;
            stats_aborts_waw += ps_remote->aborts_waw;
            stats_commits += ps_remote->commits;
            stats_duration += ps_remote->tx_duration;
            stats_max_retries = stats_max_retries < ps_remote->max_retries ? ps_remote->max_retries : stats_max_retries;
            stats_total += ps_remote->commits + ps_remote->aborts;

            if (++stats_received >= NUM_APP_NODES) {
                print_global_stats();
            }
            // zmq thingy, needs a reply
            sys_ps_command_reply(sender, PS_DUMMY_REPLY, 
                    (tm_addr_t)ps_remote->address,
                    0,
                    NO_CONFLICT);

            EXIT(0);
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
}
