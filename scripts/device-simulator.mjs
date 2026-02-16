#!/usr/bin/env node
/**
 * Device Simulator - Simulates Papyrix E-Reader Device
 *
 * This simulates a REAL e-reader device connecting to Calibre:
 * - Broadcasts "hello" on UDP to discover Calibre server
 * - Connects to Calibre's TCP port
 * - Handles protocol handshake (capabilities, device info, etc.)
 * - Receives books and saves to ./received_books/
 *
 * Protocol Reference: calibre/devices/smart_device_app/driver.py
 *
 * Usage:
 *   node device-simulator.mjs                      # Auto-discover Calibre
 *   node device-simulator.mjs -i 192.168.1.100     # Direct connect to IP
 */

import dgram from "dgram";
import net from "net";
import fs from "fs";
import path from "path";
import os from "os";
import crypto from "crypto";

const BROADCAST_PORTS = [54982, 48123, 39001, 44044, 59678];
const DEFAULT_PORT = 9090;
const STORAGE_DIR = "./received_books";

const OPCODES = {
  OK: 0,
  SET_CALIBRE_DEVICE_INFO: 1,
  SET_CALIBRE_DEVICE_NAME: 2,
  GET_DEVICE_INFORMATION: 3,
  TOTAL_SPACE: 4,
  FREE_SPACE: 5,
  GET_BOOK_COUNT: 6,
  SEND_BOOKLISTS: 7,
  SEND_BOOK: 8,
  GET_INITIALIZATION_INFO: 9,
  BOOK_DONE: 11,
  NOOP: 12,
  DELETE_BOOK: 13,
  SEND_BOOK_METADATA: 16,
  DISPLAY_MESSAGE: 17,
  CALIBRE_BUSY: 18,
  SET_LIBRARY_INFO: 19,
  ERROR: 20,
};

const OPCODE_NAMES = Object.fromEntries(
  Object.entries(OPCODES).map(([k, v]) => [v, k])
);

class DeviceSimulator {
  constructor(options = {}) {
    this.calibreHost = options.host || null;
    this.calibrePort = options.port || DEFAULT_PORT;
    this.deviceName = "Papyrix Reader";
    this.deviceId = crypto.randomUUID();

    this.socket = null;
    this.recvBuffer = Buffer.alloc(0);
    this.connected = false;

    // Storage
    this.totalSpace = 32 * 1024 * 1024 * 1024; // 32GB
    this.freeSpace = 28 * 1024 * 1024 * 1024; // 28GB
    this.books = new Map();

    // Book receiving state
    this.receivingBook = null;

    // Library info from Calibre
    this.libraryName = null;
    this.libraryUuid = null;

    // Ensure storage directory exists
    if (!fs.existsSync(STORAGE_DIR)) {
      fs.mkdirSync(STORAGE_DIR, { recursive: true });
    }
  }

  async start() {
    console.log(`\n╔════════════════════════════════════════════════════════════╗`);
    console.log(`║            Papyrix Device Simulator                        ║`);
    console.log(`╚════════════════════════════════════════════════════════════╝\n`);

    if (this.calibreHost) {
      // Direct connection
      await this.connect(this.calibreHost, this.calibrePort);
    } else {
      // Auto-discover
      console.log("[MODE] Discovering Calibre server...\n");
      const server = await this.discoverCalibre();
      if (server) {
        await this.connect(server.host, server.port);
      }
    }
  }

  async discoverCalibre() {
    return new Promise((resolve) => {
      const socket = dgram.createSocket("udp4");
      let found = false;
      let closed = false;

      const cleanup = () => {
        if (!closed) {
          closed = true;
          try { socket.close(); } catch (e) { /* ignore */ }
        }
      };

      socket.on("error", (err) => {
        console.error(`[UDP] Error: ${err.message}`);
        cleanup();
        resolve(null);
      });

      socket.on("message", (msg, rinfo) => {
        if (found) return;

        const message = msg.toString().trim();
        const match = message.match(/calibre wireless device client.*?;(\d+),(\d+)$/);
        if (match) {
          found = true;
          const port = parseInt(match[2], 10);
          console.log(`[UDP] Found Calibre at ${rinfo.address}:${port}`);
          cleanup();
          resolve({ host: rinfo.address, port });
        }
      });

      socket.bind(() => {
        socket.setBroadcast(true);
        console.log(`[UDP] Broadcasting "hello" to find Calibre...`);

        let portIndex = 0;
        const broadcast = () => {
          if (found || portIndex >= BROADCAST_PORTS.length) {
            if (!found && !closed) {
              console.log("[UDP] No Calibre server found\n");
              console.log("[HINT] Make sure Calibre has 'Start wireless device connection' enabled");
              console.log("       (Connect/share menu -> Start wireless device connection)\n");
              cleanup();
              resolve(null);
            }
            return;
          }

          const port = BROADCAST_PORTS[portIndex++];
          socket.send("hello", port, "255.255.255.255", () => {});
          setTimeout(broadcast, 500);
        };

        broadcast();
      });
    });
  }

