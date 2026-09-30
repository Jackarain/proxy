/**
 * RFC1951 (raw deflate) 压缩实现, 使用固定 Huffman 码与预置字典窗口,
 * 产出的 zlib 流与 Android 端 `ZLibCodec(level, dictionary)` 解码端兼容.
 *
 * 压缩比略低于 zlib 的动态 Huffman 编码, 但实现简单且二维码体积足够小.
 */

const MIN_MATCH: number = 3;
const MAX_MATCH: number = 258;
const WINDOW_SIZE: number = 32768;
const HASH_BITS: number = 15;
const HASH_SIZE: number = 1 << HASH_BITS;
const HASH_MASK: number = HASH_SIZE - 1;
/** 单个位置的最大候选匹配数, 控制压缩耗时. */
const MAX_CHAIN: number = 256;

const LENGTH_BASE: number[] = [
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
  35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
];
const LENGTH_EXTRA: number[] = [
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
  3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
];
const DIST_BASE: number[] = [
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
  257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
];
const DIST_EXTRA: number[] = [
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
  7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
];

/** LSB 优先的位写入器. */
class BitWriter {
  private buf: Uint8Array;
  private len: number = 0;
  private bitBuf: number = 0;
  private bitCount: number = 0;

  constructor(hint: number) {
    this.buf = new Uint8Array(hint > 64 ? hint : 64);
  }

  writeBits(value: number, count: number): void {
    this.bitBuf |= (value << this.bitCount);
    this.bitCount += count;
    while (this.bitCount >= 8) {
      this.push(this.bitBuf & 0xff);
      this.bitBuf = this.bitBuf >>> 8;
      this.bitCount -= 8;
    }
  }

  /** 写入 Huffman 码 (码字按高位在前的顺序发送). */
  writeCode(code: number, bits: number): void {
    this.writeBits(reverseBits(code, bits), bits);
  }

  private push(byte: number): void {
    if (this.len >= this.buf.length) {
      const grown: Uint8Array = new Uint8Array(this.buf.length * 2);
      grown.set(this.buf, 0);
      this.buf = grown;
    }
    this.buf[this.len++] = byte;
  }

  finish(): Uint8Array {
    if (this.bitCount > 0) {
      this.push(this.bitBuf & 0xff);
      this.bitBuf = 0;
      this.bitCount = 0;
    }
    return this.buf.slice(0, this.len);
  }
}

/** 按位反转 (deflate Huffman 码的位序). */
function reverseBits(value: number, bits: number): number {
  let result: number = 0;
  let v: number = value;
  for (let i = 0; i < bits; i++) {
    result = (result << 1) | (v & 1);
    v = v >>> 1;
  }
  return result;
}

/** 固定 Huffman 字面量/长度码. */
function writeLiteralOrLength(writer: BitWriter, symbol: number): void {
  if (symbol < 144) {
    writer.writeCode(0x30 + symbol, 8);
  } else if (symbol < 256) {
    writer.writeCode(0x190 + (symbol - 144), 9);
  } else if (symbol < 280) {
    writer.writeCode(symbol - 256, 7);
  } else {
    writer.writeCode(0xC0 + (symbol - 280), 8);
  }
}

function lengthIndex(length: number): number {
  for (let i = LENGTH_BASE.length - 1; i >= 0; i--) {
    if (length >= LENGTH_BASE[i]) {
      return i;
    }
  }
  return 0;
}

function distanceIndex(distance: number): number {
  for (let i = DIST_BASE.length - 1; i >= 0; i--) {
    if (distance >= DIST_BASE[i]) {
      return i;
    }
  }
  return 0;
}

function hashAt(buf: Uint8Array, pos: number): number {
  return (((buf[pos] << 10) ^ (buf[pos + 1] << 5) ^ buf[pos + 2])) & HASH_MASK;
}

/** 匹配搜索结果. */
interface Match {
  length: number;
  distance: number;
}

/**
 * 在窗口内查找最长匹配. [limit] 为可匹配的最大长度 (不超过数据末尾).
 */
function findMatch(buf: Uint8Array, pos: number, limit: number, head: Int32Array,
  prev: Int32Array): Match {
  let best: Match = { length: 0, distance: 0 };
  if (pos + MIN_MATCH > buf.length) {
    return best;
  }
  const hash: number = hashAt(buf, pos);
  let candidate: number = head[hash];
  head[hash] = pos;
  if (pos + 1 < buf.length) {
    prev[pos] = candidate;
  }
  let chain: number = MAX_CHAIN;
  const minPos: number = pos > WINDOW_SIZE ? pos - WINDOW_SIZE : 0;
  while (candidate >= minPos && chain > 0) {
    if (buf[candidate] === buf[pos] &&
      buf[candidate + best.length] === buf[pos + best.length]) {
      let length: number = 0;
      while (length < limit && buf[candidate + length] === buf[pos + length]) {
        length++;
      }
      if (length > best.length) {
        best = { length: length, distance: pos - candidate };
        if (length >= limit) {
          break;
        }
      }
    }
    candidate = prev[candidate];
    chain--;
  }
  return best;
}

