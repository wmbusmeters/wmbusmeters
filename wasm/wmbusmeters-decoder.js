// wmbusmeters-decoder.js
// Komfortabler JS-Wrapper um das generierte WASM-Modul

import WMBusMetersFactory from './wmbusmeters.js';

let _module = null;
let _decode = null;
let _free   = null;

/**
 * Modul einmalig initialisieren.
 * Muss vor decode() aufgerufen werden.
 */
export async function init() {
    if (_module) return;
    _module = await WMBusMetersFactory();
    _decode = _module.cwrap('wm_decode', 'string', ['string', 'string', 'string']);
    _free   = _module.cwrap('wm_free_result', null, []);
}

/**
 * Decode a raw wMbus telegram.
 *
 * @param {string} hexTelegram  - Raw telegram as hex string
 *                                z.B. "2C446850281604061B7A870020..."
 * @param {string} [key=""]    - AES-128 key als 32-stelliger Hex-String
 *                                z.B. "00112233445566778899AABBCCDDEEFF"
 * @param {string} [driver="auto"] - Treiber-Hint, z.B. "multical21", "izar"
 *
 * @returns {object} Parsed result:
 * {
 *   ok: true,
 *   manufacturer: "KAM",
 *   id: "06041628",
 *   version: 27,
 *   type: "Cold water meter",
 *   values: {
 *     total_m3: 1.234,
 *     ...
 *   }
 * }
 */
export function decode(hexTelegram, key = "", driver = "auto") {
    if (!_module) throw new Error("Call init() first");

    const jsonStr = _decode(hexTelegram, key, driver);
    _free();

    try {
        return JSON.parse(jsonStr);
    } catch (e) {
        return { ok: false, error: "JSON parse failed: " + jsonStr };
    }
}

/**
 * Version string des WASM-Moduls.
 */
export function version() {
    if (!_module) throw new Error("Call init() first");
    return _module.ccall('wm_version', 'string', [], []);
}

// ── Beispiel (Node.js) ────────────────────────────────────────────────────────
// import { init, decode } from './wmbusmeters-decoder.js';
//
// await init();
//
// const result = decode(
//   "2C446850281604061B7A870020050000B8157A0B00200910A0C7F36D3007254C13",
//   "00112233445566778899AABBCCDDEEFF"
// );
//
// console.log(result);
// {
//   ok: true,
//   manufacturer: "KAM",
//   id: "06041628",
//   version: 27,
//   type: "Cold water meter",
//   values: { total_m3: 1.234, target_m3: 1.100 }
// }
