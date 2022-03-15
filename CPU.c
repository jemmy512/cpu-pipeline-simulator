/** Code by @author Wonsun Ahn
 *
 * Implements the five stages of the processor pipeline.  The code you will be
 * modifying mainly. */

#include <inttypes.h>
#include <assert.h>
#include "CPU.h"
#include "trace.h"
#include <stdio.h>
#include <set>
#include <tuple>

unsigned int cycle_number = 0;
unsigned int inst_number = 0;

std::deque<dynamic_inst> IF, ID, WB;
dynamic_inst EX_ALU = {0}, MEM_ALU = {0};
dynamic_inst EX_lwsw = {0}, MEM_lwsw = {0};

/* sample.1-wide CYCLE NUMBER: 14 */
std::set<int> RawDependence;

/* sample.2-wide-opt CYCLE NUMBER: 76
This is the case: some continues BRANCH insts */
std::set<int> BranchTaken;

// J: control signal: is Ctrl_xx happen
bool Ctrl_stucutual_mem = 0;
bool Ctrl_branch_taken = 0;

// J: can do Ex ID
bool Ctrl_EX = 1;
bool Ctrl_ID = 1;

int do_issue(int ex_size);

bool is_ALU(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type != ti_NOP && inst.type != ti_LOAD && inst.type != ti_STORE;
}

bool is_itype(dynamic_inst dinst) {
  return dinst.inst.type == ti_ITYPE;
}

bool is_reg_wt(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type == ti_RTYPE || inst.type == ti_ITYPE || inst.type == ti_LOAD;;
}

bool is_lwsw(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type == ti_LOAD || inst.type == ti_STORE;
}

bool is_lw(dynamic_inst dinst) {
  return dinst.inst.type == ti_LOAD;
}

bool is_sw(dynamic_inst dinst) {
  return dinst.inst.type == ti_STORE;
}

bool is_NOP(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type == ti_NOP;
}

bool is_jtype(dynamic_inst dinst) {
  return dinst.inst.type == ti_JTYPE;
}

bool is_jrtype(dynamic_inst dinst) {
  return dinst.inst.type == ti_JRTYPE;
}

bool is_branch(dynamic_inst dinst) {
  return dinst.inst.type == ti_BRANCH;
}

bool is_special(dynamic_inst dinst) {
  return dinst.inst.type == ti_SPECIAL;
}

bool is_older(dynamic_inst dinst1, dynamic_inst dinst2) {
  return is_NOP(dinst2) || (!is_NOP(dinst1) && dinst1.seq < dinst2.seq);
}

/* This is a design flaw of 255:
* if the reg is not used, 255 stands for invalid reg number,
* other wise 255 presents the valide number.
So we use -2, -3, -4 to represent invalid reg number of dReg, aReg, bReg, respectively.

[x] reg is not used:
STORE: (Seq:        9)(Regs: 255[x],  28,      18) EX
  NOP: (Seq:        0)(Regs:      0,   0,       0) ID
ITYPE: (Seq:       10)(Regs:      29,  29, 255[x]) IF */
std::tuple<int, int, int> get_dst_src_regs(dynamic_inst dinst1, dynamic_inst dinst2) {
  int reg_2d = [&dinst2]() -> int {
    if (is_sw(dinst2) || is_branch(dinst2) || is_jtype(dinst2) || is_jrtype(dinst2) || is_special(dinst2)) {
      return -2;
    }
    return dinst2.inst.dReg;
  }();

  int reg_1a = [&dinst1]() -> int {
    if (is_jtype(dinst1) || is_special(dinst1)) {
      return -3;
    }
    return dinst1.inst.sReg_a;
  }();

  int reg_1b = [&dinst1]() -> int {
    if (is_itype(dinst1) || is_lw(dinst1) || is_jtype(dinst1) || is_jrtype(dinst1) || is_special(dinst1)) {
      return -4;
    }
    return dinst1.inst.sReg_b;
  }();

  return { reg_2d, reg_1a, reg_1b };
}

bool is_raw(dynamic_inst dinst1, dynamic_inst dinst2) {
  auto [reg_2d, reg_1a, reg_1b] = get_dst_src_regs(dinst1, dinst2);
  return !is_NOP(dinst2) && (reg_2d == reg_1a || reg_2d == reg_1b);
}

bool is_mem_forward(dynamic_inst dinst1, dynamic_inst dinst2) {
  auto [reg_2d, reg_1a, reg_1b] = get_dst_src_regs(dinst1, dinst2);
  return is_lw(dinst2) && (reg_2d == reg_1a || reg_2d == reg_1b);
}

