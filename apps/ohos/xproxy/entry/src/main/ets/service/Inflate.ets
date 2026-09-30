import { util } from '@kit.ArkTS';

/**
 * RFC1951 (raw deflate) 解压实现, 支持预置字典窗口, 用于解析 Android 端
 * `xproxy2:` 二维码 (deflate + 预共享字典) 与 `xproxy1:` (gzip) 二维码.
 *
 * 预置字典按 zlib 语义处理: 只取字典末尾 32KB 作为初始滑动窗口, 反引用
 * 距离可回退到字典范围内.
 */

const MAX_BITS: number = 15;

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
/** 动态 Huffman 的码长码表顺序. */
const CODE_LENGTH_ORDER: number[] = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];

/** 码长 1..15 的计数与符号表 (规范 Huffman 解码). */
class Huffman {
  counts: number[] = [];
  symbols: number[] = [];

  constructor(counts: number[], symbols: number[]) {
    this.counts = counts;
    this.symbols = symbols;
  }
}

/** LSB 优先的位读取器. */
class BitReader {
  private data: Uint8Array;
  private pos: number;
  private bitBuf: number = 0;
  private bitCount: number = 0;

  constructor(data: Uint8Array, pos: number) {
    this.data = data;
    this.pos = pos;
  }

  readBit(): number {
    if (this.bitCount === 0) {
      if (this.pos >= this.data.length) {
        throw new Error('deflate 数据提前结束');
      }
      this.bitBuf = this.data[this.pos++];
      this.bitCount = 8;
    }
    const bit: number = this.bitBuf & 1;
    this.bitBuf = this.bitBuf >>> 1;
    this.bitCount--;
    return bit;
  }

  readBits(count: number): number {
    let value: number = 0;
    for (let i = 0; i < count; i++) {
      value |= this.readBit() << i;
    }
    return value;
  }

  /** 丢弃当前字节内剩余比特 (stored 块边界对齐). */
  alignToByte(): void {
    this.bitBuf = 0;
    this.bitCount = 0;
  }

  readAlignedByte(): number {
    if (this.bitCount !== 0) {
      throw new Error('deflate 位对齐错误');
    }
    if (this.pos >= this.data.length) {
      throw new Error('deflate 数据提前结束');
    }
    return this.data[this.pos++];
  }
}

/** 可增长输出缓冲, 起始内容为预置字典 (作为初始滑动窗口). */
class ByteSink {
  private buf: Uint8Array;
  private len: number;
  private seed: number;

  constructor(seed: Uint8Array) {
    const capacity: number = seed.length + 65536;
    this.buf = new Uint8Array(capacity);
    this.buf.set(seed, 0);
    this.len = seed.length;
    this.seed = seed.length;
  }

  write(byte: number): void {
    if (this.len >= this.buf.length) {
      const grown: Uint8Array = new Uint8Array(this.buf.length * 2);
      grown.set(this.buf, 0);
      this.buf = grown;
    }
    this.buf[this.len++] = byte & 0xff;
  }

  /** 复制回引用 (距离可能落在字典窗口内, 逐字节复制以支持重叠). */
  copyBack(distance: number, length: number): void {
    let src: number = this.len - distance;
    if (src < 0) {
      throw new Error(`deflate 回引用距离非法: ${distance}`);
    }
    for (let i = 0; i < length; i++) {
      this.write(this.buf[src++]);
    }
  }

  /** 去掉字典前缀, 返回实际解压结果. */
  result(): Uint8Array {
    return this.buf.slice(this.seed, this.len);
  }
}

