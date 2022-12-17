#include "simulate.h"

#define uint unsigned int

#define debugmode 1

long int simulate(struct memory *mem, struct assembly *as, int start_addr, FILE *log_file) 
{

    int regs[32]; 
    int pc = start_addr;
    int counter = 0;

    int continue_flag = 1;

    for (int i = 0; i < 32; i++)
    {
      regs[i] = 0;
    }

    while(continue_flag)
    {
      if(counter > 100000) 
      {
        printf("failed");
        continue_flag = 0;
      }

      int inst = memory_rd_w(mem, pc);
      int opcode = inst & 0x7f;    

      int rd = (inst >> 7) & 0x1f;
            
      if (debugmode && counter < 100)
      {
        printf("opcode: %x, rd: %d\n", opcode, rd);
      }


      switch(opcode) 
      {
          case 0b0000011:
          {
            int rs1 = (inst & 0x000f8000) >> 15;
          
            // load insts: lb, lh, lw, lbu, lhu
            int funct3 = (inst >> 12) & 0x7;

            // upper 12 bits to lower 12 bits
            int imm = inst >> 20;

            // sign bit is at (12)
            // check and mask/extend upper 19 bits if set
            if (imm & 0x00001000) 
            {
              imm = imm | 0xffffe000;
            }
            int addr = regs[rs1] + imm;

            switch(funct3) 
            {
                case 0b000:
                {
                  // lb
                  int data = memory_rd_b(mem, addr);
                  // sign extend
                  if(data & 0x00000080) 
                  {
                    data = data | 0xffffff00;
                  }
                  regs[rd] = data;
                }
                break;

                case 0b001:
                {
                  // lh
                  int data = memory_rd_h(mem, addr);
                  // sign extend
                  if(data & 0x00008000) 
                  {
                    data = data | 0xffff0000;
                  }
                  regs[rd] = data;
                }
                break;
                case 0b010:
                {
                  // lw
                  int data = memory_rd_w(mem, addr);
                  regs[rd] = data;
                }
                break;

                case 0b100:
                {
                  // lbu
                  int data = memory_rd_b(mem, addr);
                  regs[rd] = data;
                }
                break;

                case 0b101:
                {
                  // lhu
                  int data = memory_rd_h(mem, addr);
                  regs[rd] = data;
                }
                break;
            }
          }
          break;

          case 0b0010011:
          {
            // immediate
            // addi, slli, slti, sitiu, xori, srli, srai, ori, andi
            int funct3 = (inst & 0x00007000) >> 12;
            int funct7 = (inst >> 25); // left most bits no need to mask
            int rs1 = (inst & 0x000f8000) >> 15;
            int shamt = (inst & 0x01f00000) >> 20;
            int rs2 = shamt;
            int imm = inst >> 20; // left most bits no need to mask

            int imm_ex = imm;
            // sign bit is at (11)
            // check and mask/extend upper 20 bits if set
            if (imm_ex  & 0x00000800) 
            {
              imm_ex = imm_ex  | 0xffff0000;
            }
          
            // use pointers to get exact bits
            int rs1int = regs[rs1];
            int rs2int = regs[rs2];
            uint rs1u = *((uint*)(&rs1int));
            uint rs2u = *((uint*)(&rs2int));

            switch(funct3 & 0x7) 
            {
              case 0b000: // addi
              {
                uint res = *((uint*)(&(regs[rs1])));
                res += *((uint*)(&imm_ex));
                regs[rd] = *((int*)(&res));
              } 
              break;

              case 0b010: // slti
              {
                if(regs[rs1] < imm_ex)
                {
                  regs[rd] = 1;
                } 
                else 
                {
                  regs[rd] = 0;
                }
              } 
              break;

              case 0b011: // sltiu
              {

                if(rs1u < rs2u)
                {
                  regs[rd] = 1;
                } 
                else 
                {
                  regs[rd] = 0;
                }
              } 
              break;

              case 0b100: // xori
              {
                regs[rd] = regs[rs1] ^ imm_ex;
              } 
              break;

              case 0b110: // ori
              {
                regs[rd] = regs[rs1] | imm_ex;
              } 
              break;

              case 0b111: // andi
              {
                regs[rd] = regs[rs1] & imm_ex;
              } 
              break;

              case 0b001: // slli
              {
                // cast to uint for logical shift
                uint res = rs1u;
                res = res << shamt;
                   regs[rd] = *((int*)(&res));
              } 
              break;

              case 0b101: // srai / srli
              {
                 if(funct7) 
                 {
                   // srai
                   // integer shift is arithmetic by default
                   regs[rd] = regs[rs1] >> shamt;
                 }
                 else
                 {
                   // srli
                   // cast to uint for logical shift
                   uint res = rs1u;
                   res = res >> shamt;
                   regs[rd] = *((int*)(&res));
                 }
              } 
              break;
            }
          }
          break;

          case 0b0010111:
          {
            // auipc
            int imm = inst & 0xfffff000;
            regs[rd] = imm + pc;
          }
          break;

          case 0b0100011:
          {
            // sb,sh,sw
            int funct3 = (inst & 0x00007000) >> 12; 
            int rs1 = (inst & 0x000f8000) >> 15;
            int rs2 = (inst & 0x01f00000) >> 20;

            int imm = ((inst & 0xfe000000) >> 20) | // bit 31.25 to 11.5
                      ((inst & 0x00000f80) >> 7);    // bit 11.7 to 4.0

            // sign bit is at (11)
            // check and mask/extend upper 20 bits if set
            if (imm & 0x00000800) 
            {
              imm = imm | 0xffff0000;
            }

            int addr = regs[rs1] + imm;
            int data = regs[rs2];

            switch(funct3 & 0x7)
            {
              // sb
              case 0b000:
              {
                memory_wr_b(mem, addr, data);
              }
              break;

              // sh
              case 0b001:
              {
                memory_wr_h(mem, addr, data);
              }
              break;

              // sw
              case 0b010:
              {
                memory_wr_w(mem, addr, data);
              }
              break;
            }
          }
          break;

          case 0b0110011:
          {
            // non immediate
                        
            // add, sub, sil, sit, sltu, xor, srl, ara, or, and
            // mul, mulh, mulhau, mulhu, div, divu, rem, remu
            int funct3 = (inst >> 12) & 0x7;
            int funct7 = (inst >> 25); // left most bits no need to mask
            int rs1 = (inst & 0x000f8000) >> 15;
            int rs2 = (inst & 0x01f00000) >> 20;
          
            int rs1int = regs[rs1];
            int rs2int = regs[rs2];

            // use pointers to get exact bits
            uint rs1u = *((uint*)(&rs1int));
            uint rs2u = *((uint*)(&rs2int));

            if(funct7 & 0x1)
            {
              // mul extensions
              switch(funct3 & 0x7)
              {
                case 0b000:
                {
                  // mul
                  uint res = rs1u * rs2u;
                  regs[rd] = *((int*)(&res));
                }
                break;

                case 0b001:
                {
                  // mulh
                  long rs1l = rs1;
                  long rs2l = rs2;

                  long resl = rs1l * rs2l;
                  regs[rd] = (resl >> 32);
                }
                break;

                case 0b010:
                {
                  // mulhsu
                  long rs1l = rs1;
                  unsigned long rs2l = rs2;

                  long resl = rs1l * rs2l;
                  regs[rd] = (resl >> 32);

                }
                break;

                case 0b011:
                {
                  // mulhu
                  unsigned long rs1l = rs1;
                  unsigned long rs2l = rs2;

                  long resl = rs1l * rs2l;
                  regs[rd] = (resl >> 32);
                }
                break;

                case 0b100:
                {
                  // div
                  if (rs2int)
                  {
                    regs[rd] = rs1int/rs2int;
                  }
                  else
                  {
                    // divide by 0
                    regs[rd] = -1;
                  }
                }
                break;

                case 0b101:
                {
                  // divu
                  if (rs2u)
                  {
                    uint res = rs1u/rs2u;
                    regs[rd] = *((int*)(&res));
                  }
                  else
                  {
                    // divide by 0
                    regs[rd] = 0xffffffff;
                  }

                }
                break;

                case 0b110:
                {
                  // rem
                  if (rs2int)
                  {
                    regs[rd] = rs1int%rs2int;
                  }
                  else
                  {
                    // divide by 0
                    regs[rd] = rs1int;
                  }
                }
                break;

                case 0b111:
                {
                  // remu
                  if (rs2u)
                  {
                    uint res = rs1u%rs2u;
                    regs[rd] = *((int*)(&res));
                  }
                  else
                  {
                    // divide by 0
                    regs[rd] = rs1int;
                  }
                }
                break;
              }

            } 
            else
            {
              switch(funct3 &0x7)
              {
                case 0b000:
                {
                  // add/sub
                  if(funct7)
                  {
                    // sub
                    uint res = rs1u - rs2u; 
                    regs[rd] = *((int*)(&res));
                  }
                  else
                  {
                    // add
                    uint res = rs1u + rs2u; 
                    regs[rd] = *((int*)(&res));
                  }
                }
                break;

                case 0b001:
                {
                  // sll
                   // cast to uint for logical shift
                  uint res = rs1u;
                  // extract lower bits of rs2 value
                  uint shamt = rs2u & 0x1f;
                  res = res << shamt;
                     regs[rd] = *((int*)(&res));
                }
                break;

                case 0b010:
                {
                  // slt
                  if(rs1int < rs2int)
                  {
                    regs[rd] = 1;
                  }
                  else
                  {
                    regs[rd] = 0;
                  }
                }
                break;

                case 0b011:
                {
                  // sltu
                  if(rs1u < rs2u)
                  {
                    regs[rd] = 1;
                  }
                  else
                  {
                    regs[rd] = 0;
                  }
                }
                break;

                case 0b100:
                {
                  // xor
                  regs[rd] = rs1int ^ rs2int;
                }
                break;

                case 0b101:
                {
                  if(funct7)
                  {
                    // sra
                    // extract lower bits of rs2 value
                    int shamt = rs2int & 0x1f;
                    regs[rd] = rs1int >> shamt;
                  }
                  else
                  {
                    // srl
                    // cast to uint for logical shift
                    uint res = rs1u;
                    // extract lower bits of rs2 value
                    uint shamt = rs2u & 0x1f;
                    res = res >> shamt;
                    regs[rd] = *((int*)(&res));
                  }
                }
                break;

                case 0b110:
                {
                  // or
                  regs[rd] = rs1int | rs2int;

                }
                break;

                case 0b111:
                {
                  // and
                  regs[rd] = rs1int & rs2int;
                }
                break;
              }
            }        
          }
          break;

          case 0b0110111:
          {
            // lui (load upper immediate 20 upper bits)
            int imm = inst & 0xfffff000;
            regs[rd] = imm;

          }
          break;

          case 0b1100011:
          {
            // branch insts: beq, bne, blt, bge, bltu, bgeu
            // funct3 is 12-14
            // masking and shifting bits
            int funct3 = (inst & 0x00007000) >> 12; 
            int rs1 = (inst & 0x000f8000) >> 15;
            int rs2 = (inst & 0x01f00000) >> 20;
          
            int imm = ((inst & 0x80000000) >> 19) | // bit 31 to 12
                      ((inst & 0x7e00000) >> 20)  | // bit 30.25 to 10.5
                      ((inst & 0x0000f00) >> 7)   | // bit 11.8 to 4.1
                      ((inst & 0x0000080) << 4);    // bit 7 to 11

            // sign bit is at (12)
            // check and mask/extend upper 19 bits if set
            if (imm & 0x00001000) 
            {
              imm = imm | 0xffffe000;
            }
            
            switch(funct3 & 0x7)
            {
              case 0b000:
              {
                // beq
                if (debugmode)
                {
                  printf("beq %d %d %x\n", rs1, rs2, imm);
                  printf("%x\n");
                }

                if(regs[rs1] == regs[rs2])
                {
                  pc = pc + imm - 4;
                }
              }
              break;

              case 0b001:
              {
                // bne
                if (debugmode)
                {
                  printf("bne %d %d %x\n", rs1, rs2, imm);
                }

                if(regs[rs1] != regs[rs2])
                {
                  pc = pc + imm - 4;
                }
              }
              break;

              case 0b100:
              {
                // blt
                if (debugmode)
                {
                  printf("blt %d %d %x\n", rs1, rs2, imm);
                }

                if(regs[rs1] < regs[rs2])
                {
                  pc = pc + imm - 4;
                }
              }
              break;

              case 0b101:
              {
                // bge
                if (debugmode)
                {
                  printf("bge %d %d %x\n", rs1, rs2, imm);
                }

                if(regs[rs1] >= regs[rs2])
                {
                  pc = pc + imm - 4;
                }
              }
              break;

              case 0b110:
              {
                // bltu
                if (debugmode)
                {
                  printf("bltu %d %d %x\n", rs1, rs2, imm);
                }

                if((uint)regs[rs1] < (uint)regs[rs2])
                {
                  pc = pc + imm - 4;
                }
              }
              break;

              case 0b111:
              {
                // bgeu
                if (debugmode)
                {
                  printf("bgeu %d %d %x\n", rs1, rs2, imm);
                }

                if((uint)regs[rs1] >= (uint)regs[rs2])
                {
                  pc = pc + imm - 4;
                }
              }
              break;

              default: 
              // something bad happend
              continue_flag = 0;
              break;
            }
          }
          break;

          case 0b1101111:
          {
            // jal
            int imm = ((inst & 0x80000000) >> 11) | // bit 31 to 20
                      ((inst & 0x7fe00000) >> 20) | // bit 30.21 to 10.1
                      ((inst & 0x00100000) >> 9)  | // bit 20 to 11
                      ((inst & 0x000ff000));        // bit 19.12 to 19.12
            regs[rd] = pc + 4;
            // sign bit is at (20)
            // check and mask/extend upper 11 bits if set
            if (imm & 0x00100000) 
            {
              imm = imm | 0xffe00000;
            }

            // subtract 4 to compensate for shared pc increment
            pc += imm-4; 
                    
          }
          break;

          case 0b1100111:
          {
            // jalr
            int imm = inst >> 20;
            int rs1 = (inst & 0x000f8000) >> 15;

            // sign bit is at (11)
            // check and mask/extend upper 20 bits if set
            if (imm & 0x00000800) 
            {
              imm = imm | 0xfffff000;
            }

            regs[rd] = pc +4;
            int target = regs[rs1] + imm;

            // mask out least signicant bit
            target = target & 0xfffffffe;

            // subtract 4 to compensate for shared pc increment
            pc = target - 4;
          }
          break;

          case 0b1110011:
          {
            // ecall
            int call_type = regs[17];
            switch(call_type)
            {
              case 1:
              {
                int c = getchar();
                // A7
                regs[17] = c;
              }
              break;

              case 2:
              {
                // A6
                int c = regs[16];
                putchar(c);
              }
              break;

              case 3:
              case 93:
              {
                counter += 1;
                pc += 4;
                return counter;
              }
            }
          }
          break;

          default:
          {
            continue_flag = 0;
            if (debugmode)
            {
              printf("invalid instruction: %x (%x)", opcode, inst);
            }
          }
          break;
      }

      //check pc alignment
      if(pc & 0x3)
      {
        // if pc is not 32 bit aligned then exit 
        continue_flag = 0; 
      }

      // increment pc and counter after instruction
      pc += 4;
      counter += 1;
    }
    return counter;
}