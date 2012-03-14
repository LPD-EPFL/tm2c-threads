#define _GNU_SOURCE
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <zmq.h>

#include "common.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "pgas.h"
#include "mcore_malloc.h"

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

	// Now, pin the process to the right core (NODE_ID == core id)
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(NODE_ID(), &mask);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
		PRINT("Problem with setting processor affinity: %s\n",
			  strerror(errno));
		EXIT(3);
	}

	zmq_context = zmq_init(1);
}

void
term_system()
{
	shm_unlink("/appBarrier");
	shm_unlink("/globalBarrier");
	if (the_responder != NULL) {
		zmq_close(the_responder);
		the_responder = NULL;
	}
	zmq_term(zmq_context);
}

sys_t_vcharp
sys_shmalloc(size_t size)
{
	return MCORE_shmalloc(size);
}

void
sys_shfree(sys_t_vcharp ptr)
{
	MCORE_shfree(ptr);
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
			snprintf(send_spec_path, 255, "nodes.[%d].conn_spec", j); // extra check if the node is right
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
			dsl_node_addrs[dsln++] = a_socket;
		}
	}
	MCORE_shmalloc_init(100000*sizeof(int));
}

void
sys_dsl_init(void)
{
	fprintf(stderr, "HERE sys_dsl_init\n");
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
		perror(NULL);
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

int
sys_sendcmd(void* data, size_t len, nodeid_t to)
{
	assert((to>=0)&&(to<NUM_DSL_NODES));
	zmq_msg_t request;
	zmq_msg_init_size(&request, len);

	PRINTD("sys_sendcmd\n");
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

	PRINTD("sys_recvcmd\n");
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
		zmq_msg_t request;
		zmq_msg_init(&request);
		zmq_recv(the_responder, &request, 0);
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
		PRINTD("dsl_communication: got message %d from %u\n", ps_remote->type, sender);

		switch (ps_remote->type) {
			case PS_SUBSCRIBE:

#ifdef DEBUG_UTILIZATION
				read_reqs_num++;
#endif

#ifdef PGAS
				sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
									(tm_addr_t)ps_remote->address, 
									PGAS_read(ps_remote->address),
									try_subscribe(sender, ps_remote->address));
#else
				sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, 
									(tm_addr_t)ps_remote->address, 
									NULL,
									try_subscribe(sender, ps_remote->address));
#endif
				break;
			case PS_PUBLISH:
				{

#ifdef DEBUG_UTILIZATION
					write_reqs_num++;
#endif

					CONFLICT_TYPE conflict = try_publish(sender, ps_remote->address);
#ifdef PGAS
					if (conflict == NO_CONFLICT) {
						/*
						   union {
						   int i;
						   unsigned short s[2];
						   } convert;
						   convert.i = ps_remote->write_value;
						   PRINT("\t\t\tWriting (val:%d|nxt:%d) to address %d", convert.s[0], convert.s[1], ps_remote->address);
						   */
						write_set_pgas_insert(PGAS_write_sets[sender],
											  ps_remote->write_value, 
											  ps_remote->address);
					}
#endif
					sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE, 
										(tm_addr_t)ps_remote->address,
										NULL,
										conflict);
					break;
				}
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
											  *PGAS_read(ps_remote->address) + ps_remote->write_value,
											  ps_remote->address);
					}
					sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE,
										ps_remote->address,
										NULL,
										conflict);
					break;
				}
#endif
			case PS_REMOVE_NODE:
#ifdef PGAS
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
				ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
				sys_ps_command_reply(sender, PS_DUMMY_REPLY,
									 (tm_addr_t)ps_remote->address,
									 NULL,
									 NO_CONFLICT);
				break;
			case PS_PUBLISH_FINISH:
				ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
				sys_ps_command_reply(sender, PS_DUMMY_REPLY,
									 (tm_addr_t)ps_remote->address,
									 NULL,
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

				sys_ps_command_reply(sender, PS_DUMMY_REPLY,
									 (tm_addr_t)ps_remote->address,
									 NULL,
									 NO_CONFLICT);
				if (++stats_received >= NUM_APP_NODES) {
					if (NODE_ID() == 0) {
						print_global_stats();

						print_hashtable_usage();

					}

#ifdef DEBUG_UTILIZATION
					PRINT("*** Completed requests: %d", read_reqs_num + write_reqs_num);
#endif

					EXIT(0);
				}
			default:
				PRINTD("REMOTE MSG: ??");
		}
	}
}

tm_intern_addr_t
to_intern_addr(tm_addr_t addr)
{
#ifdef PGAS
#else
	return (tm_intern_addr_t)addr;
#endif
}

tm_addr_t
to_addr(tm_intern_addr_t i_addr)
{
#ifdef PGAS
#else
	return (tm_addr_t)i_addr;
#endif
}

