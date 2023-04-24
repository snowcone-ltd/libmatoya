// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.


// Global State

const MTY_IS_WORKER = typeof importScripts == 'function';

const MTY = {
	file: !MTY_IS_WORKER ? new URL(document.currentScript.src).pathname : '',

	wsObj: {},
	wsIndex: 1,

	cursorId: 0,
	threadId: 1,
	cursorCache: {},
	cursorClass: '',
	defaultCursor: false,
	synthesizeEsc: true,
	relative: false,
	gps: [false, false, false, false],
	posX: 0,
	posY: 0,
};


// Helpers

function mty_mem() {
	return MTY.memory.buffer;
}

function mty_mem_view() {
	return new DataView(mty_mem());
}

function mty_buf_to_js_str(buf) {
	return (new TextDecoder()).decode(buf);
}

function mty_b64_to_buf(str) {
	return Uint8Array.from(atob(str), c => c.charCodeAt(0))
}

function mty_buf_to_b64(buf) {
	let str = '';
	for (let x = 0; x < buf.length; x++)
		str += String.fromCharCode(buf[x]);

	return btoa(str);
}

function mty_copy_str(ptr, buf) {
	const heap = new Uint8Array(mty_mem(), ptr);
	heap.set(buf);
	heap[buf.length] = 0;
}

function mty_strlen(buf) {
	let len = 0;
	for (; len < 0x7FFFFFFF && buf[len] != 0; len++);

	return len;
}

function mty_wait(sync) {
	if (Atomics.compareExchange(sync, 0, 0, 1) == 0) {
		Atomics.wait(sync, 0, 1);

	} else {
		Atomics.store(sync, 0);
	}
}

function mty_signal(sync) {
	if (Atomics.compareExchange(sync, 0, 0, 1) == 0) {

	} else {
		Atomics.store(sync, 0);
		Atomics.notify(sync, 0);
	}
}


// Public helpers

function MTY_GetUint32(ptr) {
	return mty_mem_view().getUint32(ptr, true);
}

function MTY_SetUint32(ptr, value) {
	mty_mem_view().setUint32(ptr, value, true);
}

function MTY_SetUint16(ptr, value) {
	mty_mem_view().setUint16(ptr, value, true);
}

function MTY_SetInt32(ptr, value) {
	mty_mem_view().setInt32(ptr, value, true);
}

function MTY_GetUint8(ptr) {
	return mty_mem_view().getUint8(ptr);
}

function MTY_SetInt8(ptr, value) {
	mty_mem_view().setInt8(ptr, value);
}

function MTY_SetFloat(ptr, value) {
	mty_mem_view().setFloat32(ptr, value, true);
}

function MTY_SetUint64(ptr, value) {
	mty_mem_view().setBigUint64(ptr, BigInt(value), true);
}

function MTY_GetUint64(ptr, value) {
	return mty_mem_view().getBigUint64(ptr, true);
}

function MTY_GetInt32(ptr) {
	return mty_mem_view().getInt32(ptr, true);
}

function MTY_Memcpy(cptr, abuffer) {
	const heap = new Uint8Array(mty_mem(), cptr, abuffer.length);
	heap.set(abuffer);
}

function MTY_StrToJS(ptr) {
	const len = mty_strlen(new Uint8Array(mty_mem(), ptr));
	const slice = new Uint8Array(mty_mem(), ptr, len)

	const cpy = new Uint8Array(slice.byteLength);
	cpy.set(new Uint8Array(slice));

	return (new TextDecoder()).decode(cpy);
}

function MTY_StrToC(js_str, ptr, size) {
	if (size == 0)
		return;

	const buf = (new TextEncoder()).encode(js_str);
	const copy_size = buf.length < size ? buf.length : size - 1;
	mty_copy_str(ptr, new Uint8Array(buf, 0, copy_size));

	return ptr;
}


// Input

function mty_is_visible() {
	if (document.hidden != undefined) {
		return !document.hidden;

	} else if (document.webkitHidden != undefined) {
		return !document.webkitHidden;
	}

	return true;
}

function mty_scaled(num) {
	return Math.round(num * window.devicePixelRatio);
}

function mty_run_action() {
	setTimeout(() => {
		if (MTY.action) {
			MTY.action();
			MTY.action = null;
		}
	}, 100);
}

