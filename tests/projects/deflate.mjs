/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// The zlib format (RFC 1950) compressor of zlib (deflate.c, trees.c), levels
// 4 to 9, window 32 KB, memory level 8, default strategy: the same bytes as
// zlib.compress(data, level) of Python and compress2() of C. Node's own zlib
// (the Chromium fork) finds other matches, so its output differs for some data.
// The code follows the structure and names of zlib.

const MIN_MATCH = 3;
const MAX_MATCH = 258;
const MIN_LOOKAHEAD = MAX_MATCH + MIN_MATCH + 1;
const W_BITS = 15;
const W_SIZE = 1 << W_BITS;
const W_MASK = W_SIZE - 1;
const WINDOW_SIZE = 2 * W_SIZE;
const MAX_DIST = W_SIZE - MIN_LOOKAHEAD;
const MEM_LEVEL = 8;
const HASH_BITS = MEM_LEVEL + 7;
const HASH_SIZE = 1 << HASH_BITS;
const HASH_MASK = HASH_SIZE - 1;
const HASH_SHIFT = Math.floor((HASH_BITS + MIN_MATCH - 1) / MIN_MATCH);
const LIT_BUFSIZE = 1 << (MEM_LEVEL + 6);
const TOO_FAR = 4096;
const NIL = 0;

// [good_length, max_lazy, nice_length, max_chain] of the levels 4 .. 9
// (deflate_slow)
const CONFIGURATION_TABLE = {
    4: [4, 4, 16, 16],
    5: [8, 16, 32, 32],
    6: [8, 16, 128, 128],
    7: [8, 32, 128, 256],
    8: [32, 128, 258, 1024],
    9: [32, 258, 258, 4096],
};

////////////////////////////////////////////////////////////////////////////////
// trees.c

const LENGTH_CODES = 29;
const LITERALS = 256;
const L_CODES = LITERALS + 1 + LENGTH_CODES;
const D_CODES = 30;
const BL_CODES = 19;
const HEAP_SIZE = 2 * L_CODES + 1;
const MAX_BITS = 15;
const MAX_BL_BITS = 7;
const END_BLOCK = 256;
const REP_3_6 = 16;
const REPZ_3_10 = 17;
const REPZ_11_138 = 18;
const STORED_BLOCK = 0;
const STATIC_TREES = 1;
const DYN_TREES = 2;
const SMALLEST = 1;

const EXTRA_LBITS = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
const EXTRA_DBITS = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];
const EXTRA_BLBITS = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7];
const BL_ORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];

// a tree: fc is Freq or Code, dl is Dad or Len (the unions of ct_data)
function ctData(size) {
    return { fc: new Uint16Array(size), dl: new Uint16Array(size) };
}

function biReverse(code, len) {
    let res = 0;
    do {
        res |= code & 1;
        code >>>= 1;
        res <<= 1;
    } while (--len > 0);
    return res >>> 1;
}

function genCodes(tree, maxCode, blCount) {
    const nextCode = new Uint16Array(MAX_BITS + 1);
    let code = 0;
    for (let bits = 1; bits <= MAX_BITS; bits++) {
        code = (code + blCount[bits - 1]) << 1;
        nextCode[bits] = code;
    }
    for (let n = 0; n <= maxCode; n++) {
        const len = tree.dl[n];
        if (len === 0) {
            continue;
        }
        tree.fc[n] = biReverse(nextCode[len]++, len);
    }
}

const staticLtree = ctData(L_CODES + 2);
const staticDtree = ctData(D_CODES);
const distCode = new Uint8Array(512);
const lengthCode = new Uint8Array(MAX_MATCH - MIN_MATCH + 1);
const baseLength = new Int32Array(LENGTH_CODES);
const baseDist = new Int32Array(D_CODES);

