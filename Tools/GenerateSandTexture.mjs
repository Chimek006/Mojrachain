import { mkdirSync, existsSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { deflateSync } from "node:zlib";

const SIZE = 1024;
const output = resolve("Content", "Materials", "T_Sand_Dry.png");

function hashValue(x, y, seed) {
  let value = (x * 374761393 + y * 668265263 + seed * 1442695041) >>> 0;
  value ^= value >>> 13;
  value = Math.imul(value, 1274126177) >>> 0;
  value ^= value >>> 16;
  return value / 4294967295;
}

function smooth(value) {
  return value * value * (3 - 2 * value);
}

function tiledNoise(x, y, period, seed) {
  const cells = SIZE / period;
  const px = x / period;
  const py = y / period;
  const baseX = Math.floor(px);
  const baseY = Math.floor(py);
  const x0 = ((baseX % cells) + cells) % cells;
  const y0 = ((baseY % cells) + cells) % cells;
  const x1 = (x0 + 1) % cells;
  const y1 = (y0 + 1) % cells;
  const tx = smooth(px - baseX);
  const ty = smooth(py - baseY);
  const top = hashValue(x0, y0, seed) + (hashValue(x1, y0, seed) - hashValue(x0, y0, seed)) * tx;
  const bottom = hashValue(x0, y1, seed) + (hashValue(x1, y1, seed) - hashValue(x0, y1, seed)) * tx;
  return top + (bottom - top) * ty;
}

function clampByte(value) {
  return Math.max(0, Math.min(255, Math.round(value)));
}

function chunk(type, data) {
  const buffer = Buffer.alloc(12 + data.length);
  buffer.writeUInt32BE(data.length, 0);
  buffer.write(type, 4, 4, "ascii");
  data.copy(buffer, 8);
  buffer.writeUInt32BE(crc32(Buffer.concat([Buffer.from(type, "ascii"), data])), 8 + data.length);
  return buffer;
}

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

if (!existsSync(output)) {
  const rows = Buffer.alloc(SIZE * (1 + SIZE * 4));
  let offset = 0;
  for (let y = 0; y < SIZE; y += 1) {
    rows[offset++] = 0;
    for (let x = 0; x < SIZE; x += 1) {
      const broad = tiledNoise(x, y, 256, 11);
      const medium = tiledNoise(x, y, 64, 23);
      const fine = tiledNoise(x, y, 16, 37);
      const grain = hashValue(x, y, 71);
      const variation = (broad - 0.5) * 28 + (medium - 0.5) * 18;
      const grainDelta = grain > 0.72 ? (grain - 0.5) * 18 : 0;
      rows[offset++] = clampByte(204 + variation + (fine - 0.5) * 10 + grainDelta);
      rows[offset++] = clampByte(151 + variation * 0.78 + (fine - 0.5) * 8 + grainDelta * 0.72);
      rows[offset++] = clampByte(82 + variation * 0.42 + (fine - 0.5) * 6 + grainDelta * 0.30);
      rows[offset++] = 255;
    }
  }

  const header = Buffer.from("\x89PNG\r\n\x1a\n", "binary");
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(SIZE, 0);
  ihdr.writeUInt32BE(SIZE, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  const png = Buffer.concat([
    header,
    chunk("IHDR", ihdr),
    chunk("IDAT", deflateSync(rows, { level: 9 })),
    chunk("IEND", Buffer.alloc(0)),
  ]);
  mkdirSync(dirname(output), { recursive: true });
  writeFileSync(output, png);
  console.log(`Generated ${output}`);
} else {
  console.log(`Already exists: ${output}`);
}

