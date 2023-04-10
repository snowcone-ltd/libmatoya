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
	Atomics.wait(sync, 0, 0);
	Atomics.store(sync, 0, 0);
}

function MTY_Signal(sync) {
	Atomics.store(sync, 0, 1);
	Atomics.notify(sync, 0);
}


// Image

function mty_decompress_image(input, func) {
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
	//if (document.hasFocus())
	//	mty_poll_gamepads(app, controller);

	// Poll position changes
	if (MTY.lastX != window.screenX || MTY.lastY != window.screenY) {
		MTY.lastX = window.screenX;
		MTY.lastY = window.screenY;

		// Send move event
	}

	// Poll size changes and resize the canvas
	const rect = MTY.canvas.getBoundingClientRect();

	// send rect event
	MTY.worker.postMessage({
		type: 'raf',
		lastX: window.screenX,
		lastY: window.screenY,
		rect: rect,
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
	requestAnimationFrame(mty_raf);

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
		}
	};

	return true;
}