(function trStaticInit() {
    let length = 0;
    let code;
    for (code = 0; code < LENGTH_CODES - 1; code++) {
        baseLength[code] = length;
        for (let n = 0; n < (1 << EXTRA_LBITS[code]); n++) {
            lengthCode[length++] = code;
        }
    }
    lengthCode[length - 1] = code;
    let dist = 0;
    for (code = 0; code < 16; code++) {
        baseDist[code] = dist;
        for (let n = 0; n < (1 << EXTRA_DBITS[code]); n++) {
            distCode[dist++] = code;
        }
    }
    dist >>= 7;
    for (; code < D_CODES; code++) {
        baseDist[code] = dist << 7;
        for (let n = 0; n < (1 << (EXTRA_DBITS[code] - 7)); n++) {
            distCode[256 + dist++] = code;
        }
    }
    const blCount = new Uint16Array(MAX_BITS + 1);
    let n = 0;
    while (n <= 143) {
        staticLtree.dl[n++] = 8;
        blCount[8]++;
    }
    while (n <= 255) {
        staticLtree.dl[n++] = 9;
        blCount[9]++;
    }
    while (n <= 279) {
        staticLtree.dl[n++] = 7;
        blCount[7]++;
    }
    while (n <= 287) {
        staticLtree.dl[n++] = 8;
        blCount[8]++;
    }
    genCodes(staticLtree, L_CODES + 1, blCount);
    for (n = 0; n < D_CODES; n++) {
        staticDtree.dl[n] = 5;
        staticDtree.fc[n] = biReverse(n, 5);
    }
})();

function dCode(dist) {
    return dist < 256 ? distCode[dist] : distCode[256 + (dist >>> 7)];
}

const STATIC_L_DESC = { staticTree: staticLtree, extraBits: EXTRA_LBITS, extraBase: LITERALS + 1, elems: L_CODES, maxLength: MAX_BITS };
const STATIC_D_DESC = { staticTree: staticDtree, extraBits: EXTRA_DBITS, extraBase: 0, elems: D_CODES, maxLength: MAX_BITS };
const STATIC_BL_DESC = { staticTree: null, extraBits: EXTRA_BLBITS, extraBase: 0, elems: BL_CODES, maxLength: MAX_BL_BITS };

////////////////////////////////////////////////////////////////////////////////
// the deflate state

class DeflateState {
    constructor(input, level) {
        const [goodLength, maxLazy, niceLength, maxChain] = CONFIGURATION_TABLE[level];
        this.level = level;
        this.goodMatch = goodLength;
        this.maxLazyMatch = maxLazy;
        this.niceMatch = niceLength;
        this.maxChainLength = maxChain;

        this.input = input;
        this.nextIn = 0;

        this.window = new Uint8Array(WINDOW_SIZE);
        this.prev = new Uint16Array(W_SIZE);
        this.head = new Uint16Array(HASH_SIZE);
        this.insH = 0;
        this.strstart = 0;
        this.blockStart = 0;
        this.lookahead = 0;
        this.insert = 0;
        this.matchLength = MIN_MATCH - 1;
        this.prevLength = MIN_MATCH - 1;
        this.matchAvailable = 0;
        this.matchStart = 0;
        this.prevMatch = 0;

        this.dynLtree = ctData(HEAP_SIZE);
        this.dynDtree = ctData(2 * D_CODES + 1);
        this.blTree = ctData(2 * BL_CODES + 1);
        this.lDesc = { dynTree: this.dynLtree, maxCode: 0, statDesc: STATIC_L_DESC };
        this.dDesc = { dynTree: this.dynDtree, maxCode: 0, statDesc: STATIC_D_DESC };
        this.blDesc = { dynTree: this.blTree, maxCode: 0, statDesc: STATIC_BL_DESC };
        this.blCount = new Uint16Array(MAX_BITS + 1);
        this.heap = new Int32Array(2 * L_CODES + 1);
        this.heapLen = 0;
        this.heapMax = 0;
        this.depth = new Uint8Array(2 * L_CODES + 1);
        this.symBuf = [];
        this.optLen = 0;
        this.staticLen = 0;

        this.out = [];
        this.biBuf = 0;
        this.biValid = 0;

        this.initBlock();
    }

    // output

