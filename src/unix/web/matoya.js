// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.


// Global State

const MTY = {
	module: null,
	alloc: 0,
	free: 0,
	audio: null,
	cbuf: null,
	kbMap: null,
	keysRev: {},
	wakeLock: null,
	reqs: {},
	reqIndex: 0,
	endFunc: () => {},
	cursorId: 0,
	cursorCache: {},
	cursorClass: '',
	defaultCursor: false,
	synthesizeEsc: true,
	relative: false,
	gps: [false, false, false, false],
	action: null,
	lastX: 0,
	lastY: 0,
	keys: {},
	clip: null,

	// GL
	gl: null,
	glver: 'webgl',
	glIndex: 0,
	glObj: {},

	// WASI
	arg0: '',
	fds: {},
	fdIndex: 64,
	preopen: false,
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

function MTY_SetUint32(ptr, value) {
	mty_mem_view().setUint32(ptr, value, true);
}

function MTY_SetUint16(ptr, value) {
	mty_mem_view().setUint16(ptr, value, true);
}

function MTY_SetInt32(ptr, value) {
	mty_mem_view().setInt32(ptr, value, true);
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

function MTY_GetUint32(ptr) {
	return mty_mem_view().getUint32(ptr, true);
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

function MTY_Wait(sync) {
	if (Atomics.compareExchange(sync, 0, 0, 1) == 0) {
		Atomics.wait(sync, 0, 1);

	} else {
		Atomics.store(sync, 0);
	}
}

function MTY_Signal(sync) {
	if (Atomics.compareExchange(sync, 0, 0, 1) == 0) {

	} else {
		Atomics.store(sync, 0);
		Atomics.notify(sync, 0);
	}
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

function MTY_SetAction(action) {
	MTY.action = action;

	// In case click handler doesn't happen
	mty_run_action();
}

function mty_correct_relative() {
	if (!document.pointerLockElement && MTY.relative)
		MTY.gl.canvas.requestPointerLock();
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

function mty_add_input_events() {
	MTY.canvas.addEventListener('mousemove', (ev) => {
		let x = mty_scaled(ev.clientX);
		let y = mty_scaled(ev.clientY);

		if (MTY.relative) {
			x = ev.movementX;
			y = ev.movementY;
		}

		MTY.worker.postMessage({
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

			MTY.worker.postMessage(msg);

			msg.pressed = false;
			MTY.worker.postMessage(msg);
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

		MTY.worker.postMessage({
			type: 'button',
			pressed: true,
			button: ev.button,
			x: mty_scaled(ev.clientX),
			y: mty_scaled(ev.clientY),
		});
	});

	window.addEventListener('mouseup', (ev) => {
		ev.preventDefault();

		MTY.worker.postMessage({
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

		MTY.worker.postMessage({
			type: 'wheel',
			x: x,
			y: y,
		});
	}, {passive: true});

	window.addEventListener('keydown', (ev) => {
		mty_correct_relative();

		MTY.worker.postMessage({
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
		MTY.worker.postMessage({
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
		MTY.worker.postMessage({
			type: 'focus',
			focus: false,
		});
	});

	window.addEventListener('focus', (ev) => {
		MTY.worker.postMessage({
			type: 'focus',
			focus: true,
		});
	});

	window.addEventListener('resize', (ev) => {
		MTY.worker.postMessage({
			type: 'resize',
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
						MTY.worker.postMessage({
							type: 'drop',
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

			MTY.worker.postMessage({
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
			MTY.worker.postMessage({
				type: 'disconnect',
				id: x,
				state: 2,
			});

			MTY.gps[x] = false;
		}
	}
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

function mty_raf() {
	// Poll gamepads
	if (document.hasFocus())
		mty_poll_gamepads();

	// Poll position changes
	if (MTY.lastX != window.screenX || MTY.lastY != window.screenY) {
		MTY.lastX = window.screenX;
		MTY.lastY = window.screenY;

		MTY.worker.postMessage({
			type: 'move',
		});
	}

	// Poll size changes and resize the canvas
	const rect = MTY.canvas.getBoundingClientRect();

	// send rect event
	MTY.worker.postMessage({
		type: 'raf',
		lastX: window.screenX,
		lastY: window.screenY,
		hasFocus: document.hasFocus(),
		screenWidth: screen.width,
		screenHeight: screen.height,
		fullscreen: document.fullscreenElement != null,
		visible: mty_is_visible(),
		canvasWidth: mty_scaled(rect.width),
		canvasHeight: mty_scaled(rect.height),
	});

	requestAnimationFrame(mty_raf);
}

async function mty_decode_image(msg) {
	const jinput = new Uint8Array(mty_mem(), msg.input, msg.size);

	const cpy = new Uint8Array(msg.size);
	cpy.set(jinput);

	const img = new Image();
	img.src = URL.createObjectURL(new Blob([cpy]));

	await img.decode();

	const width = img.naturalWidth;
	const height = img.naturalHeight;

	if (msg.type == 'image-size') {
		MTY_SetInt32(msg.buf, width);
		MTY_SetInt32(msg.buf + 4, height);

	} else {
		const canvas = new OffscreenCanvas(width, height);
		const ctx = canvas.getContext('2d');
		ctx.drawImage(img, 0, 0, width, height);

		const imgData = ctx.getImageData(0, 0, width, height);
		MTY_Memcpy(msg.buf, imgData.data);
	}

	MTY_Signal(msg.sync);
}

function MTY_Start(bin, userEnv, endFunc, glver) {
	if (!mty_supports_wasm() || !mty_supports_web_gl())
		return false;

	// Set up full window canvas
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

	MTY.canvas = document.createElement('canvas');
	MTY.canvas.style.width = '100%';
	MTY.canvas.style.height = '100%';
	document.body.appendChild(MTY.canvas);

	// Set up the clipboard
	MTY.clip = document.createElement('textarea');
	MTY.clip.style.position = 'absolute';
	MTY.clip.style.left = '-9999px';
	MTY.clip.autofocus = true;
	document.body.appendChild(MTY.clip);

	// Init position, update loop
	MTY.lastX = window.screenX;
	MTY.lastY = window.screenY;
	MTY.devicePixelRatio = window.devicePixelRatio;
	requestAnimationFrame(mty_raf);

	// Add input events
	mty_add_input_events();

	MTY.worker = new Worker('/lib/matoya-worker.js');

	MTY.memory = new WebAssembly.Memory({
		initial: 512,   // 32 MB
		maximum: 16384, // 1 GB
		shared: true,
	});

	const offscreen = MTY.canvas.transferControlToOffscreen();

	MTY.worker.postMessage({
		type: 'init',
		bin: bin,
		devicePixelRatio: window.devicePixelRatio,
		args: window.location.search,
		hostname: window.location.hostname,
		userEnv: Object.keys(userEnv),
		canvas: offscreen,
		memory: MTY.memory,
	}, [offscreen]);

	MTY.worker.onmessage = (ev) => {
		const msg = ev.data;

		switch (msg.type) {
			case 'user-env':
				MTY_SetInt32(msg.rbuf, userEnv[msg.name](...msg.args));
				MTY_Signal(msg.sync);
				break;
			case 'image':
			case 'image-size':
				mty_decode_image(msg);
				break;
			case 'title':
				document.title = msg.title;
				break;
			case 'get-ls-size':
				const val = window.localStorage[msg.key];

				if (val) {
					const bval = mty_b64_to_buf(val);
					MTY_SetUint32(msg.buf, bval.byteLength);

				} else {
					MTY_SetUint32(msg.buf, 0);
				}

				MTY_Signal(msg.sync);
				break;
			case 'get-ls': {
				const val = window.localStorage[msg.key];
				const bval = mty_b64_to_buf(val);

				MTY_Memcpy(msg.buf, bval);
				MTY_Signal(msg.sync);
				break;
			}
			case 'set-ls':
				window.localStorage[msg.key] = msg.val;
				MTY_Signal(msg.sync);
				break;
		}
	};

	return true;
}
