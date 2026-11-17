# Project conventions

- Use C++23 and object-oriented interfaces.
- Always separate concerns. Keep domain models, ledger/validation rules,
  cryptographic helpers, CLI/input-output, build tooling, and tests in focused
  components with clear responsibilities. Split components when responsibilities
  diverge; avoid unrelated logic in the same class or file. Domain code must not
  depend on CLI or presentation code.
- Keep `module.hpp` files as declaration/header indexes only: no business logic,
  executable code, or mutable state. Public indexes must not expose internals.
- "Module" means namespace in this project's naming convention, not a filename
  or a C++20 module unit. Public namespaces have no underscores (`corecoin`);
  private namespaces have a double underscore prefix (`corecoin::__detail`,
  `corecoin::__cli`, `corecoin::__tests`). This is the user's explicit convention,
  despite C++ reserving double-underscore identifiers.
- Filenames do not need underscore prefixes. Keep public headers under
  `include/corecoin/` and internal headers under `src/`; do not expose internal
  headers through the library's PUBLIC include directories.
- Maintain `module.hpp` index headers, analogous to TypeScript `index.ts`:
  `include/corecoin/module.hpp` aggregates the public API; `src/module.hpp`
  aggregates reusable private helpers. Consumers use the appropriate index.
  Never include the private index from a public header. Implementation files
  may include their own declaration header directly.
- User-requested macro convention: triple underscore (`___`) prefix. Prefer typed
  constants and `#pragma once` instead of introducing reserved macro identifiers.
  There are currently no project-defined macros.
- Always regenerate the root `compile_commands.json` after changing sources, headers,
  source layout, or compiler flags. In Ubuntu WSL run
  `python3 scripts/build.py --configure-only`; `--test` refreshes, builds, and tests.
  Do not hand-edit the generated database. Keep it present for Clang tooling.
- CMake builds must export the compilation database and refresh the root copy.
