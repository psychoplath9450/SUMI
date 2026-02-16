#!/usr/bin/env node
/**
 * Calibre Simulator - Simulates Calibre Desktop App
 *
 * This simulates the REAL Calibre desktop app protocol:
 * - Starts TCP server on port 9090
 * - Listens on UDP discovery ports for device "hello" messages
 * - Accepts device connections
 * - Sends books to device
 *
 * Protocol flow (matches real Calibre 8.x):
 * 1. GET_INITIALIZATION_INFO
 * 2. GET_DEVICE_INFORMATION
 * 3. SET_CALIBRE_DEVICE_INFO
 * 4. FREE_SPACE
 * 5. SET_LIBRARY_INFO
 * 6. NOOP (no response expected)
 * 7. SEND_BOOKLISTS (no response expected)
 * 8. Optional: SEND_BOOK for each book
 *
 * Usage:
 *   node calibre-simulator.mjs                    # Start server, wait for device
 *   node calibre-simulator.mjs -s book.epub       # Send book after connect
 */

import dgram from "dgram";
import net from "net";
import fs from "fs";
import path from "path";
import os from "os";

const BROADCAST_PORTS = [54982, 48123, 39001, 44044, 59678];
const DEFAULT_PORT = 9090;

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
  DISPLAY_MESSAGE: 17,
  CALIBRE_BUSY: 18,
  SET_LIBRARY_INFO: 19,
  ERROR: 20,
};

const OPCODE_NAMES = Object.fromEntries(
  Object.entries(OPCODES).map(([k, v]) => [v, k])
);

class CalibreSimulator {
  constructor(options = {}) {
    this.port = options.port || DEFAULT_PORT;
    this.bookFile = options.bookFile || null;
    this.tcpServer = null;
    this.udpSockets = [];
    this.clientSocket = null;
    this.recvBuffer = Buffer.alloc(0);
    this.connected = false;
    this.deviceInfo = null;
    this.protocolState = 'idle';
    this.deviceStoreUuid = null;
  }

  async start() {
    console.log(`\n╔════════════════════════════════════════════════════════════╗`);
    console.log(`║            Calibre Desktop App Simulator                   ║`);
    console.log(`╚════════════════════════════════════════════════════════════╝\n`);

    // Start TCP server
    await this.startTCPServer();

    // Start UDP listener
    await this.startUDPListener();

    console.log(`\n[STATUS] Calibre is ready!`);
    console.log(`         Waiting for device to connect...\n`);
  }

  async startTCPServer() {
    return new Promise((resolve, reject) => {
      this.tcpServer = net.createServer((socket) => {
        if (this.clientSocket) {
          console.log("[TCP] Rejecting additional connection");
          socket.end();
          return;
        }

        this.clientSocket = socket;
        this.recvBuffer = Buffer.alloc(0);

        const clientAddr = `${socket.remoteAddress}:${socket.remotePort}`;
        console.log(`\n[TCP] ✓ Device connected from ${clientAddr}\n`);
        this.connected = true;

        // Stop UDP listeners once connected
        for (const s of this.udpSockets) {
          try { s.close(); } catch (e) { /* ignore */ }
        }
        this.udpSockets = [];

        socket.on("data", (data) => {
          this.recvBuffer = Buffer.concat([this.recvBuffer, data]);
          this.processMessages();
        });

        socket.on("close", () => {
          console.log("\n[TCP] Device disconnected");
          this.connected = false;
          this.clientSocket = null;
          this.protocolState = 'idle';
        });

        socket.on("error", (err) => {
          console.error(`[TCP] Error: ${err.message}`);
        });

        // Start protocol handshake
        setTimeout(() => this.startHandshake(), 100);
      });

      this.tcpServer.listen(this.port, "0.0.0.0", () => {
        console.log(`[TCP] Server listening on port ${this.port}`);
        resolve();
      });

      this.tcpServer.on("error", reject);
    });
  }

