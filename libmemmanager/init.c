#include "api.h"
#include "vmm.h"

SetUpConfig setUpConfig;
FIFOPhysicalFrame *phyFrameLLHead = NULL;
ThirdChanceFrame *thirdChanceHead = NULL;

void mm_init(enum policy_type policy, void *vm, int vm_size,
             int num_frames, int page_size, struct mm_stats *stats) {
    
    setUpConfig.policy = policy;
    setUpConfig.vm = vm;
    setUpConfig.vm_size = vm_size;
    setUpConfig.num_frames = num_frames;
    setUpConfig.page_size = page_size;
    setUpConfig.stats = stats;

    mprotect(setUpConfig.vm, (size_t)setUpConfig.vm_size, PROT_NONE);
    
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segfault_handler;
    
    int ret = sigaction(SIGSEGV, &sa, NULL);
    
    if (ret != 0) {
        fprintf(stderr, "sigaction() error .\n");
        exit(1);
    }

    return;
}

void mm_finish() {
    if (setUpConfig.policy == MM_FIFO) {
        FIFOPhysicalFrame *curr = phyFrameLLHead;
        while (curr != NULL) {
            FIFOPhysicalFrame *next = curr->next;
            free(curr);
            curr = next;
        }
        phyFrameLLHead = NULL;
    } else if (setUpConfig.policy == MM_THIRD) {
        if (thirdChanceHead != NULL) {
            ThirdChanceFrame *curr = thirdChanceHead->next;
    
            while (curr != thirdChanceHead) {
                ThirdChanceFrame *next_node = curr->next;
                free(curr);
                curr = next_node;
                next_node = next_node->next;
            }
            free(thirdChanceHead);
        }
        thirdChanceHead = NULL;
    }
    return;
}