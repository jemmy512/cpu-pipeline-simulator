/** Code by @author Wonsun Ahn
 *
 * Implements the five stages of the processor pipeline.  The code you will be
 * modifying mainly.
 */

#include <inttypes.h>
#include <assert.h>
#include "CPU.h"
#include "trace.h"
#include <stdio.h>

unsigned int cycle_number = 0;
unsigned int inst_number = 0;

std::deque<dynamic_inst> IF, ID, WB;
dynamic_inst EX_ALU = {0}, MEM_ALU = {0};
dynamic_inst EX_lwsw = {0}, MEM_lwsw = {0};

bool WB_ctrl = 1;
bool MEM_ctrl[2] = {1, 1};
bool EX_ctrl[3] = {1, 1, 1};
bool Ctrl_ID = 1;
bool Ctrl_IF = 1;
bool Ctrl_branch_taken = 0;

bool is_ALU(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type != ti_NOP && inst.type != ti_LOAD && inst.type != ti_STORE;
}

bool is_lwsw(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type == ti_LOAD || inst.type == ti_STORE;
}

bool is_lw(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type == ti_LOAD;
}

bool is_NOP(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type == ti_NOP;
}

bool is_branch(dynamic_inst dinst) {
  return dinst.inst.type == ti_BRANCH;
}

bool is_older(dynamic_inst dinst1, dynamic_inst dinst2) {
  return is_NOP(dinst2) || (!is_NOP(dinst1) && dinst1.seq < dinst2.seq);
}

dynamic_inst get_NOP() {
  dynamic_inst dinst = {0};
  return dinst;
}

bool is_finished()
{
  /* Finished when pipeline is completely empty */
  if (IF.size() > 0 || ID.size() > 0) return 0;
  if (!is_NOP(EX_ALU) || !is_NOP(MEM_ALU) || !is_NOP(EX_lwsw) || !is_NOP(MEM_lwsw)) {
    return 0;
  }
  return 1;
}

// int writeback()
// {
//   static unsigned int cur_seq = 1;
//   bool hasHazard = false;
//   bool hasWB = !is_NOP(MEM_ALU) || !is_NOP(MEM_lwsw);

//   WB.clear();

//   // J: structual memory write hazard
//   if (config->regFileWritePorts < config->pipelineWidth) {
//     hasHazard = true;

//     if (is_older(MEM_ALU, MEM_lwsw)) {
//       WB.push_back(MEM_ALU);
//       MEM_ALU = get_NOP();
//     } else {
//       WB.push_back(MEM_lwsw);
//       MEM_lwsw = get_NOP();
//     }
//   }

//   // J: RAW
//   std::deque<dynamic_inst> IDTmp;

//   while (hasWB && ID.size() > 0) {
//     int reg_a = ID.front().inst.sReg_a;
//     int reg_b = ID.front().inst.sReg_b;
//     int alu_dReg = MEM_ALU.inst.dReg;
//     int ls_dReg = MEM_lwsw.inst.dReg;

//     if (alu_dReg == reg_a || alu_dReg == reg_b || ls_dReg == reg_a || ls_dReg == reg_b) {
//       hasHazard = true;
//       IF.push_front(ID.front());
//     } else {
//       IDTmp.push_back(ID.front());
//     }
//     ID.pop_front();
//   }
//   ID.insert(ID.end(), IDTmp.begin(), IDTmp.end());

//   // J: WAW
//   if (config->pipelineWidth > 1) {
//     if (!is_NOP(MEM_ALU) && !is_NOP(MEM_ALU) && MEM_ALU.inst.dReg == MEM_lwsw.inst.dReg) {
//       hasHazard = true;
//       if (is_older(MEM_ALU, MEM_lwsw)) {
//         WB.push_back(MEM_ALU);
//         MEM_ALU = get_NOP();
//       } else {
//         WB.push_back(MEM_lwsw);
//         MEM_lwsw = get_NOP();
//       }
//     }
//   }

//   if (!hasHazard) {
//     if (is_older(MEM_ALU, MEM_lwsw)) {
//       WB.push_back(MEM_ALU);
//       MEM_ALU = get_NOP();
//       WB.push_back(MEM_lwsw);
//       MEM_lwsw = get_NOP();
//     } else {
//       WB.push_back(MEM_lwsw);
//       MEM_lwsw = get_NOP();
//       WB.push_back(MEM_ALU);
//       MEM_ALU = get_NOP();
//     }
//   }

