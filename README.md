# Network Chat Application

## Academic Project

This is a university practice for the Distributed Systems Programming course (INSD_PSDI_U2_Actividad_Práctica). The project implements a multi-threaded client-server chat application using C++ and Berkeley sockets to demonstrate concurrent programming and network communication concepts.

## Overview

A real-time chat system where multiple clients can connect to a server and exchange messages. The application supports both public broadcasts and private messaging between users.

## Features

- Multi-client support with concurrent connections
- Public messaging (broadcast to all users)
- Private messaging between specific users
- Thread-safe operations using mutexes
- Graceful connection management
- Color-coded console output

## Architecture

### Server Architecture
The server operates on a multi-threaded model:
- **Main Thread**: Accepts incoming client connections
- **Worker Threads**: One dedicated thread per connected client
- **Shared State**: Thread-safe user map (`map<string, int>`) protected by mutexes

### Client Architecture
Each client uses two threads:
- **Main Thread**: Handles user input and sends messages to the server
- **Receive Thread**: Continuously listens for incoming messages from the server

## Protocol Design

The application uses a binary protocol with three message types:

| Type | Value | Description |
|------|-------|-------------|
| PUBLIC | 0 | Broadcast message |
| PRIVATE | 1 | Private message to specific user |
| NOTIFICATION | 2 | Server notification |

## Prerequisites

- **Compiler**: G++ with C++11 support or higher
- **Build System**: CMake 3.5 or higher
- **Operating System**: Linux/Unix-based system (uses POSIX sockets)
- **Libraries**: pthread (POSIX Threads)

## Building the Project

1. Navigate to the project directory:
   ```bash
   cd net-chat
   ```

2. Generate build files and compile:
   ```bash
   cmake .
   make
   ```

This generates two executables: `server` and `client`.

## Usage

### Start the Server

```bash
./server
```

The server listens on port 3000.

### Connect Clients

In separate terminals, run:

```bash
./client
```

Enter a username when prompted.

### Commands

- **Public message**: Type your message and press Enter
- **Private message**: `/msg <username> <message>`
- **Exit**: `exit()`

## Project Structure

```
net-chat/
├── CMakeLists.txt      # Build configuration
├── server.cpp          # Server implementation
├── client.cpp          # Client implementation
├── utils.h             # Header declarations
├── utils.cpp           # Network utilities
└── README.md           # Documentation
```

## Implementation Details

### Server
- Multi-threaded architecture (one thread per client)
- Maintains a thread-safe user map
- Routes messages based on type (public/private)
- Handles client connections and disconnections

### Client
- Two threads: main (sending) and receiver (listening)
- Parses user commands
- Handles public and private message formatting

### Thread Safety
Uses C++11 mutexes to protect shared data structures:
```cpp
std::mutex users_mutex;
std::lock_guard<std::mutex> lock(users_mutex);
```

## Academic Information

**Course**: Distributed Systems Programming (PSDI)  
**Unit**: 2 - Practical Activity  
**Institution**: U-TAD  
**Date**: November 2025
