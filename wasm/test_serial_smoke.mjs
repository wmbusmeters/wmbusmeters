#!/usr/bin/env node
// ============================================================================
// test_serial_smoke.mjs
//
// Smoke-Test: serial_web WASM bindings inkl. receive
// Testet den kompletten Datenpfad: open → send → onData → receive → close
//
// Nutzung:  cd wmbusmeters/wasm && node test_serial_smoke.mjs
// ============================================================================

const mock = { deviceCount: 2, opens: [], writes: [] };

globalThis.getDeviceCount = () => mock.deviceCount;
globalThis.openDeviceFull = (index, baud, databits, stopbits, parity) => {
    mock.opens.push({ index, baud, databits, stopbits, parity });
};
globalThis.writeSerial = (index, ptr, len) => {
    mock.writes.push({ index, len });
};

let passed = 0, failed = 0;

function test(name, fn) {
    try { fn(); console.log(`  ✅ ${name}`); passed++; }
    catch (e) { console.log(`  ❌ ${name}\n     ${e.message}`); failed++; }
}

function eq(a, b, label = '') {
    if (a !== b) throw new Error(`${label} expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`);
}

function call(fn, ...args) {
    const raw = fn(...args);
    free();
    return JSON.parse(raw);
}

let free;

async function main() {
    console.log('==> Lade WASM...');
    const WMBusMetersFactory = (await import('./dist/wmbusmeters.js')).default;
    const M = await WMBusMetersFactory();

    const version       = M.cwrap('wm_version',        'string', []);
    free                = M.cwrap('wm_free_result',     null,     []);
    const listDrivers   = M.cwrap('wm_list_drivers',    'string', []);
    const serialList    = M.cwrap('wm_serial_list',     'string', []);
    const serialOpen    = M.cwrap('wm_serial_open',     'string', ['number','number','number','number','number']);
    const serialClose   = M.cwrap('wm_serial_close',    null,     ['number']);
    const serialSend    = M.cwrap('wm_serial_send',     'string', ['number','string']);
    const serialReceive = M.cwrap('wm_serial_receive',  'string', ['number']);
    const onData        = M.cwrap('serial_on_data',     null,     ['number','number','number']);

    console.log('\n── Version ──');
    test('wm_version liefert String', () => {
        const v = version();
        eq(typeof v, 'string');
        eq(v.length > 0, true, 'nicht leer');
    });

    console.log('\n── Treiber ──');
    test('wm_list_drivers liefert Array', () => {
        const r = call(listDrivers);
        eq(r.ok, true);
        eq(Array.isArray(r.drivers), true);
        eq(r.drivers.length > 0, true, 'mind. 1');
    });

    console.log('\n── Serial List ──');
    test('2 Mock-Geräte sichtbar', () => {
        const r = call(serialList);
        eq(r.ok, true);
        eq(r.devices.length, 2);
    });

    console.log('\n── Serial Open ──');
    test('open device 0', () => {
        mock.opens = [];
        const r = call(serialOpen, 0, 9600, 8, 1, 0);
        eq(r.ok, true);
        eq(r.device, 'webserial:0');
        eq(mock.opens.length, 1);
        eq(mock.opens[0].baud, 9600);
    });

    console.log('\n── Serial Send ──');
    test('send DEADBEEF = 4 Bytes', () => {
        mock.writes = [];
        const r = call(serialSend, 0, 'DEADBEEF');
        eq(r.ok, true);
        eq(mock.writes[0].len, 4);
    });

    test('send auf unbekanntes Gerät', () => {
        eq(call(serialSend, 99, 'AA').ok, false);
    });

    console.log('\n── RX: onData → receive (Kompletter Pfad) ──');
    test('receive liefert leeren Buffer vor onData', () => {
        const r = call(serialReceive, 0);
        eq(r.ok, true);
        eq(r.data, '');
        eq(r.len, 0);
    });

    test('onData + receive: 4 Bytes Roundtrip', () => {
        const testData = new Uint8Array([0xCA, 0xFE, 0xBA, 0xBE]);
        const ptr = M._malloc(testData.length);
        M.HEAPU8.set(testData, ptr);
        onData(0, ptr, testData.length);
        M._free(ptr);

        const r = call(serialReceive, 0);
        eq(r.ok, true);
        eq(r.data, 'cafebabe');
        eq(r.len, 4);
    });

    test('receive nach drain ist wieder leer', () => {
        const r = call(serialReceive, 0);
        eq(r.ok, true);
        eq(r.len, 0);
    });

    test('onData mehrfach → receive akkumuliert', () => {
        const d1 = new Uint8Array([0x01, 0x02]);
        const d2 = new Uint8Array([0x03, 0x04, 0x05]);
        let ptr = M._malloc(d1.length);
        M.HEAPU8.set(d1, ptr);
        onData(0, ptr, d1.length);
        M._free(ptr);

        ptr = M._malloc(d2.length);
        M.HEAPU8.set(d2, ptr);
        onData(0, ptr, d2.length);
        M._free(ptr);

        const r = call(serialReceive, 0);
        eq(r.ok, true);
        eq(r.data, '0102030405');
        eq(r.len, 5);
    });

    test('receive auf unbekanntem Gerät → Fehler', () => {
        eq(call(serialReceive, 99).ok, false);
    });

    console.log('\n── Serial Close ──');
    test('close + send danach fehlschlägt', () => {
        serialClose(0);
        eq(call(serialSend, 0, 'AA').ok, false);
    });

    test('close + receive danach fehlschlägt', () => {
        eq(call(serialReceive, 0).ok, false);
    });

    console.log(`\n${'═'.repeat(50)}`);
    console.log(`  ${passed} bestanden, ${failed} fehlgeschlagen`);
    console.log('═'.repeat(50));
    process.exit(failed > 0 ? 1 : 0);
}

main().catch(e => { console.error('Fatal:', e); process.exit(1); });
