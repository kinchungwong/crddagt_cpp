# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**crddagt_cpp** is a C++ library for runtime representation of CRDDAGT (Create, Read, Destroy Directed Acyclic Graph of Tasks/Transactions).

The library allows tasks and their data dependencies to be represented as a DAG in memory and queried for correctness, consistency, and other properties. It does not execute tasks, but serves as a core building block for declarative, phased (define, validate, execute) task graph parallel execution systems.

**Design Goal:** Minimal dependencies. The library depends only on the C++ Standard Library. GoogleTest is used for unit testing only.

## Technology Stack

- **C++ Standard**: C++17 only (strictly no C++20 features - downstream compatibility required)
- **OS**: Ubuntu 24.04, amd64
- **Compiler**: GCC 14.x
- **Build System**: CMake 3.26+
- **Testing**: GoogleTest
- **Namespace**: All code resides in namespace `crddagt`

## Build Commands

### Standalone Build (Primary)

When working with crddagt_cpp as a standalone repository:

```bash
# Configure
cmake -S . -B build/default -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build/default

# Run tests
./build/default/crddagt_cpp_tests

# Generate Doxygen docs (requires Doxygen installed)
doxygen Doxyfile
```

Build outputs go to `build/default/`.

### Monorepo Build (When Available)

When crddagt_cpp is used as a submodule in the monorepo with `build.sh` available:

```bash
# From the monorepo workspace root (parent of crddagt_cpp/)
./build.sh --project crddagt_cpp --configure --build --run-tests

# Individual steps
./build.sh --project crddagt_cpp --configure    # CMake configure
./build.sh --project crddagt_cpp --build        # Build only
./build.sh --project crddagt_cpp --run-tests    # Run tests only
./build.sh --project crddagt_cpp --build-docs   # Generate Doxygen docs
```

To detect monorepo context, check for `../build.sh` existence. Always qualify with directory to avoid ambiguity with other build.sh scripts.

## Known Issues: External Dependency Paths

**Status: To be fixed in a future update.**

The current `CMakeLists.txt` references external dependencies using relative paths that assume a monorepo layout:

```cmake
# GoogleTest - assumes ../googletest/download exists
set(GTEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../googletest/download")

# OpenCV - assumes ../opencv-mini/build/out exists
find_package(OpenCV REQUIRED PATHS "../opencv-mini/build/out" NO_DEFAULT_PATH)
```

**Impact:**
- Standalone builds will fail CMake configure unless these paths are satisfied
- The OpenCV dependency is linked (in both `src/CMakeLists.txt` and `tests/CMakeLists.txt`) but not actually used in current source code

**Workaround for standalone use:**
- GoogleTest: Clone GoogleTest to `../googletest/download` relative to the project root
- OpenCV: Either provide a compatible OpenCV build at the expected path, or modify CMakeLists.txt to make OpenCV optional

This will be addressed in a future update to support fully standalone builds.

## Project Structure

```
crddagt_cpp/
├── src/
│   ├── main.cpp              # Entry point (placeholder)
│   └── crddagt/
│       └── common/           # Core library headers and sources
│           ├── graph_core.hpp
│           ├── iterable_union_find.hpp
│           ├── opaque_ptr_key.hpp
│           └── ...
├── tests/
│   └── common/               # GoogleTest test files
├── thoughts/                 # Timestamped design documents
├── docs/
│   └── generated/            # Doxygen output (gitkeep)
├── term_logs/                # Build/run logs (read-only for agents)
├── build/                    # Build output (gitkeep)
├── CMakeLists.txt
├── Doxyfile
├── AGENTS.md                 # Detailed agent instructions
└── README.md
```

### Key Components

- **GraphCore**: Core DAG representation and validation
- **IterableUnionFind**: Union-Find data structure with iteration support
- **OpaquePtrKey / OpkUniqueList**: Type-erased pointer handling utilities
- **UniqueSharedWeakList**: Smart pointer management utilities

## Thoughts Folder Convention

The `thoughts/` folder contains timestamped design documents capturing point-in-time decisions.

**Filename format**: `YYYY-MM-DD_HHMMSS_description.md` (timezone: `America/Los_Angeles`)

```bash
TIMESTAMP=$(TZ=':America/Los_Angeles' date '+%Y-%m-%d_%H%M%S')
```

**Required header fields:**
- Title (must match filename)
- Author (human or AI)
- Date (in America/Los_Angeles timezone)
- Document status: **Active**, **Dormant**, or **Frozen**
  - Active: Under collaborative construction, may see rewrites
  - Dormant: Not actively worked on, may have inline amendments
  - Frozen: Historical document, do not modify

## Code Standards

### Algorithm Correctness
Provenance of algorithm correctness is critical. Implementations of Union-Find, Kahn's algorithm (topological sort), and Tarjan's SCC must be verifiable.

### Thread Safety Documentation
Every class must include a Doxygen comment with a thread-safety responsibility statement. Facade-implementation pairs must document their thread-safety boundaries.

### C++ Correctness
- Strict adherence to C++ standards
- No undefined behavior
- Memory safety
- Strict aliasing compliance

### Natural Language
Only report high-impact issues in markdown files and code comments. Excessive nitpicking on language is discouraged.

## Agentic Capabilities

- **Socratic**: Turn-based questioning dialogue for thinking through problems. Use when escalating from rubber duck debugging.

- **Mikado**: Planning exercise for breaking down complex refactoring into smaller steps. References "The Mikado Method" and "Working Effectively With Legacy Code" (seams concept).

## Additional Resources

See `AGENTS.md` for comprehensive agent instructions including detailed guidelines on file organization, code review standards, and project-specific conventions.
