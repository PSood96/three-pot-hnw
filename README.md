# 3-Pot Hold & Win — C++ Math Sim

Standalone C++17 math simulator ported from the fast path in `../ThreePotHoldWin_v1.html` (`fastBaseRound`, `fastHoldWin`, `fastSimN`). Spec numbers match `../Math_Proposal_3Pot_HoldWin.md`.

## Build

```bash
cd cpp
make
```

Requires Apple clang++ / any C++17 compiler. Binary: `bin/threepot_sim`.

```bash
make clean   # remove obj/ and bin/
```

## VS Code / Cursor

1. **Open the `cpp` folder** as the workspace (`File → Open Folder…` → `NewGame_3PotIdea/cpp`).
2. Install recommended extensions when prompted (or install **C/C++** by Microsoft manually).
3. **Build:** `Cmd+Shift+B` (runs `make`).
4. **Run / debug:** open **Run and Debug** (`Cmd+Shift+D`), pick e.g. `Run: main (100k)`, press **F5**.

Configs live in `.vscode/` (`tasks.json`, `launch.json`, `c_cpp_properties.json`).

> Open the **`cpp`** folder itself (not the parent `NewGame_3PotIdea`), so `${workspaceFolder}` points at the Makefile.

## Run

```bash
./bin/threepot_sim --mode main --spins 1000000 --seed 42
./bin/threepot_sim --mode bb1  --spins 250000  --seed 1
./bin/threepot_sim --mode bb2  --spins 250000  --seed 1
./bin/threepot_sim --mode pot1 --spins 250000  --seed 1
./bin/threepot_sim --mode pot2 --spins 250000  --seed 1
./bin/threepot_sim --mode pot3 --spins 250000  --seed 1
```

| Flag | Default | Meaning |
|------|---------|---------|
| `--mode` | `main` | `main` · `bb1` · `bb2` · `pot1` · `pot2` · `pot3` |
| `--spins` | `100000` | Rounds to simulate |
| `--seed` | `1` | `mt19937_64` seed |

### Modes / stake

| Mode | Stake | Entry |
|------|-------|--------|
| `main` | 1× | Base game + natural Hold & Win (1/220) |
| `bb1` | 80× | Bonus Buy 1 — equal ⅓ Pot1 / Pot2 / Pot3 |
| `bb2` | 200× | Bonus Buy 2 — all 3 pots (`fs2`) |
| `pot1` / `pot2` / `pot3` | 80× | Single-pot buy (`fs1A` / `fs1B` / `fs1C`) |

## Output

Report includes RTP %, mean × bet, hit rate, feature rate, pot-use / tier hits, win bands (0 / &lt;1 / &lt;5 / &lt;20 / &lt;100 / 100+), and runtime.

## Layout

```
include/   types, rng, config, base_game, hold_win, sim
src/       implementations + CLI main
```

## Notes

- Money uses 2-decimal rounding like the HTML `round2`.
- `fs2` only when the bonus **starts** with all 3 pots; mid-unlock still grants pot effects.
- RNG is not bit-identical to JS `Math.random()` — compare RTP at large N, not spin-by-spin.
