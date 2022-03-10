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

// J: control signal
bool Ctrl_sturt_mem = 1;
bool Ctrl_EX = 1;
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

bool is_sw(dynamic_inst dinst) {
  return dinst.inst.type == ti_STORE;
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

int writeback()
{
  static unsigned int cur_seq = 1;
  bool continueWB = true;

  // J: data_raw 2
  // J: enable Ctrl_IF & Ctrl_ID if WB is finished
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

  // J: structural_memory
  if (config->splitCaches == false) {
    for (const auto& inst : WB) {
      if (is_lw(inst)) {
        Ctrl_sturt_mem = true;
        break;
      }
    }
  }

  if (verbose) {/* print the instruction exiting the pipeline if verbose=1 */
    for (auto i = 0; i < (int) WB.size(); i++) {
      printf("[%d: WB] %s\n", cycle_number, get_instruction_string(WB[i], true));
      if (!is_NOP(WB[i])) {
        if (config->pipelineWidth > 1 && config->regFileWritePorts == 1) {
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
  // J: enable fetch code for branch taken since branch inst is done
  if (is_branch(EX_ALU)) {
    Ctrl_branch_taken = false;
  }

  // J: data_raw with mem forwarding
  if (config->enableForwarding) {
    for (const auto& dinst : WB) {
      if (is_lw(dinst)) {
        int reg_d = dinst.inst.dReg;

        for (auto i = 0; !Ctrl_EX && i < ID.size(); ++i) {
          int reg_a = ID[i].inst.sReg_a;
          int reg_b = ID[i].inst.sReg_b;

          if (reg_d == reg_a || reg_d == reg_b) {
            Ctrl_EX = true;
            Ctrl_ID = true;
            Ctrl_IF = true;
          }
        }
      }
    }
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
  if (config->splitCaches == false && is_lw(MEM_lwsw)) {
    Ctrl_sturt_mem = false;
  }

  return insts;
}

int issue()
{
  /* in-order issue */
  int insts = 0;
  while (Ctrl_EX && ID.size() > 0) {
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

  // J: data_raw 2 with ex forwarding
  if (config->enableForwarding && (is_lw(MEM_lwsw) || is_lw(EX_lwsw))) {
    if (!is_NOP(EX_ALU)) {
      ID.push_front(EX_ALU);
    }

    for (auto dinst : { MEM_lwsw, EX_lwsw }) {
      int reg_d = dinst.inst.dReg;
      for (auto i = 0; i < ID.size() && Ctrl_EX && !is_NOP(dinst); ++i) {
        int reg_a = ID[i].inst.sReg_a;
        int reg_b = ID[i].inst.sReg_b;

        if (reg_d == reg_a || reg_d == reg_b) {
          Ctrl_EX = false;
          Ctrl_ID = false;
          Ctrl_IF = false;

          EX_ALU = get_NOP();
        }
      }
    }

    if (!is_NOP(EX_ALU)) {
      ID.pop_front();
    }
  }

  return insts;
}

int decode()
{
  int insts = 0;
  while (Ctrl_ID && (int)IF.size() > 0 && (int)ID.size() < config->pipelineWidth) {
    ID.push_back(IF.front());
    IF.pop_front();
    insts++;
  }

  // J: data_raw without forwarding
  if (Ctrl_ID && config->enableForwarding == false) {
    ID.push_front(EX_ALU);
    ID.push_front(EX_lwsw);

    for (int i = 2; i < ID.size(); ++i) {
        int reg_a = ID[i].inst.sReg_a;
        int reg_b = ID[i].inst.sReg_b;

      for (int j = i-1; j >= 0 && ID.size() > 2; --j) {
        int reg_d = ID[j].inst.dReg;

        if ((is_ALU(ID[j]) || is_lw(ID[j])) && (reg_d == reg_a || reg_d == reg_b)) {
          for (int k = ID.size() - 1; k >= i; --k) {
            insts--;
            IF.push_front(ID.back());
            ID.pop_back();
          }

          Ctrl_IF = false;
          Ctrl_ID = false;
          break;
        } else {
          Ctrl_IF = true;
          Ctrl_ID = true;
        }
      }
    }

    ID.pop_front();
    ID.pop_front();
  }

  return insts;
}


int fetch()
{
  static unsigned int cur_seq = 1;
  int insts = 0;
  dynamic_inst dinst;
  instruction *tr_entry = NULL;

  auto doFetch = []() {
    bool doFetchForBranch = false;

    // J: fetch if branch is not taken
    if (!Ctrl_branch_taken)
      doFetchForBranch = true;

    // J: fetch if branch is taken but predictor is enabled
    if (Ctrl_branch_taken && config->branchPredictor && config->branchTargetBuffer)
      doFetchForBranch = true;

    return doFetchForBranch && Ctrl_IF && Ctrl_sturt_mem;
  };

  /* copy trace entry(s) into IF stage */
  while (doFetch() && (int)IF.size() < config->pipelineWidth) {
    size_t size = trace_get_item(&tr_entry); /* put the instruction into a buffer */
    if (size > 0) {
      dinst.inst = *tr_entry;
      dinst.seq = cur_seq++;
      IF.push_back(dinst);
      insts++;

      if (verbose) {/* print the instruction entering the pipeline if verbose=1 */
        printf("[%d: IF] %s\n", cycle_number, get_instruction_string(IF.back(), true));
      }

      if (is_branch(dinst) && dinst.inst.Addr != dinst.inst.PC + 4) {
        Ctrl_branch_taken = true;
        // J: don't fetch any inst if branch taken
        break;
      }
    } else {
      break;
    }
  }

  inst_number += insts;

  return insts;
}
