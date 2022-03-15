./five_stage -t traces/q4.tr -c confs/1-wide.conf

./five_stage -t traces/structural_memory.tr -c confs/1-wide.conf

# Done

## structural_memory

```c++
+ Number of cycles : 23
+ IPC (Instructions Per Cycle) : 0.4348
./five_stage -t traces/structural_memory.tr -c confs/1-wide.conf

+ Number of cycles : 14
+ IPC (Instructions Per Cycle) : 0.7143
./five_stage -t traces/structural_memory.tr -c confs/1-wide-opt.conf

// TODO
+ Number of cycles : 17
+ IPC (Instructions Per Cycle) : 0.5882
./five_stage -t traces/structural_memory.tr -c confs/2-wide.conf

+ Number of cycles : 14
+ IPC (Instructions Per Cycle) : 0.7143
./five_stage -t traces/structural_memory.tr -c confs/2-wide-opt.conf
```

## structural_regfile
```c++
+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/structural_regfile.tr -c confs/1-wide.conf

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/structural_regfile.tr -c confs/1-wide-opt.conf

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/structural_regfile.tr -c confs/2-wide.conf

+ Number of cycles : 5
+ IPC (Instructions Per Cycle) : 0.4000
./five_stage -t traces/structural_regfile.tr -c confs/2-wide-opt.conf
```

## data_waw
```c++
+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/data_waw.tr -c confs/1-wide.conf

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/data_waw.tr -c confs/1-wide-opt.conf

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/data_waw.tr -c confs/2-wide.conf

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/data_waw.tr -c confs/2-wide-opt.conf
```

## control_not_taken
```c++
+ Number of cycles : 7
+ IPC (Instructions Per Cycle) : 0.4286
./five_stage -t traces/control_not_taken.tr -c confs/1-wide.conf

+ Number of cycles : 7
+ IPC (Instructions Per Cycle) : 0.4286
./five_stage -t traces/control_not_taken.tr -c confs/1-wide-opt.conf

+ Number of cycles : 7
+ IPC (Instructions Per Cycle) : 0.4286
./five_stage -t traces/control_not_taken.tr -c confs/2-wide.conf

+ Number of cycles : 7
+ IPC (Instructions Per Cycle) : 0.4286
./five_stage -t traces/control_not_taken.tr -c confs/2-wide-opt.conf
```

## control_taken
```c++
+ Number of cycles : 9
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/control_taken.tr -c confs/1-wide.conf

+ Number of cycles : 7
+ IPC (Instructions Per Cycle) : 0.4286
./five_stage -t traces/control_taken.tr -c confs/1-wide-opt.conf

+ Number of cycles : 9
+ IPC (Instructions Per Cycle) : 0.2222
./five_stage -t traces/control_taken.tr -c confs/2-wide.conf

+ Number of cycles : 7
+ IPC (Instructions Per Cycle) : 0.4286
./five_stage -t traces/control_taken.tr -c confs/2-wide-opt.conf
```

## data_raw
```c++
+ Number of cycles : 9
+ IPC (Instructions Per Cycle) : 0.2222
./five_stage -t traces/data_raw.tr -c confs/1-wide.conf

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/data_raw.tr -c confs/1-wide-opt.conf

+ Number of cycles : 9
+ IPC (Instructions Per Cycle) : 0.2222
./five_stage -t traces/data_raw.tr -c confs/2-wide.conf

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/data_raw.tr -c confs/2-wide-opt.conf
```

## data_load_use
```c++
+ Number of cycles : 9
+ IPC (Instructions Per Cycle) : 0.2222
./five_stage -t traces/data_load_use.tr -c confs/1-wide.conf

+ Number of cycles : 7
+ IPC (Instructions Per Cycle) : 0.2857
./five_stage -t traces/data_load_use.tr -c confs/1-wide-opt.conf

+ Number of cycles : 9
+ IPC (Instructions Per Cycle) : 0.2222
./five_stage -t traces/data_load_use.tr -c confs/2-wide.conf

+ Number of cycles : 7
+ IPC (Instructions Per Cycle) : 0.2857
./five_stage -t traces/data_load_use.tr -c confs/2-wide-opt.conf
```

## sample
```c++
./five_stage -t traces/sample.tr -c confs/2-wide-opt.conf
```

# TODO

structural_regfile.2-wide.diff

./five_stage -t traces/structural_regfile.tr -c confs/2-wide.conf


data_waw.2-wide.diff

data_waw.2-wide-opt.diff

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/data_waw.tr -c confs/2-wide.conf

+ Number of cycles : 6
+ IPC (Instructions Per Cycle) : 0.3333
./five_stage -t traces/data_waw.tr -c confs/2-wide-opt.conf


lw dst src

1 lw $t2, 12($s0)
2 la $t0, player_x
3 lw $t1, 0($t0)
4 addi $t1, $t1, 16
5 blt $t1, $t2, loop
1 lw $t2, 12($s0)

&t_sReg_a, &t_sReg_b, &t_dReg
addi t_dReg, t_sReg_a, t_sReg_b

R for R-type, L for load, S for store, B for branch or N for No-op

PC Type src_a src_b dst address

3 = s0

4 L 3 255 2 4097
8 I 0 255 0 345677
12 L 0 255 1 4578
16 I 1 255 1 16
20 B 1 2 255 4
4 L 3 255 2 4097

if (L.Rt == F.

Now, the file "q4.tr" contains the following instructions:
   LOAD: (Seq:        1)(Regs:   2,   3, 255)(Addr: 4097)(PC: 4)
  ITYPE: (Seq:        2)(Regs:   0, 255, 255)(Addr: 345677)(PC: 8)
   LOAD: (Seq:        3)(Regs:   1,   0, 255)(Addr: 4578)(PC: 12)
  ITYPE: (Seq:        4)(Regs:   1,   1, 255)(Addr: 16)(PC: 16)
 BRANCH: (Seq:        5)(Regs: 255,   1,   2)(Addr: 4)(PC: 20)