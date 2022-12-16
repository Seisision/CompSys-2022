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
          // load instructions: LB, LH, LW, LBU, LHU
          int funct3 = (instruction >> 12) & 0x3f;

        }
        break;

        case 0b0010011:
        {
          // immediate
          // addi
        }
        break;

        case 0b0110011:
        {
          // non immediate
          // add
          int funct3 = (instruction >> 12) & 0x3f;
        }
        break;

        case 0b1101111:
        {
          // JAL
        }
        break;

        case 0b1100111:
        {
          // JALR
        }
        break;

        case 0b1100011:
        {
          // branch instructions: BEQ, BNE, BLT, BGE, BLTU, BGEU
          // funct3 is 12-14
          int funct3 = (instruction >> 12) & 0x3f;
        }
        break;
    }

}