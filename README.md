# ESP32 App Framework

A modern C++ framework for building structured ESP32 applications using ESP‑IDF 5.x.  
It provides a clean architecture for Wi‑Fi provisioning, embedded web UI delivery, modular API handlers, and runtime configuration.

This project is currently under active development and the internal structure may change as the framework evolves.

## Why this exists

ESP‑IDF is powerful but low‑level. Most real applications end up rewriting the same
patterns: provisioning, embedded UI, API routing, credential storage, and runtime
configuration. This framework provides a clean, modern foundation so you can focus
on your application logic instead of boilerplate.

## Features

- Wi‑Fi provisioning (AP → STA)
- Embedded web UI served from flash
- Modular HTTP API handlers
- Credential storage (NVS)
- Clear separation of provisioning and runtime modes
- Modern C++ (17/20) design

## Getting Started

Prerequisites:
- ESP‑IDF 5.x installed
- Python 3.8+
- VS Code with ESP‑IDF extension (recommended)

Build and flash:

```bash
idf.py build
idf.py flash monitor