/** 把位置 [pos] 插入哈希链. */
function insertPosition(buf: Uint8Array, pos: number, head: Int32Array, prev: Int32Array): void {
  if (pos + MIN_MATCH > buf.length) {
    return;
  }
  const hash: number = hashAt(buf, pos);
  prev[pos] = head[hash];
  head[hash] = pos;
}

function adler32(data: Uint8Array): number {
  let a: number = 1;
  let b: number = 0;
  for (let i = 0; i < data.length; i++) {
    a = (a + data[i]) % 65521;
    b = (b + a) % 65521;
  }
  return ((b << 16) | a) >>> 0;
}

/** 取字典末尾 32KB 作为初始窗口. */
function windowTail(dict: Uint8Array | undefined): Uint8Array {
  if (dict === undefined || dict.length === 0) {
    return new Uint8Array(0);
  }
  return dict.length > WINDOW_SIZE ? dict.slice(dict.length - WINDOW_SIZE) : dict;
}

/**
 * 压缩为 zlib 流 (RFC1950), [dict] 为预置字典.
 *
 * 字典以 FDICT 标记写入流头, 并附带字典 adler32 校验值.
 */
export function zlibCompress(data: Uint8Array, dict: Uint8Array | undefined): Uint8Array {
  const seed: Uint8Array = windowTail(dict);
  const buf: Uint8Array = new Uint8Array(seed.length + data.length);
  buf.set(seed, 0);
  buf.set(data, seed.length);

  const head: Int32Array = new Int32Array(HASH_SIZE).fill(-1);
  const prev: Int32Array = new Int32Array(buf.length).fill(-1);
  for (let i = 0; i < seed.length; i++) {
    insertPosition(buf, i, head, prev);
  }

  const writer = new BitWriter(data.length + 64);
  writer.writeBits(1, 1); // BFINAL
  writer.writeBits(1, 2); // BTYPE = 固定 Huffman

  let pos: number = seed.length;
  const end: number = buf.length;
  while (pos < end) {
    const limit: number = Math.min(MAX_MATCH, end - pos);
    const match: Match = findMatch(buf, pos, limit, head, prev);
    if (match.length >= MIN_MATCH) {
      const lengthCode: number = lengthIndex(match.length);
      writeLiteralOrLength(writer, 257 + lengthCode);
      writer.writeBits(match.length - LENGTH_BASE[lengthCode], LENGTH_EXTRA[lengthCode]);
      const distCode: number = distanceIndex(match.distance);
      writer.writeCode(distCode, 5);
      writer.writeBits(match.distance - DIST_BASE[distCode], DIST_EXTRA[distCode]);
      for (let i = 1; i < match.length; i++) {
        insertPosition(buf, pos + i, head, prev);
      }
      pos += match.length;
    } else {
      writeLiteralOrLength(writer, buf[pos]);
      pos++;
    }
  }
  writeLiteralOrLength(writer, 256); // 块结束

  const body: Uint8Array = writer.finish();
  const hasDict: boolean = seed.length > 0;
  const cmf: number = 0x78;
  let flg: number = (2 << 6) | (hasDict ? 0x20 : 0);
  flg = (flg & 0xe0) | ((31 - (((cmf << 8) | (flg & 0xe0)) % 31)) % 31);

  const total: number = 2 + (hasDict ? 4 : 0) + body.length + 4;
  const out: Uint8Array = new Uint8Array(total);
  out[0] = cmf;
  out[1] = flg;
  let offset: number = 2;
  if (hasDict) {
    const dictId: number = adler32(dict as Uint8Array);
    out[offset++] = (dictId >>> 24) & 0xff;
    out[offset++] = (dictId >>> 16) & 0xff;
    out[offset++] = (dictId >>> 8) & 0xff;
    out[offset++] = dictId & 0xff;
  }
  out.set(body, offset);
  offset += body.length;
  const checksum: number = adler32(data);
  out[offset++] = (checksum >>> 24) & 0xff;
  out[offset++] = (checksum >>> 16) & 0xff;
  out[offset++] = (checksum >>> 8) & 0xff;
  out[offset] = checksum & 0xff;
  return out;
}
