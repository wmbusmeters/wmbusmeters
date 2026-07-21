// Copyright (C) 2019-2022 Fredrik Öhrström (CC0-1.0)
// Defines documented in the Manual for the AMBER wM-Bus Modules Version 2.7

#define CMD_STX                     0xFF

#define CMD_TYPE_REQ                (0 << 6)
#define CMD_TYPE_CNF                (2 << 6)

#define CMD_DATA                    0x00
#define CMD_DATA_REQ                (CMD_DATA | CMD_TYPE_REQ)
#define CMD_DATA_CNF                (CMD_DATA | CMD_TYPE_CNF)

#define CMD_DATARETRY               0x02
#define CMD_DATARETRY_REQ           (CMD_DATARETRY | CMD_TYPE_REQ)
#define CMD_DATARETRY_CNF           (CMD_DATARETRY | CMD_TYPE_CNF)

#define CMD_DATA_IND                0x03
#define CMD_DATA_IND_REQ            (CMD_DATA_IND | CMD_TYPE_REQ)
#define CMD_DATA_IND_CNF            (CMD_DATA_IND | CMD_TYPE_CNF)

#define CMD_SET_MODE                0x04
#define CMD_SET_MODE_REQ            (CMD_SET_MODE | CMD_TYPE_REQ)
#define CMD_SET_MODE_CNF            (CMD_SET_MODE | CMD_TYPE_CNF)

#define CMD_RESET                   0x05
#define CMD_RESET_REQ               (CMD_RESET | CMD_TYPE_REQ)
#define CMD_RESET_CNF               (CMD_RESET | CMD_TYPE_CNF)

#define CMD_SET_CHANNEL             0x06
#define CMD_SET_CHANNEL_REQ         (CMD_SET_CHANNEL | CMD_TYPE_REQ)
#define CMD_SET_CHANNEL_CNF         (CMD_SET_CHANNEL | CMD_TYPE_CNF)

#define CMD_SET                     0x09
#define CMD_SET_REQ                 (CMD_SET | CMD_TYPE_REQ)
#define CMD_SET_CNF                 (CMD_SET | CMD_TYPE_CNF)

#define CMD_SETUARTSPEED            0x10
#define CMD_SETUARTSPEED_REQ        (CMD_SETUARTSPEED | CMD_TYPE_REQ)
#define CMD_SETUARTSPEED_CNF        (CMD_SETUARTSPEED | CMD_TYPE_CNF)

#define CMD_GET                     0x0A
#define CMD_GET_REQ                 (CMD_GET | CMD_TYPE_REQ)
#define CMD_GET_CNF                 (CMD_GET | CMD_TYPE_CNF)

#define CMD_GET_SERIALNO            0x0B
#define CMD_GET_SERIALNO_REQ        (CMD_GET_SERIALNO | CMD_TYPE_REQ)
#define CMD_GET_SERIALNO_CNF        (CMD_GET_SERIALNO | CMD_TYPE_CNF)

#define CMD_GET_FWRELEASE           0x0C
#define CMD_GET_FWRELEASE_REQ       (CMD_GET_FWRELEASE | CMD_TYPE_REQ)
#define CMD_GET_FWRELEASE_CNF       (CMD_GET_FWRELEASE | CMD_TYPE_CNF)

#define CMD_RSSI                    0x0D
#define CMD_RSSI_REQ                (CMD_RSSI | CMD_TYPE_REQ)
#define CMD_RSSI_CNF                (CMD_RSSI | CMD_TYPE_CNF)

#define CMD_FACTORYRESET            0x11
#define CMD_FACTORYRESET_REQ        (CMD_FACTORYRESET | CMD_TYPE_REQ)
#define CMD_FACTORYRESET_CNF        (CMD_FACTORYRESET | CMD_TYPE_CNF)

#define CMD_DATA_PRELOAD            0x30
#define CMD_DATA_PRELOAD_REQ        (CMD_DATA_PRELOAD | CMD_TYPE_REQ)
#define CMD_DATA_PRELOAD_CNF        (CMD_DATA_PRELOAD | CMD_TYPE_CNF)

#define CMD_DATA_CLR_PRELOAD        0x31
#define CMD_DATA_CLR_PRELOAD_REQ    (CMD_DATA_CLR_PRELOAD | CMD_TYPE_REQ)
#define CMD_DATA_CLR_PRELOAD_CNF    (CMD_DATA_CLR_PRELOAD | CMD_TYPE_CNF)

#define CMD_SET_AES_KEY             0x50
#define CMD_SET_AES_KEY_REQ         (CMD_SET_AES_KEY | CMD_TYPE_REQ)
#define CMD_SET_AES_KEY_CNF         (CMD_SET_AES_KEY | CMD_TYPE_CNF)

#define CMD_CLR_AES_KEY             0x51
#define CMD_CLR_AES_KEY_REQ         (CMD_CLR_AES_KEY | CMD_TYPE_REQ)
#define CMD_CLR_AES_KEY_CNF         (CMD_CLR_AES_KEY | CMD_TYPE_CNF)

#define CMD_GET_AES_DEV             0x52
#define CMD_GET_AES_DEV_REQ         (CMD_GET_AES_DEV | CMD_TYPE_REQ)
#define CMD_GET_AES_DEV_CNF         (CMD_GET_AES_DEV | CMD_TYPE_CNF)

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
        X(R2Meter,0x0A,txrx,Meter bi-directional.) \
        X(R2Other,0x0B,txrx,Collector bi-directional.) \
        X(C1Meter,0x0C,tx,Meter transmits often more energy efficient that T.) \
        X(C2Meter,0x0D,txrx,Meter bi-directional setting.) \
        X(C2Other,0x0E,txrx,Collector bi-directional setting.) \
        X(PinSelect,0x0F,pinselect,)                           \
        X(N1a,0x01,txrx,)          \
        X(N2a,0x02,txrx,)          \
        X(N1b,0x03,txrx,)          \
        X(N2b,0x04,txrx,)          \
        X(N1c,0x05,txrx,)          \
        X(N2c,0x06,txrx,)          \
        X(N1d,0x07,txrx,)          \
        X(N2d,0x08,txrx,)          \
        X(N1e,0x09,txrx,)          \
        X(N2e,0x0A,txrx,)          \
        X(N1f,0x0B,txrx,)          \
        X(N2f,0x0C,txrx,)          \
        X(UNKNOWN,0xFF,unknown,)

enum class LinkModeAMB {
#define X(name,num,sendrcv,info) name = num,
LIST_OF_AMBER_8465_8336_LINK_MODES
#undef X
};

std::string toString(LinkModeAMB lm);
