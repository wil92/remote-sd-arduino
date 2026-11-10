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
        replaceChildren(...children) { this.children = children; },
        appendChild(child) { this.children.push(child); },
        querySelectorAll(selector) {
            return this.children.flatMap(child =>
                child.tag === selector ? [child] : child.querySelectorAll(selector));
        },
        removeAttribute(name) { delete this[name]; },
        focus() {},
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
        upload_button: Object.assign(element(), { disabled: true }),
        status: element(),
        file_input: Object.assign(element(), { files: [] }),
        file_page: element(),
        file_list: element(),
        upload_overlay: element(),
        upload_progress: element(),
        upload_message: element(),
    };
    Object.defineProperty(elements.file_input, "value", {
        get() { return this.files.length ? "selected" : ""; },
        set(value) { if (value === "") this.files = []; },
    });
    const selectFile = () => {
        elements.file_input.files = [{ name: "test.gcode" }];
        elements.file_input.onchange();
        assert.equal(elements.upload_button.disabled, false);
    };
    const requests = [];
    let result = response(500);
    let pendingResponse;
    let reloads = 0;
    let xhr;
    let beforeUnload;
    let confirmResult = false;
    const confirmations = [];
    const context = {
        document: {
            getElementById: id => elements[id],
            createElement: tag => Object.assign(element(), { tag }),
        },
        window: {
            addEventListener(name, callback) {
                if (name === "beforeunload") beforeUnload = callback;
            },
            confirm(message) {
                confirmations.push(message);
                return confirmResult;
            },
        },
        location: { reload() { ++reloads; } },
        FormData: class {
            append(key, value) { this[key] = value; }
        },
        XMLHttpRequest: class {
            constructor() {
                xhr = this;
                this.events = {};
                this.upload = { addEventListener: (name, callback) => {
                    this.progress = callback;
                } };
            }
            open(method, url) { this.method = method; this.url = url; }
            addEventListener(name, callback) { this.events[name] = callback; }
            send(data) { this.data = data; }
            finish(status) { this.status = status; this.events.load(); }
        },
        fetch: async (url, options) => {
            requests.push({ url, options });
            if (pendingResponse) return pendingResponse;
            return result;
        },
        encodeURIComponent,
    };
    vm.runInNewContext(script[1], context);
    const uploadTag = html.match(/<button[^>]*\bid="?upload_button"?[^>]*>/);
    assert.ok(uploadTag);
    assert.match(uploadTag[0], /\bdisabled\b/);
    assert.equal(elements.upload_button.disabled, true);
    await elements.upload_button.onclick();
    assert.match(elements.status.textContent, /Choose/);
    assert.equal(requests.length, 0);
    selectFile();
    elements.file_input.files = [];
    elements.file_input.onchange();
    assert.equal(elements.upload_button.disabled, true);
    selectFile();
    let upload = elements.upload_button.onclick();
    assert.equal(xhr.url, "/upload");
    assert.equal(xhr.method, "POST");
    assert.equal(xhr.data.file.name, "test.gcode");
    assert.equal(elements.upload_overlay.hidden, false);
    assert.equal(elements.file_page.inert, true);
    assert.equal(elements.file_input.disabled, true);
    let prevented = false;
    const event = { preventDefault() { prevented = true; } };
    beforeUnload(event);
    assert.equal(prevented, true);
    assert.equal(event.returnValue, "");
    xhr.progress({ lengthComputable: true, loaded: 25, total: 100 });
    assert.equal(elements.upload_progress.value, 25);
    assert.match(elements.upload_message.textContent, /25%/);
    xhr.progress({ lengthComputable: true, loaded: 100, total: 100 });
    assert.match(elements.upload_message.textContent, /Waiting/);
    await context.del("ignored");
    assert.equal(requests.length, 0);
    assert.equal(confirmations.length, 0);
    xhr.finish(500);
    await upload;
    assert.match(elements.status.textContent, /500/);
    assert.equal(elements.file_input.files.length, 0);
    assert.equal(elements.upload_button.disabled, true);
    assert.equal(elements.upload_overlay.hidden, true);
    assert.equal(elements.file_page.inert, false);
    assert.equal(reloads, 0);
    assert.equal(requests.length, 0);
    result = response(200, JSON.stringify([{ name: 'a"<b>.gcode', size: 12 }]));
    let finishRefresh;
    pendingResponse = new Promise(resolve => { finishRefresh = resolve; });
    selectFile();
    upload = elements.upload_button.onclick();
    assert.equal(elements.upload_progress.value, undefined);
    xhr.finish(204);
    await new Promise(resolve => setImmediate(resolve));
    assert.equal(requests[0].url, "/api/files");
    assert.equal(elements.upload_overlay.hidden, false);
    assert.equal(elements.file_page.inert, true);
    await context.del("ignored during refresh");
    assert.equal(requests.length, 1);
    assert.equal(confirmations.length, 0);
    finishRefresh(result);
    await upload;
    pendingResponse = null;
    assert.equal(requests.length, 1);
    assert.equal(requests[0].url, "/api/files");
    assert.equal(elements.file_list.children[0].children[0].textContent, 'a"<b>.gcode - 12');
    assert.equal(elements.file_list.children[0].children[1].disabled, false);
    assert.equal(elements.file_input.files.length, 0);
    assert.equal(elements.upload_button.disabled, true);
    assert.equal(reloads, 0);
    prevented = false;
    beforeUnload({ preventDefault() { prevented = true; } });
    assert.equal(prevented, false);
    await elements.file_list.children[0].children[1].onclick();
    assert.equal(confirmations[0], 'Delete "a"<b>.gcode" from the SD card?');
    assert.equal(requests.length, 1);
    assert.equal(elements.file_page.inert, false);
    assert.equal(reloads, 0);
    result = response(500);
    selectFile();
    upload = elements.upload_button.onclick();
    xhr.finish(204);
    await upload;
    assert.match(elements.status.textContent, /Upload succeeded.*file list/);
    assert.equal(elements.upload_overlay.hidden, true);
    assert.equal(elements.file_input.files.length, 0);
    assert.equal(elements.upload_button.disabled, true);
    assert.equal(requests.length, 2);
    selectFile();
    upload = elements.upload_button.onclick();
    xhr.events.error();
    await upload;
    assert.match(elements.status.textContent, /Network error/);
    assert.equal(elements.file_input.files.length, 0);
    assert.equal(elements.upload_button.disabled, true);
    assert.equal(requests.length, 2);
    result = response(400);
    confirmResult = true;
    await context.del("a & b.gcode");
    assert.equal(confirmations[1], 'Delete "a & b.gcode" from the SD card?');
    assert.equal(requests[2].url, "/?file=a%20%26%20b.gcode");
    assert.match(elements.status.textContent, /Delete failed/);
    assert.equal(elements.upload_button.disabled, true);
    assert.equal(reloads, 0);
    result = response(204);
    await context.del("a & b.gcode");
    assert.equal(reloads, 1);
    assert.equal(requests.filter(request => request.url === "/api/files").length, 2);
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