function mty_set_action(action) {
	MTY.action = action;

	// In case click handler doesn't happen
	mty_run_action();
}

function mty_correct_relative() {
	if (!document.pointerLockElement && MTY.relative)
		MTY.canvas.requestPointerLock();
}

function mty_get_mods(ev) {
	let mods = 0;

	if (ev.shiftKey) mods |= 0x01;
	if (ev.ctrlKey)  mods |= 0x02;
	if (ev.altKey)   mods |= 0x04;
	if (ev.metaKey)  mods |= 0x08;

	if (ev.getModifierState("CapsLock")) mods |= 0x10;
	if (ev.getModifierState("NumLock") ) mods |= 0x20;

	return mods;
}

function mty_add_input_events(thread) {
	MTY.canvas.addEventListener('mousemove', (ev) => {
		let x = mty_scaled(ev.clientX);
		let y = mty_scaled(ev.clientY);

		if (MTY.relative) {
			x = ev.movementX;
			y = ev.movementY;
		}

		thread.postMessage({
			type: 'motion',
			relative: MTY.relative,
			x: x,
			y: y,
		});
	});

	document.addEventListener('pointerlockchange', (ev) => {
		// Left relative via the ESC key, which swallows a natural ESC keypress
		if (!document.pointerLockElement && MTY.synthesizeEsc) {
			const msg = {
				type: 'key',
				pressed: true,
				code: 'Escape',
				key: 'Escape',
				mods: 0,
			};

			thread.postMessage(msg);

			msg.pressed = false;
			thread.postMessage(msg);
		}

		MTY.synthesizeEsc = true;
	});

	window.addEventListener('click', (ev) => {
		// Popup blockers can interfere with window.open if not called from within the 'click' listener
		mty_run_action();
		ev.preventDefault();
	});

	window.addEventListener('mousedown', (ev) => {
		mty_correct_relative();
		ev.preventDefault();

		thread.postMessage({
			type: 'button',
			pressed: true,
			button: ev.button,
			x: mty_scaled(ev.clientX),
			y: mty_scaled(ev.clientY),
		});
	});

	window.addEventListener('mouseup', (ev) => {
		ev.preventDefault();

		thread.postMessage({
			type: 'button',
			pressed: false,
			button: ev.button,
			x: mty_scaled(ev.clientX),
			y: mty_scaled(ev.clientY),
		});
	});

	MTY.canvas.addEventListener('contextmenu', (ev) => {
		ev.preventDefault();
	});

	MTY.canvas.addEventListener('dragover', (ev) => {
		ev.preventDefault();
	});

	MTY.canvas.addEventListener('wheel', (ev) => {
		let x = ev.deltaX > 0 ? 120 : ev.deltaX < 0 ? -120 : 0;
		let y = ev.deltaY > 0 ? 120 : ev.deltaY < 0 ? -120 : 0;

		thread.postMessage({
			type: 'wheel',
			x: x,
			y: y,
		});
	}, {passive: true});

	window.addEventListener('keydown', (ev) => {
		mty_correct_relative();

		thread.postMessage({
			type: 'key',
			pressed: true,
			code: ev.code,
			key: ev.key,
			mods: mty_get_mods(ev),
		});

		if (MTY.kbGrab)
			ev.preventDefault();
	});

	window.addEventListener('keyup', (ev) => {
		thread.postMessage({
			type: 'key',
			pressed: false,
			code: ev.code,
			key: '',
			mods: mty_get_mods(ev),
		});

		if (MTY.kbGrab)
			ev.preventDefault();
	});

	window.addEventListener('blur', (ev) => {
		thread.postMessage({
			type: 'focus',
			focus: false,
		});
	});

	window.addEventListener('focus', (ev) => {
		thread.postMessage({
			type: 'focus',
			focus: true,
		});
	});

	window.addEventListener('resize', (ev) => {
		const rect = MTY.canvas.getBoundingClientRect();

		thread.postMessage({
			type: 'resize',
			width: mty_scaled(rect.width),
			height: mty_scaled(rect.height),
		});
	});

	MTY.canvas.addEventListener('drop', (ev) => {
		ev.preventDefault();

		if (!ev.dataTransfer.items)
			return;

		for (let x = 0; x < ev.dataTransfer.items.length; x++) {
			if (ev.dataTransfer.items[x].kind == 'file') {
				let file = ev.dataTransfer.items[x].getAsFile();

				const reader = new FileReader();
				reader.addEventListener('loadend', (fev) => {
					if (reader.readyState == 2) {
						thread.postMessage({
							type: 'drop',
							name: file.name,
							data: reader.result,
						}, [reader.result]);
					}
				});

				reader.readAsArrayBuffer(file);
				break;
			}
		}
	});
}