  async startUDPListener() {
    // Listen on broadcast ports for "hello" messages from devices
    for (const port of BROADCAST_PORTS) {
      try {
        const socket = dgram.createSocket({ type: "udp4", reuseAddr: true });

        socket.on("message", (msg, rinfo) => {
          const message = msg.toString().trim();
          if (message === "hello") {
            console.log(`[UDP] Received "hello" from ${rinfo.address}:${rinfo.port}`);

            // Respond with Calibre's info
            const hostname = os.hostname().split(".")[0];
            const response = `calibre wireless device client (on ${hostname});0,${this.port}`;

            socket.send(response, rinfo.port, rinfo.address, (err) => {
              if (err) {
                console.error(`[UDP] Response error: ${err.message}`);
              } else {
                console.log(`[UDP] Sent response: "${response}"`);
              }
            });
          }
        });

        socket.on("error", () => {
          // Silently ignore bind errors
        });

        await new Promise((resolve) => {
          socket.bind(port, "0.0.0.0", () => {
            console.log(`[UDP] Listening on port ${port}`);
            resolve();
          });
        });

        this.udpSockets.push(socket);
      } catch (err) {
        // Ignore errors for individual ports
      }
    }
  }

  async startHandshake() {
    console.log("[PROTOCOL] Starting handshake...\n");
    this.protocolState = 'init';

    await this.send(OPCODES.GET_INITIALIZATION_INFO, {
      calibre_version: [8, 16, 2],
      serverProtocolVersion: 1,
      validExtensions: ["epub", "mobi", "pdf", "txt", "azw3", "md", "xtc", "xtch"],
      passwordChallenge: "",
      currentLibraryName: "Test Library",
      currentLibraryUUID: "test-uuid-1234-5678-9abc-def012345678",
      pubdateFormat: "MMM yyyy",
      timestampFormat: "dd MMM yyyy",
      lastModifiedFormat: "dd MMM yyyy",
      canSupportUpdateBooks: true,
      canSupportLpathChanges: true,
    });
  }

  async processMessages() {
    while (this.recvBuffer.length > 0) {
      let lenEnd = 0;
      while (lenEnd < this.recvBuffer.length && this.recvBuffer[lenEnd] >= 0x30 && this.recvBuffer[lenEnd] <= 0x39) {
        lenEnd++;
      }

      if (lenEnd === 0 || lenEnd >= this.recvBuffer.length) return;

      const msgLen = parseInt(this.recvBuffer.slice(0, lenEnd).toString(), 10);
      if (this.recvBuffer.length < lenEnd + msgLen) return;

      const msgData = this.recvBuffer.slice(lenEnd, lenEnd + msgLen).toString();
      this.recvBuffer = this.recvBuffer.slice(lenEnd + msgLen);

      try {
        const parsed = JSON.parse(msgData);
        await this.handleMessage(parsed[0], parsed[1] || {});
      } catch (e) {
        console.error(`[ERROR] Parse error: ${e.message}`);
      }
    }
  }

