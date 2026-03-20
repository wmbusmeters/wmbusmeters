#!/usr/bin/env node
// ============================================================================
// test_serial_smoke.mjs
//
// Smoke-Test für die serial_web WASM-Bindings.
// Mockt die JS-Funktionen die serial_web.cc per EM_ASM aufruft.
//
// Nutzung:
//   cd wmbusmeters/wasm
//   node test_serial_smoke.mjs
//
// Voraussetzung: WASM gebaut (dist/wmbusmeters.js + .wasm vorhanden)
// ============================================================================

import { createRequire } from 'module';
const require = createRequire(import.meta.url);

// ── Mock-State ──────────────────────────────────────────────────────────────
const mock = {
    deviceCount: 2,
    opens: [],
    writes: [],
};

// ── Mocks registrieren BEVOR das WASM-Modul lädt ────────────────────────────
// serial_web.cc ruft diese per EM_ASM({...}) auf

globalThis.getDeviceCount = () => mock.deviceCount;

globalThis.openDeviceFull = (index, baud, databits, stopbits, parity) => {
    mock.opens.push({ index, baud, databits, stopbits, parity });
};

globalThis.writeSerial = (index, ptr, len) => {
    mock.writes.push({ index, len });
};

// ── Test-Harness ────────────────────────────────────────────────────────────
let passed = 0;
let failed = 0;

function test(name, fn) {
    try {
        fn();
        console.log(`  ✅ ${name}`);
        passed++;
    } catch (e) {
        console.log(`  ❌ ${name}`);
        console.log(`     ${e.message}`);
        failed++;
    }
}

function eq(actual, expected, label = '') {
    if (actual !== expected) {
        throw new Error(`${label} expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    }
}

// ── Main ────────────────────────────────────────────────────────────────────
async function main() {
    console.log('==> Lade WASM-Modul...');

    const WMBusMetersFactory = (await import('./dist/wmbusmeters.js')).default;
    const M = await WMBusMetersFactory();

    // Funktionen wrappen
    const version      = M.cwrap('wm_version',       'string', []);
    const freeResult   = M.cwrap('wm_free_result',    null,     []);
    const listDrivers  = M.cwrap('wm_list_drivers',   'string', []);
    const serialList   = M.cwrap('wm_serial_list',    'string', []);
    const serialOpen   = M.cwrap('wm_serial_open',    'string', ['number','number','number','number','number']);
    const serialClose  = M.cwrap('wm_serial_close',   null,     ['number']);
    const serialSend   = M.cwrap('wm_serial_send',    'string', ['number','string']);
    const onData       = M.cwrap('serial_on_data',    null,     ['number','number','number']);

    // Helper: JSON-Antwort parsen + Result freigeben
    function call(fn, ...args) {
        const raw = fn(...args);
        freeResult();
        return JSON.parse(raw);
    }

    console.log('\n── Version ──');
    test('wm_version liefert String', () => {
        const v = version();
        eq(typeof v, 'string');
        eq(v.length > 0, true, 'nicht leer');
    });

    console.log('\n── Treiber ──');
    test('wm_list_drivers liefert Array', () => {
        const res = call(listDrivers);
        eq(res.ok, true);
        eq(Array.isArray(res.drivers), true);
        eq(res.drivers.length > 0, true, 'mindestens 1 Treiber');
    });

    console.log('\n── Serial List ──');
    test('wm_serial_list zeigt 2 Mock-Geräte', () => {
        const res = call(serialList);
        eq(res.ok, true);
        eq(res.devices.length, 2);
        eq(res.devices[0], 'webserial:0');
        eq(res.devices[1], 'webserial:1');
    });

    test('wm_serial_list zeigt 0 Geräte wenn keins da', () => {
        mock.deviceCount = 0;
        const res = call(serialList);
        eq(res.ok, true);
        eq(res.devices.length, 0);
        mock.deviceCount = 2; // zurücksetzen
    });

    console.log('\n── Serial Open ──');
    test('open mit Parametern ruft openDeviceFull auf', () => {
        mock.opens = [];
        const res = call(serialOpen, 0, 9600, 8, 1, 0);
        eq(res.ok, true);
        eq(res.device, 'webserial:0');
        eq(mock.opens.length, 1);
        eq(mock.opens[0].baud, 9600);
        eq(mock.opens[0].databits, 8);
    });

    test('open zweites Gerät', () => {
        const res = call(serialOpen, 1, 115200, 8, 1, 0);
        eq(res.ok, true);
        eq(res.device, 'webserial:1');
    });

    console.log('\n── Serial Send ──');
    test('send DEADBEEF = 4 Bytes', () => {
        mock.writes = [];
        const res = call(serialSend, 0, 'DEADBEEF');
        eq(res.ok, true);
        eq(mock.writes.length, 1);
        eq(mock.writes[0].index, 0);
        eq(mock.writes[0].len, 4);
    });

    test('send auf unbekanntes Gerät schlägt fehl', () => {
        const res = call(serialSend, 99, 'AA');
        eq(res.ok, false);
    });

    test('send mit ungültigem Hex schlägt fehl', () => {
        const res = call(serialSend, 0, 'ZZZZ');
        eq(res.ok, false);
    });

    console.log('\n── serial_on_data (RX) ──');
    test('serial_on_data crasht nicht', () => {
        const data = new Uint8Array([0x01, 0x02, 0x03]);
        const ptr = M._malloc(data.length);
        M.HEAPU8.set(data, ptr);
        onData(0, ptr, data.length);
        M._free(ptr);
        // kein Crash = Erfolg
    });

    test('serial_on_data auf unbekanntem Index crasht nicht', () => {
        const data = new Uint8Array([0xFF]);
        const ptr = M._malloc(1);
        M.HEAPU8.set(data, ptr);
        onData(999, ptr, 1);
        M._free(ptr);
    });

    console.log('\n── Serial Close ──');
    test('close + send danach schlägt fehl', () => {
        serialClose(0);
        const res = call(serialSend, 0, 'AA');
        eq(res.ok, false);
    });

    test('close auf nie geöffnetem Gerät crasht nicht', () => {
        serialClose(42);
        // kein Crash = Erfolg
    });

    // ── Ergebnis ─────────────────────────────────────────────────────────────
    console.log(`\n${'═'.repeat(50)}`);
    console.log(`  ${passed} bestanden, ${failed} fehlgeschlagen`);
    console.log('═'.repeat(50));

    process.exit(failed > 0 ? 1 : 0);
}

main().catch(e => {
    console.error('Fatal:', e);
    process.exit(1);
});