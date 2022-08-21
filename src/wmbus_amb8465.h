// Copyright (C) 2019-2022 Fredrik Öhrström (CC0-1.0)
// Defines documented in the Manual for the AMBER wM-Bus Modules Version 2.7

#define AMBER_SERIAL_SOF 0xFF

#define CMD_DATA_REQ             0x00
#define CMD_DATARETRY_REQ        0x02
#define CMD_DATA_IND             0x03
#define CMD_SET_MODE_REQ         0x04
#define CMD_RESET_REQ            0x05
#define CMD_SET_CHANNEL_REQ      0x06
#define CMD_SET_REQ              0x09
#define CMD_GET_REQ              0x0A
#define CMD_SERIALNO_REQ         0x0B
#define CMD_FWV_REQ              0x0C
#define CMD_RSSI_REQ             0x0D
#define CMD_SETUARTSPEED_REQ     0x10
#define CMD_FACTORYRESET_REQ     0x11
#define CMD_DATA_PRELOAD_REQ     0x30
#define CMD_DATA_CLR_PRELOAD_REQ 0x31
#define CMD_SET_AES_KEY_REQ      0x50
#define CMD_CLR_AES_KEY_REQ      0x51
#define CMD_GET_AES_DEV_REQ      0x52

// Both 8465 868MHz and 8336 169Mhz link modes.
#define LIST_OF_AMBER_8465_8336_LINK_MODES \
    X(USER_SELECTED,0x00,txrx,All parameters user selected.)        \
        X(S1,0x01,tx,Meter transmit once per day long preamble.)   \
        X(S1m,0x02,tx,Meter transmit once per day short preamble.) \
        X(S2,0x03,txrx,Collector receive mode for S1 and S1-m. Might need tuning of preamble.) \
        X(Reserved,0x04,reserved,)                                      \
        X(T1Meter,0x05,tx,Meter transmits often.) \
        X(T1Other,0x06,tx,Collector transmits to meter.) \
        X(T2Meter,0x07,txrx,Meter bi-directional setting.) \
        X(T2Other,0x08,txrx,Collector bi-directional setting.) \
        X(C2T2Other,0x09,txrx,Collector receive C/T send using latest received type.) \
        X(R2Meter,0x0a,txrx,Meter bi-directional.) \
        X(R2Other,0x0b,txrx,Collector bi-directional.) \
        X(C1Meter,0x0c,tx,Meter transmits often more energy efficient that T.) \
        X(C2Meter,0x0d,txrx,Meter bi-directional setting.) \
        X(C2Other,0x0e,txrx,Collector bi-directional setting.) \
        X(PinSelect,0x0f,pinselect,)                           \
        X(N1a,0x01,txrx,)          \
        X(N2a,0x02,txrx,)          \
        X(N1b,0x03,txrx,)          \
        X(N2b,0x04,txrx,)          \
        X(N1c,0x05,txrx,)          \
        X(N2c,0x06,txrx,)          \
        X(N1d,0x07,txrx,)          \
        X(N2d,0x08,txrx,)          \
        X(N1e,0x09,txrx,)          \
        X(N2e,0x0a,txrx,)          \
        X(N1f,0x0b,txrx,)          \
        X(N2f,0x0c,txrx,)          \
        X(UNKNOWN,0xff,unknown,)

enum class LinkModeAMB {
#define X(name,num,sendrcv,info) name = num,
LIST_OF_AMBER_8465_8336_LINK_MODES
#undef X
};

string toString(LinkModeAMB lm);
