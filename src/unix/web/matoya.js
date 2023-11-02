// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.


// Global State

let MTY_MEMORY;
let MTY_CURRENT_SCRIPT;

// Worker
if (typeof importScripts == 'function') {
	MTY_CURRENT_SCRIPT = location;

// Main thread
} else {
	MTY_CURRENT_SCRIPT = new URL(document.currentScript.src);

	window.MTY = {
		wsIndex: 1,
		wsObj: {},
		cursorId: 0,
		threadId: 1,
		cursorCache: {},
		cursorClass: '',
		defaultCursor: false,
		synthesizeEsc: true,
		relative: false,
		gps: [false, false, false, false],
	};
}


// Memory

function mty_encode(str) {
	return new TextEncoder().encode(str);
}

function mty_decode(buf) {
	return new TextDecoder().decode(buf);
}

function mty_strlen(buf) {
	let len = 0;
	for (; buf[len] != 0; len++);

	return len;
}

function mty_memcpy(ptr, buf) {
	new Uint8Array(MTY_MEMORY.buffer, ptr, buf.byteLength).set(buf);
}

function mty_strcpy(ptr, buf) {
	mty_memcpy(ptr, buf);
	mty_set_int8(ptr + buf.byteLength, 0);
}

function mty_dup(ptr, size) {
	return new Uint8Array(MTY_MEMORY.buffer, ptr).slice(0, size);
}

function mty_str_to_js(ptr) {
	const buf = new Uint8Array(MTY_MEMORY.buffer, ptr);

	return mty_decode(buf.slice(0, mty_strlen(buf)));
}

function mty_str_to_c(str, ptr, size) {
	const buf = mty_encode(str);

	if (buf.byteLength >= size)
		throw 'mty_str_to_c overflow'

	mty_strcpy(ptr, buf);
}

function mty_get_uint8(ptr) {
	return new DataView(MTY_MEMORY.buffer).getUint8(ptr);
}

function mty_set_int8(ptr, value) {
	new DataView(MTY_MEMORY.buffer).setInt8(ptr, value);
}

function mty_set_uint16(ptr, value) {
	new DataView(MTY_MEMORY.buffer).setUint16(ptr, value, true);
}

function mty_get_uint32(ptr) {
	return new DataView(MTY_MEMORY.buffer).getUint32(ptr, true);
}

function mty_set_uint32(ptr, value) {
	new DataView(MTY_MEMORY.buffer).setUint32(ptr, value, true);
}

function mty_get_uint64(ptr, value) {
	return new DataView(MTY_MEMORY.buffer).getBigUint64(ptr, true);
}

function mty_set_uint64(ptr, value) {
	new DataView(MTY_MEMORY.buffer).setBigUint64(ptr, BigInt(value), true);
}

function mty_set_float(ptr, value) {
	new DataView(MTY_MEMORY.buffer).setFloat32(ptr, value, true);
}


// Base64

function mty_buf_to_b64(buf) {
	let bstr = '';
	for (let x = 0; x < buf.byteLength; x++)
		bstr += String.fromCharCode(buf[x]);

	return btoa(bstr);
}

function mty_b64_to_buf(b64) {
	const bstr = atob(b64);
	const buf = new Uint8Array(bstr.length);

	for (let x = 0; x < bstr.length; x++)
		buf[x] = bstr.charCodeAt(x);

	return buf;
}


// Synchronization

function mty_wait(sync) {
	if (Atomics.compareExchange(sync, 0, 0, 1) == 0)
		Atomics.wait(sync, 0, 1);

	Atomics.store(sync, 0, 0);
}

function mty_signal(sync, allow_miss = false) {
	if (Atomics.compareExchange(sync, 0, 0, 1) != 0)
		while (Atomics.notify(sync, 0, 1) == 0 && !allow_miss);
}

function MTY_SignalPtr(csync) {
	mty_signal(new Int32Array(MTY_MEMORY.buffer, csync, 1));
}


// Input

