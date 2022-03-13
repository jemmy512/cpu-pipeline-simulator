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

bool branch_taken_begin(dynamic_inst dinst) {
  if (is_branch(dinst) && dinst.inst.Addr != dinst.inst.PC + 4) {
    Ctrl_branch_taken = true;
    // J: don't fetch any inst if branch taken
    return true;
  }

  return false;
}

void branch_taken_end() {
  if (is_branch(EX_ALU)) {
    Ctrl_branch_taken = false;
  }
}

void structural_memory_begin() {
  if (config->splitCaches == false && is_lw(MEM_lwsw)) {
    Ctrl_sturt_mem = false;
  }
}

void structural_memory_end() {
  if (config->splitCaches == false) {
    for (const auto& inst : WB) {
      if (is_lw(inst)) {
        Ctrl_sturt_mem = true;
        break;
      }
    }
  }
}

bool structural_regfile() {
  bool continueWB = true;

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

  return continueWB;;
}

void data_raw_mem_forward_begin() {
  if (config->enableForwarding && (is_lw(MEM_lwsw) || is_lw(EX_lwsw))) {
    if (!is_NOP(EX_ALU)) {
      ID.push_front(EX_ALU);
    }

    for (auto dinst : { MEM_lwsw, EX_lwsw }) {
      int reg_d = dinst.inst.dReg;
      for (auto i = 0; i < ID.size() && Ctrl_EX && !is_NOP(dinst) && reg_d != 255; ++i) {
        int reg_a = ID[i].inst.sReg_a;
        int reg_b = ID[i].inst.sReg_b;

        if (reg_d == reg_a || reg_d == reg_b) {
          Ctrl_EX = false;
          Ctrl_ID = false;

          EX_ALU = get_NOP();
        }
      }
    }

    if (!is_NOP(EX_ALU)) {
      ID.pop_front();
    }
  }
}

void data_raw_mem_forward_end() {
  if (config->enableForwarding) {
    for (const auto& dinst : WB) {
      if (is_lw(dinst)) {
        int reg_d = dinst.inst.dReg;

        for (auto i = 0; !Ctrl_EX && i < ID.size(); ++i) {
          int reg_a = ID[i].inst.sReg_a;
          int reg_b = ID[i].inst.sReg_b;

          if (reg_d != 255 && (reg_d == reg_a || reg_d == reg_b)) {
            Ctrl_EX = true;
            Ctrl_ID = true;
          }
        }
      }
    }
  }
}

void data_raw_no_forward_begin() {
  if (config->enableForwarding == false) {
    ID.push_front(EX_ALU);
    ID.push_front(EX_lwsw);

    for (int i = 2; i < ID.size(); ++i) {
        int reg_a = ID[i].inst.sReg_a;
        int reg_b = ID[i].inst.sReg_b;

      for (int j = i-1; j >= 0 && ID.size() > 2; --j) {
        int reg_d = ID[j].inst.dReg;

        bool is_reg_wt = is_ALU(ID[j]) || is_lw(ID[j]);
        if (is_reg_wt && reg_d != 255 && (reg_d == reg_a || reg_d == reg_b)) {
          for (int k = ID.size() - 1; k >= i; --k) {
            // insts--;
            IF.push_front(ID.back());
            ID.pop_back();
          }

          Ctrl_ID = false;
          break;
        } else {
          Ctrl_ID = true;
        }
      }
    }

    ID.pop_front();
    ID.pop_front();
  }
}

void data_raw_no_forward_end() {
  if (config->enableForwarding == false) {
    bool doOp = false;

    if (IF.size() > 0) {
      int reg_a = IF.front().inst.sReg_a;
      int reg_b = IF.front().inst.sReg_b;
      for (auto i = 0; i < WB.size() && !doOp; ++i) {
        int reg_d = WB[i].inst.dReg;
        bool is_reg_wt = is_ALU(WB[i]) || is_lw(WB[i]);
        doOp = is_reg_wt && reg_d != 255 && (reg_d == reg_a || reg_d == reg_b);
      }
    }

    if (doOp) {
      Ctrl_ID = true;
    }
  }
}

bool data_waw() {
  bool continueWB = true;

  if (config->regFileWritePorts == 2) {
    bool is_reg_wt = is_ALU(MEM_ALU) && is_lw(MEM_lwsw);

    if (is_reg_wt && MEM_ALU.inst.dReg != 255 && MEM_ALU.inst.dReg == MEM_lwsw.inst.dReg) {
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

  return continueWB;
}

int writeback()
{
  static unsigned int cur_seq = 1;
  bool continueWB = true;

  data_raw_no_forward_end();

  WB.clear();

  if (continueWB) {
    continueWB = structural_regfile();
  }

  if (continueWB) {
    continueWB = data_waw();
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

  structural_memory_end();

  data_raw_mem_forward_end();

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
  branch_taken_end();

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

  structural_memory_begin();

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
    // else if (is_NOP(ID.front())) {
    //   if (!is_NOP(EX_ALU) && !is_NOP(EX_lwsw)) {
    //     break;
    //   }
    //   ID.pop_front();
    // }
    else {
      assert(0);
    }
    insts++;
  }

  data_raw_mem_forward_begin();

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

  if (Ctrl_ID) {
    data_raw_no_forward_begin();
  }

  return insts;
}


int fetch()
{
  static unsigned int cur_seq = 1;
  int insts = 0;
  dynamic_inst dinst;
  instruction *tr_entry = NULL;

  auto canFetch = []() {
    bool doFetchForBranch = false;

    // J: fetch if branch is not taken
    if (!Ctrl_branch_taken)
      doFetchForBranch = true;

    // J: fetch if branch is taken but predictor is enabled
    if (Ctrl_branch_taken && config->branchPredictor && config->branchTargetBuffer)
      doFetchForBranch = true;

    return doFetchForBranch && Ctrl_sturt_mem;
  };

  /* copy trace entry(s) into IF stage */
  while (canFetch() && (int)IF.size() < config->pipelineWidth) {
    size_t size = trace_get_item(&tr_entry); /* put the instruction into a buffer */
    if (size > 0) {
      dinst.inst = *tr_entry;
      dinst.seq = cur_seq++;
      IF.push_back(dinst);
      insts++;

      if (verbose) {/* print the instruction entering the pipeline if verbose=1 */
        printf("[%d: IF] %s\n", cycle_number, get_instruction_string(IF.back(), true));
      }

      if (branch_taken_begin(dinst)) {
        break;
      }
    } else {
      break;
    }
  }

  inst_number += insts;

  return insts;
}