// Gamepads

function mty_poll_gamepads() {
	const gps = navigator.getGamepads();

	for (let x = 0; x < 4; x++) {
		const gp = gps[x];

		if (gp) {
			let state = 0;

			// Connected
			if (!MTY.gps[x]) {
				MTY.gps[x] = true;
				state = 1;
			}

			let lx = 0;
			let ly = 0;
			let rx = 0;
			let ry = 0;
			let lt = 0;
			let rt = 0;
			let buttons = 0;

			if (gp.buttons) {
				if (gp.buttons[6]) lt = gp.buttons[6].value;
				if (gp.buttons[7]) rt = gp.buttons[7].value;

				for (let i = 0; i < gp.buttons.length && i < 32; i++)
					if (gp.buttons[i].pressed)
						buttons |= 1 << i;
			}

			if (gp.axes) {
				if (gp.axes[0]) lx = gp.axes[0];
				if (gp.axes[1]) ly = gp.axes[1];
				if (gp.axes[2]) rx = gp.axes[2];
				if (gp.axes[3]) ry = gp.axes[3];
			}

			thread.postMessage({
				type: 'gamepad',
				id: x,
				state: state,
				buttons: buttons,
				lx: lx,
				ly: ly,
				rx: rx,
				ry: ry,
				lt: lt,
				rt: rt,
			});

		// Disconnected
		} else if (MTY.gps[x]) {
			thread.postMessage({
				type: 'disconnect',
				id: x,
				state: 2,
			});

			MTY.gps[x] = false;
		}
	}
}


// Web

function mty_alert(title, msg) {
	window.alert(MTY_StrToJS(title) + '\n\n' + MTY_StrToJS(msg));
}

function mty_set_fullscreen(fullscreen) {
	if (fullscreen && !document.fullscreenElement) {
		if (navigator.keyboard)
			navigator.keyboard.lock(["Escape"]);

		document.documentElement.requestFullscreen();

	} else if (!fullscreen && document.fullscreenElement) {
		document.exitFullscreen();

		if (navigator.keyboard)
			navigator.keyboard.unlock();
	}
}

async function mty_wake_lock(enable) {
	try {
		if (enable && !MTY.wakeLock) {
			MTY.wakeLock = await navigator.wakeLock.request('screen');

		} else if (!enable && MTY.wakeLock) {
			MTY.wakeLock.release();
			MTY.wakeLock = undefined;
		}
	} catch (e) {
		MTY.wakeLock = undefined;
	}
}

function mty_rumble_gamepad(id, low, high) {
	const gps = navigator.getGamepads();
	const gp = gps[id];

	if (gp && gp.vibrationActuator)
		gp.vibrationActuator.playEffect('dual-rumble', {
			startDelay: 0,
			duration: 2000,
			weakMagnitude: low,
			strongMagnitude: high,
		});
}

function mty_show_cursor(show) {
	MTY.canvas.style.cursor = show ? '': 'none';
}

function mty_set_pointer_lock(enable) {
	if (enable && !document.pointerLockElement) {
		MTY.canvas.requestPointerLock();

	} else if (!enable && document.pointerLockElement) {
		MTY.synthesizeEsc = false;
		document.exitPointerLock();
	}

	MTY.relative = enable;
}

function mty_use_default_cursor(use_default) {
	if (MTY.cursorClass.length > 0) {
		if (use_default) {
			MTY.canvas.classList.remove(MTY.cursorClass);

		} else {
			MTY.canvas.classList.add(MTY.cursorClass);
		}
	}

	MTY.defaultCursor = use_default;
}