  async connect(host, port) {
    return new Promise((resolve, reject) => {
      console.log(`[TCP] Connecting to ${host}:${port}...`);

      this.socket = new net.Socket();
      this.recvBuffer = Buffer.alloc(0);

      this.socket.connect(port, host, () => {
        console.log(`[TCP] ✓ Connected to Calibre\n`);
        this.connected = true;
        resolve();
      });

      this.socket.on("data", (data) => {
        if (this.receivingBook) {
          this.handleBookData(data);
        } else {
          this.recvBuffer = Buffer.concat([this.recvBuffer, data]);
          this.processMessages();
        }
      });

      this.socket.on("close", () => {
        console.log("\n[TCP] Disconnected from Calibre");
        this.connected = false;
        this.socket = null;
      });

      this.socket.on("error", (err) => {
        console.error(`[TCP] Error: ${err.message}`);
        reject(err);
      });
    });
  }

  processMessages() {
    while (this.recvBuffer.length > 0) {
      // Find length prefix
      let lenEnd = 0;
      while (lenEnd < this.recvBuffer.length &&
             this.recvBuffer[lenEnd] >= 0x30 &&
             this.recvBuffer[lenEnd] <= 0x39) {
        lenEnd++;
      }

      if (lenEnd === 0 || lenEnd >= this.recvBuffer.length) return;

      const msgLen = parseInt(this.recvBuffer.slice(0, lenEnd).toString(), 10);
      if (this.recvBuffer.length < lenEnd + msgLen) return;

      const msgData = this.recvBuffer.slice(lenEnd, lenEnd + msgLen).toString();
      this.recvBuffer = this.recvBuffer.slice(lenEnd + msgLen);

      try {
        const [opcode, payload] = JSON.parse(msgData);
        this.handleMessage(opcode, payload || {});
      } catch (e) {
        console.error(`[ERROR] Parse error: ${e.message}`);
      }
    }
  }

  handleMessage(opcode, payload) {
    const name = OPCODE_NAMES[opcode] || `UNKNOWN(${opcode})`;
    console.log(`[RECV] ${name}`);

    switch (opcode) {
      case OPCODES.GET_INITIALIZATION_INFO:
        this.handleInitInfo(payload);
        break;

      case OPCODES.GET_DEVICE_INFORMATION:
        this.send(OPCODES.OK, {
          device_info: { device_store_uuid: this.deviceId, device_name: this.deviceName },
          device_version: "Papyrix 1.0",
          version: "1.0",
        });
        break;

      case OPCODES.SET_CALIBRE_DEVICE_INFO:
      case OPCODES.SET_CALIBRE_DEVICE_NAME:
        this.send(OPCODES.OK, {});
        break;

      case OPCODES.SET_LIBRARY_INFO:
        this.handleLibraryInfo(payload);
        break;

      case OPCODES.TOTAL_SPACE:
        console.log(`       → Reporting ${Math.round(this.totalSpace / 1024 / 1024 / 1024)}GB total`);
        this.send(OPCODES.OK, { total_space_on_device: this.totalSpace });
        break;

      case OPCODES.FREE_SPACE:
        console.log(`       → Reporting ${Math.round(this.freeSpace / 1024 / 1024 / 1024)}GB free`);
        this.send(OPCODES.OK, { free_space_on_device: this.freeSpace });
        break;

      case OPCODES.GET_BOOK_COUNT:
        console.log(`       → Reporting ${this.books.size} books`);
        this.send(OPCODES.OK, { count: this.books.size, willStream: true, willScan: true });
        for (const book of this.books.values()) {
          this.send(OPCODES.OK, book);
        }
        break;

      case OPCODES.SEND_BOOKLISTS:
        // Calibre sends this with wait_for_response=False - do NOT respond!
        console.log(`       → Calibre has ${payload.count || 0} metadata updates`);
        break;

      case OPCODES.SEND_BOOK:
        this.handleSendBook(payload);
        break;

      case OPCODES.SEND_BOOK_METADATA:
        console.log(`       → Metadata for: "${payload.title || 'Unknown'}"`);
        this.send(OPCODES.OK, {});
        break;

      case OPCODES.DELETE_BOOK:
        this.handleDeleteBook(payload);
        break;

      case OPCODES.DISPLAY_MESSAGE:
        console.log(`       → Message: "${payload.message || ''}"`);
        this.send(OPCODES.OK, {});
        break;

      case OPCODES.NOOP:
        // Only respond to NOOP with empty payload
        // Calibre sends NOOP with payload like {"count": 0} with wait_for_response=False
        if (Object.keys(payload).length === 0) {
          this.send(OPCODES.OK, {});
        } else {
          console.log(`       → NOOP with payload (no response): ${JSON.stringify(payload)}`);
        }
        break;

      default:
        this.send(OPCODES.OK, {});
    }
  }