bool is_ex_forward(dynamic_inst dinst1, dynamic_inst dinst2) {
  auto [reg_2d, reg_1a, reg_1b] = get_dst_src_regs(dinst1, dinst2);
  return !is_NOP(dinst2) && !is_lw(dinst2) && (reg_2d == reg_1a || reg_2d == reg_1b);
}

dynamic_inst get_NOP() {
  dynamic_inst dinst = {0};
  return dinst;
}

bool is_finished()
{
  /* Finished when pipeline is completely empty */
  if (IF.size() > 0 || ID.size() > 0) {
    return 0;
  }

  if (!is_NOP(EX_ALU) || !is_NOP(MEM_ALU) || !is_NOP(EX_lwsw) || !is_NOP(MEM_lwsw)) {
    return 0;
  }

  return 1;
}

void push_older(std::deque<dynamic_inst>& queue, dynamic_inst& dinst1, dynamic_inst& dinst2) {
  if (is_older(dinst1, dinst2)) {
    queue.push_back(dinst1);
    dinst1 = get_NOP();
  } else {
    queue.push_back(dinst2);
    dinst2 = get_NOP();
  }
}

void push_inorder(std::deque<dynamic_inst>& queue, dynamic_inst& dinst1, dynamic_inst& dinst2) {
  if (is_older(dinst1, dinst2)) {
    queue.push_back(dinst1);
    dinst1 = get_NOP();
    queue.push_back(dinst2);
    dinst2 = get_NOP();
  } else {
    queue.push_back(dinst2);
    dinst2 = get_NOP();
    queue.push_back(dinst1);
    dinst1 = get_NOP();
  }
}

bool branch_taken_begin(dynamic_inst dinst) {
  // J: branch taken: branch target is not the next inst pc
  // J: sample.2-wide-opt cycle 31
  if (is_branch(dinst) && (dinst.inst.Addr != dinst.inst.PC + 4)) {
    BranchTaken.emplace(dinst.seq);
    Ctrl_branch_taken = true;
    return true;
  }

  return false;
}

void branch_taken_end_ex() {
  BranchTaken.erase(EX_ALU.seq);

  if (BranchTaken.empty()) {
    Ctrl_branch_taken = false;
  }
}

void branch_taken_end_id() {
  if (config->branchPredictor && config->branchTargetBuffer) {
    for (auto& dinst : ID) {
      BranchTaken.erase(dinst.seq);
    }

    if (BranchTaken.empty()) {
      Ctrl_branch_taken = false;
    }
  }
}

void structural_memory_begin() {
  if (!config->splitCaches && is_lw(MEM_lwsw)) {
    Ctrl_stucutual_mem = true;
  }
}

void structural_memory_end() {
  if (!config->splitCaches) {
    for (const auto& dinst : WB) {
      if (is_lw(dinst)) {
        Ctrl_stucutual_mem = false;
        break;
      }
    }
  }
}

bool structural_regfile() {
  bool continue_wb = true;

  if (is_reg_wt(MEM_ALU) && is_reg_wt(MEM_lwsw) && (config->regFileWritePorts == 1)) {
    continue_wb = false;

    push_older(WB, MEM_ALU, MEM_lwsw);

    while (WB.size() < config->pipelineWidth) {
      WB.push_back(get_NOP());
    }
  }

  return continue_wb;;
}

int data_raw_forward_begin() {
  if (config->enableForwarding) {
    ID.push_front(MEM_lwsw);
    int ex_size = ID.size();
    bool is_first_ctrl_ex = false;

    for (auto i = 1; i < ID.size() && Ctrl_EX; ++i) {
      for (int j = i-1; j >= 0 && Ctrl_EX; --j) {
        if (is_raw(ID[i], ID[j])) {
          ex_size = i;
          Ctrl_EX = false;
          is_first_ctrl_ex = true;
          RawDependence.emplace(ID[j].seq);
        }
      }
    }

    ID.pop_front();

    if (is_first_ctrl_ex) {
      return do_issue(ex_size - 1);
    }

    return ex_size - 1;
  }

  return ID.size();
}

void data_raw_forward_end_ex() {
  if (config->enableForwarding) {
    WB.push_back(MEM_ALU);
    WB.push_back(MEM_lwsw);

    for (const auto& dinst : WB) {
      if (RawDependence.count(dinst.seq)) {
        for (auto i = 0; !Ctrl_EX && i < ID.size(); ++i) {
          if (is_ex_forward(ID[i], dinst)) {
            RawDependence.erase(dinst.seq);
            Ctrl_EX = true;
          }
        }
      }
    }

    WB.pop_back();
    WB.pop_back();
  }
}

