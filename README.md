<img src="assets/icon.svg" width="64" height="64" alt="">

# CoreCoin

An object-oriented C++23 blockchain coin prototype. The `corecoin` library is
separate from the CLI and uses OpenSSL 3 for key operations.

## Build and run

Requires OpenSSL 3 development headers and libcrypto, Python 3 (build script) or CMake 3.20+, and a C++23-capable compiler with its standard library
(for example Visual Studio 2022 with Desktop development with C++, GCC 13+, or
Clang 17+ with an appropriate SDK).

Initialize the pinned GoogleTest submodule before building the tests:

```sh
git submodule update --init --recursive
```

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

With Visual Studio's multi-configuration generator:

```powershell
.\build\Release\corecoin-cli.exe --demo
.\build\Release\corecoin-cli.exe --shell
```

With Ninja or Make on Linux/macOS, executables are `./build/corecoin-cli` and
`./build/corecoin-tests`. Pass `-DCMAKE_BUILD_TYPE=Release` when configuring.

Recommended on this machine: use the build script in Ubuntu WSL. Every invocation
refreshes `compile_commands.json` in the repository root and build directory before
compiling, including when the build fails. It discovers all source files and records
the actual compiler, C++23 flags, and absolute public/private include paths.

```sh
python3 scripts/build.py --test
./build-local/corecoin-cli --demo
# Refresh editor metadata without compiling:
python3 scripts/build.py --configure-only
# If Clang and its standard library are installed:
python3 scripts/build.py --compiler clang++ --test
```

CMake also enables `CMAKE_EXPORT_COMPILE_COMMANDS`. Use Ninja or Unix Makefiles
for database export; each build copies it to the repository root. Visual Studio's
generator does not export this database. The database contains machine-specific
absolute paths: always regenerate it in the environment where the editor runs.

## Try the coin

### VS Code on this Windows setup

Open this project in **Ubuntu WSL** so IntelliSense uses the same GCC toolchain
as the verified build. In VS Code, run **WSL: Reopen Folder in WSL** and select
Ubuntu (or **WSL: Open Folder in WSL...** if your default distribution differs).
The project path there is `/mnt/c/Users/ertun/.t3/projects/core-coin`.
Install the Microsoft **C/C++** extension in **WSL: Ubuntu** when prompted.

The checked-in `.vscode/c_cpp_properties.json` selects `/usr/bin/g++`, C++23,
and the root compilation database. `.clangd` and the clangd extension settings
select the same database; clangd may query GCC for its standard-library paths.
Use either Microsoft C/C++ IntelliSense or clangd as your language server.
Run **Tasks: Run Task → CoreCoin: refresh compile_commands.json** after changing
sources or flags, or allow the folder-open task. **Ctrl+Shift+B** refreshes,
builds, and tests. After refreshing, use **clangd: Restart language server** if
old include errors remain. Automatic CMake
configuration is disabled for this workspace because CMake is not installed
in this WSL environment.

If `<cstdint>` cannot be found in a native Windows window, that window is not
using the WSL compiler. Reopen it in WSL; adding a project include directory
cannot supply the missing standard library. For native Windows development,
install Visual Studio Build Tools with **Desktop development with C++** and
the Windows SDK, then select that compiler via **C/C++: Edit Configurations (UI)**.

### CLI

The initial wallet is **`founder`**, funded with **11,000 CORE**
(1,100,000,000,000 Gil) in the fixed genesis block. Every new chain has exactly
this allocation. Opening or selecting a wallet never mints more coins.

The default `--demo` sends 12 CORE from founder to Bob, then 2 CORE back to
founder, mining each transfer for `miner`. Final balances are founder
10,990 CORE, Bob 10 CORE, and miner 22 CORE.

In `--shell`, enter:

```text
receive
balance
send bob 1000000000
mine miner
wallet bob
receive
balance
send founder 100000000
mine miner
balance founder
balance bob
chain
validate
quit
```

CoreCoin's smallest indivisible unit is called **Gil**, like Bitcoin's satoshi:
**1 CORE = 100,000,000 Gil**, and **1 Gil = 0.00000001 CORE**.
The shell selects `founder` on startup. `wallet ADDRESS` selects another named
wallet (an unused address starts at zero). `receive` displays the selected
wallet's address to share with the sender; receiving does not mint coins.
`send TO GIL` sends from the selected wallet. The legacy `send FROM TO GIL`
form also works. `balance` shows confirmed and pending-adjusted balances;
`balance ADDRESS` inspects a specific wallet.

`send-core TO CORE` accepts decimal CORE amounts with up to eight decimal places.
`history [ADDRESS]` displays confirmed and pending transactions for an account.
Use `wallet-create FILE` to create and select an encrypted key-backed wallet;
enter a password of at least 12 characters at the prompt. Use `receive` to get
its address. From the founder wallet, send to that address and mine the transfer;
then use `wallet-open FILE` and the password to select it and send signed transfers.
Quote paths containing spaces. Back up the encrypted file and keep the password
separately: neither can be recovered if lost. Password input is hidden on a terminal;
redirected input is not protected from the process supplying it.

