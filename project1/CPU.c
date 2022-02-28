/** Code by @author Wonsun Ahn
 *
 * Implements the five stages of the processor pipeline.  The code you will be
 * modifying mainly.
 */

#include <inttypes.h>
#include <assert.h>
#include "CPU.h"
#include "trace.h"

unsigned int cycle_number = 0;
unsigned int inst_number = 0;

std::deque<dynamic_inst> IF, ID, WB;
dynamic_inst EX_ALU = {0}, MEM_ALU = {0};
dynamic_inst EX_lwsw = {0}, MEM_lwsw = {0};

bool is_ALU(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type != ti_NOP && inst.type != ti_LOAD && inst.type != ti_STORE;
}

bool is_lwsw(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type == ti_LOAD || inst.type == ti_STORE;
}

bool is_NOP(dynamic_inst dinst) {
  instruction inst = dinst.inst;
  return inst.type == ti_NOP;
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

  WB.clear();
  if (is_older(MEM_ALU, MEM_lwsw)) {
    WB.push_back(MEM_ALU);
    MEM_ALU = get_NOP();
    WB.push_back(MEM_lwsw);
    MEM_lwsw = get_NOP();
  }
  else {
    WB.push_back(MEM_lwsw);
    MEM_lwsw = get_NOP();
    WB.push_back(MEM_ALU);
    MEM_ALU = get_NOP();
  }

  // J: structual memory write hazard
  if (config->regFileWritePorts < config->pipelineWidth) {
    if (is_older(MEM_ALU, MEM_lwsw)) {
      WB.push_back(MEM_ALU);
      MEM_ALU = get_NOP();
    } else {
      WB.push_back(MEM_lwsw);
      MEM_lwsw = get_NOP();
    }
  }

  // J: RAW
  while (ID.size() > 0) {
    int reg_a = ID.back().inst.sReg_a;
    int reg_b = ID.back().inst.sReg_b;
    int alu_dReg = MEM_ALU.inst.dReg;
    int ls_dReg = MEM_lwsw.inst.dReg;

    if (alu_dReg == reg_a || alu_dReg == reg_b || ls_dReg == reg_a || ls_dReg == reg_b) {
      IF.push_front(ID.back());
      ID.pop_back();
    }
  }

  // J: WAW
  if (config->pipelineWidth == 2) {
    if ( MEM_ALU.inst.dReg == MEM_lwsw.inst.dReg) {
      if (is_older(MEM_ALU, MEM_lwsw)) {
        WB.push_back(MEM_ALU);
        MEM_ALU = get_NOP();
      }
      else {
        WB.push_back(MEM_lwsw);
        MEM_lwsw = get_NOP();
      }
    }
  }

  if (verbose) {/* print the instruction exiting the pipeline if verbose=1 */
    for (int i = 0; i < (int) WB.size(); i++) {
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

  // J: structual memory read hazard
  if (config->splitCaches == false && IF.size() > 0) {
    for (int i = 0; i < config->pipelineWidth; ++i) {
      IF.push_front(get_NOP());
    }
  }

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
  while ((int)IF.size() > 0 && (int)ID.size() < config->pipelineWidth) {
    ID.push_back(IF.front());
    IF.pop_front();
    insts++;
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
  while((int)IF.size() < config->pipelineWidth) {
    size_t size = trace_get_item(&tr_entry); /* put the instruction into a buffer */
    if (size > 0) {
      dinst.inst = *tr_entry;
      dinst.seq = cur_seq++;
      IF.push_back(dinst);
      insts++;
    } else {
      break;
    }
    if (verbose) {/* print the instruction entering the pipeline if verbose=1 */
      printf("[%d: IF] %s\n", cycle_number, get_instruction_string(IF.back(), true));
    }
  }
  inst_number += insts;
  return insts;
}
