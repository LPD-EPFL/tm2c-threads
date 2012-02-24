#include "common.h"
#include "iRCCE.h"

void 
init_system(int* argc, char** argv[])
{
	RCCE_init(argc, argv);
	iRCCE_init();
}

void
term_system()
{
	// noop
}

sys_t_vcharp 
sys_shmalloc(size_t size)
{
	return RCCE_shmalloc(size);
}

int
sys_shfree(sys_t_vcharp ptr)
{
	RCCE_shfree(ptr);
}