function mty_set_png_cursor(buffer, size, hot_x, hot_y) {
	if (buffer) {
		const buf = new Uint8Array(mty_mem(), buffer, size);
		const b64_png = mty_buf_to_b64(buf);

		if (!MTY.cursorCache[b64_png]) {
			MTY.cursorCache[b64_png] = `cursor-x-${MTY.cursorId}`;

			const style = document.createElement('style');
			style.type = 'text/css';
			style.innerHTML = `.cursor-x-${MTY.cursorId++} ` +
				`{cursor: url(data:image/png;base64,${b64_png}) ${hot_x} ${hot_y}, auto;}`;
			document.querySelector('head').appendChild(style);
		}

		if (MTY.cursorClass.length > 0)
			MTY.canvas.classList.remove(MTY.cursorClass);

		MTY.cursorClass = MTY.cursorCache[b64_png];

		if (!MTY.defaultCursor)
			MTY.canvas.classList.add(MTY.cursorClass);

	} else {
		if (!MTY.defaultCursor && MTY.cursorClass.length > 0)
			MTY.canvas.classList.remove(MTY.cursorClass);

		MTY.cursorClass = '';
	}
}


// Image

async function mty_decode_image(input) {
	const img = new Image();
	img.src = URL.createObjectURL(new Blob([input]));

	await img.decode();

	const width = img.naturalWidth;
	const height = img.naturalHeight;

	const canvas = new OffscreenCanvas(width, height);
	const ctx = canvas.getContext('2d');
	ctx.drawImage(img, 0, 0, width, height);

	return ctx.getImageData(0, 0, width, height);
}


// Net

function mty_ws_new(obj) {
	MTY.wsObj[MTY.wsIndex] = obj;

	return MTY.wsIndex++;
}

function mty_ws_del(index) {
	let obj = MTY.wsObj[index];

	MTY.wsObj[index] = undefined;
	delete MTY.wsObj[index];

	return obj;
}

function mty_ws_obj(index) {
	return MTY.wsObj[index];
}

async function mty_http_request(url, method, headers, body, buf) {
	let error = false
	let size = 0;
	let status = 0;
	let data = null;

	try {
		const response = await fetch(url, {
			method: method,
			headers: headers,
			body: body
		});

		const res_ab = await response.arrayBuffer();
		data = new Uint8Array(res_ab);

		status = response.status;
		size = data.byteLength;

	} catch (err) {
		console.error(err);
		error = true;
	}

	return {
		data,
		error,
		size,
		status,
	};
}

async function mty_ws_connect(url) {
	return new Promise((resolve, reject) => {
		const ws = new WebSocket(url);
		const sab = new SharedArrayBuffer(4);
		ws.sync = new Int32Array(sab, 0, 1);
		ws.closeCode = 0;
		ws.msgs = [];

		ws.onclose = (evt) => {
			ws.closeCode = evt.code == 1005 ? 1000 : evt.code;
			resolve(null);
		};

		ws.onerror = (err) => {
			console.error(err);
			resolve(null);
		};

		ws.onopen = () => {
			resolve(ws);
		};

		ws.onmessage = (evt) => {
			ws.msgs.push(evt.data);
			Atomics.notify(ws.sync, 0, 1);
		};
	});
}

async function mty_ws_read(ws, timeout) {
	let msg = ws.msgs.shift()

	if (!msg) {
		const r0 = Atomics.waitAsync(ws.sync, 0, 0, timeout);
		const r1 = await r0.value;

		if (r1 != 'timed-out')
			msg = ws.msgs.shift()
	}

	return msg ? new TextEncoder().encode(msg) : null;
}


// Entry

function mty_supports_wasm() {
	try {
		if (typeof WebAssembly == 'object' && typeof WebAssembly.instantiate == 'function') {
			const module = new WebAssembly.Module(Uint8Array.of(0x0, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00));

			if (module instanceof WebAssembly.Module)
				return new WebAssembly.Instance(module) instanceof WebAssembly.Instance;
		}
	} catch (e) {}

	return false;
}

function mty_supports_web_gl() {
	try {
		return document.createElement('canvas').getContext('webgl');
	} catch (e) {}

	return false;
}

function mty_window_info() {
	const rect = MTY.canvas.getBoundingClientRect();

	return {
		posX: window.screenX,
		posY: window.screenY,
		relative: MTY.relative,
		devicePixelRatio: window.devicePixelRatio,
		hasFocus: document.hasFocus(),
		screenWidth: screen.width,
		screenHeight: screen.height,
		fullscreen: document.fullscreenElement != null,
		visible: mty_is_visible(),
		canvasWidth: mty_scaled(rect.width),
		canvasHeight: mty_scaled(rect.height),
	};
}

