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
#include <task.h>

#include "common.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "pgas.h"
#include "mcore_malloc.h"

// barrier stuff
#define MAX_FILENAME_LENGTH 1024

// libtask related stuff 
#define STACK (32*1024)

struct conn_spec {
	int proto;
	int port;
	char address[MAX_FILENAME_LENGTH];
};

static int dsl_nodes_sockets[MAX_NODES]; // holds the sockets to all dsl nodes at app node

static int parse_conn_spec_string(char* spec, struct conn_spec* out);
void dsl_listentask(void* v);

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
	if (*argc < 2) {
		fprintf(stderr, "Not enough parameters (%d)\n", *argc);
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s -total=TOTAL_NODES ...\n", argv[0]);
		EXIT(1);
	}

	int p = 1;
	int found = 0;
	while (p < *argc) {
		if (strncmp("-total=", argv[p], strlen("-total=")) == 0) {
			char *cf = argv[p] + strlen("-total=");
			MY_TOTAL_NODES = atoi(cf);
			argv[p] = NULL;
			found = 1;
		}
		p++;
	}
	if (!found) {
		fprintf(stderr, "Did not pass all parameters\n");
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s -total=TOTAL_NODES ...\n", argv[0]);
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

	MY_NODE_ID = 0;

	nodeid_t rank;
	for (rank = 1; rank < MY_TOTAL_NODES; rank++) {
		PRINTD("Forking child %u", rank);
		pid_t child = fork();
		if (child < 0) {
			PRINT("Failure in fork():\n%s", strerror(errno));
		} else if (child == 0) {
			goto fork_done;
		}
	}
	rank = 0;

fork_done:
	PRINTD("Initializing child %u", rank);
	MY_NODE_ID = rank;

	// Now, pin the process to the right core (NODE_ID == core id)
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(NODE_ID(), &mask);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
		PRINT("Problem with setting processor affinity: %s\n",
			  strerror(errno));
		EXIT(3);
	}
}