Amounts in `send` are integer Gil; for example, `send bob 1` sends 1 Gil.
Transfers become confirmed after mining. The shell saves confirmed blocks to `corecoin.chain` and validates them on restart.
Pass a different path with `--shell PATH`. If an interrupted save leaves only a
backup, startup validates and restores it. If both files exist, inspect them
manually before reopening. Pending transactions are held in memory
and are lost on exit; mine them to confirm and save them. These addresses work only within
the same local chain; there is no transfer between separate running processes.
Names use 1–64 ASCII letters, digits, underscores, or hyphens.

## Design

Here, "module" means namespace, not a filename or a C++ `export module` unit.
The public namespace is `corecoin`; private namespaces use a double underscore
prefix: `corecoin::__detail`, `corecoin::__cli`, and `corecoin::__tests`.
Namespace naming expresses an internal API convention, not language-enforced
access control. Public headers live in `include/corecoin/`; private headers such
as `src/crypto/sha256.hpp` stay out of the public include path. Filenames need no prefix.
Each API layer has a `module.hpp` index header, analogous to TypeScript's
`index.ts`. Applications use `#include <corecoin/module.hpp>` to access the public
classes and functions. Internal code and tests can include `src/module.hpp`
(as `#include "module.hpp"` using the private include path) for reusable
`corecoin::__detail` helpers. The public index never includes the private index.
These headers aggregate declarations using `#include`; they do not use C++20
module exports. CLI and test helpers remain local to their implementation files.
C++ reserves double-underscore identifiers; these namespace names follow the
requested project convention with that portability limitation.
The requested macro prefix is `___`; no project macros are currently needed,
so typed constants and `#pragma once` avoid reserved macro names.

- `Transaction` encapsulates sender, recipient, amount, and sequential account
  nonce. Length-prefixed serialization produces deterministic SHA-256 IDs.
- `Wallet` handles named account selection, querying balances, and submitting
  transfers. Address checks, amount formatting, and genesis construction live
  in separate components. The wallet does not mine or print output.
- `SigningKey` generates Ed25519 keys, signs messages, verifies signatures, and
  stores keys in password-encrypted PKCS#8 files. Back up the encrypted file and
  keep its password separately; losing either prevents recovery. Construct a
  `Wallet` from a `SigningKey` to sign transfers from its `cc_` address.
  The ledger verifies signatures during submission and chain replay.
- `Block` commits to its height, previous hash, timestamp, difficulty, nonce,
  and ordered transactions. Mining searches for leading hexadecimal zeroes.
- `Ledger` owns account balances and nonces and applies transfer rules.
- CMake builds the amount, cryptography, transaction, ledger, chain, wallet,
  storage, and CLI input components as separate libraries. The `corecoin`
  interface target gathers the public components for consumers.
- `Blockchain` owns the chain, pending pool, and confirmed/projected ledgers.
  Submission checks pending balances to prevent double spending and nonces to
  prevent replay. A block contains at most 1,000 transfers and one final reward.
- The internal `ChainValidator` independently replays every block from a fixed genesis anchor,
  checking hashes, proof of work, links, heights, timestamps, transaction rules,
  reward amounts, and overflow. Rewards are applied after transfers.
- `ChainConfig` sets fixed difficulty (1–4, default 3) and reward (default 11 CORE).
  Each mining call has a 10-million-hash attempt limit. Total issuance is capped
  at 11,000,000 CORE, including the founder allocation. The final reward is reduced
  to the remaining supply, and later blocks carry a zero reward. There is no halving.
  Genesis is a fixed unmined anchor committing to the founder's
  11,000 CORE allocation. Validation replays that allocation before transfers;
  it rejects any different genesis allocation. This changes the genesis hash
  from the earlier empty-genesis prototype.

The library exposes immutable views of chain data; mining and submission keep
state unchanged on rejected operations. Pending incoming funds may be spent
by a later transaction in the same ordered block. Current-block rewards cannot.

## Scope

This is an educational local prototype, **not suitable for real funds**.
The founder and other named accounts remain unsigned: any local caller can spend
from them. Key-backed `cc_` accounts require valid signatures, but cannot secure
legacy balances until they are transferred. It has no peer-to-peer
network, distributed consensus/fork selection, transaction fees,
difficulty adjustment, or audited cryptography. Proof of work demonstrates
local integrity checks; it does not by itself make this a secure currency.

GoogleTest suites cover standard SHA-256 vectors, transfers, pending double spends, replay,
invalid amounts/accounts, overflow, and forged blocks with recomputed proof of work.
