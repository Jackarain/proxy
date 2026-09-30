import { socket } from '@kit.NetworkKit';
import { util } from '@kit.ArkTS';
import { Logger } from '../common/Logger';

const log = new Logger('launcher');
const WS_GUID: string = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
const MAX_BUFFER: number = 4 * 1024 * 1024;

/** 控制通道角色. */
export class ClientRole {
  static readonly PROXY: string = 'proxy';
  static readonly UI: string = 'ui';
}

/** 控制通道事件回调. */
export interface LauncherEvents {
  /** proxy 连接建立 (握手完成). */
  onProxyConnected(): void;
  /** proxy 连接断开. */
  onProxyClosed(): void;
  /** 收到 proxy 消息 (JSON-RPC 文本). */
  onProxyMessage(text: string): void;
  /** 收到界面侧消息 (JSON-RPC 文本). */
  onUiMessage(text: string): void;
}

/** 一个已建立的控制通道连接. */
class WsClient {
  id: number = 0;
  role: string = ClientRole.UI;
  conn: socket.TCPSocketConnection;
  handshakeDone: boolean = false;
  buffer: Uint8Array = new Uint8Array(0);
  fragmentOpcode: number = 0;
  fragment: Uint8Array = new Uint8Array(0);

  constructor(id: number, conn: socket.TCPSocketConnection) {
    this.id = id;
    this.conn = conn;
  }
}

/**
 * 本地 WebSocket 控制服务端.
 *
 * libproxy 启动后主动连接 `ws://127.0.0.1:<port>/<token>` 注册实例并上报
 * status/log, 服务端可下发 set_config / set_tun_fd / shutdown 等 RPC, 并响应
 * proxy 的 protect 请求 (放行对外 socket). 界面侧以 `/<token>/ui` 连接同一
 * 服务获取状态与日志.
 */
export class LauncherServer {
  private server: socket.TCPSocketServer | undefined = undefined;
  private token: string = '';
  private port: number = 0;
  private nextClientId: number = 1;
  private clients: WsClient[] = [];
  private proxyClient: WsClient | undefined = undefined;
  private events: LauncherEvents;
  private closed: boolean = false;

  constructor(events: LauncherEvents) {
    this.events = events;
  }

  getPort(): number {
    return this.port;
  }

  getToken(): string {
    return this.token;
  }

  isProxyConnected(): boolean {
    return this.proxyClient !== undefined;
  }

  /**
   * 启动监听. [preferredPort] 优先使用 (<=0 由系统分配); 端口被占用时
   * 退回随机端口. 返回实际监听端口.
   */
  async start(preferredPort: number, token: string): Promise<number> {
    this.token = token;
    this.server = socket.constructTCPSocketServerInstance();
    const server = this.server;
    server.on('connect', (conn: socket.TCPSocketConnection) => {
      this.handleConnect(conn);
    });
    server.on('error', (err: Error) => {
      log.warn(`launcher server error: ${err.message}`);
    });

    if (preferredPort > 0) {
      try {
        await server.listen({ address: '127.0.0.1', port: preferredPort, family: 1 });
        this.port = preferredPort;
        log.info(`launcher listening on ${this.port}`);
        return this.port;
      } catch (err) {
        log.warn(`launcher listen ${preferredPort} failed, fallback to random port`);
      }
    }
    for (let i = 0; i < 16; i++) {
      const candidate: number = 32000 + Math.floor(Math.random() * 28000);
      try {
        await server.listen({ address: '127.0.0.1', port: candidate, family: 1 });
        this.port = candidate;
        log.info(`launcher listening on ${this.port}`);
        return this.port;
      } catch (err) {
        // 端口占用, 换一个重试.
      }
    }
    throw new Error('控制通道端口全部被占用');
  }

  /* ---------------------------- 连接处理 ---------------------------- */