void
term_system()
{
	shm_unlink("/appBarrier");
	shm_unlink("/globalBarrier");
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

void
sys_tm_init()
{
	init_barrier();
}

void
sys_ps_init_(void)
{
	// Setup connection to the dsl nodes
	// first, sleep a bit to give dsl nodes time to start
	BARRIERW
	unsigned int j;
	for (j = 0; j < NUM_UES; j++) {
		// initialize the sockets to all dsl nodes.
		// read the configuration, and get the address from there
		if (j % DSLNDPERNODES == 0) {
			char send_spec_path[256];
			snprintf(send_spec_path, 255, "nodes.[%d].conn_spec", j); // extra check if the node is right
			const char* send_spec_string = NULL;
			if (config_lookup_string(the_config, send_spec_path, &send_spec_string) == CONFIG_FALSE) {
				PRINT("Could not read the parameter from the config file: %s\n", send_spec_path);
				EXIT(2);
			}
			PRINTD("Connecting to %d: %s\n", j, send_spec_string);

			struct conn_spec spec;

			if (parse_conn_spec_string(send_spec_string, &spec) != 0) {
				PRINT("Problem parsing specification string <%s>", send_spec_string);
				EXIT(1);
			}

			int rfd = -1;
redo_netdial:
			if ((rfd = netdial(spec.proto, spec.address, spec.port)) < 0) {
				if (errno == ECONNREFUSED) {
					sleep(1);
					PRINT("ps_init: need to redial");
					goto redo_netdial;
				}

				PRINT("ps_init: problem with netdial on %d %s:%d",
					  spec.proto, spec.address, spec.port);
				PRINT("libtask: %s", taskgetstate());
				PRINT("error: %s", strerror(errno));
				close(rfd);
				EXIT(1);
			}
			fdnoblock(rfd);
			dsl_nodes_sockets[j] = rfd;
		} else {
			dsl_nodes_sockets[j] = -1;
		}
	}
	MCORE_shmalloc_init(100000*sizeof(int));
	PRINTD("sys_ps_init: done");
}

struct conn_spec* dsl_spec;

void
sys_dsl_init(void)
{
	char conn_spec_path[256];
	snprintf(conn_spec_path, 255, "nodes.[%d].conn_spec", ID);
	const char* conn_spec_string = NULL;
	if (config_lookup_string(the_config, conn_spec_path, &conn_spec_string) == CONFIG_FALSE) {
		PRINT("Could not read the parameter from the config file: %s\n", conn_spec_path);
		EXIT(2);
	}

	// Now, allocate and setup a socket, to reply to clients
	dsl_spec = (struct conn_spec*)malloc(sizeof(struct conn_spec));
	if (dsl_spec == NULL) {
		PRINT("sys_dsl_init: problem allocating memory");
		EXIT(1);
	}

	if (parse_conn_spec_string(conn_spec_string, dsl_spec) != 0) {
		PRINT("Problem parsing specification string <%s>", conn_spec_string);
		EXIT(1);
	}
}

void
sys_dsl_term(void)
{
	nodeid_t j;
	for (j = 0; j < NUM_UES; j++) {
		if (j % DSLNDPERNODES != 0) {
		}
	}
}

void
sys_ps_term(void)
{
	nodeid_t j;
	for (j = 0; j < NUM_UES; j++) {
		if (j % DSLNDPERNODES == 0) {
		}
	}
}

int
sys_sendcmd(void* data, size_t len, nodeid_t to)
{
	assert((to>=0)&&(to<TOTAL_NODES()));
	assert(dsl_nodes_sockets[to]!=-1);

	PRINTD("sys_sendcmd: to: %u\n", to);

	size_t error = 0;
	while (error < len) {
		error = fdwrite(dsl_nodes_sockets[to], data, len);
	}

	PRINTD("sys_sendcmd: sent %d", error);
	return (error==len)?0:-1;
}

int
sys_sendcmd_all(void* data, size_t len)
{
	int rc = 0;

	nodeid_t to;
	for (to=0; to < TOTAL_NODES(); to++) {
		if (to % DSLNDPERNODES == 0) {
			PRINTD("sys_sendcmd_all: to = %u, rc = %d", to, rc);
			sys_sendcmd(data, len, to);
		}
	}

	return rc;
}

int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
	PRINTD("sys_recvcmd: from = %u", from);

	int ret = fdread(dsl_nodes_sockets[from], data, len);
	if (ret >= sizeof(PS_COMMAND)) {
		return 0;
	} else {
		PRINT("sys_recvcmd: problem receiving command: (%d, %u) %s",
			  ret, sizeof(PS_COMMAND), strerror(errno));
		return -1;
	}
}

// If value == NULL, we just return the address.
// Otherwise, we return the value.
static inline void 
sys_ps_command_reply(int fd,
                    PS_COMMAND_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response)
{
	PS_COMMAND reply;
	reply.nodeId = ID;
	reply.type = command;
	reply.response = response;

    PRINTD("sys_ps_command_reply: src=%u target=%d", reply.nodeId, fd);
#ifdef PGAS
	if (value != NULL) {
		reply.value = *value;
		PRINTD("sys_ps_command_reply: read value %u\n", reply.value);
	} else {
		reply.address = (uintptr_t)address;
	}
#else
	reply.address = (uintptr_t)address;
#endif

	size_t error = 0;
	while (error < sizeof(PS_COMMAND)) {
		error = fdwrite(fd, (char*)&reply, sizeof(PS_COMMAND));
	}
}


void
dsl_communication()
{
	/* Here, it does nothing */
	/*tasksystem();*/
	dsl_listentask((void*)dsl_spec);
}


