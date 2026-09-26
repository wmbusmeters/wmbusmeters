# Telegram data flow

End to end path of one telegram. Receiver abstraction and SDR child
processes are decision 0004.

```mermaid
flowchart LR
    R["Receivers<br/>im871a / amb8465 / cul / rc1180<br/>rtlwmbus / rtl433 (child processes)<br/>simulation files"] --> L["Link layer<br/>format A/B CRC strip<br/>3-out-of-6 check"]
    L --> S["Security<br/>modes 5 / 7 / 10<br/>AES CBC / AES-CCM tag"]
    S --> P["dvparser<br/>DIF/VIF records"]
    P --> M["drivers.cc<br/>mvt lookup in builtins_mvts_"]
    M --> F["driver_dynamic.cc<br/>field matching"]
    F --> O["printer.cc<br/>json / fields / mqtt / http / files"]
```

The sequence of the security step is shown in
[decode-security.md](decode-security.md).