function mty_scaled(num) {
	return Math.round(num * window.devicePixelRatio);
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

function mty_set_pointer_lock(enable) {
	if (enable && !document.pointerLockElement) {
		MTY.canvas.requestPointerLock();

	} else if (!enable && document.pointerLockElement) {
		MTY.synthesizeEsc = false;
		document.exitPointerLock();
	}

	MTY.relative = enable;
}

function mty_allow_default(ev) {
	// The "allowed" browser hotkey list. Copy/Paste, Refresh, fullscreen, developer console, and tab switching

	return ((ev.ctrlKey || ev.metaKey) && ev.code == 'KeyV') ||
		((ev.ctrlKey || ev.metaKey) && ev.code == 'KeyC') ||
		((ev.ctrlKey || ev.shiftKey) && ev.code == 'KeyI') ||
		(ev.ctrlKey && ev.code == 'KeyR') ||
		(ev.ctrlKey && ev.code == 'F5') ||
		(ev.ctrlKey && ev.code == 'Digit1') ||
		(ev.ctrlKey && ev.code == 'Digit2') ||
		(ev.ctrlKey && ev.code == 'Digit3') ||
		(ev.ctrlKey && ev.code == 'Digit4') ||
		(ev.ctrlKey && ev.code == 'Digit5') ||
		(ev.ctrlKey && ev.code == 'Digit6') ||
		(ev.ctrlKey && ev.code == 'Digit7') ||
		(ev.ctrlKey && ev.code == 'Digit8') ||
		(ev.ctrlKey && ev.code == 'Digit9') ||
		(ev.code == 'F5') ||
		(ev.code == 'F11') ||
		(ev.code == 'F12');
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
				type: 'keyboard',
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
			type: 'scroll',
			x: x,
			y: y,
		});
	}, {passive: true});

	window.addEventListener('keydown', (ev) => {
		mty_correct_relative();

		thread.postMessage({
			type: 'keyboard',
			pressed: true,
			code: ev.code,
			key: ev.key,
			mods: mty_get_mods(ev),
		});

		if (MTY.kb_grab || !mty_allow_default(ev))
			ev.preventDefault();
	});

	window.addEventListener('keyup', (ev) => {
		thread.postMessage({
			type: 'keyboard',
			pressed: false,
			code: ev.code,
			key: '',
			mods: mty_get_mods(ev),
		});

		if (MTY.kb_grab || !mty_allow_default(ev))
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
		const rect = mty_update_canvas(MTY.canvas);

		thread.postMessage({
			type: 'size',
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


// Dialog

function mty_alert(title, msg) {
	window.alert(mty_str_to_js(title) + '\n\n' + mty_str_to_js(msg));
}


// URI opener

function mty_run_action() {
	setTimeout(() => {
		if (MTY.action) {
			MTY.action();
			delete MTY.action;
		}
	}, 100);
}

function mty_set_action(action) {
	MTY.action = action;

	// In case click handler doesn't happen
	mty_run_action();
}


// Window

function mty_is_visible() {
	if (document.hidden != undefined) {
		return !document.hidden;

	} else if (document.webkitHidden != undefined) {
		return !document.webkitHidden;
	}

	return true;
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

function mty_update_canvas(canvas) {
	const rect = canvas.getBoundingClientRect();
	canvas.width = rect.width;
	canvas.height = rect.height;

	return rect;
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
			delete MTY.wakeLock;
		}
	} catch (e) {
		delete MTY.wakeLock;
	}
}


// Cursor

function mty_show_cursor(show) {
	MTY.canvas.style.cursor = show ? '': 'none';
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

function mty_set_cursor(url, hot_x, hot_y) {
	if (url) {
		if (!MTY.cursorCache[url]) {
			MTY.cursorCache[url] = `cursor-x-${MTY.cursorId}`;

			const style = document.createElement('style');
			style.type = 'text/css';
			style.innerHTML = `.cursor-x-${MTY.cursorId++} ` +
				`{cursor: url(${url}) ${hot_x} ${hot_y}, auto;}`;
			document.querySelector('head').appendChild(style);
		}

		if (MTY.cursorClass.length > 0)
			MTY.canvas.classList.remove(MTY.cursorClass);

		MTY.cursorClass = MTY.cursorCache[url];

		if (!MTY.defaultCursor)
			MTY.canvas.classList.add(MTY.cursorClass);

	} else {
		if (!MTY.defaultCursor && MTY.cursorClass.length > 0)
			MTY.canvas.classList.remove(MTY.cursorClass);

		MTY.cursorClass = '';
	}
}

function mty_set_png_cursor(buf, hot_x, hot_y) {
	const url = buf ? 'data:image/png;base64,' + mty_buf_to_b64(buf) : null;
	mty_set_cursor(url, hot_x, hot_y);
}

function mty_set_rgba_cursor(buf, width, height, hot_x, hot_y) {
	let url = null;

	if (buf) {
		if (!MTY.ccanvas) {
			MTY.ccanvas = document.createElement('canvas');
			MTY.cctx = MTY.ccanvas.getContext('2d');
		}

		MTY.ccanvas.width = width;
		MTY.ccanvas.height = height;

		const image = MTY.cctx.getImageData(0, 0, width, height);
		image.data.set(buf);

		MTY.cctx.putImageData(image, 0, 0);

		url = MTY.ccanvas.toDataURL();
	}

	mty_set_cursor(url, hot_x, hot_y);
}


// Gamepads

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

function mty_poll_gamepads(thread) {
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
				type: 'controller',
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
				type: 'controller-disconnect',
				id: x,
				state: 2,
			});

			MTY.gps[x] = false;
		}
	}
}


