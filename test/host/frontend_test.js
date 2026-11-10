const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const { Blob } = require("node:buffer");

function response(status, body = "") {
    return {
        ok: status >= 200 && status < 300,
        status,
        text: async () => body,
        json: async () => JSON.parse(body),
    };
}

function element(value = "") {
    return {
        value,
        textContent: "",
        disabled: false,
        children: [],
        classList: { toggle(name, enabled) { this[name] = enabled; } },
        replaceChildren() { this.children = []; },
        appendChild(child) { this.children.push(child); },
    };
}

async function exercise(path) {
    const html = fs.readFileSync(path, "utf8");
    const script = html.match(/<script>([\s\S]*?)<\/script>/);
    assert.ok(script, `Missing script in ${path}`);
    const elements = {
        summit: element(),
        refresh: element(),
        networks: element(),
        scan_status: element(),
        ssid: element("my network"),
        password: element("p&ss"),
        name: element("Remote SD"),
        status: element(),
    };
    const requests = [];
    let startResult = response(202, "Scanning");
    let scanResults = [
        response(202, "Scanning"),
        response(200, JSON.stringify(['Home "Wi-Fi"', "<test>"])),
    ];
    let saveResult = response(204);
    const context = {
        Blob,
        document: {
            getElementById: id => elements[id],
            createElement: () => element(),
        },
        fetch: async (url, options) => {
            requests.push({ url, options });
            if (url === "/scan") return startResult;
            if (url === "/refresh-ssid") return scanResults.shift() || response(202);
            return saveResult;
        },
        setTimeout: resolve => resolve(),
        encodeURIComponent,
    };
    vm.runInNewContext(script[1], context);
    await new Promise(resolve => setImmediate(resolve));
    assert.equal(requests.filter(request => request.url === "/scan").length, 1);
    assert.equal(requests.filter(request => request.url === "/refresh-ssid").length, 2);
    assert.deepEqual(elements.networks.children.map(child => child.value), ['Home "Wi-Fi"', "<test>"]);
    assert.match(elements.scan_status.textContent, /Choose/);
    assert.equal(elements.refresh.disabled, false);

    scanResults = [response(200, "[]")];
    await elements.refresh.onclick();
    assert.equal(elements.networks.children.length, 0);
    assert.match(elements.scan_status.textContent, /manually/);

    startResult = response(500, "Radio error");
    await elements.refresh.onclick();
    assert.match(elements.scan_status.textContent, /Radio error/);
    assert.equal(elements.scan_status.classList.error, true);
    assert.equal(elements.refresh.disabled, false);

    startResult = response(202);
    scanResults = [response(500, "Scan failed")];
    await elements.refresh.onclick();
    assert.match(elements.scan_status.textContent, /Scan failed/);
    scanResults = [];
    await elements.refresh.onclick();
    assert.match(elements.scan_status.textContent, /timed out/);
    assert.equal(elements.refresh.disabled, false);

    const savesBefore = () => requests.filter(request => request.url.startsWith("/?")).length;
    for (const invalid of ["", "é".repeat(33), "bad\nname"]) {
        elements.name.value = invalid;
        await elements.summit.onclick();
        assert.equal(savesBefore(), 0);
        assert.match(elements.status.textContent, /Device name/);
    }
    elements.name.value = "Printer";
    elements.ssid.value = "";
    await elements.summit.onclick();
    assert.match(elements.status.textContent, /SSID/);
    assert.equal(savesBefore(), 0);

    elements.ssid.value = "my network";
    saveResult = response(400, "Invalid SSID");
    await elements.summit.onclick();
    assert.match(elements.status.textContent, /Invalid SSID/);
    assert.equal(elements.summit.disabled, false);
    saveResult = response(500);
    await elements.summit.onclick();
    assert.match(elements.status.textContent, /HTTP 500/);
    assert.equal(elements.summit.disabled, false);

    saveResult = response(204);
    elements.name.value = 'Printer "A" & é';
    await elements.summit.onclick();
    const save = requests.filter(request => request.url.startsWith("/?")).pop();
    const query = new URLSearchParams(save.url.slice(2));
    assert.equal(query.get("name"), 'Printer "A" & é');
    assert.equal(query.get("ssid"), "my network");
    assert.equal(query.get("password"), "p&ss");
    assert.equal(save.options.method, "POST");
    assert.match(elements.status.textContent, /Device configured/);
    assert.equal(elements.refresh.disabled, true);
}

async function exerciseUpload(path) {
    const html = fs.readFileSync(path, "utf8");
    const script = html.match(/<script>([\s\S]*?)<\/script>/);
    const elements = {
        upload_button: element(),
        status: element(),
        file_input: { files: [] },
    };
    const requests = [];
    let result = response(500);
    let reloads = 0;
    const context = {
        document: { getElementById: id => elements[id] },
        location: { reload() { ++reloads; } },
        FormData: class {
            append(key, value) { this[key] = value; }
        },
        fetch: async (url, options) => {
            requests.push({ url, options });
            return result;
        },
        encodeURIComponent,
    };
    vm.runInNewContext(script[1], context);
    await elements.upload_button.onclick();
    assert.match(elements.status.textContent, /Choose/);
    assert.equal(requests.length, 0);
    elements.file_input.files = [{ name: "test.gcode" }];
    await elements.upload_button.onclick();
    assert.match(elements.status.textContent, /500/);
    assert.equal(elements.upload_button.disabled, false);
    assert.equal(reloads, 0);
    result = response(204);
    await elements.upload_button.onclick();
    assert.equal(requests[1].url, "/upload");
    assert.equal(requests[1].options.body.file.name, "test.gcode");
    assert.equal(reloads, 1);
    result = response(400);
    await context.del("a & b.gcode");
    assert.equal(requests[2].url, "/?file=a%20%26%20b.gcode");
    assert.match(elements.status.textContent, /Delete failed/);
    assert.equal(reloads, 1);
    result = response(204);
    await context.del("a & b.gcode");
    assert.equal(reloads, 2);
}

(async () => {
    await exercise("frontend/server-config.index.html");
    await exercise("lib/webserver/views/server-config.index.h");
    await exerciseUpload("frontend/upload-files.index.html");
    await exerciseUpload("lib/webserver/views/upload-files.index.h");
    const setup = fs.readFileSync("lib/webserver/views/server-config.index.h", "utf8");
    const upload = fs.readFileSync("lib/webserver/views/upload-files.index.h", "utf8");
    assert.equal(setup.match(/<style>([\s\S]*?)<\/style>/)[1],
                 upload.match(/<style>([\s\S]*?)<\/style>/)[1]);
    console.log("Frontend tests passed");
})().catch(error => {
    console.error(error);
    process.exitCode = 1;
});