    putByte(c) {
        this.out.push(c & 0xff);
    }

    putShort(w) {
        this.putByte(w & 0xff);
        this.putByte((w >>> 8) & 0xff);
    }

    putShortMSB(b) {
        this.putByte(b >>> 8);
        this.putByte(b & 0xff);
    }

    sendBits(value, length) {
        this.biBuf |= value << this.biValid;
        this.biValid += length;
        while (this.biValid >= 8) {
            this.putByte(this.biBuf);
            this.biBuf >>>= 8;
            this.biValid -= 8;
        }
    }

    sendCode(c, tree) {
        this.sendBits(tree.fc[c], tree.dl[c]);
    }

    biWindup() {
        if (this.biValid > 0) {
            this.putByte(this.biBuf);
        }
        this.biBuf = 0;
        this.biValid = 0;
    }

    // trees

    initBlock() {
        for (let n = 0; n < L_CODES; n++) {
            this.dynLtree.fc[n] = 0;
        }
        for (let n = 0; n < D_CODES; n++) {
            this.dynDtree.fc[n] = 0;
        }
        for (let n = 0; n < BL_CODES; n++) {
            this.blTree.fc[n] = 0;
        }
        this.dynLtree.fc[END_BLOCK] = 1;
        this.optLen = 0;
        this.staticLen = 0;
        this.symBuf = [];
    }

    smaller(tree, n, m) {
        return tree.fc[n] < tree.fc[m] || (tree.fc[n] === tree.fc[m] && this.depth[n] <= this.depth[m]);
    }

    pqdownheap(tree, k) {
        const v = this.heap[k];
        let j = k << 1;
        while (j <= this.heapLen) {
            if (j < this.heapLen && this.smaller(tree, this.heap[j + 1], this.heap[j])) {
                j++;
            }
            if (this.smaller(tree, v, this.heap[j])) {
                break;
            }
            this.heap[k] = this.heap[j];
            k = j;
            j <<= 1;
        }
        this.heap[k] = v;
    }

    genBitlen(desc) {
        const tree = desc.dynTree;
        const maxCode = desc.maxCode;
        const stree = desc.statDesc.staticTree;
        const extra = desc.statDesc.extraBits;
        const base = desc.statDesc.extraBase;
        const maxLength = desc.statDesc.maxLength;
        let h;
        let overflow = 0;

        this.blCount.fill(0);
        tree.dl[this.heap[this.heapMax]] = 0;

        for (h = this.heapMax + 1; h < HEAP_SIZE; h++) {
            const n = this.heap[h];
            let bits = tree.dl[tree.dl[n]] + 1;
            if (bits > maxLength) {
                bits = maxLength;
                overflow++;
            }
            tree.dl[n] = bits;
            if (n > maxCode) {
                continue;
            }
            this.blCount[bits]++;
            let xbits = 0;
            if (n >= base) {
                xbits = extra[n - base];
            }
            const f = tree.fc[n];
            this.optLen += f * (bits + xbits);
            if (stree) {
                this.staticLen += f * (stree.dl[n] + xbits);
            }
        }
        if (overflow === 0) {
            return;
        }
        do {
            let bits = maxLength - 1;
            while (this.blCount[bits] === 0) {
                bits--;
            }
            this.blCount[bits]--;
            this.blCount[bits + 1] += 2;
            this.blCount[maxLength]--;
            overflow -= 2;
        } while (overflow > 0);
        for (let bits = maxLength; bits !== 0; bits--) {
            let n = this.blCount[bits];
            while (n !== 0) {
                const m = this.heap[--h];
                if (m > maxCode) {
                    continue;
                }
                if (tree.dl[m] !== bits) {
                    this.optLen += (bits - tree.dl[m]) * tree.fc[m];
                    tree.dl[m] = bits;
                }
                n--;
            }
        }
    }

