////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

const cp = require("child_process");
const path = require("path");

function getAdbPath() {
    return process.env.ANDROID_HOME
        ? path.resolve(process.env.ANDROID_HOME, "platform-tools/adb")
        : "adb";
}

function getEmulatorPath() {
    return process.env.ANDROID_HOME
        ? path.resolve(process.env.ANDROID_HOME, "tools/emulator")
        : "emulator";
}

const adb = {
    exec(args, returnStdOut = true, verbose = true) {
        const path = getAdbPath();
        if (verbose) {
            console.log(`Executing ${path} ${args.join(" ")}`);
        }
        return cp.execFileSync(
            path,
            args,
            returnStdOut ? { encoding: "utf8" } : { stdio: "inherit" },
        );
    },
    spawn(args) {
        const path = getAdbPath();
        console.log(`Spawning ${path} ${args.join(" ")}`);
        return cp.spawn(path, args, { stdio: "inherit" });
    },
    reverseServerPort(port) {
        adb.exec(["reverse", `tcp:${port}`, `tcp:${port}`], false);
    },
    devices() {
        const output = adb.exec(["devices"]).trim().split("\n");
        // Remove the "List of devices attached"
        const [intro, ...lines] = output;
        if (intro !== "List of devices attached") {
            throw new Error("Unexpected output from ADB");
        }
        // Return the list of devices
        return lines.map(line => {
            const [id, state] = line.split("\t");
            return { id, state };
        });
    },
    logcat(...args) {
        return adb.spawn(["logcat", ...args]);
    },
    shell(...args) {
        return adb.exec(["shell", ...args]);
    },
    shellPidOf(packageName) {
        return adb.exec(["shell", `pidof -s ${packageName}`], true, false).trim();
    }
};

const emulator = {
    exec(args, returnStdOut = true) {
        const path = getEmulatorPath();
        console.log(`Running ${path} ${args.join(" ")}`);
        return execSync(path, args, returnStdOut);
    },
    devices() {
        return emulator.exec(["-list-avds"]).trim().split("\n");
    },
    start(avd) {
        const path = getEmulatorPath();
        return cp.spawn(path, ["-avd", avd, "-verbose"]);
    },
};

module.exports = { adb, emulator };