/*
 * Seeding the rand()
 */
void
srand_core()
{
	double timed_ = wtime();
	unsigned int timeprfx_ = (unsigned int) timed_;
	unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
	srand(time_ + (13 * (ID + 1)));
}

double
wtime(void)
{
	struct timeval t;
	gettimeofday(&t,NULL);
	return (double)t.tv_sec + ((double)t.tv_usec)/1000000.0;
}

void 
udelay(unsigned int micros)
{
	usleep(micros);
}

// barrier stuff
#define MAX_FILENAME_LENGTH 200

static nodeid_t num_tm_nodes;
static nodeid_t num_dsl_nodes;

volatile nodeid_t* aux;
volatile nodeid_t* apps;
volatile nodeid_t* apps2;
volatile nodeid_t* nodes;
volatile nodeid_t* nodes2;
volatile nodeid_t* auxNodes;

void
init_barrier()
{
	//initialize the parameters of the barriers
	num_tm_nodes = NUM_APP_NODES;
	num_dsl_nodes = NUM_DSL_NODES;

	fprintf(stderr, "init_barrier: num_tm_nodes: %u, num_dsl_nodes: %u\n",
			num_tm_nodes, num_dsl_nodes);
	int created;

	//map the shared memory areas
	char keyApp[MAX_FILENAME_LENGTH];
	sprintf(keyApp,"/appBarrier");

	created = FALSE;
	int shmfdApp = shm_open(keyApp, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
	if (shmfdApp < 0) {
		if (errno != EEXIST)
		{
			perror("In shm_open");
			exit(1);
		}
		//this time it is ok if it already exists
		shmfdApp = shm_open(keyApp, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
		if (shmfdApp < 0)
		{
			perror("In shm_open");
			exit(1);
		}
	} else {
		//only if it is just created
		created = TRUE;
		ftruncate(shmfdApp, sizeof(int)*2);
	}

	apps = (volatile nodeid_t*)mmap(NULL, sizeof(nodeid_t)*2,
									PROT_READ | PROT_WRITE,
									MAP_SHARED, shmfdApp, 0);
	apps2 = &apps[1];
	if (apps == NULL) {
		perror("In mmap");
		EXIT(1);
	}
	fprintf(stderr, "init_barrier: created %d, apps = %u\n", created, *apps);

	//do this just once
	if (created) {
		*apps = 0;
		*apps2 = 0;
	}

	char keyGlobal[MAX_FILENAME_LENGTH];
	sprintf(keyGlobal,"/globalBarrier");

	created = FALSE;
	int shmfdGlobal = shm_open(keyGlobal,
							   O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
	if (shmfdGlobal < 0) {
		if (errno != EEXIST) {
			perror("In shm_open");
			EXIT(1);
		}
		//this time it is ok if it already exists
		shmfdGlobal = shm_open(keyGlobal, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
		if (shmfdGlobal < 0) {
			perror("In shm_open");
			EXIT(1);
		}
	} else {
		//only if it is just created
		created = TRUE;
		ftruncate(shmfdGlobal, sizeof(int)* 2);
	}

	nodes = (volatile nodeid_t*)mmap(NULL, sizeof(nodeid_t) * 2,
									 PROT_READ | PROT_WRITE,
									 MAP_SHARED, shmfdGlobal, 0);
	nodes2 = &nodes[1];
	if (nodes == NULL) {
		perror("In mmap");
		EXIT(1);
	}

	//do this just once
	if (created) {
		*nodes = 0;
		*nodes2 = 0;
	}

	//TODO make sure this is always zero at program start
}

void
app_barrier()
{
	nodeid_t i = __sync_fetch_and_add(apps,1);
	fprintf(stderr, "in app_barrier, i=%u, num_tm_nodes = %u\n", i,
			num_tm_nodes);
	if (i==(num_tm_nodes-1)) {
		//all the clients have reached the barrier
		*apps = 0;
		aux = apps;
		apps = apps2;
		apps2 = aux; 
		return;
	}

	while (*apps!=0);

	aux = apps;
	apps = apps2;
	apps2 = aux;
}

void
global_barrier()
{
	nodeid_t i = __sync_fetch_and_add(nodes,1);
	fprintf(stderr, "in global_barrier, i=%u, num_tm_nodes = %u, num_dsl_nodes = %u\n", i,
			num_tm_nodes, num_dsl_nodes);
	if (i==(num_tm_nodes+num_dsl_nodes-1)) {
		*nodes = 0;
		auxNodes = nodes;
		nodes = nodes2;
		nodes2 = auxNodes;
		return;
	}

	while (*nodes!=0);

	auxNodes = nodes;
	nodes = nodes2;
	nodes2= auxNodes;
}