    buildTree(desc) {
        const tree = desc.dynTree;
        const stree = desc.statDesc.staticTree;
        const elems = desc.statDesc.elems;
        let maxCode = -1;
        let node;

        this.heapLen = 0;
        this.heapMax = HEAP_SIZE;
        for (let n = 0; n < elems; n++) {
            if (tree.fc[n] !== 0) {
                this.heap[++this.heapLen] = maxCode = n;
                this.depth[n] = 0;
            } else {
                tree.dl[n] = 0;
            }
        }
        while (this.heapLen < 2) {
            node = this.heap[++this.heapLen] = maxCode < 2 ? ++maxCode : 0;
            tree.fc[node] = 1;
            this.depth[node] = 0;
            this.optLen--;
            if (stree) {
                this.staticLen -= stree.dl[node];
            }
        }
        desc.maxCode = maxCode;
        for (let n = this.heapLen >> 1; n >= 1; n--) {
            this.pqdownheap(tree, n);
        }
        node = elems;
        do {
            const n = this.heap[SMALLEST];
            this.heap[SMALLEST] = this.heap[this.heapLen--];
            this.pqdownheap(tree, SMALLEST);
            const m = this.heap[SMALLEST];
            this.heap[--this.heapMax] = n;
            this.heap[--this.heapMax] = m;
            tree.fc[node] = tree.fc[n] + tree.fc[m];
            this.depth[node] = (this.depth[n] >= this.depth[m] ? this.depth[n] : this.depth[m]) + 1;
            tree.dl[n] = tree.dl[m] = node;
            this.heap[SMALLEST] = node++;
            this.pqdownheap(tree, SMALLEST);
        } while (this.heapLen >= 2);
        this.heap[--this.heapMax] = this.heap[SMALLEST];
        this.genBitlen(desc);
        genCodes(tree, maxCode, this.blCount);
    }

    scanTree(tree, maxCode) {
        let prevlen = -1;
        let nextlen = tree.dl[0];
        let count = 0;
        let maxCount = 7;
        let minCount = 4;
        if (nextlen === 0) {
            maxCount = 138;
            minCount = 3;
        }
        tree.dl[maxCode + 1] = 0xffff;
        for (let n = 0; n <= maxCode; n++) {
            const curlen = nextlen;
            nextlen = tree.dl[n + 1];
            if (++count < maxCount && curlen === nextlen) {
                continue;
            } else if (count < minCount) {
                this.blTree.fc[curlen] += count;
            } else if (curlen !== 0) {
                if (curlen !== prevlen) {
                    this.blTree.fc[curlen]++;
                }
                this.blTree.fc[REP_3_6]++;
            } else if (count <= 10) {
                this.blTree.fc[REPZ_3_10]++;
            } else {
                this.blTree.fc[REPZ_11_138]++;
            }
            count = 0;
            prevlen = curlen;
            if (nextlen === 0) {
                maxCount = 138;
                minCount = 3;
            } else if (curlen === nextlen) {
                maxCount = 6;
                minCount = 3;
            } else {
                maxCount = 7;
                minCount = 4;
            }
        }
    }

    sendTree(tree, maxCode) {
        let prevlen = -1;
        let nextlen = tree.dl[0];
        let count = 0;
        let maxCount = 7;
        let minCount = 4;
        if (nextlen === 0) {
            maxCount = 138;
            minCount = 3;
        }
        for (let n = 0; n <= maxCode; n++) {
            const curlen = nextlen;
            nextlen = tree.dl[n + 1];
            if (++count < maxCount && curlen === nextlen) {
                continue;
            } else if (count < minCount) {
                do {
                    this.sendCode(curlen, this.blTree);
                } while (--count !== 0);
            } else if (curlen !== 0) {
                if (curlen !== prevlen) {
                    this.sendCode(curlen, this.blTree);
                    count--;
                }
                this.sendCode(REP_3_6, this.blTree);
                this.sendBits(count - 3, 2);
            } else if (count <= 10) {
                this.sendCode(REPZ_3_10, this.blTree);
                this.sendBits(count - 3, 3);
            } else {
                this.sendCode(REPZ_11_138, this.blTree);
                this.sendBits(count - 11, 7);
            }
            count = 0;
            prevlen = curlen;
            if (nextlen === 0) {
                maxCount = 138;
                minCount = 3;
            } else if (curlen === nextlen) {
                maxCount = 6;
                minCount = 3;
            } else {
                maxCount = 7;
                minCount = 4;
            }
        }
    }