//   if (verbose) {/* print the instruction exiting the pipeline if verbose=1 */
//     for (auto i = 0; i < (int) WB.size(); i++) {
//       printf("[%d: WB] %s\n", cycle_number, get_instruction_string(WB[i], true));
//       if(!is_NOP(WB[i])) {
//         if(config->pipelineWidth > 1 && config->regFileWritePorts == 1) {
//           // There is a corner case where an instruction without a
//           // destination register can get pulled in out of sequence but
//           // other than that, it should be strictly in-order.
//         } else {
//           assert(WB[i].seq == cur_seq);
//         }
//         cur_seq++;
//       }
//     }
//   }
//   return WB.size();
// }

int writeback()
{
  static unsigned int cur_seq = 1;
  bool continueWB = true;

  // J: data_raw 2
  bool doOp = false;
  for (auto i = 0; i < WB.size(); ++i) {
    doOp = doOp || !is_NOP(WB[i]);
  }

  if (doOp) {
    Ctrl_IF = true;
    Ctrl_ID = true;
  }

  WB.clear();

  // J: structural_regfile
  if (is_ALU(MEM_ALU) && is_lw(MEM_lwsw) && (config->regFileWritePorts == 1)) {
    continueWB = false;

    if (is_older(MEM_ALU, MEM_lwsw)) {
      WB.push_back(MEM_ALU);
      MEM_ALU = get_NOP();
    } else {
      WB.push_back(MEM_lwsw);
      MEM_lwsw = get_NOP();
    }
  }

  // J: data_waw
  if (continueWB && config->regFileWritePorts == 2) {
    if (is_ALU(MEM_ALU) && is_lw(MEM_lwsw) && MEM_ALU.inst.dReg == MEM_lwsw.inst.dReg) {
      continueWB = false;

      if (is_older(MEM_ALU, MEM_lwsw)) {
        WB.push_back(MEM_ALU);
        MEM_ALU = get_NOP();
      } else {
        WB.push_back(MEM_lwsw);
        MEM_lwsw = get_NOP();
      }
    }
  }

  if (continueWB) {
    if (is_older(MEM_ALU, MEM_lwsw)) {
      WB.push_back(MEM_ALU);
      MEM_ALU = get_NOP();
      WB.push_back(MEM_lwsw);
      MEM_lwsw = get_NOP();
    } else {
      WB.push_back(MEM_lwsw);
      MEM_lwsw = get_NOP();
      WB.push_back(MEM_ALU);
      MEM_ALU = get_NOP();
    }
  }

  if (verbose) {/* print the instruction exiting the pipeline if verbose=1 */
    for (auto i = 0; i < (int) WB.size(); i++) {
      printf("[%d: WB] %s\n", cycle_number, get_instruction_string(WB[i], true));
      if(!is_NOP(WB[i])) {
        if(config->pipelineWidth > 1 && config->regFileWritePorts == 1) {
          // There is a corner case where an instruction without a
          // destination register can get pulled in out of sequence but
          // other than that, it should be strictly in-order.
        } else {
          assert(WB[i].seq == cur_seq);
        }
        cur_seq++;
      }
    }
  }

  return WB.size();
}

int memory()
{
  if (is_branch(EX_ALU)) {
    Ctrl_branch_taken = false;
  }

  int insts = 0;
  if (is_NOP(MEM_ALU)) {
    MEM_ALU = EX_ALU;
    EX_ALU = get_NOP();
    insts++;
  }

  if (is_NOP(MEM_lwsw)) {
    MEM_lwsw = EX_lwsw;
    EX_lwsw = get_NOP();
    insts++;
  }

  // J: structural_memory
  // if (config->splitCaches == false && is_lw(MEM_lwsw)) {
  //   int i = IF.size() - 1;
  //   int nopCount = 0;
  //   while (i >= 0 && is_NOP(IF[i--])) {
  //     ++nopCount;
  //   }

  //   i = config->pipelineWidth - nopCount;
  //   while (i--) {
  //     IF.push_back(get_NOP());
  //   }
  // }

  // print_pipeline();

  return insts;
}

