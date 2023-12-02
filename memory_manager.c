//
// Created by Aarya Reddy
//


#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched/mm.h>
#include <linux/module.h>



static int pid = 0;
static struct task_struct* task = NULL;
static int test_case;
static struct hrtimer no_restart_hr_timer;

module_param(pid, int, 0);

// Initialize memory statistics variables
unsigned long total_rss = 0;
unsigned long total_swap = 0;
unsigned long total_wss = 0;

/* Test and clear the accessed bit of a given pte entry. vma is the pointer
to the memory region, addr is the address of the page, and ptep is a pointer
to a pte. It returns 1 if the pte was accessed, or 0 if not accessed. */
/* The ptep_test_and_clear_young() is architecture dependent and is not
exported to be used in a kernel module. You will need to add its
implementation as follows to your kernel module. */
int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep)
{
	int ret = 0;
	if (pte_young(*ptep))
		ret = test_and_clear_bit(_PAGE_BIT_ACCESSED,(unsigned long *) &ptep->pte);
	return ret;
}




static void parse_vma(void)
{
    struct vm_area_struct *vma = NULL;
    struct mm_struct *mm = NULL;

    if(pid > 0){
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        if(task && task->mm){
            mm = task->mm;
            // TODO 2: mm_struct to initialize the VMA_ITERATOR (-- Assignment 4)
            // Hint: Only one line of code is needed
            VMA_ITERATOR(vmi,mm,0);


            // TODO 3: Iterate through the VMA linked list with for_each_vma (-- Assignment 4)
            // Hint: Only one line of code is needed
	
            for_each_vma(vmi,vma){
                // TODO 4: Iterate through each page of the VMA 
                // declear "page" as unsigned long, start from vma->vm_start to vma->vm_end with PAGE_SIZE step 
                // Hint: Only one line of code is needed

                unsigned long vpage;
                for(vpage = vma->vm_start; vpage < vma->vm_end; vpage += PAGE_SIZE)
                {
                    // TODO 5: Use pgd_offset, p4d_offset, pud_offset, pmd_offset, pte_offset_map to get the page table entry
                    // Hint: Copy from Background Knowledge in the instruction
                    // Hint: change the address in the instruction to "page" variable you defined above
                    
                    pgd_t *pgd;
                    p4d_t *p4d;
                    pmd_t *pmd;
                    pud_t *pud;
                    pte_t *ptep, pte;
                    
                    pgd = pgd_offset(mm, vpage); // get pgd from mm and the page address
                    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
                    // check if pgd is bad or does not exist
                        continue;
                    }
                    
                    p4d = p4d_offset(pgd, vpage); //get p4d from from pgd and the page address
                        if (p4d_none(*p4d) || p4d_bad(*p4d)) {
                        // check if p4d is bad or does not exist
                        continue;
                    }
                        
                    pud = pud_offset(p4d, vpage); // get pud from from p4d and the page address
                        if (pud_none(*pud) || pud_bad(*pud)) {
                        // check if pud is bad or does not exist
                        continue;
                    }

                    pmd = pmd_offset(pud, vpage); // get pmd from from pud and the page address
                    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
                        // check if pmd is bad or does not exist
                        continue;
                    }

                    ptep = pte_offset_kernel(pmd, vpage); // get pte from pmd and the page address

                    pte = *ptep;
                    
                    // TODO 6: use pte_none(pte) to check if the page table entry is valid
                    // Hint: one if statement here
                        
                    if (pte_none(pte)){
                    	continue;
                    }
                    // TODO 7: use pte_present(pte) to check if the page is in memory, otherwise it is in swap
                    // TODO 8: use pte_young(pte) to check if the page is actively used
                    // if it is actively used, update wss and clear the accessed bit by: "test_and_clear_bit(_PAGE_BIT_ACCESSED,(unsigned long *)ppte);"
         
		            if (pte_present(pte)) {
		            	total_rss++;
		                if (pte_young(pte)) {
		                    total_wss++;
		                    //ptep_test_and_clear_young(vma,vpage,ptep);
		                    test_and_clear_bit(_PAGE_BIT_ACCESSED,(unsigned long *) &ptep->pte);

		                }
		                
		            } else {
		                total_swap++;
		            }
                   
                }
            } 
        }
    }
}

unsigned long timer_interval_ns = 10e9; // 10 sec timer
static struct hrtimer hr_timer;

enum hrtimer_restart timer_callback( struct hrtimer *timer_for_restart )
{
    ktime_t currtime , interval;
    currtime  = ktime_get();
    interval = ktime_set(0,timer_interval_ns);
    hrtimer_forward(timer_for_restart, currtime , interval);
    total_rss = 0;
    total_swap = 0;
    total_wss = 0;
    parse_vma();
    printk("[PID-%i]:[RSS:%lu KB] [Swap:%lu KB] [WSS:%lu KB]\n", pid, total_rss*4, total_swap*4, total_wss*4);
    return HRTIMER_RESTART;
}


int memory_init(void){
    printk("CSE330 Project 3 Kernel Module Inserted\n");
    ktime_t ktime;
    ktime = ktime_set( 0, timer_interval_ns );
    hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
    hr_timer.function = &timer_callback;
    hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL );
    return 0;
}


void memory_cleanup(void){
    int ret;
    ret = hrtimer_cancel( &hr_timer );
    if (ret) printk("HR Timer cancelled ...\n");
    printk("CSE330 Project 3 Kernel Module Removed\n");
}


module_init(memory_init);
module_exit(memory_cleanup);

MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AaryaReddy");
MODULE_DESCRIPTION("CSE330 Project 3 Memory Management\n");