function mty_update_interval(thread) {
	// Poll gamepads
	if (document.hasFocus())
		mty_poll_gamepads();

	// Poll position changes
	if (MTY.posX != window.screenX || MTY.posY != window.screenY) {
		MTY.posX = window.screenX;
		MTY.posY = window.screenY;

		thread.postMessage({
			type: 'move',
		});
	}

	// send rect event
	thread.postMessage({
		type: 'window-update',
		windowInfo: mty_window_info(),
	});
}

function mty_thread_start(threadId, bin, baseFile, wasmBuf, memory, startArg, userEnv, kbMap, glver, name) {
	const worker = new Worker(baseFile.replace('.js', '-worker.js'), {name: name});

	worker.onmessage = mty_thread_message;

	worker.postMessage({
		type: 'init',
		file: baseFile,
		bin: bin,
		wasmBuf: wasmBuf,
		glver: glver,
		windowInfo: mty_window_info(),
		args: window.location.search,
		hostname: window.location.hostname,
		userEnv: userEnv ? Object.keys(userEnv) : [],
		kbMap: kbMap,
		startArg: startArg,
		threadId: threadId,
		memory: memory,
	});

	return worker;
}

async function MTY_Start(bin, userEnv, glver) {
	if (!mty_supports_wasm() || !mty_supports_web_gl())
		return false;

	MTY.bin = bin;
	MTY.userEnv = userEnv;
	MTY.glver = glver;

	// Canvas container
	const html = document.querySelector('html');
	html.style.width = '100%';
	html.style.height = '100%';
	html.style.margin = 0;

	const body = document.querySelector('body');
	body.style.width = '100%';
	body.style.height = '100%';
	body.style.background = 'black';
	body.style.overflow = 'hidden';
	body.style.margin = 0;

	// Drawing surface
	MTY.canvas = document.createElement('canvas');
	MTY.renderer = MTY.canvas.getContext('bitmaprenderer');
	MTY.canvas.style.width = '100%';
	MTY.canvas.style.height = '100%';
	document.body.appendChild(MTY.canvas);

	// WASM binary
	const wasmRes = await fetch(bin);
	MTY.wasmBuf = await wasmRes.arrayBuffer();

	// Shared global memory
	MTY.memory = new WebAssembly.Memory({
		initial: 512,   // 32 MB
		maximum: 16384, // 1 GB
		shared: true,
	});

	// Load keyboard map
	MTY.kbMap = {};
	if (navigator.keyboard) {
		const layout = await navigator.keyboard.getLayoutMap();

		layout.forEach((currentValue, index) => {
			MTY.kbMap[index] = currentValue;
		});
	}

	// Main thread
	MTY.mainThread = mty_thread_start(MTY.threadId, bin, MTY.file, MTY.wasmBuf, MTY.memory,
		0, userEnv, MTY.kbMap, MTY.glver, 'main');

	// Init position, update loop
	MTY.posX = window.screenX;
	MTY.posY = window.screenY;
	setInterval(() => {
		mty_update_interval(MTY.mainThread);
	}, 10);

	// Add input events
	mty_add_input_events(MTY.mainThread);

	return true;
}

