# Driver pipeline

Build-time generation and runtime load of meter drivers, decision 0003.

```mermaid
flowchart LR
    X["drivers/src/*.xmq<br/>library.xmq"] --> G["drivers/generate.sh<br/>generate_index.sh"]
    G --> DB["src/generated_database.cc<br/>tests/generated_tests.xmq<br/>(generated, do not edit)"]
    DB --> B["wmbusmeters binary<br/>builtins_mvts_"]
    T["Incoming telegram"] --> M{"mvt match<br/>mfct, version, type"}
    M -- "hit" --> L["DriverDynamic::load<br/>parse xmq, match fields"]
    M -- "miss" --> A["--analyze<br/>pickMeterDriver"]
    D["--analyze=file.xmq"] --> L
    L --> O["fields -> printer"]
```

`library.xmq` provides shared field definitions installed into the C++
code at build time (work in progress).