    buildBlTree() {
        this.scanTree(this.dynLtree, this.lDesc.maxCode);
        this.scanTree(this.dynDtree, this.dDesc.maxCode);
        this.buildTree(this.blDesc);
        let maxBlindex;
        for (maxBlindex = BL_CODES - 1; maxBlindex >= 3; maxBlindex--) {
            if (this.blTree.dl[BL_ORDER[maxBlindex]] !== 0) {
                break;
            }
        }
        this.optLen += 3 * (maxBlindex + 1) + 5 + 5 + 4;
        return maxBlindex;
    }

    sendAllTrees(lcodes, dcodes, blcodes) {
        this.sendBits(lcodes - 257, 5);
        this.sendBits(dcodes - 1, 5);
        this.sendBits(blcodes - 4, 4);
        for (let rank = 0; rank < blcodes; rank++) {
            this.sendBits(this.blTree.dl[BL_ORDER[rank]], 3);
        }
        this.sendTree(this.dynLtree, lcodes - 1);
        this.sendTree(this.dynDtree, dcodes - 1);
    }

    compressBlock(ltree, dtree) {
        for (const [distance, lc] of this.symBuf) {
            if (distance === 0) {
                this.sendCode(lc, ltree);
            } else {
                let code = lengthCode[lc];
                this.sendCode(code + LITERALS + 1, ltree);
                let extra = EXTRA_LBITS[code];
                if (extra !== 0) {
                    this.sendBits(lc - baseLength[code], extra);
                }
                const dist = distance - 1;
                code = dCode(dist);
                this.sendCode(code, dtree);
                extra = EXTRA_DBITS[code];
                if (extra !== 0) {
                    this.sendBits(dist - baseDist[code], extra);
                }
            }
        }
        this.sendCode(END_BLOCK, ltree);
    }

    trStoredBlock(buf, storedLen, last) {
        this.sendBits((STORED_BLOCK << 1) + last, 3);
        this.biWindup();
        this.putShort(storedLen);
        this.putShort(~storedLen & 0xffff);
        for (let i = 0; i < storedLen; i++) {
            this.putByte(this.window[buf + i]);
        }
    }

    trFlushBlock(buf, storedLen, last) {
        this.buildTree(this.lDesc);
        this.buildTree(this.dDesc);
        const maxBlindex = this.buildBlTree();
        let optLenb = (this.optLen + 3 + 7) >>> 3;
        const staticLenb = (this.staticLen + 3 + 7) >>> 3;
        if (staticLenb <= optLenb) {
            optLenb = staticLenb;
        }
        if (storedLen + 4 <= optLenb && buf !== null) {
            this.trStoredBlock(buf, storedLen, last);
        } else if (staticLenb === optLenb) {
            this.sendBits((STATIC_TREES << 1) + last, 3);
            this.compressBlock(staticLtree, staticDtree);
        } else {
            this.sendBits((DYN_TREES << 1) + last, 3);
            this.sendAllTrees(this.lDesc.maxCode + 1, this.dDesc.maxCode + 1, maxBlindex + 1);
            this.compressBlock(this.dynLtree, this.dynDtree);
        }
        this.initBlock();
        if (last) {
            this.biWindup();
        }
    }

    // a literal or a match; true when the block is full
    trTally(dist, lc) {
        this.symBuf.push([dist, lc]);
        if (dist === 0) {
            this.dynLtree.fc[lc]++;
        } else {
            dist--;
            this.dynLtree.fc[lengthCode[lc] + LITERALS + 1]++;
            this.dynDtree.fc[dCode(dist)]++;
        }
        return this.symBuf.length === LIT_BUFSIZE - 1;
    }