async function mty_thread_message(ev) {
	const msg = ev.data;

	switch (msg.type) {
		case 'user-env':
			MTY_SetInt32(msg.rbuf, MTY.userEnv[msg.name](...msg.args));
			mty_signal(msg.sync);
			break;
		case 'thread': {
			MTY.threadId++;

			const worker = mty_thread_start(MTY.threadId, MTY.bin, MTY.file, MTY.wasmBuf, MTY.memory,
				msg.startArg, MTY.userEnv, MTY.kbMap, MTY.glver, 'thread-' + MTY.threadId);

			MTY_SetUint32(msg.buf, MTY.threadId);
			mty_signal(msg.sync);
			break;
		}
		case 'present':
			MTY.renderer.transferFromImageBitmap(msg.image);
			break;
		case 'image': {
			const image = await mty_decode_image(msg.input);

			this.tmp = image.data;
			MTY_SetInt32(msg.buf + 0, image.width);
			MTY_SetInt32(msg.buf + 4, image.height);

			mty_signal(msg.sync);
			break;
		}
		case 'title':
			document.title = msg.title;
			break;
		case 'get-ls': {
			const val = window.localStorage[msg.key];

			if (val) {
				this.tmp = mty_b64_to_buf(val);
				MTY_SetUint32(msg.buf, this.tmp.byteLength);

			} else {
				MTY_SetUint32(msg.buf, 0);
			}

			mty_signal(msg.sync);
			break;
		}
		case 'set-ls':
			window.localStorage[msg.key] = msg.val;
			mty_signal(msg.sync);
			break;
		case 'alert':
			mty_alert(msg.title, msg.msg);
			break;
		case 'fullscreen':
			mty_set_fullscreen(msg.fullscreen);
			break;
		case 'wake-lock':
			mty_wake_lock(msg.enable);
			break;
		case 'rumble':
			mty_rumble_gamepad(msg.id, msg.low, msg.high);
			break;
		case 'show-cursor':
			mty_show_cursor(msg.show);
			break;
		case 'get-clip': {
			const enc = new TextEncoder();
			const text = await navigator.clipboard.readText();

			this.tmp = enc.encode(text);
			MTY_SetUint32(msg.buf, this.tmp.byteLength);
			mty_signal(msg.sync);
			break;
		}
		case 'set-clip':
			navigator.clipboard.writeText(MTY_StrToJS(msg.text));
			break;
		case 'pointer-lock':
			mty_set_pointer_lock(msg.enable);
			break;
		case 'cursor-default':
			mty_use_default_cursor(msg.use_default);
			break;
		case 'cursor':
			mty_set_png_cursor(msg.buffer, msg.size, msg.hot_x, msg.hot_y);
			break;
		case 'uri':
			mty_set_action(() => {
				window.open(MTY_StrToJS(msg.uri), '_blank');
			});
			break;
		case 'http': {
			const res = await mty_http_request(msg.url, msg.method, msg.headers,
				msg.body);

			this.tmp = res.data;
			MTY_SetUint32(msg.buf + 0, res.error ? 1 : 0);
			MTY_SetUint32(msg.buf + 4, res.size);
			MTY_SetUint32(msg.buf + 8, res.status);

			mty_signal(msg.sync);
			break;
		}
		case 'ws': {
			const ws = await mty_ws_connect(msg.url);
			MTY_SetUint32(msg.buf, ws ? mty_ws_new(ws) : 0);
			mty_signal(msg.sync);
			break;
		}
		case 'ws-read': {
			MTY_SetUint32(msg.cbuf, 3); // MTY_ASYNC_ERROR

			const ws = mty_ws_obj(msg.ctx);

			if (ws) {
				if (ws.closeCode != 0) {
					MTY_SetUint32(msg.cbuf, 1); // MTY_ASYNC_DONE

				} else {
					const buf = await mty_ws_read(ws, msg.timeout);

					if (buf) {
						if (buf.length < msg.size) {
							MTY_Memcpy(msg.buf, buf);
							MTY_SetInt8(msg.buf + buf.length, 0);
							MTY_SetUint32(msg.cbuf, 0); // MTY_ASYNC_OK
						}

					} else {
						MTY_SetUint32(msg.cbuf, 2); // MTY_ASYNC_CONTINUE
					}
				}
			}

			mty_signal(msg.sync);
			break;
		}
		case 'ws-write': {
			const ws = mty_ws_obj(msg.ctx);
			if (ws)
				ws.send(msg.text)
			break;
		}
		case 'ws-close': {
			const ws = mty_ws_obj(msg.ctx);
			if (ws) {
				ws.close();
				mty_ws_del(msg.ctx);
			}
			break;
		}
		case 'ws-code': {
			MTY_SetUint16(msg.cbuf, 0);

			const ws = mty_ws_obj(msg.ctx);
			if (ws)
				MTY_SetUint16(msg.cbuf, ws.closeCode);

			mty_signal(msg.sync);
			break;
		}
		case 'async-copy':
			MTY_Memcpy(msg.buf, this.tmp);
			this.tmp = undefined;

			mty_signal(msg.sync);
			break;
	}
}
