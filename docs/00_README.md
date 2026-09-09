# wmbusmeters Custom Driver Documentation — README

> **Author**: Metronix Engineering Team  
> **Last Updated**: September 2026

---

## What Is This?

This folder contains documentation for creating, testing, and deploying custom wmbusmeters XMQ meter drivers. There are two workflow documents, each designed for a different audience.

---

## File 1: `01_AI_AGENT_CONTEXT.md`

### Who is it for?
**AI coding agents** (e.g., Gemini, Claude, ChatGPT, Copilot, or any LLM-based coding assistant).

### What does it do?
This file provides **structured context** that an AI agent can ingest to autonomously:

1. **Understand** the wmbusmeters XMQ driver format and all its configuration options
2. **Analyze** a raw MBUS telegram using the `--analyze` command
3. **Generate** a new `.xmq` driver file with the correct field mappings
4. **Build** the project (regenerate the driver database + recompile the binary)
5. **Test** the driver against the provided MBUS frame and verify correctness
6. **Iterate** on the driver until the output matches expected values

### How to use it:
1. Open a conversation with your AI coding agent
2. Provide this file as context (paste it, attach it, or point the agent to the file path)
3. Give the agent:
   - The **MBUS frame format document** (PDF/text describing the telegram structure)
   - The **raw MBUS frame** (hex string)
   - The **decryption key** (if encrypted, otherwise say "unencrypted" or "NOKEY")
4. The agent will follow the workflow to create the driver, build, and test it

### What the AI agent CANNOT do:
- **Deployment** — the agent cannot SSH into the blackbox server or run docker commands there. After the driver is built and tested locally, a human must handle deployment (see the manual guide).

---

## File 2: `02_MANUAL_DRIVER_GUIDE.md`

### Who is it for?
**Human developers and engineers** who need to manually create drivers or understand the process.

### What does it do?
This is a **step-by-step manual** that walks a human through the entire process:

1. **Gathering inputs** — what you need before you start (MBUS frame, format document, key)
2. **Analyzing the telegram** — running `--analyze` to understand the raw data
3. **Writing the XMQ driver** — creating the driver file with field mappings, including reference tables for all valid meter types, VIF ranges, and quantities
4. **Building** — regenerating the driver database and recompiling wmbusmeters
5. **Testing** — verifying the driver output matches expected values
6. **Deploying** — committing to git, pushing to GitHub, SSHing into the blackbox server, and rebuilding the Docker container

### How to use it:
Simply read the document from top to bottom and follow each step. It's designed to be self-contained — no prior wmbusmeters knowledge required.

### Key sections:
- **Quick Reference Card** — a one-glance summary of all important commands
- **Troubleshooting** — common issues and how to fix them (including expired GitHub tokens for Docker builds)
- **Full XMQ Example** — a complete, minimal driver you can use as a starting template

---

## The Driver Development Workflow (Summary)

```
   ┌─────────────┐     ┌──────────────┐     ┌───────────────┐
   │  MBUS Frame  │     │ Frame Format │     │  Decryption   │
   │  (hex data)  │     │  Document    │     │  Key (opt.)   │
   └──────┬───────┘     └──────┬───────┘     └───────┬───────┘
          │                    │                     │
          └────────────┬───────┘─────────────────────┘
                       │
                       ▼
              ┌─────────────────┐
              │  Analyze Frame  │  ← build/wmbusmeters --analyze
              └────────┬────────┘
                       │
                       ▼
              ┌─────────────────┐
              │  Create Driver  │  ← drivers/src/<name>.xmq
              │  (.xmq file)   │
              └────────┬────────┘
                       │
                       ▼
              ┌─────────────────┐
              │  Build Project  │  ← cd drivers && make install
              │                 │    make -j$(nproc)
              └────────┬────────┘
                       │
                       ▼
              ┌─────────────────┐
              │  Test Driver    │  ← build/wmbusmeters --format=json
              │                 │    --ppjson <frame> ...
              └────────┬────────┘
                       │
                 ┌─────┴─────┐
                 │  Pass?    │
                 └─────┬─────┘
              No ┌─────┘─────┐ Yes
                 ▼           ▼
          ┌───────────┐ ┌──────────────────┐
          │ Fix & Re- │ │  Deploy          │
          │ build     │ │  1. git commit   │
          └───────────┘ │  2. git push     │
                        │  3. ssh server   │
                        │  4. docker build │
                        │  5. docker up    │
                        └──────────────────┘
```

---

## Where Do the Driver Files Live?

| Path | Description |
|------|-------------|
| `drivers/src/*.xmq` | **Source XMQ driver files** — this is where you create new drivers |
| `src/generated_database.cc` | Auto-generated C++ file containing all compiled drivers (DO NOT edit manually) |
| `tests/generated_tests.xmq` | Auto-generated test cases from all driver XMQ files |
| `build/wmbusmeters` | The compiled binary |
| `drivers/generate.sh` | Script that generates `generated_database.cc` from XMQ sources |
| `drivers/test.sh` | Script that runs tests for a specific driver |
| `drivers/test_case.sh` | Script that executes individual test cases |

---

## Need Help?

- **XMQ format questions**: Look at existing drivers in `drivers/src/` for examples
- **MBUS protocol questions**: Refer to EN 13757 standard or the OMS specification
- **Build issues**: Make sure `xmq` CLI tool is installed and in your PATH
- **Deployment issues**: Check `docker/.env` for expired GitHub tokens
