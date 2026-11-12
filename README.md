# KRYPT-OS v2.0

**Windows Cryptographic Extension & System Lockdown Module**

## Overview

KRYPT-OS is an advanced system-level encryption module designed for forced data protection (AES-256-CBC) and interactive session lockdown. The program implements persistence mechanisms, anti-analysis techniques (Anti-VM / Anti-Debug), and backup destruction.

## Key Features

- **AES-256 Encryption**: Utilizes native Windows CryptoAPI for block encryption (reads in 4 KB chunks, allowing processing of files of any size without memory overflow).
- **Anti-VM / Anti-Debug**: Checks the environment for debuggers (x64dbg, OllyDbg) and virtual machine processes (VirtualBox, VMware). If detected, execution is aborted.
- **Backup Destruction**: Executes system utilities (`vssadmin`, `wmic`, `bcdedit`) to delete shadow copies and disable system recovery.
- **Aggressive Lockdown**: Installs a low-level keyboard hook (`WH_KEYBOARD_LL`) to block `Win`, `Alt+Tab`, `Alt+F4`, and `Ctrl+Esc`.
- **Process Termination**: A background thread kills `Taskmgr.exe` and `Process Hacker` every second, and disables Task Manager via the registry.
- **Persistence**: Copies itself to a hidden directory `%APPDATA%\KryptOS` (with HIDDEN+SYSTEM attributes) and adds itself to the startup registry key (`HKCU\...\Run`).

## Visual Interface

The interface is built on pure WinAPI (GDI) and includes:

1. **Matrix Rain**: Background animation of falling characters.
2. **ASCII Skull**: Custom skull rendering.
3. **Live Terminal**: Real-time encryption log output (in English).
4. **Countdown Timer**: A timer (3 minutes by default) that triggers a forced shutdown (`shutdown /s /f /t 0`) upon expiry.

## System Requirements

- **OS**: Windows 10 / 11 (x64)
- **Privileges**: Administrator (required for shadow copy deletion, hook installation, and killing system processes).
- **Compiler**: MinGW (GCC) or MSVC.

## Compilation

```bash
g++ main.cpp -o svchost.exe -mwindows -lole32 -lshlwapi -lcrypt32 -static -lstdc++fs -O2