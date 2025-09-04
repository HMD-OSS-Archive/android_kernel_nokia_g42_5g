/*

store kernel panic info to sharememory.

*/
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/sched.h>
#include <asm/stacktrace.h>


#include "smem_type.h"

// increase crash buffer for store linux banner&subsys crash reason zzw 20200528
#define CRASH_BUFFER_SIZE		1024+256

static struct pt_regs panic_pt_regs;
static struct pt_regs *panic_pt_regs_ptr = NULL;
static char crash_message[CRASH_BUFFER_SIZE] = {0};
bool fatal_reboot_state = false;

/*

store crash log to sharememory 
(now use SMEM_RESERVED_RESET_LOG, for sm7250. other platform may need change)

*/
void store2smem()
{
	int ret;
	unsigned int *pBuffer = NULL;
	size_t size = min(strlen(crash_message) + 1, sizeof(crash_message));
	
	ret = qcom_smem_alloc(QCOM_SMEM_HOST_ANY, SMEM_RESERVED_RESET_LOG, size);
	if(ret < 0)
	{
		pr_err("Failed to alloc SMEM_RESERVED_RESET_LOG  sharememory\n");
        return;
	}
	
	pBuffer = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_RESERVED_RESET_LOG, &size);
    if (IS_ERR(pBuffer)) {
        pr_err("Failed to acquire SMEM_RESERVED_RESET_LOG entry\n");
        return;
    }
    if(fatal_reboot_state == true)
    {
        strcpy((char *)pBuffer, "FatalReboot:");    	
    }
    else
    {		
	      strcpy((char *)pBuffer, "CRASH:");
	  }
    memcpy((char *)pBuffer+strlen((char *)pBuffer), crash_message, size);
}


void store_crash_message(const char *fmt, ...) {
    size_t size;
    va_list args;

	size = sizeof(crash_message) - strlen(crash_message);
    if (size <= 0)
        return;

    va_start(args, fmt);
    vsnprintf(crash_message + strlen(crash_message), size, fmt, args);
    va_end(args);
}


void dump_kernel_fault(const char *desc, unsigned long addr) 
{
    store_crash_message("Unable to handle kernel %s at virtual address %016lx\n", desc, addr);
}


void dump_kernel_die(const char *desc, int err, struct pt_regs *regs) 
{
    store_crash_message("Internal error: %s: %x\n", desc, err);
    memcpy(&panic_pt_regs, regs, sizeof(struct pt_regs));
    panic_pt_regs_ptr = &panic_pt_regs;
}


// store subsystem fail reason to RESET LOG buffer zzw 20200528 begin
void dump_subsys_fault_reason(const char *desc, const char * reason)
{
    store_crash_message("%s subsystem failure reason: %s.\n", desc, reason);
}
// store subsystem fail reason to RESET LOG buffer zzw 20200528 end

void store_kernel_panic_smem(const char *desc) {
    struct stackframe frame;
    struct task_struct *tsk = current;
    struct pt_regs *regs = panic_pt_regs_ptr;
    int skip = 0;

	store_crash_message("software version: %s\n", HMD_SW_VERSION);

    // store linux banner to RESET LOG zzw 20200528 add
    store_crash_message("linux banner: %s\n", linux_banner);
    
    if(fatal_reboot_state == true)
    {
        store_crash_message("FatalReboot: %s\n", desc);	
    }
    else
    {		
		    store_crash_message("Kernel panic - not syncing: %s\n", desc);
		    store_crash_message("CPU: %d PID: %d Comm: %.20s\n",
		            raw_smp_processor_id(), tsk->pid, tsk->comm);
		
		    if (regs != NULL) {
				store_crash_message("pc : %pS\n", (void *)regs->pc);
		#ifdef CONFIG_ARM64
				store_crash_message("lr : %pS\n", (void *)regs->regs[30]);
		#endif
		        store_crash_message("sp : %016llx\n", regs->sp);
		        
				skip = 1;
		    }
		
		    if (!try_get_task_stack(tsk))
		        return;
		
		    frame.fp = (unsigned long)__builtin_frame_address(0);
		    frame.pc = (unsigned long)store_kernel_panic_smem;
		
		    store_crash_message("CALL TRACK:\n");
		
		    do {
		        if (!skip) {
		            store_crash_message(" %pS\n", (void *)frame.pc);
		        } else if (frame.fp == frame_pointer(regs)) {
		            skip = 0;
		            store_crash_message(" %pS\n", (void *)regs->pc);
		        }
		    } while (!unwind_frame(tsk, &frame));
		
		    put_task_stack(tsk);
    }
    store2smem();

    panic_pt_regs_ptr = NULL;
}

void dump_set_fatal_reboot(const char *desc, const char * reason)
{
    fatal_reboot_state = true;
}