    flushBlock(last) {
        this.trFlushBlock(this.blockStart >= 0 ? this.blockStart : null, this.strstart - this.blockStart, last);
        this.blockStart = this.strstart;
    }

    // deflate.c

    updateHash(h, c) {
        return ((h << HASH_SHIFT) ^ c) & HASH_MASK;
    }

    insertString(str) {
        this.insH = this.updateHash(this.insH, this.window[str + (MIN_MATCH - 1)]);
        const matchHead = this.prev[str & W_MASK] = this.head[this.insH];
        this.head[this.insH] = str;
        return matchHead;
    }

    slideHash() {
        for (let n = 0; n < HASH_SIZE; n++) {
            const m = this.head[n];
            this.head[n] = m >= W_SIZE ? m - W_SIZE : NIL;
        }
        for (let n = 0; n < W_SIZE; n++) {
            const m = this.prev[n];
            this.prev[n] = m >= W_SIZE ? m - W_SIZE : NIL;
        }
    }

    readBuf(at, size) {
        const len = Math.min(this.input.length - this.nextIn, size);
        this.window.set(this.input.subarray(this.nextIn, this.nextIn + len), at);
        this.nextIn += len;
        return len;
    }

    fillWindow() {
        do {
            let more = WINDOW_SIZE - this.lookahead - this.strstart;
            if (this.strstart >= W_SIZE + MAX_DIST) {
                this.window.copyWithin(0, W_SIZE, W_SIZE + W_SIZE - more);
                this.matchStart -= W_SIZE;
                this.strstart -= W_SIZE;
                this.blockStart -= W_SIZE;
                if (this.insert > this.strstart) {
                    this.insert = this.strstart;
                }
                this.slideHash();
                more += W_SIZE;
            }
            if (this.nextIn === this.input.length) {
                break;
            }
            const n = this.readBuf(this.strstart + this.lookahead, more);
            this.lookahead += n;

            if (this.lookahead + this.insert >= MIN_MATCH) {
                let str = this.strstart - this.insert;
                this.insH = this.window[str];
                this.insH = this.updateHash(this.insH, this.window[str + 1]);
                while (this.insert) {
                    this.insH = this.updateHash(this.insH, this.window[str + MIN_MATCH - 1]);
                    this.prev[str & W_MASK] = this.head[this.insH];
                    this.head[this.insH] = str;
                    str++;
                    this.insert--;
                    if (this.lookahead + this.insert < MIN_MATCH) {
                        break;
                    }
                }
            }
        } while (this.lookahead < MIN_LOOKAHEAD && this.nextIn !== this.input.length);
    }

    longestMatch(curMatch) {
        const window = this.window;
        let chainLength = this.maxChainLength;
        let bestLen = this.prevLength;
        let niceMatch = this.niceMatch;
        const limit = this.strstart > MAX_DIST ? this.strstart - MAX_DIST : NIL;
        const strend = this.strstart + MAX_MATCH;
        let scanEnd1 = window[this.strstart + bestLen - 1];
        let scanEnd = window[this.strstart + bestLen];

        if (this.prevLength >= this.goodMatch) {
            chainLength >>>= 2;
        }
        if (niceMatch > this.lookahead) {
            niceMatch = this.lookahead;
        }

        do {
            const match = curMatch;
            if (window[match + bestLen] !== scanEnd ||
                window[match + bestLen - 1] !== scanEnd1 ||
                window[match] !== window[this.strstart] ||
                window[match + 1] !== window[this.strstart + 1]) {
                continue;
            }
            let scan = this.strstart + 2;
            let m = match + 2;
            for (;;) {
                let equal = true;
                for (let k = 0; k < 8; k++) {
                    if (window[++scan] !== window[++m]) {
                        equal = false;
                        break;
                    }
                }
                if (!equal || scan >= strend) {
                    break;
                }
            }
            const len = MAX_MATCH - (strend - scan);
            if (len > bestLen) {
                this.matchStart = curMatch;
                bestLen = len;
                if (len >= niceMatch) {
                    break;
                }
                scanEnd1 = window[this.strstart + bestLen - 1];
                scanEnd = window[this.strstart + bestLen];
            }
        } while ((curMatch = this.prev[curMatch & W_MASK]) > limit && --chainLength !== 0);

        if (bestLen <= this.lookahead) {
            return bestLen;
        }
        return this.lookahead;
    }

