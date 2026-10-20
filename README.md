# Simple Shell

A simple Linux shell implemented in C.

## Features
- Execute commands using `fork()` and `execvp()`
- Run commands in the background with `&`
- List background processes with `bglist` or `jobs`
- View and save command history with `history`
- Log shell events using syslog

## Requirements
- Linux or WSL
- GCC

## Compile and Run
```bash
gcc -Wall -Wextra main.c -o my_shell
./my_shell
```

## Limitations
- Pipes and shell operators are not supported.
- Quoted arguments are not fully supported.

