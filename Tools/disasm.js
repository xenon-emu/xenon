#!/run/current-system/sw/bin/env node
"use strict";

const fs = require("fs");

const CF_OPCODE = {
  0: "NOP", 1: "EXEC", 2: "EXEC_END", 3: "COND_EXEC",
  4: "COND_EXEC_END", 5: "COND_PRED_EXEC", 6: "COND_PRED_EXEC_END",
  7: "LOOP_START", 8: "LOOP_END", 9: "COND_CALL",
  10: "RETURN", 11: "COND_JMP", 12: "ALLOC",
  13: "COND_EXEC_PRED_CLEAN", 14: "COND_EXEC_PRED_CLEAN_END",
  15: "MARK_VS_FETCH_DONE",
};

const FETCH_OPCODE = {
  0: "VTX_FETCH",
  1: "TEX_FETCH",
};

const SCALAR_OPCODE = {
  0: "ADDs", 1: "ADD_PREVs", 2: "MULs", 25: "SUBs",
  48: "SIN", 49: "COS",
};

const VECTOR_OPCODE = {
  0: "ADDv",
  1: "MULv",
  2: "MAXv",
  3: "MINv",
  4: "SETEv",
  5: "SETGTv",
  6: "SETGTEv",
  7: "SETNEv",
  8: "FRACv",
  9: "TRUNCv",
  10: "FLOORv",
  11: "MULADDv",
  12: "CNDEv",
  13: "CNDGTEv",
  14: "CNDGTv",
  15: "DOT4v",
  16: "DOT3v",
  17: "DOT2ADDv",
  18: "CUBEv",
  19: "MAX4v",
  20: "PRED_SETE_PUSHv",
  21: "PRED_SETNE_PUSHv",
  22: "PRED_SETGT_PUSHv",
  23: "PRED_SETGTE_PUSHv",
  24: "KILLEv",
  25: "KILLGTv",
  26: "KILLGTEv",
  27: "KILLNEv",
  28: "DSTv",
  29: "MOVAv",
};

const DX9_VECTOR_MAP = {
  ADDv: "add",
  MULv: "mul",
  MAXv: "max",
  MINv: "min",
  DOT3v: "dp3",
  DOT4v: "dp4",
  MOVAv: "mov",
  MULADDv: "mad",
};

const DX9_SCALAR_MAP = {
  ADDs: "add",
  MULs: "mul",
  SUBs: "sub",
  SIN: "sin",
  COS: "cos",
};

function r(n) { return `r${n}`; }
function v(n) { return `v${n}`; }
function c(n) { return `c${n}`; }
function o(n) { return `o${n}`; }

function maskFromBits(bits) {
  const chans = ["x", "y", "z", "w"];
  let out = "";
  for (let i = 0; i < 4; i++) {
    if (bits & (1 << i)) out += chans[i];
  }
  return out.length ? "." + out : "";
}

function bits(v, lo, hi) {
  const width = hi - lo + 1;
  const mask = width === 32 ? 0xFFFFFFFF : ((1 << width) - 1);
  return (v >>> lo) & mask;
}

function readDwords(buf, endian) {
  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const out = new Uint32Array(buf.byteLength / 4);

  for (let i = 0; i < out.length; i++) {
    out[i] = dv.getUint32(i * 4, endian === "le");
  }

  return out;
}

function unpackCFPair(words, i) {
  const cfa = {
    d0: words[i],
    d1: words[i + 1] & 0xFFFF,
  };

  const cfb = {
    d0: (words[i + 1] >> 16) | (words[i + 2] << 16),
    d1: words[i + 2] >> 16,
  };

  return [decodeCF(cfa.d0, cfa.d1), decodeCF(cfb.d0, cfb.d1)];
}

function decodeCF(w0, w1) {
  const opc = bits(w1, 12, 15);

  return {
    opc,
    name: CF_OPCODE[opc] || `CF_${opc}`,
    address: bits(w0, 0, 11),
    count: bits(w0, 12, 14),
    serialize: bits(w0, 16, 27),
    pred: bits(w1, 10, 10),
  };
}

function decodeALU(w0, w1, w2) {
  const scalarOp = SCALAR_OPCODE[bits(w0, 26, 31)] || "S?";
  const vectorOp = VECTOR_OPCODE[bits(w2, 24, 28)] || "V?";

  const vdst = bits(w0, 0, 5);
  const sdst = bits(w0, 8, 13);
  const vmask = bits(w0, 16, 19);

  const src1_reg = bits(w2, 0, 7);
  const src2_reg = bits(w2, 8, 15);
  const src3_reg = bits(w2, 16, 23);

  const src1_sel = bits(w2, 31, 31);
  const src2_sel = bits(w2, 30, 30);
  const src3_sel = bits(w2, 29, 29);

  const vecMnemonic = DX9_VECTOR_MAP[vectorOp];
  const scaMnemonic = DX9_SCALAR_MAP[scalarOp];

  function src(sel, reg) {
    return sel ? c(reg) : r(reg);
  }

  return {
    vectorOp,
    scalarOp,
    vecMnemonic,
    scaMnemonic,
    vdst,
    sdst,
    vmask,
    src1: src(src1_sel, src1_reg),
    src2: src(src2_sel, src2_reg),
    src3: src(src3_sel, src3_reg),
  };
}