void data_raw_forward_end_mem() {
  if (config->enableForwarding) {
    for (const auto& dinst : WB) {
      if (RawDependence.count(dinst.seq)) {
        for (auto i = 0; !Ctrl_EX && i < ID.size(); ++i) {
          if (is_mem_forward(ID[i], dinst)) {
            RawDependence.erase(dinst.seq);
            Ctrl_EX = true;
          }
        }
      }
    }
  }
}

int data_raw_no_forward_begin() {
  int insts = 0;

  if (!config->enableForwarding) {
    ID.push_front(EX_ALU);
    ID.push_front(EX_lwsw);
    ID.push_front(MEM_ALU); // sample.1-wide CYCLE NUMBER: 44
    ID.push_front(MEM_lwsw);

    for (auto i = 0; i < WB.size(); ++i) {
      ID.push_front(WB[i]);
    }

    for (int i = 4 + WB.size(); i < ID.size() && Ctrl_ID; ++i) {
      for (int j = i-1; j >= 0 && ID.size() > 4 + WB.size() && Ctrl_ID; --j) {
        if (is_raw(ID[i], ID[j])) {
          RawDependence.emplace(ID[j].seq);

          for (int k = ID.size() - 1; k >= i; --k) {
            ++insts;
            IF.push_front(ID.back());
            ID.pop_back();
          }
          Ctrl_ID = false;
        }
      }
    }

    ID.pop_front();
    ID.pop_front();
    ID.pop_front();
    ID.pop_front();
    for (auto i = 0; i < WB.size(); ++i) {
      ID.pop_front();
    }
  }

  return insts;
}

void data_raw_no_forward_end() {
  if (!config->enableForwarding && IF.size() > 0) {
    for (auto i = 0; i < WB.size(); ++i) {
      if (RawDependence.count(WB[i].seq)) {
        RawDependence.erase(WB[i].seq);
        Ctrl_ID = true;
        return;
      }
    }
  }
}

bool data_waw() {
  bool continue_wb = true;

  if (config->regFileWritePorts == 2) {
    bool is_multi_wt = is_reg_wt(MEM_ALU) && is_reg_wt(MEM_lwsw);

    if (is_multi_wt && MEM_ALU.inst.dReg == MEM_lwsw.inst.dReg) {
      continue_wb = false;

      push_older(WB, MEM_ALU, MEM_lwsw);

      while (WB.size() < config->pipelineWidth) {
        WB.push_back(get_NOP());
      }
    }
  }

  return continue_wb;
}

int writeback()
{
  static unsigned int cur_seq = 1;
  bool continue_wb = true;

  data_raw_no_forward_end();

  WB.clear();

  if (continue_wb) {
    continue_wb = structural_regfile();
  }

  if (continue_wb) {
    continue_wb = data_waw();
  }

  if (continue_wb) {
    push_inorder(WB, MEM_ALU, MEM_lwsw);
  }

  structural_memory_end();

  data_raw_forward_end_mem();

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
  branch_taken_end_ex();

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
  data_raw_forward_end_ex();

  /* in-order issue */
  int ex_size = data_raw_forward_begin();

  if (Ctrl_EX && ex_size) {
    return do_issue(ex_size);
  }

  return ex_size;
}

int do_issue(int ex_size) {
  int insts = 0;

  while (ex_size-- > 0) {
    if (is_ALU(ID.front())) {
      if (!is_NOP(EX_ALU)) {
        break;
      }
      EX_ALU = ID.front();
      ID.pop_front();
    } else if (is_lwsw(ID.front())) {
      if (!is_NOP(EX_lwsw)) {
        break;
      }
      EX_lwsw = ID.front();
      ID.pop_front();
    } else {
      assert(0);
    }
    insts++;
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

  if (Ctrl_ID) {
    insts -= data_raw_no_forward_begin();
  }

  branch_taken_end_id();

  return insts;
}

int fetch()
{
  static unsigned int cur_seq = 1;
  int insts = 0;
  dynamic_inst dinst;
  instruction *tr_entry = NULL;

  auto canFetch = []() {
    return !Ctrl_branch_taken && !Ctrl_stucutual_mem;
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

      // J: don't fetch any more inst if branch taken
      if (branch_taken_begin(dinst)) {
        goto fetch_done;
      }
    } else {
      break;
    }
  }

fetch_done:
  inst_number += insts;

  return insts;
}