  private handleConnect(conn: socket.TCPSocketConnection): void {
    const client = new WsClient(this.nextClientId++, conn);
    this.clients.push(client);
    conn.on('message', (info: socket.SocketMessageInfo) => {
      this.handleData(client, new Uint8Array(info.message));
    });
    conn.on('close', () => {
      this.handleClose(client);
    });
    conn.on('error', (err: Error) => {
      log.warn(`launcher client error: ${err.message}`);
      this.handleClose(client);
    });
  }

  private handleClose(client: WsClient): void {
    const index: number = this.clients.indexOf(client);
    if (index >= 0) {
      this.clients.splice(index, 1);
    }
    if (this.proxyClient === client) {
      this.proxyClient = undefined;
      this.events.onProxyClosed();
    }
  }

  private handleData(client: WsClient, chunk: Uint8Array): void {
    client.buffer = concatBytes(client.buffer, chunk);
    if (client.buffer.length > MAX_BUFFER) {
      log.warn('launcher buffer overflow, closing client');
      this.closeClient(client);
      return;
    }
    if (!client.handshakeDone) {
      this.tryHandshake(client);
    }
    if (client.handshakeDone) {
      this.parseFrames(client);
    }
  }

  /** 解析 HTTP 升级请求, 校验 token 路径后回 101. */
  private tryHandshake(client: WsClient): void {
    const text: string = utf8Decode(client.buffer);
    const headerEnd: number = text.indexOf('\r\n\r\n');
    if (headerEnd < 0) {
      return;
    }
    const header: string = text.substring(0, headerEnd);
    const lines: string[] = header.split('\r\n');
    const requestLine: string = lines.length > 0 ? lines[0] : '';
    const parts: string[] = requestLine.split(' ');
    const path: string = parts.length > 1 ? parts[1] : '';
    let key: string = '';
    for (const line of lines) {
      const colon: number = line.indexOf(':');
      if (colon < 0) {
        continue;
      }
      const name: string = line.substring(0, colon).trim().toLowerCase();
      if (name === 'sec-websocket-key') {
        key = line.substring(colon + 1).trim();
      }
    }
    const expected: string[] = [`/${this.token}`, `/${this.token}/ui`];
    if (key.length === 0 || expected.indexOf(path) < 0) {
      log.warn(`launcher reject upgrade path=${path}`);
      this.sendRaw(client, 'HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n');
      this.closeClient(client);
      return;
    }
    client.role = path === `/${this.token}` ? ClientRole.PROXY : ClientRole.UI;
    const accept: string = this.acceptKey(key);
    this.sendRaw(client,
      'HTTP/1.1 101 Switching Protocols\r\n' +
      'Upgrade: websocket\r\n' +
      'Connection: Upgrade\r\n' +
      `Sec-WebSocket-Accept: ${accept}\r\n\r\n`);
    client.handshakeDone = true;
    // 余下字节按帧继续处理.
    const consumed: number = utf8Encode(text.substring(0, headerEnd + 4)).length;
    client.buffer = client.buffer.slice(consumed);
    if (client.role === ClientRole.PROXY) {
      this.proxyClient = client;
      this.events.onProxyConnected();
    }
    log.info(`launcher upgrade ok role=${client.role}`);
  }

  private acceptKey(key: string): string {
    const hash: Uint8Array = sha1Sync(utf8Encode(key + WS_GUID));
    return new util.Base64Helper().encodeToStringSync(hash);
  }

  /* ----------------------------- 帧解析 ----------------------------- */

