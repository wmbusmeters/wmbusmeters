# Decode security (modes 5, 7 and 10)

Fail-closed behaviour with once-per-telegram warnings, decision 0002.

```mermaid
flowchart TD
    T["Encrypted telegram"] --> K{"Key configured?"}
    K -- "no" --> NK["No decode<br/>warn once"]
    K -- "yes" --> SM{"TPL security mode"}
    SM -- "5 / 7 (AES CBC + MAC)" --> CBC["Decrypt blocks<br/>verify MAC"]
    SM -- "10 (OMS profile D)" --> CCM["Decrypt blocks<br/>verify aes-ccm tag"]
    CBC -- "mac ok" --> OK["Payload accepted"]
    CCM -- "tag ok" --> OK
    CBC -- "mac failed" --> FD["FAILED_DECODE<br/>telegram ignored<br/>warn once"]
    CCM -- "tag failed" --> FD
```

Real telegrams for both paths live in `drivers/src/kamwater.xmq` and in
`tests/test_qwds_walkby_nokey.sh`.