static void
process_message(PS_COMMAND* ps_remote, int fd)
{
	nodeid_t sender = ps_remote->nodeId;
	PRINTD("process_message: got message %d from %u", ps_remote->type, sender);

	switch (ps_remote->type) {
		case PS_SUBSCRIBE:

#ifdef DEBUG_UTILIZATION
			read_reqs_num++;
#endif

#ifdef PGAS
			sys_ps_command_reply(fd, PS_SUBSCRIBE_RESPONSE,
								 (tm_addr_t)ps_remote->address, 
								 PGAS_read(ps_remote->address),
								 try_subscribe(sender, ps_remote->address));
#else
			sys_ps_command_reply(fd, PS_SUBSCRIBE_RESPONSE, 
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
				sys_ps_command_reply(fd, PS_PUBLISH_RESPONSE, 
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
				sys_ps_command_reply(fd, PS_PUBLISH_RESPONSE,
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
			break;
		case PS_UNSUBSCRIBE:
			ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
			break;
		case PS_PUBLISH_FINISH:
			ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
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

/*
 * expects:
 * tcp://server:port
 */
static int
parse_conn_spec_string(char* spec, struct conn_spec* out)
{
	char *pos = spec;
	if (strncmp(pos, "tcp://", 6) == 0) {
		out->proto = TCP;
	} else if (strncmp(pos, "udp://", 6) == 0) {
		out->proto = UDP;
	} else {
		PRINT("Unknown protocol: %s", spec);
		return -1;
	}
	pos += 6;

	char* colon = pos;
	if ((colon = strchr(pos, ':')) != NULL) {
		*colon = '\0';
		size_t len = colon-pos+1;
		strncpy(out->address, pos, len);
		*colon = ':';
	} else {
		PRINT("No port specified: %s", spec);
		return -1;
	}
	colon++;

	out->port = atoi(colon);
	return 0;
}

void
dsl_recvtask(void* cfd)
{
	long tmp = (long)cfd;
	int remotefd = tmp;
	PS_COMMAND* ps_remote = (PS_COMMAND*)malloc(sizeof(PS_COMMAND));

	while (1) {
		size_t ret = fdread(remotefd, ps_remote, sizeof(PS_COMMAND));
		if (ret >= sizeof(PS_COMMAND)) {
			PRINTD("dsl_recvtask(): one message (size %d) received on the socket fd=%d\n",
				   ret, remotefd);
			//DS_ITimer::handle_timeouts();
#ifdef DEBUG_MSG
			{
				int i;
				char* p=(char*)ps_remote;
				fprintf(stderr, "psc -> [");
				for (i=0; i<sizeof(PS_COMMAND); i++) {
					fprintf(stderr, "%02hhx ", p[i]);
				}
				fprintf(stderr, "]\n");
			};
#endif

			process_message(ps_remote, remotefd);
			taskyield();
			PRINTD("dsl_recvtask(): processing message is finished fd=%d\n", remotefd);
		} else {
			if (ret == 0) {
				PRINT("dsl_recvtask(): returning\n");
				return;
			}
			PRINT("dsl_recvtask(): [ERROR] fdread ret=%d\n", ret);
		}
	}
}

void
dsl_listentask(void* v)
{
	int cfd, fd;
	struct conn_spec* spec = (struct conn_spec*)v;

	taskname("dsl_listentask(%s:%d)",spec->address, spec->port);

	if((fd = netannounce(spec->proto,
						 spec->address,
						 spec->port)) < 0) {
		PRINT("cannot announce on tcp port %d: %s\n",
			  spec->port, strerror(errno));
		taskexitall(1);
	}
	PRINTD("dsl_listentask: listening on tcp port %d\n", spec->port);

	fdnoblock(fd);
	char remote[16];
	int rport;
	while((cfd = netaccept(fd, remote, &rport)) >= 0) {
		PRINTD("connection from %s:%d in socket %d\n", 
			   remote, rport, cfd);
		fdnoblock(cfd);
		taskcreate(dsl_recvtask, (void*)cfd, STACK);
		taskyield();
	}
}