  async handleMessage(opcode, payload) {
    const name = OPCODE_NAMES[opcode] || opcode;
    console.log(`[RECV] ${name}`);

    if (opcode === OPCODES.OK) {
      // Handle OK responses based on current state
      switch (this.protocolState) {
        case 'init':
          // Response to GET_INITIALIZATION_INFO
          this.deviceInfo = payload;
          console.log("\n[DEVICE]");
          console.log(`  Name: ${payload.deviceName}`);
          console.log(`  Kind: ${payload.deviceKind}`);
          console.log(`  Extensions: ${JSON.stringify(payload.acceptedExtensions)}`);
          console.log(`  Max packet: ${payload.maxBookContentPacketLen}`);
          this.deviceStoreUuid = payload.device_store_uuid || "unknown";

          // Check required capabilities
          const canStreamBooks = payload.canStreamBooks || false;
          const canStreamMetadata = payload.canStreamMetadata || false;
          const canReceiveBookBinary = payload.canReceiveBookBinary || false;
          const canDeleteMultiple = payload.canDeleteMultipleBooks || false;

          console.log(`  Capabilities:`);
          console.log(`    canStreamBooks: ${canStreamBooks}`);
          console.log(`    canStreamMetadata: ${canStreamMetadata}`);
          console.log(`    canReceiveBookBinary: ${canReceiveBookBinary}`);
          console.log(`    canDeleteMultipleBooks: ${canDeleteMultiple}`);

          if (!(canStreamBooks && canStreamMetadata && canReceiveBookBinary && canDeleteMultiple)) {
            console.log("\n[ERROR] ✗ Device capabilities check FAILED!");
            console.log("        Real Calibre would reject with: 'The app on your device is too old'");
            this.clientSocket.end();
            return;
          }
          console.log("  ✓ All capabilities OK\n");

          // Next: GET_DEVICE_INFORMATION
          this.protocolState = 'devinfo';
          await this.send(OPCODES.GET_DEVICE_INFORMATION, {});
          break;

        case 'devinfo':
          // Response to GET_DEVICE_INFORMATION
          console.log(`       → Device version: ${payload.device_version || "unknown"}`);
          this.protocolState = 'setdevinfo';
          await this.send(OPCODES.SET_CALIBRE_DEVICE_INFO, {
            device_store_uuid: this.deviceStoreUuid,
            device_name: payload.device_info?.device_name || "Unknown Device",
            location_code: "main",
            last_library_uuid: "test-uuid-1234-5678-9abc-def012345678",
            calibre_version: "8.16.2",
            date_last_connected: new Date().toISOString(),
            prefix: "",
          });
          break;

        case 'setdevinfo':
          // Response to SET_CALIBRE_DEVICE_INFO
          this.protocolState = 'freespace';
          await this.send(OPCODES.FREE_SPACE, {});
          break;

        case 'freespace':
          // Response to FREE_SPACE
          const gb = (payload.free_space_on_device / 1024 / 1024 / 1024).toFixed(2);
          console.log(`       → Free space: ${gb} GB\n`);

          // Next: SET_LIBRARY_INFO (with fieldMetadata like real Calibre)
          this.protocolState = 'library';
          await this.send(OPCODES.SET_LIBRARY_INFO, {
            libraryName: "Test Library",
            libraryUuid: "test-uuid-1234-5678-9abc-def012345678",
            fieldMetadata: {
              // Real Calibre sends all field definitions here
              title: { name: "Title", datatype: "text" },
              authors: { name: "Authors", datatype: "text" },
              series: { name: "Series", datatype: "series" },
              tags: { name: "Tags", datatype: "text" },
            },
            otherInfo: { id_link_rules: {} },
          });
          break;

        case 'library':
          // Response to SET_LIBRARY_INFO
          console.log("       → Library info acknowledged\n");

          // Send NOOP with payload (no response expected)
          await this.send(OPCODES.NOOP, { count: 0 });

          // Send SEND_BOOKLISTS (no response expected)
          await this.send(OPCODES.SEND_BOOKLISTS, {
            count: 0,
            collections: {},
            willStreamMetadata: true,
            supportsSync: false,
          });

          this.protocolState = 'ready';

          if (this.bookFile && fs.existsSync(this.bookFile)) {
            this.protocolState = 'sending_book';
            await this.sendBook(this.bookFile);
          } else {
            console.log("[COMPLETE] Handshake successful!");
            if (this.bookFile) {
              console.log(`[ERROR] Book file not found: ${this.bookFile}`);
            }
          }
          break;

        case 'sending_book':
          if (payload.willAccept) {
            console.log("       → Device accepted book, sending data...\n");
            await this.sendBookData();
          }
          break;

        case 'sending_data':
          console.log("\n[COMPLETE] Book transfer complete!");
          break;

        default:
          console.log("       → OK");
      }
    } else if (opcode === OPCODES.BOOK_DONE) {
      console.log("       → Book transfer confirmed by device\n");
      this.protocolState = 'complete';

      // Send a nice message
      await this.send(OPCODES.DISPLAY_MESSAGE, {
        message: "Book transferred successfully!",
      });

      console.log("[COMPLETE] All done! ✓\n");
    }
  }

