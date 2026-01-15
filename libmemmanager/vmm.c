#include "vmm.h"

void insertInLinkedList(int virtualPageNumber, bool is_write, int frame_number) {
    FIFOPhysicalFrame *new_node = (FIFOPhysicalFrame *)malloc(sizeof(FIFOPhysicalFrame));
    new_node->virtualPageNumber = virtualPageNumber;
    new_node->next = NULL;
    new_node->isWritePerformed = false;
    new_node->frame_number = frame_number;

    if (phyFrameLLHead == NULL) {
        phyFrameLLHead = new_node;
    } else {
        FIFOPhysicalFrame *curr = phyFrameLLHead;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_node;
    }

    new_node->isWritePerformed = is_write;
}

FIFOPhysicalFrame* removeFromLinkedList() {
    if (phyFrameLLHead == NULL) {
        return NULL;
    }
    FIFOPhysicalFrame *curr = phyFrameLLHead;
    phyFrameLLHead = phyFrameLLHead->next;
    return curr;
}

FIFOPhysicalFrame* getPhysicalFrame(int virtualPageNumber) {
    FIFOPhysicalFrame *curr = phyFrameLLHead;
    while (curr != NULL) {
        if (curr->virtualPageNumber == virtualPageNumber) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

int countLinkedListNodes() {
    int count = 0;
    FIFOPhysicalFrame *curr = phyFrameLLHead;
    while (curr != NULL) {
        count++;
        curr = curr->next;
    }
    return count;
}

ThirdChanceFrame* getThirdChanceFrame(int virtualPageNumber) {
    if (thirdChanceHead == NULL) {
        return NULL;
    }

    if (thirdChanceHead->virtualPageNumber == virtualPageNumber) {
        return thirdChanceHead;
    }

    ThirdChanceFrame *curr = thirdChanceHead->next;

    while (curr != thirdChanceHead) {
        if (curr->virtualPageNumber == virtualPageNumber) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;

}

int countThirdChanceNodes() {
    if (thirdChanceHead == NULL) {
        return 0;
    }
    
    int count = 1;
    ThirdChanceFrame *curr = thirdChanceHead->next;
    while (curr != thirdChanceHead) {
        count++;
        curr = curr->next;
    }

    return count;
}

void insertThirdChanceFrame(int virtualPageNumber, int is_write, int frame_number) {
    ThirdChanceFrame *new_node = (ThirdChanceFrame *)malloc(sizeof(ThirdChanceFrame));
    new_node->virtualPageNumber = virtualPageNumber;
    new_node->frame_number = frame_number;
    new_node->R = 1;
    new_node->M = is_write;
    new_node->pass_count = 0;
    
    if (thirdChanceHead == NULL) {
        new_node->next = new_node;
        thirdChanceHead = new_node;
    } else {
        ThirdChanceFrame *curr = thirdChanceHead;
        while (curr->next != thirdChanceHead) {
            curr = curr->next;
        }
        curr->next = new_node;
        new_node->next = thirdChanceHead;
    }
}

ThirdChanceFrame* findEvictionNode() {
    if (thirdChanceHead == NULL) {
        return NULL;
    }
    
    uint64_t base = (uint64_t)setUpConfig.vm;
    
    while (thirdChanceHead != NULL) {
        // printf("Curr vp: %d \n" , thirdChanceHead->virtualPageNumber);
        // printf("R bit: %d , M bit: %d , Pass count: %d \n" , thirdChanceHead->R, thirdChanceHead->M, thirdChanceHead->pass_count);
        
        // (0,0) - direct evict
        if (thirdChanceHead->R == 0 && thirdChanceHead->M == 0) {
            ThirdChanceFrame *node = thirdChanceHead;
            thirdChanceHead = thirdChanceHead->next;
            return node;
        }

        // (1,0) - (0,0)
        if (thirdChanceHead->R == 1 && thirdChanceHead->M == 0) {
            thirdChanceHead->R = 0;
            thirdChanceHead->pass_count = 0;

            mprotect((void *)(base + (uint64_t)thirdChanceHead->virtualPageNumber * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_NONE);
            thirdChanceHead = thirdChanceHead->next;
            continue;
        }

        // (1,1) - (0,1) and pc = 1
        if (thirdChanceHead->R == 1 && thirdChanceHead->M == 1) {
            thirdChanceHead->R = 0;
            thirdChanceHead->pass_count = 1;

            mprotect((void *)(base + (uint64_t)thirdChanceHead->virtualPageNumber * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_NONE);
            thirdChanceHead = thirdChanceHead->next;
            continue;
        }

        // (0,1) - pc++ and pc = 2 evict
        if (thirdChanceHead->R == 0 && thirdChanceHead->M == 1) {
            if (thirdChanceHead->pass_count == 0) {
                thirdChanceHead->pass_count = 1;
                thirdChanceHead = thirdChanceHead->next;
                continue;
            } else if (thirdChanceHead->pass_count == 1) {
                thirdChanceHead->pass_count = 2;
                thirdChanceHead = thirdChanceHead->next;
                continue;
            } else {
                ThirdChanceFrame *node = thirdChanceHead;
                thirdChanceHead = thirdChanceHead->next;
                return node;
            }
        }
        
        thirdChanceHead = thirdChanceHead->next;
    }
    
    // printf("Eviction node not found\n");
    return thirdChanceHead;
}

void removeThirdChanceFrame(ThirdChanceFrame *node) {
    if (node == NULL) {
        printf("Didn't find any node in removeThirdChanceFrame.\n");
        return;
    }
    
    if (node->next == node) {
        thirdChanceHead = NULL;
        free(node);
        return;
    }

    ThirdChanceFrame *prev = thirdChanceHead;
    while (prev->next != node) {
        prev = prev->next;
    }
    
    if (thirdChanceHead == node) {
        thirdChanceHead = node->next;
    }

    prev->next = node->next;
    free(node);
}

void segfault_handler(int sig, siginfo_t *sigInfo, void *ucontext) {
    void *addr = sigInfo->si_addr;

    uint64_t base = (uint64_t)setUpConfig.vm;
    uint64_t fault_addr = (uint64_t)addr;
    uint64_t byte_offset = fault_addr - base;

    int vp = (int)(byte_offset / (uint64_t)setUpConfig.page_size);
    int page_offset = (int)(byte_offset % (uint64_t)setUpConfig.page_size);

    ucontext_t *uctx = (ucontext_t *)ucontext;
    mcontext_t *mctx = &uctx->uc_mcontext;
    greg_t error_register = mctx->gregs[REG_ERR];
    int is_write = 0;

    if (error_register & 0b010) {
        is_write = 1;
    }

    int fault_type = 0;
    int evicted_vp = -1;
    int write_back = 0;
    uint64_t phy_addr = 0;

    if (setUpConfig.policy == MM_FIFO) {
        FIFOPhysicalFrame *phyFrame = getPhysicalFrame(vp);
        if (phyFrame != NULL) {
            if (phyFrame->isWritePerformed) {
                return;
            } else {
                if (is_write) {
                    phyFrame->isWritePerformed = true;
                    mprotect((void *)(base + (uint64_t)vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_READ | PROT_WRITE);

                    fault_type = 2;
                    //countFramesWherePhyPageIsAdded(vp)
                    phy_addr =  phyFrame->frame_number * (uint64_t)setUpConfig.page_size + page_offset;
                    mm_logger(setUpConfig.stats, vp, fault_type, evicted_vp, write_back, phy_addr);
                }

                return;
            }
        } else {
            int evicted_frame_number = -1;
            if (countLinkedListNodes() == setUpConfig.num_frames) {
                FIFOPhysicalFrame *evicted_vp_node = removeFromLinkedList();
                if (evicted_vp_node != NULL) {
                    evicted_vp = evicted_vp_node->virtualPageNumber;
                    write_back = evicted_vp_node->isWritePerformed ? 1 : 0;
                    evicted_frame_number = evicted_vp_node->frame_number;
                    mprotect((void *)(base + (uint64_t)evicted_vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_NONE);
                    free(evicted_vp_node);
                }
            }

            evicted_frame_number = evicted_frame_number != -1 ? evicted_frame_number : countLinkedListNodes();
            insertInLinkedList(vp, is_write, evicted_frame_number);
            fault_type = is_write ? 1 : 0;
            phy_addr = evicted_frame_number * (uint64_t)setUpConfig.page_size + page_offset;
            mm_logger(setUpConfig.stats, vp, fault_type, evicted_vp, write_back, phy_addr);

            if (is_write) {
                mprotect((void *)(base + (uint64_t)vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_READ | PROT_WRITE);
            } else {
                mprotect((void *)(base + (uint64_t)vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_READ);
            }
        }
    } else if (setUpConfig.policy == MM_THIRD) {
        ThirdChanceFrame *phyFrame = getThirdChanceFrame(vp);
        
        if (phyFrame != NULL) {
            if (is_write) {
                if (phyFrame->M) {
                    fault_type = 4;
                } else {
                    fault_type = 2;
                }
                
                phyFrame->R = 1;
                phyFrame->M = 1;
                phyFrame->pass_count = 0;

                mprotect((void *)(base + (uint64_t)vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_READ | PROT_WRITE);
                
                phy_addr = phyFrame->frame_number * (uint64_t)setUpConfig.page_size + page_offset;
                mm_logger(setUpConfig.stats, vp, fault_type, evicted_vp, write_back, phy_addr);
            } else {
                phyFrame->R = 1;
                phyFrame->pass_count = 0;

                mprotect((void *)(base + (uint64_t)vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_READ);
                
                fault_type = 3;
                phy_addr = phyFrame->frame_number * (uint64_t)setUpConfig.page_size + page_offset;
                mm_logger(setUpConfig.stats, vp, fault_type, evicted_vp, write_back, phy_addr);
            }
            return;
        } else {
            int evicted_frame_number = -1;
            
            if (countThirdChanceNodes() == setUpConfig.num_frames) {
                ThirdChanceFrame *evicted_node = findEvictionNode();
                // printf("New \n");
                if (evicted_node != NULL) {
                    evicted_vp = evicted_node->virtualPageNumber;
                    write_back = evicted_node->M ? 1 : 0;
                    evicted_frame_number = evicted_node->frame_number;
                    
                    removeThirdChanceFrame(evicted_node);

                    mprotect((void *)(base + (uint64_t)evicted_vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_NONE);
                }
            }

            evicted_frame_number = evicted_frame_number != -1 ? evicted_frame_number : countThirdChanceNodes();

            insertThirdChanceFrame(vp, is_write, evicted_frame_number);
            
            fault_type = is_write ? 1 : 0;
            phy_addr = evicted_frame_number * (uint64_t)setUpConfig.page_size + page_offset;
            mm_logger(setUpConfig.stats, vp, fault_type, evicted_vp, write_back, phy_addr);

            if (is_write) {
                mprotect((void *)(base + (uint64_t)vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_READ | PROT_WRITE);
            } else {
                mprotect((void *)(base + (uint64_t)vp * (uint64_t)setUpConfig.page_size), setUpConfig.page_size, PROT_READ);
            }
        }
    }
}