/** 构建规范 Huffman 解码表 (符号长度为 0 表示未使用). */
function buildHuffman(lengths: number[]): Huffman {
  const counts: number[] = new Array<number>(MAX_BITS + 1).fill(0);
  for (const length of lengths) {
    counts[length]++;
  }
  counts[0] = 0;
  const offsets: number[] = new Array<number>(MAX_BITS + 1).fill(0);
  let total: number = 0;
  for (let len = 1; len <= MAX_BITS; len++) {
    offsets[len] = total;
    total += counts[len];
  }
  const symbols: number[] = new Array<number>(total).fill(0);
  for (let symbol = 0; symbol < lengths.length; symbol++) {
    const len: number = lengths[symbol];
    if (len !== 0) {
      symbols[offsets[len]++] = symbol;
    }
  }
  return new Huffman(counts, symbols);
}

/** 解码一个 Huffman 符号. */
function decodeSymbol(reader: BitReader, table: Huffman): number {
  let code: number = 0;
  let first: number = 0;
  let index: number = 0;
  for (let len = 1; len <= MAX_BITS; len++) {
    code |= reader.readBit();
    const count: number = table.counts[len];
    if (code - first < count) {
      return table.symbols[index + (code - first)];
    }
    index += count;
    first = (first + count) << 1;
    code <<= 1;
  }
  throw new Error('deflate Huffman 码无效');
}

/** 固定 Huffman 表 (BFINAL/BTYPE=1). */
function fixedTables(): Huffman[] {
  const litLengths: number[] = new Array<number>(288).fill(0);
  for (let i = 0; i < 144; i++) {
    litLengths[i] = 8;
  }
  for (let i = 144; i < 256; i++) {
    litLengths[i] = 9;
  }
  for (let i = 256; i < 280; i++) {
    litLengths[i] = 7;
  }
  for (let i = 280; i < 288; i++) {
    litLengths[i] = 8;
  }
  const distLengths: number[] = new Array<number>(30).fill(5);
  return [buildHuffman(litLengths), buildHuffman(distLengths)];
}

/** 读取动态 Huffman 表 (BFINAL/BTYPE=2). */
function dynamicTables(reader: BitReader): Huffman[] {
  const hlit: number = reader.readBits(5) + 257;
  const hdist: number = reader.readBits(5) + 1;
  const hclen: number = reader.readBits(4) + 4;
  const codeLengths: number[] = new Array<number>(19).fill(0);
  for (let i = 0; i < hclen; i++) {
    codeLengths[CODE_LENGTH_ORDER[i]] = reader.readBits(3);
  }
  const codeLengthTable: Huffman = buildHuffman(codeLengths);

  const total: number = hlit + hdist;
  const lengths: number[] = new Array<number>(total).fill(0);
  let index: number = 0;
  while (index < total) {
    const symbol: number = decodeSymbol(reader, codeLengthTable);
    if (symbol < 16) {
      lengths[index++] = symbol;
      continue;
    }
    let repeat: number = 0;
    let value: number = 0;
    if (symbol === 16) {
      if (index === 0) {
        throw new Error('deflate 码长重复引用非法');
      }
      value = lengths[index - 1];
      repeat = 3 + reader.readBits(2);
    } else if (symbol === 17) {
      repeat = 3 + reader.readBits(3);
    } else {
      repeat = 11 + reader.readBits(7);
    }
    while (repeat > 0 && index < total) {
      lengths[index++] = value;
      repeat--;
    }
  }
  return [buildHuffman(lengths.slice(0, hlit)), buildHuffman(lengths.slice(hlit, total))];
}

/** 读取一个 stored 块 (BFINAL/BTYPE=0). */
function storedBlock(reader: BitReader, sink: ByteSink): void {
  reader.alignToByte();
  const len: number = reader.readAlignedByte() | (reader.readAlignedByte() << 8);
  const nlen: number = reader.readAlignedByte() | (reader.readAlignedByte() << 8);
  if (((len ^ 0xffff) & 0xffff) !== nlen) {
    throw new Error('deflate stored 块长度校验失败');
  }
  for (let i = 0; i < len; i++) {
    sink.write(reader.readAlignedByte());
  }
}