int issue()
{
  /* in-order issue */
  int insts = 0;
  while (ID.size() > 0) {
    if (is_ALU(ID.front())) {
      if (!is_NOP(EX_ALU)) {
        break;;
      }
      EX_ALU = ID.front();
      ID.pop_front();
    } else if (is_lwsw(ID.front())) {
      if (!is_NOP(EX_lwsw)) {
        break;;
      }
      EX_lwsw = ID.front();
      ID.pop_front();
    }
    else if (is_NOP(ID.front())) {
      if (!is_NOP(EX_ALU) && !is_NOP(EX_lwsw)) {
        break;
      }
      ID.pop_front();
    }
    else {
      assert(0);
    }
    insts++;
  }

  // J: control_not_taken
  // J: control_taken
  // if (is_branch(EX_ALU)) {
  //   bool isBranchTaken = true;
  //   int addr = EX_ALU.inst.Addr;

  //   if (!is_NOP(EX_lwsw)) {
  //     printf("LW %d | %d\n", addr, EX_lwsw.inst.PC);
  //     if (addr == EX_lwsw.inst.PC) {
  //       isBranchTaken = false;
  //     }
  //   }

  //   for (auto i = 0; i < ID.size(); ++i) {
  //     printf("ID %d | %d\n", addr, ID[i].inst.PC);
  //     if (addr == ID[i].inst.PC) {
  //       isBranchTaken = false;
  //     }
  //   }

  //   for (auto i = 0; i < IF.size(); ++i) {
  //     printf("IF %d | %d\n", addr, IF[i].inst.PC);
  //     if (addr == IF[i].inst.PC) {
  //       isBranchTaken = false;
  //     }
  //   }

  //   if (isBranchTaken) {
  //     printf("branch taken...\n");

  //     insts = 0;
  //     for (auto i = 0; i < ID.size(); ++i) {
  //       IF.push_front(ID.back());
  //       ID.pop_back();
  //     }

  //     for (auto i = 0; i < config->pipelineWidth; ++i) {
  //       IF.push_front(get_NOP());
  //     }
  //   } else {
  //     printf("branch not taken...\n");
  //   }

  // }

  return insts;
}

// int issue()
// {
//   /* in-order issue */
//   int insts = 0;
//   while (ID.size() > 0 && insts < config->pipelineWidth) {
//     if (is_ALU(ID.front())) {
//       if (is_NOP(EX_ALU)) {
//         EX_ALU = ID.front();
//         ID.pop_front();
//       }
//     } else if (is_lwsw(ID.front())) {
//       if (is_NOP(EX_lwsw)) {
//         EX_lwsw = ID.front();
//         ID.pop_front();
//       }
//     } else if (is_NOP(ID.front())) {
//       if (is_NOP(EX_ALU) || is_NOP(EX_lwsw)) {
//         ID.pop_front();
//       }
//     } else {
//       assert(0);
//     }

//     insts++;
//   }
//   return insts;
// }

int decode()
{
  int insts = 0;
  while (Ctrl_ID && (int)IF.size() > 0 && (int)ID.size() < config->pipelineWidth) {
    ID.push_back(IF.front());
    IF.pop_front();
    insts++;
  }

  // J: data_raw 1
  if (Ctrl_ID && config->enableForwarding == false) {
    int alu_dReg = EX_ALU.inst.dReg;
    int ls_dReg = EX_lwsw.inst.dReg;

    for (auto i = 0; i < ID.size(); ++i) {
      int reg_a = ID[i].inst.sReg_a;
      int reg_b = ID[i].inst.sReg_b;

      bool isALURAW = is_ALU(EX_ALU) && (alu_dReg == reg_a || alu_dReg == reg_b);
      bool isLWRAW = is_lw(EX_lwsw) && (ls_dReg == reg_a || ls_dReg == reg_b);

      if (isALURAW || isLWRAW) {
        for (auto j = ID.size() - 1; j >= i; --j) {
          insts--;
          IF.push_front(ID.back());
          ID.pop_back();
        }

        Ctrl_IF = false;
        Ctrl_ID = false;
      } else {
        Ctrl_IF = true;
        Ctrl_ID = true;
      }
    }
  }

  return insts;
}

int fetch()
{
  static unsigned int cur_seq = 1;
  int insts = 0;
  dynamic_inst dinst;
  instruction *tr_entry = NULL;

  /* copy trace entry(s) into IF stage */
  auto doFetch = []() {
    if (!Ctrl_branch_taken)
      return true;

    if (Ctrl_branch_taken && config->branchPredictor && config->branchTargetBuffer)
      return true;

    return false;
  };

  while (doFetch() && Ctrl_IF && (int)IF.size() < config->pipelineWidth) {
    size_t size = trace_get_item(&tr_entry); /* put the instruction into a buffer */
    if (size > 0) {
      dinst.inst = *tr_entry;
      dinst.seq = cur_seq++;
      IF.push_back(dinst);
      insts++;

      if (is_branch(dinst) && dinst.inst.Addr != dinst.inst.PC + 4) {
        Ctrl_branch_taken = true;
      }
    } else {
      break;
    }

    if (verbose) {/* print the instruction entering the pipeline if verbose=1 */
      printf("[%d: IF] %s\n", cycle_number, get_instruction_string(IF.back(), true));
    }
  }

  if (Ctrl_branch_taken && !config->branchPredictor && !config->branchTargetBuffer) {
    while ((int)IF.size() < config->pipelineWidth) {
      IF.push_back(get_NOP());
    }
  }

  inst_number += insts;
  return insts;
}
