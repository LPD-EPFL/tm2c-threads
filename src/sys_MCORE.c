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
#include <shrimp/shrimp.h>

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

int nodes_sockets[MAX_NODES]; // holds the sockets (both app and dsl
                              // nodes use them

static struct conn_spec* my_conn_spec; // holds connetion spec for current node

Rendez* got_message; // tasks sync on this var, when there is a new msg
PS_REPLY* ps_remote_msg; // holds the received msg

static int my_fd; // the fd on which the nodes listens

void dsl_recvtask(void* v);
void app_recvtask(void* v);

static void dsl_listentask(void* v);
static void app_listentask(void* v);
static int parse_conn_spec_string(char* spec, struct conn_spec* out);
static void init_my_conn_spec(void);
static int dial_node(nodeid_t);
static int init_connection();
INLINED void process_message(PS_COMMAND* ps_remote, int fd);
INLINED void sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response);
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
	int place;
	if (rank%2 != 0) {
		place = MY_TOTAL_NODES/2+rank/2;
	} else {
		place = rank/2;
	}
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(place, &mask);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
		PRINT("Problem with setting processor affinity: %s\n",
			  strerror(errno));
		EXIT(3);
	}

	got_message = (Rendez*)calloc(1, sizeof(Rendez));
	if (got_message == NULL) {
		PRINT("Problem allocating memory for got_message Rendez");
		EXIT(3);
	}

	// initialize the connection data
	init_my_conn_spec();

	my_fd = init_connection();
}

void
term_system()
{
	shm_unlink("/appBarrier");
	shm_unlink("/globalBarrier");
	taskexitall(0);
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

void
sys_tm_init()
{
}

void
sys_ps_init_(void)
{
	BARRIERW
	taskcreate(app_listentask, (void*)my_conn_spec, STACK);

	// Setup connection to the dsl nodes
	nodeid_t j;
	for (j = 0; j < NUM_UES; j++) {
		// initialize the sockets to all dsl nodes.
		if (j % DSLNDPERNODES == 0) {
			int rfd = dial_node(j);
			fdnoblock(rfd);
			nodes_sockets[j] = rfd;
		} else {
			nodes_sockets[j] = -1;
		}
	}
	MCORE_shmalloc_init(100000*sizeof(int));

	ps_remote_msg = NULL;
	PRINTD("sys_ps_init: done");
}

void
sys_dsl_init(void)
{
	BARRIERW

	// Setup connection to the app nodes
	nodeid_t j;
	for (j = 0; j < NUM_UES; j++) {
		// initialize the sockets to all app nodes.
		if (j % DSLNDPERNODES != 0) {
			int rfd = dial_node(j);
			fdnoblock(rfd);
			nodes_sockets[j] = rfd;
		} else {
			nodes_sockets[j] = -1;
		}
	}

}

void
sys_dsl_term(void)
{
	nodeid_t j;
	for (j = 0; j < NUM_UES; j++) {
		if (j % DSLNDPERNODES != 0) {
			if (nodes_sockets[j] != -1)
				netclose(nodes_sockets[j]);
		}
	}
	taskexit(0);
}

void
sys_ps_term(void)
{
	nodeid_t j;
	for (j = 0; j < NUM_UES; j++) {
		if (j % DSLNDPERNODES == 0) {
			if (nodes_sockets[j] != -1)
				netclose(nodes_sockets[j]);
		}
	}
	taskexit(0);
}

// If value == NULL, we just return the address.
// Otherwise, we return the value.
INLINED void 
sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response)
{
	PS_REPLY reply;
	reply.nodeId = ID;
	reply.type = command;
	reply.response = response;

	PRINTD("sys_ps_command_reply: src=%u target=%d", reply.nodeId, sender);
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

	int fd = nodes_sockets[sender];
	size_t error = 0;
	while (error < sizeof(PS_REPLY)) {
		error = fdwrite(fd, (char*)&reply, sizeof(PS_REPLY));
	}
   struct timeval s;
}


void
dsl_communication()
{
	/* Here, it does nothing */
	dsl_listentask((void*)my_conn_spec);
}


INLINED void
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
			sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
								 (tm_addr_t)ps_remote->address, 
								 PGAS_read(ps_remote->address),
								 try_subscribe(sender, ps_remote->address));
#else
			sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, 
								 (tm_addr_t)ps_remote->address, 
								 NULL,
								 //try_subscribe(sender, ps_remote->address));
                         NO_CONFLICT
                         );
#endif
			break;
		case PS_PUBLISH:
			{

         //fprintf(stderr,"publish at address %u\n",ps_remote->address);
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
			break;
		case PS_UNSUBSCRIBE:
			ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
			break;
		case PS_PUBLISH_FINISH:
			ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
			break;
		case PS_STATS:
			if (ps_remote->tx_duration) {
			    stats_aborts += ps_remote->aborts;
			    stats_commits += ps_remote->commits;
			    stats_duration += ps_remote->tx_duration;
			    stats_max_retries = stats_max_retries < ps_remote->max_retries ? ps_remote->max_retries : stats_max_retries;
			    stats_total += ps_remote->commits + ps_remote->aborts;
			  }
			  else {
			    stats_aborts_raw += ps_remote->aborts_raw;
			    stats_aborts_war += ps_remote->aborts_war;
			    stats_aborts_waw += ps_remote->aborts_waw;
			  }
			  if (++stats_received >= 2*NUM_APP_NODES) {
			    if (NODE_ID() == 0) {
			      print_global_stats();

			      print_hashtable_usage();

			    }

#ifdef DEBUG_UTILIZATION
				PRINT("*** Completed requests: %d", read_reqs_num + write_reqs_num);
#endif

				taskexitall(0);
			}
		default:
			PRINTD("REMOTE MSG: ??");
	}
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

