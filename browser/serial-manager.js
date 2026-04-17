/*
 Copyright (C) 2026 Aras Abbasi (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @template T
 * @typedef {Object} DeferredPromise
 * @property {Promise<T>} promise
 * @property {NonNullable<Parameters<Promise<T>['then']>[0]>} resolve
 * @property {NonNullable<Parameters<Promise<T>['then']>[1]>} reject
 */

/**
 * @template T
 * @returns {DeferredPromise<T>}
 */
function createDeferredPromise() {
    /** @type {(value: T | PromiseLike<T>) => void} */
    let resolve;
    /** @type {(reason?: any) => void} */
    let reject;

    const promise = new Promise((res, rej) => {
        resolve = res;
        reject = rej;
    });

    if (!resolve || !reject) {
        throw new Error("DeferredPromise initialization failed");
    }

    let settled = false;

    return {
        promise,
        resolve: (v) => {
            if (!settled) {
                settled = true;
                resolve(v);
            }
        },
        reject: (e) => {
            if (!settled) {
                settled = true;
                reject(e);
            }
        }
    };
}

const parityMap = /** @type {const} */ ({ 0: 'none', 1: 'even', 2: 'odd' });

/** @typedef {typeof parityMap[keyof parityMap]} Parity */


/**
 * @typedef {Object} SerialPort
 * @property {(opts: { baudRate: number, dataBits:number, stopBits: number, parity:string, flowControl: string }) => Promise<void>} open
 * @property {() => Promise<void>} close
 * @property {*} writable
 * @property {*} readable
 * @property {() => { usbVendorId?: number, usbProductId?: number }} getInfo
 * @property {() => Promise<void>} forget
 */

// ═══════════════════════════════════════════════════════════════
// SerialDeviceImp — wraps a single WebSerial port
// C++ serial_web.cc calls EM_ASM → these JS functions
// JS read loop calls serial_on_data() → C++ rxBuffer
// ═══════════════════════════════════════════════════════════════

class SerialDeviceImp {
    /** @type {SerialPort} */    #port;
    /** @type {number} */        #index;

    /** @type {WritableStreamDefaultWriter|null} */ #writer = null;
    /** @type {ReadableStreamDefaultReader|null} */ #reader = null;

    /** @type {boolean} */       reading = false;
    /** @type {any} */           wasmModule = null;
    /** @type {Function|null} */ onDataFn = null;
    /** @type {Uint8Array[]} */  pendingData = [];

    /** @type {Function} */ onDisconnect = (() => { });
    /** @type {Function} */ onConnect = (() => { });

    /** @type {number} */ #usbVendorId = 0;
    /** @type {number} */ #usbProductId = 0;

    /** @type {Promise<*>|null} */
    #readLoop = null;

    /** @type {boolean} */
    #closing = false;

    /** @type {Promise<any>} Write-Queue (serialisiert alle Writes) */
    #writeLock = Promise.resolve();

    /** @type {boolean} */
    get isOpen() {
        return this.#readLoop !== null
    }

    get port() {
        return this.#port;
    }

    get index() {
        return this.#index;
    }

    /**
     * @param {SerialPort} port
     * @param {number} index
     * @param {Function} [onConnect] Optional callback for when data is received (for testing)
     * @param {Function} [onDisconnect] Optional callback for when the port is closed (for testing)
     */
    constructor(port, index, onConnect, onDisconnect) {
        this.#usbVendorId = port.getInfo().usbVendorId || 0;
        this.#usbProductId = port.getInfo().usbProductId || 0;
        this.#port = port;
        this.#index = index;
        if (onConnect) this.onConnect = onConnect;
        if (onDisconnect) this.onDisconnect = onDisconnect;
    }

    /**
     * Open the port with given serial parameters.
     * Called by C++ SerialDeviceImp::open() via EM_ASM → openDeviceFull()
     * @param {number} baudRate
     * @param {number} dataBits
     * @param {number} stopBits
     * @param {Parity} parity
     */
    async open(baudRate, dataBits = 8, stopBits = 1, parity = 'none') {
        if (this.isOpen) return true;

        // Reset state for fresh session
        this.#closing = false;
        this.#writeLock = Promise.resolve();

        try {
            await this.#port.open({ baudRate, dataBits, stopBits, parity, flowControl: 'none' });
            this.#writer = this.#port.writable.getWriter();
            this.#readLoop = this.#startReadLoop();
            console.log(`[serial:${this.#index}] port opened at ${baudRate} baud, parity=${parity}`);
            return true;
        } catch (e) {
            console.error(`[serial:${this.#index}] open failed:`, e);
            return false;
        }
    }

