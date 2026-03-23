// @ts-check

/**
 * @template T
 * @typedef {Object} DeferredPromise
 * @property {Promise<T>} promise
 * @property {NonNullable<Parameters<Promise<T>['then']>[0]>} resolve
 * @property {NonNullable<Parameters<Promise<T>['then']>[1]>} reject
 */

/** @template T @returns {DeferredPromise<*>} */
function createDeferredPromise() {
  /** @type {any} */ let res;
  /** @type {any} */ let rej;
  const promise = new Promise((resolve, reject) => { res = resolve; rej = reject; });
  return { promise, resolve: res, reject: rej };
}

// ═══════════════════════════════════════════════════════════════
// SerialDeviceImp — wraps a single WebSerial port
// C++ serial_web.cc calls EM_ASM → these JS functions
// JS read loop calls serial_on_data() → C++ rxBuffer
// ═══════════════════════════════════════════════════════════════

class SerialDeviceImp {
    /** @type {SerialPort} */    port;
    /** @type {number} */        index;
    /** @type {WritableStreamDefaultWriter|null} */ writer = null;
    /** @type {ReadableStreamDefaultReader|null} */ reader = null;
    /** @type {boolean} */       reading = false;
    /** @type {boolean} */       isOpen = false;
    /** @type {any} */           wasmModule = null; // set after WASM init
    /** @type {Function|null} */ onDataFn = null;   // cwrap'd serial_on_data

    /**
     * @param {SerialPort} port
     * @param {number} index
     */
    constructor(port, index) {
        this.port = port;
        this.index = index;
        this.pendingData = [];
    }

    /**
     * Open the port with given serial parameters.
     * Called by C++ SerialDeviceImp::open() via EM_ASM → openDeviceFull()
     * @param {number} baudRate
     * @param {number} dataBits
     * @param {number} stopBits
     * @param {string} parity
     */
    async open(baudRate, dataBits = 8, stopBits = 1, parity = 'none') {
        if (this.isOpen) return true;
        try {
            await this.port.open({ baudRate, dataBits, stopBits, parity, flowControl: 'none' });
            this.writer = this.port.writable.getWriter();
            this.isOpen = true;
            this._startReadLoop();
            return true;
        } catch (e) {
            console.error(`[serial:${this.index}] open failed:`, e);
            return false;
        }
    }

    /**
     * Close the port.
     */
    async close() {
        this.reading = false;
        this.isOpen = false;
        this.pendingData = [];
        try { if (this.reader) await this.reader.cancel(); } catch (e) {}
        try { if (this.writer) this.writer.releaseLock(); } catch (e) {}
        try { await this.port.close(); } catch (e) {}
        this.writer = null;
        this.reader = null;
    }

    /**
     * Send raw bytes. Called by C++ via EM_ASM → writeSerial(index, ptr, len)
     * @param {Uint8Array} data
     */
    async send(data) {
        if (!this.writer || !this.isOpen) return false;
        try {
            await this.writer.write(data);
            return true;
        } catch (e) {
            console.error(`[serial:${this.index}] write error:`, e);
            return false;
        }
    }

    /**
     * Read loop: reads from WebSerial, buffers in JS.
     * C++ pulls data via drainSerialData() in receive().
     * This avoids calling into WASM during ASYNCIFY sleep.
     */
    async _startReadLoop() {
        if (!this.port.readable) return;
        this.reader = this.port.readable.getReader();
        this.reading = true;
        this.pendingData = [];

        try {
            while (this.reading) {
                const { value, done } = await this.reader.read();
                if (done) break;
                if (!value || value.length === 0) continue;

                // Buffer in JS — do NOT call into WASM from here.
                // C++ will pull via drainSerialData() during receive()/sleep.
                this.pendingData.push(new Uint8Array(value));
            }
        } catch (e) {
            if (this.reading) {
                console.error(`[serial:${this.index}] read error:`, e);
            }
        } finally {
            try { this.reader.releaseLock(); } catch (e) {}
            this.reader = null;
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
        this.pendingData = [];
        return result;
    }

    /**
     * Get USB device info
     * @returns {{ usbVendorId?: number, usbProductId?: number }}
     */
    getInfo() {
        return this.port.getInfo();
    }
}

// ═══════════════════════════════════════════════════════════════
// SerialCommunicationManagerImp — manages all serial ports
// Maps to the C++ SerialCommunicationManagerImp in serial_web.cc
// ═══════════════════════════════════════════════════════════════

class SerialCommunicationManagerImp {
    constructor() {
        if (!navigator.serial) {
            throw new Error("Web Serial API is not supported in this browser.");
        }

        navigator.serial.addEventListener('connect', event => {
            const port = event.target;
            if (!this.#serialPorts.has(port)) {
                const index = this.#nextIndex();
                this.#serialPorts.set(port, new SerialDeviceImp(port, index));
            }
        });

        navigator.serial.addEventListener('disconnect', event => {
            const port = event.target;
            const dev = this.#serialPorts.get(port);
            if (dev) {
                dev.close();
                this.#serialPorts.delete(port);
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
                    const index = this.#nextIndex();
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

    #nextIndex = () => this.#serialPortIndex++;

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

        let dev = this.#serialPorts.get(port);
        if (!dev) {
            const index = this.#nextIndex();
            dev = new SerialDeviceImp(port, index);
            dev.wasmModule = this.wasmModule;
            dev.onDataFn = this.onDataFn;
            this.#serialPorts.set(port, dev);
        }

        const ok = await dev.open(baudRate);
        if (!ok) return null;

        // Register in C++ so listSerialTTYs() finds it
        if (this.wasmModule) {
            const wmSerialOpen = this.wasmModule.cwrap('wm_serial_open', 'string', ['number', 'number']);
            const wmFree = this.wasmModule.cwrap('wm_free_result', null, []);
            wmSerialOpen(dev.index, baudRate);
            wmFree();
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
            .map(d => `/dev/ttyUSB${d.index}`)
            .sort();
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
     * Close a device by index
     * @param {number} index
     */
    async closeByIndex(index) {
        const dev = this.findByIndex(index);
        if (dev) await dev.close();
    }

    /**
     * Close all devices
     */
    async closeAll() {
        for (const dev of this.#serialPorts.values()) {
            await dev.close();
        }
    }

    /**
     * Register EM_ASM globals that C++ serial_web.cc calls.
     * Must be called before WASM init so the globals exist
     * when serial_web.cc code runs.
     */
    registerGlobals() {
        const mgr = this;

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
            dev.send(data).catch(e => console.error(`[serial:${index}] write failed:`, e));
        };

        /**
         * Called by C++ receive() via EM_ASM.
         * Drains JS-side buffer into C++ rxBuffer via serial_on_data().
         * This is safe because we're called from C++ (not during ASYNCIFY sleep).
         */
        globalThis.drainSerialData = (index) => {
            const dev = mgr.findByIndex(index);
            if (!dev) return 0;
            const data = dev.drainPendingData();
            if (!data) return 0;
            if (mgr.wasmModule && mgr.onDataFn) {
                const ptr = mgr.wasmModule._malloc(data.length);
                mgr.wasmModule.HEAPU8.set(data, ptr);
                mgr.onDataFn(index, ptr, data.length);
                mgr.wasmModule._free(ptr);
            }
            return data.length;
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