/** 解压一个 Huffman 块 (固定或动态). */
function huffmanBlock(reader: BitReader, sink: ByteSink, tables: Huffman[]): void {
  const literalTable: Huffman = tables[0];
  const distanceTable: Huffman = tables[1];
  while (true) {
    const symbol: number = decodeSymbol(reader, literalTable);
    if (symbol < 256) {
      sink.write(symbol);
      continue;
    }
    if (symbol === 256) {
      return;
    }
    const lengthIndex: number = symbol - 257;
    if (lengthIndex >= LENGTH_BASE.length) {
      throw new Error('deflate 长度码无效');
    }
    const length: number = LENGTH_BASE[lengthIndex] + reader.readBits(LENGTH_EXTRA[lengthIndex]);
    const distanceSymbol: number = decodeSymbol(reader, distanceTable);
    if (distanceSymbol >= DIST_BASE.length) {
      throw new Error('deflate 距离码无效');
    }
    const distance: number = DIST_BASE[distanceSymbol] + reader.readBits(DIST_EXTRA[distanceSymbol]);
    sink.copyBack(distance, length);
  }
}

/**
 * 解压 raw deflate 数据 (从 [offset] 开始, 遇到 BFINAL 块结束).
 *
 * [dict] 提供预置字典窗口 (可为空), 其末尾 32KB 作为初始历史数据.
 */
export function inflateRaw(data: Uint8Array, offset: number, dict: Uint8Array | undefined): Uint8Array {
  const seed: Uint8Array = dict === undefined
    ? new Uint8Array(0)
    : (dict.length > 32768 ? dict.slice(dict.length - 32768) : dict);
  const reader = new BitReader(data, offset);
  const sink = new ByteSink(seed);
  let final: boolean = false;
  while (!final) {
    final = reader.readBit() !== 0;
    const type: number = reader.readBits(2);
    if (type === 0) {
      storedBlock(reader, sink);
    } else if (type === 1) {
      huffmanBlock(reader, sink, fixedTables());
    } else if (type === 2) {
      huffmanBlock(reader, sink, dynamicTables(reader));
    } else {
      throw new Error('deflate 块类型无效');
    }
  }
  return sink.result();
}

/** zlib 流 (RFC1950) 解压; [dict] 为预置字典. */
export function zlibDecompress(data: Uint8Array, dict: Uint8Array | undefined): Uint8Array {
  if (data.length < 6) {
    throw new Error('zlib 数据过短');
  }
  const cmf: number = data[0];
  const flg: number = data[1];
  if ((cmf & 0x0f) !== 8) {
    throw new Error('zlib 压缩方法不支持');
  }
  if (((cmf << 8) | flg) % 31 !== 0) {
    throw new Error('zlib 头校验失败');
  }
  // FDICT 位存在时头部含 4 字节字典校验值 (DICTID), 直接跳过.
  const offset: number = (flg & 0x20) !== 0 ? 6 : 2;
  return inflateRaw(data, offset, dict);
}

/** gzip 流 (RFC1952) 解压 (旧版 xproxy1: 分享格式). */
export function gzipDecompress(data: Uint8Array): Uint8Array {
  if (data.length < 18 || data[0] !== 0x1f || data[1] !== 0x8b || data[2] !== 8) {
    throw new Error('gzip 头无效');
  }
  const flags: number = data[3];
  let offset: number = 10;
  if ((flags & 0x04) !== 0) {
    const extraLength: number = data[offset] | (data[offset + 1] << 8);
    offset += 2 + extraLength;
  }
  if ((flags & 0x08) !== 0) {
    while (offset < data.length && data[offset++] !== 0) {
    }
  }
  if ((flags & 0x10) !== 0) {
    while (offset < data.length && data[offset++] !== 0) {
    }
  }
  if ((flags & 0x02) !== 0) {
    offset += 2;
  }
  return inflateRaw(data, offset, undefined);
}

/** UTF-8 解码. */
export function utf8Decode(data: Uint8Array): string {
  return new util.TextDecoder('utf-8', { fatal: false }).decodeToString(data);
}
