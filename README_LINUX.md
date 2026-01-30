# SAmultithread_4d Linux Build Instructions

## Prerequisites
- A C++ compiler supporting C++23 (e.g., GCC 13+ or Clang 16+).
- Make

## Build
Run the following command in the project root:
```bash
make
```

The executable will be created at `bin/SAmultithread_4d`.

## Run
```bash
./bin/SAmultithread_4d
```

## Notes
- This project uses `std::print` and other C++23 features. Ensure your compiler is up to date.
- Shared memory monitoring is implemented via `shm_open` on Linux (mapped to `/SAMonitor4D`).
