#include "simulate.h"

long int simulate(struct memory *mem, struct assembly *as, int start_addr, FILE *log_file) 
{

    int regs[32]; 
    regs[0] = 0;
    int pc = start_addr;

    int instruction = memory_rd_w(mem, pc);
    int opcode = instruction & 0x7f;
    int rd = (instruction >> 7) & 0x1f;
    switch(opcode) 
    {
        case 0b0000011:
        {
          // load
        }
        break;

        case 0b0010011:
        {
          // immediate
          // addi
        }

        case 0b0110011:
        {
          // non immediate
          // add
        }
    }

}