  handleInitInfo(payload) {
    const version = payload.calibre_version?.join(".") || "unknown";
    console.log(`       → Calibre version: ${version}\n`);

    this.send(OPCODES.OK, {
      versionOK: true,
      maxBookContentPacketLen: 4096,
      acceptedExtensions: ["epub", "txt", "md", "xtc", "xtch"],
      canStreamBooks: true,
      canStreamMetadata: true,
      canReceiveBookBinary: true,
      canDeleteMultipleBooks: true,
      canSendOkToSendbook: true,
      coverHeight: 240,
      cacheUsesLpaths: true,
      canAcceptLibraryInfo: true,
      canUseCachedMetadata: true,
      deviceKind: "Papyrix E-Ink Reader",
      deviceName: this.deviceName,
      appName: "Papyrix Reader",
      ccVersionNumber: 128,
      passwordHash: "",
      useUuidFileNames: false,
      device_store_uuid: this.deviceId,
      extensionPathLengths: {},
    });
  }

  handleLibraryInfo(payload) {
    this.libraryName = payload.libraryName || "Unknown Library";
    this.libraryUuid = payload.libraryUuid || "";
    console.log(`       → Library: "${this.libraryName}"`);
    // Note: fieldMetadata can be very large but we just acknowledge it
    this.send(OPCODES.OK, {});
  }

  handleSendBook(payload) {
    console.log(`\n[BOOK] Receiving: "${payload.title || 'Unknown'}" (${payload.length} bytes)`);

    this.receivingBook = {
      lpath: payload.lpath,
      title: payload.title || "Unknown",
      authors: payload.authors || "Unknown",
      uuid: payload.uuid || crypto.randomUUID(),
      expectedSize: payload.length,
      receivedSize: 0,
      chunks: [],
    };

    this.send(OPCODES.OK, { willAccept: true });
  }

  handleBookData(data) {
    const book = this.receivingBook;
    book.chunks.push(data);
    book.receivedSize += data.length;

    const pct = Math.round((book.receivedSize / book.expectedSize) * 100);
    process.stdout.write(`\r[BOOK] Receiving: ${pct}% (${book.receivedSize}/${book.expectedSize})`);

    if (book.receivedSize >= book.expectedSize) {
      // Complete
      const bookData = Buffer.concat(book.chunks);
      const filePath = path.join(STORAGE_DIR, path.basename(book.lpath));
      fs.writeFileSync(filePath, bookData);

      console.log(`\n[BOOK] ✓ Saved to: ${filePath}\n`);

      this.books.set(book.lpath, {
        lpath: book.lpath,
        title: book.title,
        authors: [book.authors],
        uuid: book.uuid,
        size: bookData.length,
      });

      this.receivingBook = null;
      this.send(OPCODES.BOOK_DONE, {});
    }
  }

  handleDeleteBook(payload) {
    const lpaths = payload.lpaths || [];
    console.log(`       → Deleting ${lpaths.length} book(s)`);

    for (const lpath of lpaths) {
      const book = this.books.get(lpath);
      const uuid = book?.uuid || "";  // Save uuid before deletion
      if (book) {
        this.books.delete(lpath);
        const filePath = path.join(STORAGE_DIR, path.basename(lpath));
        try { fs.unlinkSync(filePath); } catch (e) { /* ignore */ }
        console.log(`       → Deleted: "${book.title}"`);
      }
      this.send(OPCODES.OK, { uuid });
    }
  }

  send(opcode, payload) {
    if (!this.socket || !this.connected) return;

    const name = OPCODE_NAMES[opcode] || opcode;
    const msg = JSON.stringify([opcode, payload]);
    const fullMsg = msg.length.toString() + msg;

    console.log(`[SEND] ${name}`);
    this.socket.write(fullMsg);
  }

  stop() {
    console.log("\n[STOP] Shutting down...");
    if (this.socket) this.socket.end();
  }
}

function getLocalIPs() {
  const ips = [];
  const nets = os.networkInterfaces();
  for (const name of Object.keys(nets)) {
    for (const iface of nets[name]) {
      if (iface.family === "IPv4" && !iface.internal) {
        ips.push(iface.address);
      }
    }
  }
  return ips;
}

async function main() {
  const args = process.argv.slice(2);
  let host = null;
  let port = DEFAULT_PORT;

  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--ip" || args[i] === "-i") host = args[++i];
    else if (args[i] === "--port" || args[i] === "-p") port = parseInt(args[++i], 10);
    else if (args[i] === "--help" || args[i] === "-h") {
      console.log(`
Device Simulator - Simulates Papyrix E-Reader

Usage:
  node device-simulator.mjs [options]

Options:
  -i, --ip <host>        Calibre IP (skip discovery)
  -p, --port <port>      Calibre port (default: 9090)
  -h, --help             Show help

Examples:
  node device-simulator.mjs                    # Auto-discover Calibre
  node device-simulator.mjs -i 192.168.1.100   # Direct connect
`);
      process.exit(0);
    }
  }

  console.log("Local IPs:", getLocalIPs().join(", "));

  const device = new DeviceSimulator({ host, port });
  process.on("SIGINT", () => { device.stop(); process.exit(0); });

  await device.start();

  // Keep running
  while (true) await new Promise(r => setTimeout(r, 1000));
}

main();