  private parseFrames(client: WsClient): void {
    while (true) {
      const buffer: Uint8Array = client.buffer;
      if (buffer.length < 2) {
        return;
      }
      const fin: boolean = (buffer[0] & 0x80) !== 0;
      const opcode: number = buffer[0] & 0x0f;
      const masked: boolean = (buffer[1] & 0x80) !== 0;
      let length: number = buffer[1] & 0x7f;
      let offset: number = 2;
      if (length === 126) {
        if (buffer.length < offset + 2) {
          return;
        }
        length = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;
      } else if (length === 127) {
        if (buffer.length < offset + 8) {
          return;
        }
        let big: number = 0;
        for (let i = 0; i < 8; i++) {
          big = big * 256 + buffer[offset + i];
        }
        length = big;
        offset += 8;
      }
      let maskKey: Uint8Array = new Uint8Array(0);
      if (masked) {
        if (buffer.length < offset + 4) {
          return;
        }
        maskKey = buffer.slice(offset, offset + 4);
        offset += 4;
      }
      if (buffer.length < offset + length) {
        return;
      }
      const payload: Uint8Array = buffer.slice(offset, offset + length);
      client.buffer = buffer.slice(offset + length);
      if (masked) {
        for (let i = 0; i < payload.length; i++) {
          payload[i] = payload[i] ^ maskKey[i % 4];
        }
      }
      this.handleFrame(client, fin, opcode, payload);
    }
  }

  private handleFrame(client: WsClient, fin: boolean, opcode: number, payload: Uint8Array): void {
    if (opcode === 0x8) {
      this.sendFrame(client, 0x8, new Uint8Array(0));
      this.closeClient(client);
      return;
    }
    if (opcode === 0x9) {
      this.sendFrame(client, 0xA, payload);
      return;
    }
    if (opcode === 0xA) {
      return;
    }
    if (opcode === 0x0) {
      client.fragment = concatBytes(client.fragment, payload);
      if (fin) {
        const message: Uint8Array = client.fragment;
        client.fragment = new Uint8Array(0);
        this.deliver(client, message);
      }
      return;
    }
    if (!fin) {
      client.fragmentOpcode = opcode;
      client.fragment = payload;
      return;
    }
    this.deliver(client, payload);
  }

  private deliver(client: WsClient, payload: Uint8Array): void {
    const text: string = utf8Decode(payload);
    if (text.length === 0) {
      return;
    }
    if (client.role === ClientRole.PROXY) {
      this.events.onProxyMessage(text);
    } else {
      this.events.onUiMessage(text);
    }
  }

  /* ------------------------------ 发送 ------------------------------ */

  private sendRaw(client: WsClient, text: string): void {
    client.conn.send({ data: utf8Encode(text).buffer as ArrayBuffer }).catch((err: Error) => {
      log.warn(`launcher send failed: ${err.message}`);
    });
  }

  private sendFrame(client: WsClient, opcode: number, payload: Uint8Array): void {
    client.conn.send({ data: buildFrame(opcode, payload).buffer as ArrayBuffer })
      .catch((err: Error) => {
        log.warn(`launcher send frame failed: ${err.message}`);
      });
  }

  /** 向 proxy 发送 JSON-RPC 文本 (二进制帧, 与 native 会话一致). */
  sendToProxy(text: string): boolean {
    const client: WsClient | undefined = this.proxyClient;
    if (client === undefined) {
      return false;
    }
    this.sendFrame(client, 0x2, utf8Encode(text));
    return true;
  }

  /** 向所有界面连接广播 JSON-RPC 通知. */
  broadcast(text: string): void {
    const payload: Uint8Array = utf8Encode(text);
    for (const client of this.clients) {
      if (client.role === ClientRole.UI) {
        this.sendFrame(client, 0x1, payload);
      }
    }
  }

  private closeClient(client: WsClient): void {
    this.handleClose(client);
    client.conn.close().catch(() => {
    });
  }

  async stop(): Promise<void> {
    if (this.closed) {
      return;
    }
    this.closed = true;
    for (const client of this.clients.slice()) {
      try {
        await client.conn.close();
      } catch (err) {
        // 忽略关闭异常.
      }
    }
    this.clients = [];
    this.proxyClient = undefined;
    const server = this.server;
    this.server = undefined;
    if (server !== undefined) {
      // TCPSocketServer 无显式关闭接口: 摘除回调后交由 GC 回收监听句柄.
      server.off('connect');
      server.off('error');
    }
  }
}

