# Whole-engine instruction attribution

Only setParameters + process is collected. All rows measure 256 blocks; Ir means executed instructions, not CPU cycles. Movement=1 here is the original small low-frequency target movement, mostly epsilon snaps. The separately proven sustained glide is tabulated below.

| Rate / block / bands / movement / OS / Mix | Baseline Ir/block | A / baseline | C / baseline | A2 / baseline |
|---|---:|---:|---:|---:|
| 48000 / 128 / 2 / 0 / 0 / 1 | 73564.66 | 1.15018 | 1.14488 | 1.15324 |
| 48000 / 128 / 2 / 1 / 0 / 1 | 74160.58 | 1.14911 | 1.14385 | 1.15214 |
| 48000 / 128 / 3 / 0 / 0 / 1 | 91630.12 | 1.27159 | 1.26734 | 1.27405 |
| 48000 / 128 / 3 / 1 / 0 / 1 | 92225.95 | 1.26995 | 1.26572 | 1.27239 |
| 48000 / 128 / 4 / 0 / 0 / 1 | 106363.80 | 1.36647 | 1.36280 | 1.36858 |
| 48000 / 128 / 4 / 1 / 0 / 1 | 106959.73 | 1.36452 | 1.36087 | 1.36662 |
| 192000 / 32 / 4 / 0 / 0 / 1 | 29328.02 | 1.33398 | 1.33050 | 1.33756 |
| 192000 / 32 / 4 / 1 / 0 / 1 | 29777.56 | 1.32928 | 1.32585 | 1.33280 |
| 48000 / 32 / 4 / 0 / 0 / 1 | 29329.71 | 1.33396 | 1.33048 | 1.33754 |
| 192000 / 256 / 4 / 0 / 0 / 1 | 209320.51 | 1.37211 | 1.36842 | 1.37395 |
| 48000 / 128 / 4 / 0 / 1 / 1 | 260684.98 | 1.14952 | 1.15052 | 1.15038 |
| 48000 / 128 / 4 / 0 / 0 / 0.5 | 147391.81 | 1.00007 | 1.00180 | 1.00006 |

A2 final five-TU rebuild repeats all 12 original guarded counts identically. Raw callgraphs and commands are under `results/h4-counts/` and `results/h4-counts-guard-final/`. Actual wrapped OS execution has positive up/down processing call counts.

## Sustained per-sample gliding

| Rate / block / bands | Baseline Ir/block | A2 Ir/block | A2 / baseline | Qualified gliding samples |
|---|---:|---:|---:|---:|
| 48000 / 128 / 2 | 87240.32 | 99793.33 | 1.14389 | 32768 |
| 48000 / 128 / 4 | 120039.41 | 160523.42 | 1.33726 | 32768 |
| 192000 / 32 / 4 | 32665.68 | 42885.70 | 1.31287 | 8192 |
| 192000 / 256 / 4 | 233173.78 | 313999.82 | 1.34663 | 65280 |

The live-cutoff/fade observer and conservative residual-distance bound run outside the collection region. The 192k/256 row has one unqualified block. These are counts, not measured deadline or native CPU guarantees.