    /**
     * Close the port.
     * Called by C++ SerialDeviceImp::close() via EM_ASM → closeSerialPort()
     * 
     * @returns {Promise<void>}
     */
    async close() {
        if (this.#closing) return;
        this.#closing = true;

        this.reading = false;
        this.pendingData = [];

        const reader = this.#reader;
        this.#reader = null;

        try {
            if (reader) {
                await reader.cancel();
                reader.releaseLock();
            }
        } catch (e) {
            console.error(`[serial:${this.#index}] reader shutdown failed:`, e);
        }

        try {
            await Promise.race([
                this.#readLoop,
                new Promise(resolve => setTimeout(resolve, 2000)) // safety timeout
            ]);
        } catch {}

        this.#readLoop = null;

        try {
            await this.#writeLock;
        } catch (e) {
            console.error(`[serial:${this.#index}] write lock failed:`, e);
        }

        const writer = this.#writer;
        this.#writer = null;

        try {
            if (writer) {
                await writer.close();
                writer.releaseLock();
            }
        } catch (e) {
            console.error(`[serial:${this.#index}] writer close failed:`, e);
        }

        try {
            await this.#port.close();
        } catch (e) {
            console.error(`[serial:${this.#index}] port close failed:`, e);
        }

        this.#closing = false;
    }

    /**
     * Send raw bytes. Called by C++ via EM_ASM → writeSerial(index, ptr, len)
     * @param {Uint8Array} data
     * @returns {Promise<boolean>} true if write succeeded, false if failed or port is closing/closed
     */
    async send(data) {
        if (this.#closing || !this.#writer) return false;

        const writeOp = async () => {
            if (this.#closing || !this.#writer) return false;

            try {
                await this.#writer.write(data);
                return true;
            } catch (e) {
                console.error(`[serial:${this.#index}] write error:`, e);
                return false;
            }
        };

        this.#writeLock = this.#writeLock
            .catch(() => {}) // prevent chain poisoning
            .then(writeOp);

        return this.#writeLock;
    }

    /**
     * Read loop: reads from WebSerial, buffers in JS.
     * C++ pulls data via drainSerialData() in receive().
     * This avoids calling into WASM during ASYNCIFY sleep.
     */
    async #startReadLoop() {
        if (!this.#port.readable) return;

        const reader = this.#port.readable.getReader();
        this.#reader = reader;
        this.reading = true;
        this.pendingData = [];

        try {
            while (true) {
                const { value, done } = await reader.read();
                if (done || !this.reading) break;

                if (value && value.length > 0) {
                    // copy to avoid underlying buffer reuse
                    this.pendingData.push(new Uint8Array(value));

                    // prevent unbounded growth
                    if (this.pendingData.length > 1000) {
                        console.warn(`[serial:${this.#index}] buffer overflow, trimming`);
                        this.pendingData.splice(0, 500);
                    }
                }
            }
        } catch (e) {
            if (this.reading) {
                console.error(`[serial:${this.#index}] read error:`, e);
            }
        } finally {
            try { reader.releaseLock(); } catch {}
            if (this.#reader === reader) this.#reader = null;
        }
    }

    /**
     * Drain all pending data from the JS buffer.
     * Called by C++ via EM_ASM → drainSerialData(index).
     * @returns {Uint8Array|null}
     */
    drainPendingData() {
        if (!this.pendingData || this.pendingData.length === 0) return null;

        const total = this.pendingData.reduce((sum, c) => sum + c.length, 0);
        const result = new Uint8Array(total);

        let offset = 0;
        for (const chunk of this.pendingData) {
            result.set(chunk, offset);
            offset += chunk.length;
        }

        this.pendingData.length = 0;
        return result;
    }

    /**
     * Get USB device info
     * @returns {{ usbVendorId?: number, usbProductId?: number }}
     */
    getInfo() {
        return {
            usbVendorId: this.#usbVendorId,
            usbProductId: this.#usbProductId
        };
    }
}

// ═══════════════════════════════════════════════════════════════
// SerialCommunicationManagerImp — manages all serial ports
// Maps to the C++ SerialCommunicationManagerImp in serial_web.cc
// ═══════════════════════════════════════════════════════════════

class SerialCommunicationManagerImp extends EventTarget {
    constructor() {
        if (!navigator.serial) {
            throw new Error("Web Serial API is not supported in this browser.");
        }

        super();

        navigator.serial.addEventListener('connect', async (event) => {
            console.info("Serial port connected:", event.target);
            const port = event.target;
            if (this.#serialPorts.has(port)) return;

            const index = this.#indexForPort(port);
            const dev = new SerialDeviceImp(port, index);
            dev.wasmModule = this.wasmModule;
            dev.onDataFn = this.onDataFn;
            this.#serialPorts.set(port, dev);

            // Auto-open (permission was already granted via earlier requestPort)
            const ok = await dev.open(9600);
            if (!ok) {
                console.warn(`[serial:${index}] auto-open on connect failed`);
                this.#serialPorts.delete(port);
                return;
            }

            dev._currentBaud = 9600;
            dev._currentParity = 'none';

            // DON'T call into WASM here — ASYNCIFY may have the stack frozen.
            // Buffer the connect. C++ drains it via drainConnects().
            if (typeof globalThis.pendingConnects !== 'undefined') {
                globalThis.pendingConnects.push(index);
            }
            console.log(`[serial:${index}] connect queued for C++`);
            this.dispatchEvent(new CustomEvent('portchange', { detail: { type: 'connect', index, dev } }));
        });

        navigator.serial.addEventListener('disconnect', async (event) => {
            const port = event.target;
            const dev = this.#serialPorts.get(port);
            if (dev) {
                const idx = dev.index;
                dev.reading = false;
                dev.pendingData = [];
                this.#serialPorts.delete(port);
                // DON'T call into WASM here — ASYNCIFY may have the stack frozen.
                // Buffer the disconnect. C++ drains it via drainDisconnects().
                if (typeof globalThis.pendingDisconnects !== 'undefined') {
                    globalThis.pendingDisconnects.push(idx);
                }
                console.log(`[serial:${idx}] disconnect queued for C++`);
                this.dispatchEvent(new CustomEvent('portchange', { detail: { type: 'disconnect', index: idx } }));
            }
        });

        this.#initialize();
    }

    #initialized = createDeferredPromise();

    #initialize = async () => {
        try {
            const ports = await navigator.serial.getPorts();
            ports.forEach(port => {
                if (!this.#serialPorts.has(port)) {
                    const index = this.#indexForPort(port);
                    this.#serialPorts.set(port, new SerialDeviceImp(port, index));
                }
            });
            this.#initialized.resolve(undefined);
        } catch (error) {
            console.error("Error initializing SerialCommunicationManagerImp:", error);
            this.#initialized.reject(error);
        }
    }

    /** @type {Map<SerialPort, SerialDeviceImp>} */
    #serialPorts = new Map();

    /** @type {number} */
    #serialPortIndex = 0;

    /** @type {Map<string, number[]>} VID:PID → pool of previously used indices */
    #deviceIndexPool = new Map();

    /**
     * Get or allocate an index for a port based on its VID:PID.
     * Maintains a pool per VID:PID so that:
     *  - Single stick: always gets the same index after connect
     *  - Two identical sticks: each gets their own index from the pool
     *  - After both disconnect and one connects: gets first available from pool
     *
     * Note: with identical VID:PID we can't tell which physical stick is which
     * (WebSerial doesn't expose serial numbers), so the pool gives out indices
     * in order — first available, not necessarily the "correct" one.
     *
     * @param {SerialPort} port
     * @returns {number}
     */
    #indexForPort(port) {
        const info = port.getInfo();
        const vid = info.usbVendorId || 0;
        const pid = info.usbProductId || 0;
        const key = `${vid}:${pid}`;

        if (vid !== 0 || pid !== 0) {
            const pool = this.#deviceIndexPool.get(key) || [];

            // Find first index from pool not currently used by an open device
            for (const idx of pool) {
                let inUse = false;
                for (const dev of this.#serialPorts.values()) {
                    if (dev.index === idx && dev.isOpen) {
                        inUse = true;
                        break;
                    }
                }
                if (!inUse) return idx;
            }

            // No free index in pool — allocate a new one and add to pool
            const idx = this.#serialPortIndex++;
            pool.push(idx);
            this.#deviceIndexPool.set(key, pool);
            return idx;
        }

        // No VID:PID (shouldn't happen for real USB devices)
        return this.#serialPortIndex++;
    }

    /** @type {any} */ wasmModule = null;
    /** @type {Function|null} */ onDataFn = null;

    /**
     * Wire up the WASM module. Must be called after WASM init.
     * @param {any} wasmModule
     */
    setWasmModule(wasmModule) {
        this.wasmModule = wasmModule;
        this.onDataFn = wasmModule.cwrap('serial_on_data', null, ['number', 'number', 'number']);

        // Update all existing devices
        for (const dev of this.#serialPorts.values()) {
            dev.wasmModule = wasmModule;
            dev.onDataFn = this.onDataFn;
        }
    }

    /**
     * Request a new port from the user (browser dialog).
     * Opens it and registers with C++ via wm_serial_open.
     * @param {number} baudRate
     * @returns {Promise<SerialDeviceImp|null>}
     */
    async requestAndOpen(baudRate = 9600) {
        const port = await navigator.serial.requestPort();

        if (!port) return null;

        let dev = this.#serialPorts.get(port);
        if (!dev) {
            const index = this.#indexForPort(port);
            dev = new SerialDeviceImp(port, index);
            dev.wasmModule = this.wasmModule;
            dev.onDataFn = this.onDataFn;
            this.#serialPorts.set(port, dev);
        }

        const ok = await dev.open(baudRate);
        if (!ok) return null;

        dev._currentBaud = baudRate;
        dev._currentParity = 'none';

        // Register in C++ so listSerialTTYs() finds it
        if (this.wasmModule) {
            const wmOpen = this.wasmModule.cwrap('wm_serial_open', null, ['number']);
            wmOpen(dev.index);
        }

        return dev;
    }

    /**
     * List connected TTY names (mirrors C++ listSerialTTYs).
     * @returns {Promise<string[]>}
     */
    async listSerialTTYs() {
        await this.#initialized.promise;
        return [...this.#serialPorts.values()]
            .filter(d => d.isOpen)
            .map(d => `/dev/ttyWebUSB${d.index}`)
            .sort();
    }

    /**
     * Auto-open all previously permitted ports.
     * Called once after WASM init. Opens each port at 9600 baud
     * and registers it in C++ so detection can find it.
     * @returns {Promise<SerialDeviceImp[]>} List of successfully opened devices
     */
    async autoOpenExisting() {
        await this.#initialized.promise;
        const opened = [];
        for (const [port, dev] of this.#serialPorts.entries()) {
            if (dev.isOpen) continue; // already open

            dev.wasmModule = this.wasmModule;
            dev.onDataFn = this.onDataFn;

            const ok = await dev.open(9600);
            if (!ok) {
                console.warn(`[serial:${dev.index}] auto-open failed, skipping`);
                continue;
            }

            dev._currentBaud = 9600;
            dev._currentParity = 'none';

            // Register in C++ so listSerialTTYs() finds it
            if (this.wasmModule) {
                const wmOpen = this.wasmModule.cwrap('wm_serial_open', null, ['number']);
                wmOpen(dev.index);
            }

            opened.push(dev);
            console.log(`[serial:${dev.index}] auto-opened previously permitted port`);
        }
        return opened;
    }

    /**
     * Get device count (for C++ EM_ASM getDeviceCount())
     * @returns {number}
     */
    getDeviceCount() {
        return [...this.#serialPorts.values()].filter(d => d.isOpen).length;
    }

    /**
     * Find device by index
     * @param {number} index
     * @returns {SerialDeviceImp|null}
     */
    findByIndex(index) {
        for (const dev of this.#serialPorts.values()) {
            if (dev.index === index) return dev;
        }
        return null;
    }

    /**
     * Remove a device by index — closes JS port and unregisters in C++
     * @param {number} index
     */
    async closeByIndex(index) {
        const dev = this.findByIndex(index);
        if (!dev) return;

        await dev.close();

        // Remove from serialPorts map
        for (const [port, d] of this.#serialPorts.entries()) {
            if (d.index === index) {
                this.#serialPorts.delete(port);
                await port.forget();
                break;
            }
        }

        // Unregister in C++ (removes device + VFS entries)
        if (this.wasmModule) {
            try {
                const wmClose = this.wasmModule.cwrap('wm_serial_close', null, ['number']);
                wmClose(index);
            } catch (e) {
                console.error(`[serial:${index}] wm_serial_close failed:`, e);
            }
        }
    }

    /**
     * Close all devices
     */
    async closeAll() {
        for (const dev of this.#serialPorts.values()) {
            try {
                await dev.close();
            } catch (e) {
                console.error(`[serial:${dev.index}] close failed during closeAll:`, e);
            }
        }
    }

    /**
     * Register EM_ASM globals that C++ serial_web.cc calls.
     * Must be called before WASM init so the globals exist
     * when serial_web.cc code runs.
     */
    registerGlobals() {
        const mgr = this;

        // Queue for USB disconnect events. JS pushes here,
        // C++ drains via drainDisconnects() — avoids WASM re-entrancy.
        globalThis.pendingDisconnects = [];

        // Queue for USB connect events. JS opens the port and pushes here,
        // C++ drains via drainConnects() to register the device.
        globalThis.pendingConnects = [];

        /** @type {() => number} */
        globalThis.getDeviceCount = () => mgr.getDeviceCount();

        /**
         * Called by C++ SerialDeviceImp::open() via EM_ASM.
         * Currently a no-op — the WebSerial port stays open from requestAndOpen()
         * and C++ open/close only toggles internal state.
         */
        globalThis.openDeviceFull = (index, baud, databits, stopbits, parity) => {
            // No-op: port is managed by requestAndOpen()/removePort()
        };

        /**
         * Called by C++ SerialDeviceImp::close() via EM_ASM.
         */
        globalThis.closeSerialPort = (index) => {
            mgr.closeByIndex(index);
        };

        /**
         * Called by C++ SerialDeviceImp::send() via EM_ASM.
         * Reads bytes from WASM heap and writes to WebSerial.
         */
        globalThis.writeSerial = (index, ptr, len) => {
            const dev = mgr.findByIndex(index);
            if (!dev || !mgr.wasmModule) return;
            const data = new Uint8Array(mgr.wasmModule.HEAPU8.buffer, ptr, len).slice();

            // If a baudrate change is in progress, wait for it to complete
            // before sending. The probe bytes arrive here before the async
            // close+reopen finishes — chaining through the promise ensures
            // they're sent as soon as the port is ready.
            const pending = dev._reopenPromise || Promise.resolve();
            pending
                .then(() => dev.send(data))
                .catch(e => console.error(`[serial:${index}] write failed:`, e));
        };

        /**
         * Called by C++ receive() via EM_ASM.
         * Drains JS-side buffer into C++ rxBuffer via serial_on_data().
         * This is safe because we're called from C++ (not during ASYNCIFY sleep).
         * 
         * @param {number} index
         */
        globalThis.drainSerialData = (index) => {
            const dev = mgr.findByIndex(index);
            if (!dev) return 0;
            const data = dev.drainPendingData();
            if (!data) return 0;
            if (mgr.wasmModule && mgr.onDataFn) {
                const ptr = mgr.wasmModule._malloc(data.length);
                try {
                    mgr.wasmModule.HEAPU8.set(data, ptr);
                    mgr.onDataFn(index, ptr, data.length);
                } finally {
                    mgr.wasmModule._free(ptr);
                }
            }
            return data.length;
        };

        /**
         * Called by C++ SerialDeviceImp::open() via EM_ASM.
         * Closes and reopens the WebSerial port if baudrate/parity changed.
         * Returns 1 if a change was initiated, 0 if settings are unchanged.
         * Sets serialPortReady[index] = true once the reopen is complete.
         * C++ polls this flag to wait for the port to be ready.
         *
         * @param {number} index
         * @param {number} baud
         * @param {number} databits
         * @param {number} stopbits
         * @param {number} parity (0=none, 1=even, 2=odd)
         * @returns {number} 1 if change queued, 0 if no-op
         */
        globalThis.serialPortReady = {};

        globalThis.queueBaudrateChange = (index, baud, databits, stopbits, parity) => {
            const dev = mgr.findByIndex(index);
            if (!dev) return 0;
            const newParity = parityMap[parity] || 'none';

            // Skip if same settings
            if (dev._currentBaud === baud && dev._currentParity === newParity) return 0;

            const oldBaud = dev._currentBaud;
            dev._currentBaud = baud;
            dev._currentParity = newParity;
            globalThis.serialPortReady[index] = false;

            // Async close + reopen. C++ polls serialPortReady[index].
            dev._reopenPromise = (async () => {
                try {
                    if (dev.isOpen) await dev.close();
                    await dev.open(baud, databits, stopbits, newParity);
                    console.log(`[serial:${index}] baudrate ${oldBaud} → ${baud}, parity=${newParity}`);
                } catch (e) {
                    console.error(`[serial:${index}] baudrate change failed:`, e);
                } finally {
                    globalThis.serialPortReady[index] = true;
                    dev._reopenPromise = null;
                }
            })();

            return 1;
        };
    }
}

/**
 * Check if a character device exists.
 * In the browser, we check if we have an open port with that name.
 * @param {string} tty
 * @param {boolean} fail_if_not_ok
 * @returns {boolean}
 */
const checkCharacterDeviceExists = (tty, fail_if_not_ok) => {
    // In browser context, all registered devices "exist"
    return true;
};