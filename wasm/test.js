'use strict';

const { describe, test } = require('node:test')
const WMBusMeters = require('./dist/wmbusmeters.js');

describe('WMBusMeters', () => {
    test('should return version', async () => {
        const Module = await WMBusMeters();
        const version = Module.cwrap('wm_version', 'string', ['string']);
        console.log(version());
    });

    test('should return list of drivers', async () => {
        const Module = await WMBusMeters();
        const listDrivers = Module.cwrap('wm_list_drivers', 'string', ['string']);
        console.log(listDrivers());
    });

    test('should decode telegram', async () => {
        const Module = await WMBusMeters();
        const hex = '34446850226677116980A0119F27020480048300C408F709143C003D341A2B0B2A0707000000000000062D114457563D71A1850000';
        const decodeTelegram = Module.cwrap('wm_decode', 'string', ['string', ['string', 'string']]);
        console.log(decodeTelegram(hex, 'NO_KEY', 'auto'));
    });

    test('aes decryption', async () => {
        const Module = await WMBusMeters();
        const hex = '6e446d141149545605077a42006005a5cddd3f1299b414047daf03717ee2af14acfcab0b90b1b578c10e88af7f3f933d51eb78c1f9b9c17ee0d1bc61a67edcd8c36a283781c271846079741ac0bf2e47f439688024a6a3fe13c0df98cea0b05dbcfd4ca4f84ca0a06b51ce12f7d8f3';
        const key = '9F5213BC13841410BB1410141515E4D5';
        const decodeTelegram = Module.cwrap('wm_decode', 'string', ['string', ['string', 'string']]);
        console.log(decodeTelegram(hex, key, 'auto'));
    });
});