// Audio

async function mty_audio_queue(ctx, sampleRate, minBuffer, maxBuffer, channels) {
	// Initialize on first queue otherwise the browser may complain about user interaction
	if (!MTY.audioCtx) {
		MTY.audioCtx = new AudioContext({sampleRate: sampleRate});

		const baseFile = MTY_CURRENT_SCRIPT.pathname;
		await MTY.audioCtx.audioWorklet.addModule(baseFile.replace('.js', '-worker.js'));

		const node = new AudioWorkletNode(MTY.audioCtx, 'MTY_Audio', {
			outputChannelCount: [channels],
			processorOptions: {
				minBuffer,
				maxBuffer,
			},
		});

		node.connect(MTY.audioCtx.destination);
		node.port.postMessage(MTY.audioObjs);
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
			body: body,
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

		ws.onclose = (ev) => {
			ws.closeCode = ev.code == 1005 ? 1000 : ev.code;
			resolve(null);
		};

		ws.onerror = (err) => {
			console.error(err);
			resolve(null);
		};

		ws.onopen = () => {
			resolve(ws);
		};

		ws.onmessage = (ev) => {
			ws.msgs.push(ev.data);
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

	return msg ? mty_encode(msg) : null;
}


// Entry

function mty_supports_web_gl() {
	try {
		return document.createElement('canvas').getContext('webgl2');
	} catch (e) {}

	return false;
}

function mty_update_interval(thread) {
	// Poll gamepads
	if (document.hasFocus())
		mty_poll_gamepads(thread);

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

function mty_thread_start(threadId, bin, wasmBuf, memory, startArg, userEnv, kbMap, psync, audioObjs, name) {
	const baseFile = MTY_CURRENT_SCRIPT.pathname;
	const worker = new Worker(baseFile.replace('.js', '-worker.js'), {name: name});

	worker.onmessage = mty_thread_message;

	worker.postMessage({
		type: 'init',
		file: baseFile,
		bin: bin,
		wasmBuf: wasmBuf,
		psync: psync,
		windowInfo: mty_window_info(),
		args: window.location.search,
		hostname: window.location.hostname,
		userEnv: userEnv ? Object.keys(userEnv) : [],
		kbMap: kbMap,
		startArg: startArg,
		threadId: threadId,
		memory: memory,
		audioObjs,
	});

	return worker;
}

async function MTY_Start(bin, container, userEnv) {
	if (!mty_supports_web_gl())
		return false;

	MTY.bin = bin;
	MTY.userEnv = userEnv;
	MTY.psync = new Int32Array(new SharedArrayBuffer(4));
	MTY.audioObjs = {
		buf: new Int16Array(new SharedArrayBuffer(1024 * 1024)),
		control: new Int32Array(new SharedArrayBuffer(32)),
	};

	// Drawing surface
	MTY.canvas = document.createElement('canvas');
	MTY.renderer = MTY.canvas.getContext('bitmaprenderer');
	MTY.canvas.style.width = '100%';
	MTY.canvas.style.height = '100%';
	container.appendChild(MTY.canvas);
	mty_update_canvas(MTY.canvas);

	// WASM binary
	const wasmRes = await fetch(bin);
	MTY.wasmBuf = await wasmRes.arrayBuffer();

	// Shared global memory
	MTY_MEMORY = new WebAssembly.Memory({
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
	MTY.mainThread = mty_thread_start(MTY.threadId, bin, MTY.wasmBuf, MTY_MEMORY,
		0, userEnv, MTY.kbMap, MTY.psync, MTY.audioObjs, 'main');

	// Init position, update loop
	MTY.posX = window.screenX;
	MTY.posY = window.screenY;
	setInterval(() => {
		mty_update_interval(MTY.mainThread);
	}, 10);

	// Vsync
	const vsync = () => {
		mty_signal(MTY.psync, true);
		requestAnimationFrame(vsync);
	};
	requestAnimationFrame(vsync);

	// Add input events
	mty_add_input_events(MTY.mainThread);

	return true;
}

async function mty_thread_message(ev) {
	const msg = ev.data;

	switch (msg.type) {
		case 'user-env':
			msg.sab[0] = MTY.userEnv[msg.name](...msg.args);
			mty_signal(msg.sync);
			break;
		case 'thread': {
			MTY.threadId++;

			const worker = mty_thread_start(MTY.threadId, MTY.bin, MTY.wasmBuf, MTY_MEMORY,
				msg.startArg, MTY.userEnv, MTY.kbMap, MTY.psync, MTY.audioObjs, 'thread-' + MTY.threadId);

			msg.sab[0] = MTY.threadId;
			mty_signal(msg.sync);
			break;
		}
		case 'present':
			MTY.renderer.transferFromImageBitmap(msg.image);
			break;
		case 'decode-image': {
			const image = await mty_decode_image(msg.input);

			this.tmp = image.data;
			msg.sab[0] = image.width;
			msg.sab[1] = image.height;

			mty_signal(msg.sync);
			break;
		}
		case 'kb-grab':
			MTY.kb_grab = msg.grab;
			break;
		case 'title':
			document.title = msg.title;
			break;
		case 'get-ls': {
			const val = window.localStorage[msg.key];

			if (val) {
				this.tmp = mty_b64_to_buf(val);
				msg.sab[0] = this.tmp.byteLength;

			} else {
				msg.sab[0] = 0;
			}

			mty_signal(msg.sync);
			break;
		}
		case 'set-ls':
			window.localStorage[msg.key] = mty_buf_to_b64(msg.val);
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
		case 'get-clip':
			// FIXME Unsupported on Firefox
			if (navigator.clipboard.readText) {
				const text = await navigator.clipboard.readText();

				this.tmp = mty_encode(text);
				msg.sab[0] = this.tmp.byteLength;

			} else {
				msg.sab[0] = 0;
			}

			mty_signal(msg.sync);
			break;
		case 'set-clip':
			navigator.clipboard.writeText(mty_str_to_js(msg.text));
			break;
		case 'pointer-lock':
			mty_set_pointer_lock(msg.enable);
			break;
		case 'cursor-default':
			mty_use_default_cursor(msg.use_default);
			break;
		case 'cursor-rgba':
			mty_set_rgba_cursor(msg.buf, msg.width, msg.height, msg.hot_x, msg.hot_y);
			break;
		case 'cursor-png':
			mty_set_png_cursor(msg.buf, msg.hot_x, msg.hot_y);
			break;
		case 'uri':
			mty_set_action(() => {
				window.open(mty_str_to_js(msg.uri), '_blank');
			});
			break;
		case 'http': {
			const res = await mty_http_request(msg.url, msg.method, msg.headers, msg.body);

			this.tmp = res.data;
			msg.sab[0] = res.error ? 1 : 0;
			msg.sab[1] = res.size;
			msg.sab[2] = res.status;

			mty_signal(msg.sync);
			break;
		}
		case 'ws-connect': {
			const ws = await mty_ws_connect(msg.url);
			msg.sab[0] = ws ? mty_ws_new(ws) : 0;
			mty_signal(msg.sync);
			break;
		}
		case 'ws-read': {
			msg.sab[0] = 3; // MTY_ASYNC_ERROR

			const ws = mty_ws_obj(msg.ctx);

			if (ws) {
				if (ws.closeCode != 0) {
					msg.sab[0] = 1; // MTY_ASYNC_DONE

				} else {
					const buf = await mty_ws_read(ws, msg.timeout);

					if (buf) {
						this.tmp = buf;
						msg.sab[0] = 0; // MTY_ASYNC_OK
						msg.sab[1] = buf.length;

					} else {
						msg.sab[0] = 2; // MTY_ASYNC_CONTINUE
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
			msg.sab[0] = 0;

			const ws = mty_ws_obj(msg.ctx);
			if (ws)
				msg.sab[0] = ws.closeCode;

			mty_signal(msg.sync);
			break;
		}
		case 'audio-queue':
			mty_audio_queue(MTY.audio, msg.sampleRate, msg.minBuffer,
				msg.maxBuffer, msg.channels);
			break;
		case 'audio-destroy':
			if (MTY.audioCtx)
				MTY.audioCtx.close();
			delete MTY.audioCtx;
			break;
		case 'async-copy':
			msg.sab8.set(this.tmp);
			delete this.tmp;

			mty_signal(msg.sync);
			break;
	}
}
