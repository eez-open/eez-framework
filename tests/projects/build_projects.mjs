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

// Generates the test projects (make_fwtest.mjs, make_fwtest_ext.mjs) and
// builds them with EEZ Studio (headless) into <project>/generated.
//
//     node tests/projects/build_projects.mjs [--studio PATH]
//
// --studio: the EEZ Studio folder, default C:\Work\eez\studio (EEZ_STUDIO).
// Studio must be compiled (build/ is up to date, e.g. by its tsc --watch).

import { spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { parseArgs } from "node:util";

const HERE = path.dirname(fileURLToPath(import.meta.url));

const { values: options } = parseArgs({
    options: {
        studio: { type: "string", default: process.env.EEZ_STUDIO || "C:\\Work\\eez\\studio" },
    },
});

function fail(message) {
    process.stderr.write(`${message}\n`);
    process.exit(1);
}

for (const generator of ["make_fwtest.mjs", "make_fwtest_ext.mjs"]) {
    const result = spawnSync(process.execPath, [path.join(HERE, generator)], { stdio: "inherit" });
    if (result.status !== 0) {
        fail(`${generator} failed (${result.status})`);
    }
}

// fwtest_ext (external assets) has fwtest as its master project
const projects = [path.join("fwtest", "fwtest.eez-project"), path.join("fwtest_ext", "fwtest_ext.eez-project")];

const electron = path.join(
    options.studio,
    "node_modules",
    "electron",
    "dist",
    process.platform === "win32" ? "electron.exe" : "electron"
);
if (!fs.existsSync(electron)) {
    fail(`${electron} not found (--studio)`);
}

// ELECTRON_RUN_AS_NODE is inherited from VS Code and makes electron run as node
const env = { ...process.env };
delete env.ELECTRON_RUN_AS_NODE;

for (const project of projects) {
    const projectPath = path.join(HERE, project);
    console.log(`Building ${projectPath}`);
    const result = spawnSync(electron, [".", "--build-project", projectPath], {
        cwd: options.studio,
        env,
        encoding: "utf8",
        maxBuffer: 64 * 1024 * 1024,
    });
    const output = `${result.stdout ?? ""}${result.stderr ?? ""}`;
    // Studio prints its result lines among Chromium cache warnings
    for (const line of output.split(/\r?\n/)) {
        if (line !== "" && !/disk_cache|gpu_disk_cache|cache_util_win/.test(line)) {
            console.log(line);
        }
    }
    if (result.status !== 0 || !output.includes("Build successfully finished")) {
        fail(`Studio build of ${project} failed`);
    }
}