  async sendBook(filePath) {
    const stats = fs.statSync(filePath);
    const fileName = path.basename(filePath);
    const ext = path.extname(filePath);

    this.currentBook = {
      filePath,
      fileName,
      size: stats.size,
      fd: fs.openSync(filePath, "r"),
    };

    console.log(`[SEND_BOOK] "${fileName}" (${stats.size} bytes)\n`);

    await this.send(OPCODES.SEND_BOOK, {
      lpath: `Calibre/${fileName}`,
      title: fileName.replace(ext, ""),
      authors: "Test Author",
      uuid: `test-${Date.now()}`,
      length: stats.size,
      calibre_id: 1,
      willStreamBooks: true,
      willStreamBinary: true,
      wantsSendOkToSendbook: true,
      canSupportLpathChanges: true,
    });
  }

  async sendBookData() {
    const { fd, size } = this.currentBook;
    const CHUNK_SIZE = 4096;
    const buffer = Buffer.alloc(CHUNK_SIZE);

    let sent = 0;
    console.log("[TRANSFER] Sending book data...\n");

    while (sent < size) {
      const remaining = size - sent;
      const chunkSize = Math.min(CHUNK_SIZE, remaining);
      const bytesRead = fs.readSync(fd, buffer, 0, chunkSize, sent);

      await new Promise((resolve, reject) => {
        this.clientSocket.write(buffer.slice(0, bytesRead), (err) => {
          if (err) reject(err);
          else resolve();
        });
      });

      sent += bytesRead;
      const pct = ((sent / size) * 100).toFixed(0);
      process.stdout.write(`\r[TRANSFER] ${pct}% (${sent}/${size} bytes)`);
    }

    fs.closeSync(fd);
    console.log("\n\n[TRANSFER] Data sent, waiting for BOOK_DONE...\n");
    this.protocolState = 'sending_data';
  }

  async send(opcode, payload) {
    if (!this.clientSocket || !this.connected) return;

    const name = OPCODE_NAMES[opcode] || opcode;
    const msg = JSON.stringify([opcode, payload]);
    const fullMsg = msg.length.toString() + msg;

    console.log(`[SEND] ${name}`);

    return new Promise((resolve, reject) => {
      this.clientSocket.write(fullMsg, (err) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }

  stop() {
    console.log("\n[STOP] Shutting down...");
    for (const s of this.udpSockets) {
      try { s.close(); } catch (e) { /* ignore */ }
    }
    if (this.clientSocket) this.clientSocket.end();
    if (this.tcpServer) this.tcpServer.close();
  }
}

function getLocalIPs() {
  const ips = [];
  const nets = os.networkInterfaces();
  for (const name of Object.keys(nets)) {
    for (const net of nets[name]) {
      if (net.family === "IPv4" && !net.internal) {
        ips.push(net.address);
      }
    }
  }
  return ips;
}

async function main() {
  const args = process.argv.slice(2);
  let port = DEFAULT_PORT;
  let bookFile = null;

  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--port" || args[i] === "-p") port = parseInt(args[++i], 10);
    else if (args[i] === "--send" || args[i] === "-s") bookFile = args[++i];
    else if (args[i] === "--help" || args[i] === "-h") {
      console.log(`
Calibre Simulator - Simulates Calibre Desktop App

Usage:
  node calibre-simulator.mjs [options]

Options:
  -p, --port <port>      TCP port (default: 9090)
  -s, --send <file>      Send book file after device connects
  -h, --help             Show help

Examples:
  node calibre-simulator.mjs              # Start server, wait for device
  node calibre-simulator.mjs -s book.epub # Send book after connect
`);
      process.exit(0);
    }
  }

  console.log("Local IPs:", getLocalIPs().join(", "));

  const calibre = new CalibreSimulator({ port, bookFile });
  process.on("SIGINT", () => { calibre.stop(); process.exit(0); });

  await calibre.start();
  while (true) await new Promise(r => setTimeout(r, 1000));
}

main();
