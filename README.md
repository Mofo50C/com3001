# COM3001 Project: Software Transactional Concurrency Control For Persistent Memory
By Mohammad Foroughi (6621031)

Minimum OS Requirements
---
- This project was tested with a emulated persistent memory environment on a 64-bit x86 Linux machine running Ubuntu 22.04.4 with Kernel version 6.5.0-35. 
    - (Check [here](https://www.intel.com/content/www/us/en/developer/articles/training/how-to-emulate-persistent-memory-on-an-intel-architecture-server.html) for kernel requirements of emulated pmem and [here](https://docs.pmem.io/persistent-memory/getting-started-guide/creating-development-environments/linux-environments/linux-memmap) for instructions on setting it up)

- Alternatively the `emulating pmem.md` file has some basic instructions.

- For actual hardware-backed pmem enviornment, the project was tested on 64-bit x86 Ubuntu 20.04.6 with Kernel version 5.4.0-172.

Pre-requisites
---
- libpmemobj: from 1.13.1 to 2.0.1 (See [here](https://github.com/pmem/pmdk))
- gcc: minimum 9.4.0
- GNU Make: minimum 4.2.1

Building
---
- Clone the repository and `cd` into the root directory.
- To build and run the benchmarks navigate to either `pmdk_stms/benchmarks` or `volatile_stms/benchmarks`
- Navigate to either the hashmap or queue benchmark.
- Run `make build` once to build all algorithms and necessary libraries.
    - NB: PMDK should already have been compiled and installed prior to this.
- Run `make bench-<alg>` replacing `<alg>` with either `tml` or `norec`.
- Finally, run `./bench-tml` or `./bench-norec` to see usage of the command.
    - NB: the persistent benchmarks require a path to a pool file on the DAX file-system as the first command line argument.