    // deflate_slow with Z_FINISH
    deflateSlow() {
        for (;;) {
            if (this.lookahead < MIN_LOOKAHEAD) {
                this.fillWindow();
                if (this.lookahead === 0) {
                    break;
                }
            }
            let hashHead = NIL;
            if (this.lookahead >= MIN_MATCH) {
                hashHead = this.insertString(this.strstart);
            }
            this.prevLength = this.matchLength;
            this.prevMatch = this.matchStart;
            this.matchLength = MIN_MATCH - 1;

            if (hashHead !== NIL && this.prevLength < this.maxLazyMatch && this.strstart - hashHead <= MAX_DIST) {
                this.matchLength = this.longestMatch(hashHead);
                if (this.matchLength <= 5 &&
                    this.matchLength === MIN_MATCH && this.strstart - this.matchStart > TOO_FAR) {
                    this.matchLength = MIN_MATCH - 1;
                }
            }
            if (this.prevLength >= MIN_MATCH && this.matchLength <= this.prevLength) {
                const maxInsert = this.strstart + this.lookahead - MIN_MATCH;
                const bflush = this.trTally(this.strstart - 1 - this.prevMatch, this.prevLength - MIN_MATCH);
                this.lookahead -= this.prevLength - 1;
                this.prevLength -= 2;
                do {
                    if (++this.strstart <= maxInsert) {
                        this.insertString(this.strstart);
                    }
                } while (--this.prevLength !== 0);
                this.matchAvailable = 0;
                this.matchLength = MIN_MATCH - 1;
                this.strstart++;
                if (bflush) {
                    this.flushBlock(0);
                }
            } else if (this.matchAvailable) {
                const bflush = this.trTally(0, this.window[this.strstart - 1]);
                if (bflush) {
                    this.flushBlock(0);
                }
                this.strstart++;
                this.lookahead--;
            } else {
                this.matchAvailable = 1;
                this.strstart++;
                this.lookahead--;
            }
        }
        if (this.matchAvailable) {
            this.trTally(0, this.window[this.strstart - 1]);
            this.matchAvailable = 0;
        }
        this.insert = this.strstart < MIN_MATCH - 1 ? this.strstart : MIN_MATCH - 1;
        this.flushBlock(1);
    }
}

function adler32(data) {
    let a = 1;
    let b = 0;
    for (const byte of data) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    return ((b << 16) | a) >>> 0;
}

// The zlib stream of data (a Buffer or Uint8Array) at the level 4 .. 9.
export function zlibCompress(data, level = 6) {
    if (!(level in CONFIGURATION_TABLE)) {
        throw new Error(`zlibCompress: level ${level} is not supported`);
    }
    const s = new DeflateState(data, level);
    const levelFlags = level === 6 ? 2 : level < 6 ? 1 : 3;
    let header = (8 + ((W_BITS - 8) << 4)) << 8;
    header |= levelFlags << 6;
    header += 31 - (header % 31);
    s.putShortMSB(header);
    s.deflateSlow();
    const adler = adler32(data);
    s.putShortMSB(adler >>> 16);
    s.putShortMSB(adler & 0xffff);
    return Buffer.from(s.out);
}

let crcTable = null;

// CRC-32 (the one of zlib and PNG) of data.
export function crc32(data) {
    if (!crcTable) {
        crcTable = new Uint32Array(256);
        for (let n = 0; n < 256; n++) {
            let c = n;
            for (let k = 0; k < 8; k++) {
                c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
            }
            crcTable[n] = c >>> 0;
        }
    }
    let crc = 0xffffffff;
    for (const byte of data) {
        crc = crcTable[(crc ^ byte) & 0xff] ^ (crc >>> 8);
    }
    return (crc ^ 0xffffffff) >>> 0;
}
