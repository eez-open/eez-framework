#!/usr/bin/env node
/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Builds and runs the eez-framework tests (Linux / WSL).
//
//     node tests/run.mjs [--no-sanitize | --tsan] [--rebuild] [runner options] [filter...]
//
// --tsan builds with ThreadSanitizer instead of AddressSanitizer and
// UndefinedBehaviorSanitizer (for the data race tests, e.g. "tsan").
// Runner options: --list, --verbose, --no-fork, --timeout=S (harness/test.cpp).
// The build goes to ~/.cache/eez-framework-tests (EEZ_TESTS_BUILD_DIR), one
// folder per tests folder and build variant. EEZ_FW_SRC selects the framework
// src folder to test (default: ../src next to this folder).
//
// On Windows it runs itself in WSL (node of the WSL login shell).

import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);

function run(command, commandArgs, options = {}) {
    const result = spawnSync(command, commandArgs, { stdio: "inherit", ...options });
    if (result.error) {
        process.stderr.write(`${command}: ${result.error.message}\n`);
        process.exit(1);
    }
    return result.status ?? 1;
}

if (process.platform === "win32") {
    // the paths of these variables are translated to WSL paths
    const forwarded = "EEZ_FW_SRC/p:EEZ_TESTS_BUILD_DIR/p:EEZ_TESTS_UPDATE_GOLDEN";
    const env = { ...process.env, WSLENV: process.env.WSLENV ? `${process.env.WSLENV}:${forwarded}` : forwarded };
    const script = spawnSync("wsl", ["-e", "wslpath", "-a", fileURLToPath(import.meta.url)], { encoding: "utf8" });
    if (script.status !== 0) {
        process.stderr.write("the tests run in WSL, and WSL is not available\n");
        process.exit(1);
    }
    process.exit(run("wsl", ["-e", "bash", "-lc", 'exec node "$0" "$@"', script.stdout.trim(), ...args], { env }));
}

let sanitize = true;
let tsan = false;
let rebuild = false;
const runnerArgs = [];
for (const arg of args) {
    if (arg === "--no-sanitize") {
        sanitize = false;
    } else if (arg === "--tsan") {
        sanitize = false;
        tsan = true;
    } else if (arg === "--rebuild") {
        rebuild = true;
    } else {
        runnerArgs.push(arg);
    }
}

const variant = tsan ? "tsan" : sanitize ? "asan" : "plain";
// the same folder name as `echo "$HERE" | md5sum | cut -c1-8`
const hash = createHash("md5").update(`${HERE}\n`).digest("hex").slice(0, 8);
const buildRoot = process.env.EEZ_TESTS_BUILD_DIR || path.join(os.homedir(), ".cache", "eez-framework-tests");
const build = path.join(buildRoot, `${hash}-${variant}`);

if (rebuild) {
    fs.rmSync(build, { recursive: true, force: true });
}

const fwSrc = process.env.EEZ_FW_SRC || path.join(HERE, "..", "src");

const onOff = value => (value ? "ON" : "OFF");
if (
    run(
        "cmake",
        [
            "-S", HERE,
            "-B", build,
            "-DCMAKE_BUILD_TYPE=Debug",
            `-DEEZ_TESTS_SANITIZE=${onOff(sanitize)}`,
            `-DEEZ_TESTS_TSAN=${onOff(tsan)}`,
            `-DFW_SRC=${fwSrc}`,
        ],
        { stdio: ["inherit", "ignore", "inherit"] }
    ) !== 0
) {
    process.exit(1);
}
if (run("cmake", ["--build", build, `-j${os.availableParallelism()}`]) !== 0) {
    process.exit(1);
}

const env = { ...process.env };
// leaks are not checked yet (a test process ends with _exit)
env.ASAN_OPTIONS = "detect_leaks=0" + (env.ASAN_OPTIONS ? `:${env.ASAN_OPTIONS}` : "");
env.UBSAN_OPTIONS = "print_stacktrace=1" + (env.UBSAN_OPTIONS ? `:${env.UBSAN_OPTIONS}` : "");

const exe = path.join(build, "eez-framework-tests");
if (tsan) {
    // ThreadSanitizer fails with "unexpected memory mapping" with the high
    // mmap randomization of newer kernels (WSL): run without ASLR
    process.exit(run("setarch", [os.machine(), "-R", exe, ...runnerArgs], { env }));
}
process.exit(run(exe, runnerArgs, { env }));