void 
udelay(unsigned int micros)
{
   double __ts_end = theTime() + ((double) micros / 1000000);
   while (theTime() < __ts_end);
   //usleep(micros);
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
	} else if (strncmp(pos, "shrimp://", 9) == 0) {
		out->proto = SHRIMP_PROTO;
		pos += 3;
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

static int
dial_node(nodeid_t id)
{
	char send_spec_path[256];
	snprintf(send_spec_path, 255, "nodes.[%u].conn_spec", id); // extra check if the node is right
	const char* send_spec_string = NULL;
	if (config_lookup_string(the_config, send_spec_path, &send_spec_string) == CONFIG_FALSE) {
		PRINT("Could not read the parameter from the config file: %s\n", send_spec_path);
		EXIT(2);
	}
	PRINTD("Connecting to %u: %s\n", id, send_spec_string);

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
		EXIT(1);
	}
	return rfd;
}

static void
init_my_conn_spec(void)
{
	char conn_spec_path[256];
	snprintf(conn_spec_path, 255, "nodes.[%d].conn_spec", NODE_ID());
	const char* conn_spec_string = NULL;
	if (config_lookup_string(the_config, conn_spec_path, &conn_spec_string) == CONFIG_FALSE) {
		PRINT("Could not read the parameter from the config file: %s\n", conn_spec_path);
		EXIT(2);
	}

	// Now, allocate and setup a socket, to reply to clients
	my_conn_spec = (struct conn_spec*)malloc(sizeof(struct conn_spec));
	if (my_conn_spec == NULL) {
		PRINT("init_my_conn_spec: problem allocating memory");
		EXIT(1);
	}

	if (parse_conn_spec_string(conn_spec_string, my_conn_spec) != 0) {
		PRINT("init_my_conn_spec: Problem parsing specification string <%s>",
			  conn_spec_string);
		EXIT(1);
	}
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
			PRINTD("dsl_recvtask(): processing message is finished fd=%d\n", remotefd);
		} else {
			if (ret == 0) {
				PRINTD("dsl_recvtask(): exiting\n");
				taskexit(0);
			}
			PRINT("dsl_recvtask(): [ERROR] fdread ret=%d\n", ret);
		}
	}
}

static int 
init_connection()
{
	int fd;
	if((fd = netannounce(my_conn_spec->proto,
						 my_conn_spec->address,
						 my_conn_spec->port)) < 0) {
		PRINT("cannot announce on port %d: %s\n",
			  my_conn_spec->port, strerror(errno));
		EXIT(1);
	}
	PRINTD("start_listening: %d port %d\n",
		   my_conn_spec->proto,
		   my_conn_spec->port);

	return fd;
}

void
dsl_listentask(void* v)
{
	taskname("dsl_listentask(%s:%d)",
			 my_conn_spec->address,
			 my_conn_spec->port);

	int cfd;

	char remote[16];
	int rport;
	fdnoblock(my_fd);
	while((cfd = netaccept(my_fd, remote, &rport)) >= 0) {
		PRINTD("connection from %s:%d in socket %d\n", 
			   remote, rport, cfd);
		fdnoblock(cfd);
		taskcreate(dsl_recvtask, (void*)cfd, STACK);
	}
}

void
app_recvtask(void* cfd)
{
	long tmp = (long)cfd;
	int remotefd = tmp;
	PS_REPLY* ps_remote = (PS_REPLY*)malloc(sizeof(PS_REPLY));

	taskname("app_recvtask(%d)", remotefd);

	fdnoblock(remotefd);
	while (1) {
		size_t ret = fdread(remotefd, ps_remote, sizeof(PS_REPLY));
		if (ret >= sizeof(PS_REPLY)) {
			PRINTD("app_recvtask(): one message (size %d) received on the socket fd=%d\n",
				   ret, remotefd);
			//DS_ITimer::handle_timeouts();
#ifdef DEBUG_MSG
			{
				int i;
				char* p=(char*)ps_remote;
				fprintf(stderr, "psc -> [");
				for (i=0; i<sizeof(PS_REPLY); i++) {
					fprintf(stderr, "%02hhx ", p[i]);
				}
				fprintf(stderr, "]\n");
			};
#endif

			ps_remote_msg = ps_remote;
			taskwakeup(got_message);
			PRINTD("app_recvtask(): processing message is finished fd=%d\n", remotefd);
		} else {
			if (ret == 0) {
				PRINTD("app_recvtask(): exit\n");
				taskexit(0);
			}
			PRINT("app_recvtask(): [ERROR] fdread ret=%d\n", ret);
		}
	}
}

void
app_listentask(void* v)
{
	taskname("app_listentask(%s:%d)",
			 my_conn_spec->address,
			 my_conn_spec->port);

	int cfd;

	char remote[16];
	int rport;
	fdnoblock(my_fd);
	tasksystem();
	while((cfd = netaccept(my_fd, remote, &rport)) >= 0) {
		PRINTD("connection from %s:%d in socket %d\n", 
			   remote, rport, cfd);
		fdnoblock(cfd);
		taskcreate(app_recvtask, (void*)cfd, STACK);
		taskyield();
	}
}