function decodeFETCH(w0, w1, w2) {
  const opc = bits(w0, 0, 4);
  const name = FETCH_OPCODE[opc] || `FETCH_${opc}`;

  const dst = bits(w0, 12, 17);
  const src = bits(w0, 5, 10);
  const constIdx = bits(w0, 20, 24);

  return {
    name,
    dst,
    src,
    constIdx,
  };
}

function disassembleShader(words) {
  let pc = 0;

  for (let i = 0; i + 2 < words.length; i += 3, pc += 2) {
    const [cfa, cfb] = unpackCFPair(words, i);

    dumpCF(cfa, pc);
    execBlock(cfa, words);

    dumpCF(cfb, pc);
    execBlock(cfb, words);

    if (cfa.name === "EXEC_END" || cfb.name === "EXEC_END")
      return;
  }
}

function dumpCF(cf, pc) {
  console.log(
    `  //CF ${pc.toString().padStart(4, "0")}: ${cf.name} addr=${cf.address} count=${cf.count}`
  );
}

function legalDXSrc(src, tempIndex) {
  if (src.startsWith("c")) {
    console.log(`    mov r${tempIndex}, ${src}`);
    return `r${tempIndex}`;
  }
  return src;
}

function emitHeader() {
  console.log("vs_3_0"); // or ps_3_0
  console.log("");
}

function execBlock(cf, words) {
  let seq = cf.serialize;

  for (let i = 0; i < cf.count; i++) {
    const slot = seq >> (i * 2);
    const isFetch = slot & 1;
    const base = (cf.address + i) * 3;

    if (isFetch) {
      const f = decodeFETCH(words[base], words[base + 1], words[base + 2]);

      if (f.name === "VTX_FETCH") {
        console.log(`    mov ${r(f.dst)}, ${v(f.src)}`);

        // For now: treat as r[dst] = r[src]; but you probably
        // want a proper vN -> rN mapping in IR.
        IR.push({ op: "mov", dst: f.dst, src1: f.src });

      } else if (f.name === "TEX_FETCH") {
        console.log(`    texld ${r(f.dst)}, ${r(f.src)}, s${f.constIdx}`);
        // TODO: add SPIR-V image/sampler mapping later
      } else {
        console.log(`    // unsupported fetch ${f.name}`);
      }
    } else {
      const a = decodeALU(words[base], words[base + 1], words[base + 2]);
      const mask = maskFromBits(a.vmask);

      if (a.vecMnemonic === "mad") {
        // assume src1, src2, src3 are register indices (you can adjust)
        const s1Index = parseInt(a.src1.slice(1));
        const s2Index = parseInt(a.src2.slice(1));
        const s3Index = parseInt(a.src3.slice(1));

        console.log(
          `    mad ${r(a.vdst)}${mask}, ${a.src1}, ${a.src2}, ${a.src3}`
        );

        IR.push({
          op: "mad",
          dst: a.vdst,
          src1: s1Index,
          src2: s2Index,
          src3: s3Index,
        });
      } else if (a.vecMnemonic === "mul") {
        const s1Index = parseInt(a.src1.slice(1));
        const s2Index = parseInt(a.src2.slice(1));

        console.log(
          `    mul ${r(a.vdst)}${mask}, ${a.src1}, ${a.src2}`
        );

        IR.push({
          op: "mul",
          dst: a.vdst,
          src1: s1Index,
          src2: s2Index,
        });
      } else if (a.vecMnemonic === "max") {
        const s1Index = parseInt(a.src1.slice(1));
        const s2Index = parseInt(a.src2.slice(1));

        console.log(
          `    max ${r(a.vdst)}${mask}, ${a.src1}, ${a.src2}`
        );

        IR.push({
          op: "max",
          dst: a.vdst,
          src1: s1Index,
          src2: s2Index,
        });
      } else if (a.scaMnemonic) {
        console.log(
          `    ${a.scaMnemonic} ${r(a.sdst)}, ${a.src3}`
        );
        // You can add scalar ops to IR as well
      } else {
        console.log(
          `    // unknown ALU v=${a.vectorOp} s=${a.scalarOp}`
        );
      }
    }
  }
}

if (process.argv.length < 3) {
  console.error("Usage: ./disasm.js <file> [be|le]");
  process.exit(1);
}

const FILE_PATH = process.argv[2];
const ENDIAN = process.argv[3] || "be";

const buffer = fs.readFileSync(FILE_PATH);
const dwords = readDwords(buffer, ENDIAN);

console.log(`Loaded ${dwords.length} dwords (${buffer.length} bytes)`);
console.log(`Endian: ${ENDIAN.toUpperCase()}`);
console.log("---------------------------------------------------");

const IR = []; // global-ish list of high-level instructions

disassembleShader(dwords);