/* ------------------------------ 工具函数 ------------------------------ */

function buildFrame(opcode: number, payload: Uint8Array): Uint8Array {
  const length: number = payload.length;
  let headerSize: number = 2;
  if (length >= 65536) {
    headerSize = 10;
  } else if (length >= 126) {
    headerSize = 4;
  }
  const frame: Uint8Array = new Uint8Array(headerSize + length);
  frame[0] = 0x80 | opcode;
  if (length >= 65536) {
    frame[1] = 127;
    let value: number = length;
    for (let i = 9; i >= 2; i--) {
      frame[i] = value % 256;
      value = Math.floor(value / 256);
    }
  } else if (length >= 126) {
    frame[1] = 126;
    frame[2] = (length >> 8) & 0xff;
    frame[3] = length & 0xff;
  } else {
    frame[1] = length;
  }
  frame.set(payload, headerSize);
  return frame;
}

function concatBytes(a: Uint8Array, b: Uint8Array): Uint8Array {
  if (a.length === 0) {
    return b.slice();
  }
  if (b.length === 0) {
    return a;
  }
  const out: Uint8Array = new Uint8Array(a.length + b.length);
  out.set(a, 0);
  out.set(b, a.length);
  return out;
}

const UTF8_DECODER: util.TextDecoder = util.TextDecoder.create('utf-8');
const UTF8_ENCODER: util.TextEncoder = new util.TextEncoder();

function utf8Decode(data: Uint8Array): string {
  return UTF8_DECODER.decodeToString(data);
}

function utf8Encode(text: string): Uint8Array {
  return UTF8_ENCODER.encodeInto(text);
}

/** SHA-1 摘要 (同步实现, 用于 WebSocket 握手). */
function sha1Sync(data: Uint8Array): Uint8Array {
  const h: Uint32Array = new Uint32Array([0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0]);
  const ml: number = data.length * 8;
  const withOne: number = data.length + 1;
  const total: number = withOne + ((64 - ((withOne + 8) % 64)) % 64) + 8;
  const msg: Uint8Array = new Uint8Array(total);
  msg.set(data, 0);
  msg[data.length] = 0x80;
  for (let i = 0; i < 8; i++) {
    msg[total - 1 - i] = Math.floor(ml / Math.pow(2, 8 * i)) & 0xff;
  }
  const w: Uint32Array = new Uint32Array(80);
  for (let offset = 0; offset < total; offset += 64) {
    for (let i = 0; i < 16; i++) {
      w[i] = (msg[offset + i * 4] << 24) | (msg[offset + i * 4 + 1] << 16) |
        (msg[offset + i * 4 + 2] << 8) | msg[offset + i * 4 + 3];
    }
    for (let i = 16; i < 80; i++) {
      const v: number = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
      w[i] = (v << 1) | (v >>> 31);
    }
    let a: number = h[0];
    let b: number = h[1];
    let c: number = h[2];
    let d: number = h[3];
    let e: number = h[4];
    for (let i = 0; i < 80; i++) {
      let f: number = 0;
      let k: number = 0;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      const temp: number = (((a << 5) | (a >>> 27)) + f + e + k + w[i]) >>> 0;
      e = d;
      d = c;
      c = (b << 30) | (b >>> 2);
      b = a;
      a = temp;
    }
    h[0] = (h[0] + a) >>> 0;
    h[1] = (h[1] + b) >>> 0;
    h[2] = (h[2] + c) >>> 0;
    h[3] = (h[3] + d) >>> 0;
    h[4] = (h[4] + e) >>> 0;
  }
  const out: Uint8Array = new Uint8Array(20);
  for (let i = 0; i < 5; i++) {
    out[i * 4] = (h[i] >>> 24) & 0xff;
    out[i * 4 + 1] = (h[i] >>> 16) & 0xff;
    out[i * 4 + 2] = (h[i] >>> 8) & 0xff;
    out[i * 4 + 3] = h[i] & 0xff;
  }
  return out;
}
