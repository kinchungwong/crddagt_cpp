# `workspace/crddagt_cpp/AGENTS.md`

## Stated goal

This C++ project contains the core essentials of a runtime representation of the CRDDAGT (Create, Read, Destroy Directed Acyclic Graph of Tasks/Transactions) model.

The runtime representation does not provide any facility to execute actual tasks, but allows such tasks and their data dependencies to be represented as a DAG in memory, and queried for correctness, consistency, and other properties.

It is meant to be used as a core building block in creating a declarative and phased (define, validate, execute) task graph parallel execution library in C++. The original grand scheme was meant to support declaring large scale OpenCV image processing flow graphs and to automate the parallelization and memory management to the fullest extent.

In order to keep this core project minimal and focused, it does not depend on OpenCV or any other third-party libraries except the C++ Standard Library. Googletest is used for unit testing only.

## Technology: C++ projects

Project crddagt (repository name `crddagt_cpp`) is written in C++.

- C++ Standard: C++17.
    - **Strictly NO** access to C++20 or later features, since downstream users may remain on C++17 for a long time.
- OS: Ubuntu 24.04, amd64 only.
- Compiler: GCC 14.x only.
- Build system: CMake 3.26.x and up.
- Third-party dependencies:
    - Googletest
    - **Strictly NO** other third-party dependencies allowed.

## Namespace

All code in this project reside in the top-level namespace `crddagt`.

## File organization

All C++ header files in this project are meant to be included via a top-level include path `crddagt/`.

- src
    - crddagt
        - (first-level headers and sources in the same directory)
        - (second-level namespaces)
            - (second-level headers and sources in same directory)
- tests
    - crddagt

## Git repository layout (relevant for Git-related or GitHub-related AI agent tasks)

Currently, `crddagt_cpp` is in early bootstrapping stage, and exists as a subproject folder in the monorepo "repo_20251205". In the future, it will be transferred to a GitHub repository of its own. The transition will necessitate significant CI/CD retooling. Do not prematurely optimize for this transition unless explicitly initiated by the human user.

## The thoughts folder

The `thoughts` folder contains design notes, implementation plans, retrospectives, and sometimes even rants related to the project.

Use this as a source for epistemic discovery.

Some are from humans and some from AI. What is important to know is that each of these documents contains a point-in-time snapshot of the thinking process during the development of the project. Some information might not be true anymore; some decisions might have been overturned. But each thought document that is allowed to be merged into the main branch has been reviewed and accepted by a human at the time. The level of thinking that went into each document may vary, but is typically very intense.

Only markdown files (`*.md`) allowed. Filename **must** start with a timestamp, and must be based on UTC-08 time zone (`America/Los_Angeles`). It is mandatory so that thoughts files from all sources (including human-created ones) are in chronological order, which is needed for anyone to reconstruct the decision process chronologically and what new decisions have superseded the older ones.

```sh
TIMESTAMP=$(TZ=':America/Los_Angeles' date '+%Y-%m-%d_%H%M%S')
```

All thoughts files **must** contain a header section with the following information:

- The first line must be the Title, which must **correctly** mention its own filename.
- Author (human or AI)
- Date (in UTC-08 time zone, `America/Los_Angeles`)
- Document status: Active, Dormant, or Frozen.
    - Active means it is in the process of being collaboratively constructed and may see big rewrites.
    - Dormant means it is not being actively worked on, but may be revisited in the future. Inline notes may be inserted into parts that became amended, superseded, or irrelevant. Do not perform major rewrites or deletions.
    - Frozen means it is a historical document that should not be modified anymore, even if they contain outdated, superseded, or incorrect information. This is necessary to prevent unnecessary churn committed by AI agents.

## General Agentic skills for this project

- Strong C++ programming skills, especially in modern C++ (C++17).
- Strong understanding of data structures and algorithms, especially graph algorithms. *(Most importantly: Union-Find, Kahn, and Tarjan SCC.)*
- Familiarity with build systems, especially CMake.
- Advanced usage of Googletest, such as parametrized tests.
- Interpretation of additional printouts from unit tests to maintain alignment of AI agents between expected and actual code behavior.
- Human users may request a "Socratic", a turn-based questioner-driven dialogue to help them think through problems. When Socratic is requested, it is implied that an escalation from "Rubber Ducky Debugging" is warranted by the situation.
- Human users may request a "Mikado", a planning exercise to break down complex code-refactoring tasks into smaller, manageable steps. *(Footnote: Mikado refers to the book "The Mikado Method". Any use of Mikado should automatically bring up the skills from the book "Working Effectively With Legacy Code", for example the creation of "seams".)*

## Instructions for AI code reviews

The provenance of correctness of algorithms is of utmost importance.

The correctness of the C++ implementation of said algorithms is equally important.

The correctness of the C++ code concerns both its execution-time behaviors, and its adherence to the C++ code standard, such as strict avoidance of undefined behavior, memory safety, and strict-aliasing rules.

Class-level documentation strings ("Doxygen comments") must include a brief and precise thread-safety responsibility statement. Some classes are twinned as facade-implementation pairs; each must have their thread-safety responsibility boundary described.

Regarding natural language proofreading (English in markdown files and code comments), only high-impact issues should be reported. Excessively nitpicking on language will be penalized and filed as bug reports against the AI code review system.
