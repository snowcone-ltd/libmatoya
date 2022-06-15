// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

// Global state

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
	frameCtr: 0,
	swapInterval: 1,
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


// Private helpers

function mty_mem() {
	return MTY.module.instance.exports.memory.buffer;
}

function mty_mem_view() {
	return new DataView(mty_mem());
}

function mty_char_to_js(buf) {
	let str = '';

	for (let x = 0; x < 0x7FFFFFFF && x < buf.length; x++) {
		if (buf[x] == 0)
			break;

		str += String.fromCharCode(buf[x]);
	}

	return str;
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


// WASM utility

function MTY_CFunc(ptr) {
	return MTY.module.instance.exports.__indirect_function_table.get(ptr);
}

function MTY_Alloc(size, el) {
	return MTY_CFunc(MTY.alloc)(size, el ? el : 1);
}

function MTY_Free(ptr) {
	MTY_CFunc(MTY.free)(ptr);
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

function MTY_Memcpy(cptr, abuffer) {
	const heap = new Uint8Array(mty_mem(), cptr, abuffer.length);
	heap.set(abuffer);
}

function MTY_StrToJS(ptr) {
	return mty_char_to_js(new Uint8Array(mty_mem(), ptr));
}

function MTY_StrToC(js_str, ptr, size) {
	const view = new Uint8Array(mty_mem(), ptr);

	let len = 0;
	for (; len < js_str.length && len < size - 1; len++)
		view[len] = js_str.charCodeAt(len);

	// '\0' character
	view[len] = 0;

	return ptr;
}


// <unistd.h> stubs

const MTY_UNISTD_API = {
	gethostname: function (cbuf, size) {
		MTY_StrToC(location.hostname, cbuf, size);
	},
	flock: function (fd, flags) {
		return 0;
	},
};


// GL

function mty_gl_new(obj) {
	MTY.glObj[MTY.glIndex] = obj;

	return MTY.glIndex++;
}

function mty_gl_del(index) {
	let obj = MTY.glObj[index];

	MTY.glObj[index] = undefined;
	delete MTY.glObj[index];

	return obj;
}

function mty_gl_obj(index) {
	return MTY.glObj[index];
}

const MTY_GL_API = {
	glGenFramebuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_SetUint32(ids + x * 4, mty_gl_new(MTY.gl.createFramebuffer()));
	},
	glDeleteFramebuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY.gl.deleteFramebuffer(mty_gl_del(MTY_GetUint32(ids + x * 4)));
	},
	glBindFramebuffer: function (target, fb) {
		MTY.gl.bindFramebuffer(target, fb ? mty_gl_obj(fb) : null);
	},
	glBlitFramebuffer: function (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter) {
		MTY.gl.blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
	},
	glFramebufferTexture2D: function (target, attachment, textarget, texture, level) {
		MTY.gl.framebufferTexture2D(target, attachment, textarget, mty_gl_obj(texture), level);
	},
	glEnable: function (cap) {
		MTY.gl.enable(cap);
	},
	glIsEnabled: function (cap) {
		return MTY.gl.isEnabled(cap);
	},
	glDisable: function (cap) {
		MTY.gl.disable(cap);
	},
	glViewport: function (x, y, width, height) {
		MTY.gl.viewport(x, y, width, height);
	},
	glGetIntegerv: function (name, data) {
		const p = MTY.gl.getParameter(name);

		switch (name) {
			// object
			case MTY.gl.READ_FRAMEBUFFER_BINDING:
			case MTY.gl.DRAW_FRAMEBUFFER_BINDING:
			case MTY.gl.ARRAY_BUFFER_BINDING:
			case MTY.gl.TEXTURE_BINDING_2D:
			case MTY.gl.CURRENT_PROGRAM:
				MTY_SetUint32(data, mty_gl_new(p));
				break;

			// int32[4]
			case MTY.gl.VIEWPORT:
			case MTY.gl.SCISSOR_BOX:
				for (let x = 0; x < 4; x++)
					MTY_SetUint32(data + x * 4, p[x]);
				break;

			// int
			case MTY.gl.ACTIVE_TEXTURE:
			case MTY.gl.BLEND_SRC_RGB:
			case MTY.gl.BLEND_DST_RGB:
			case MTY.gl.BLEND_SRC_ALPHA:
			case MTY.gl.BLEND_DST_ALPHA:
			case MTY.gl.BLEND_EQUATION_RGB:
			case MTY.gl.BLEND_EQUATION_ALPHA:
				MTY_SetUint32(data, p);
				break;
		}

		MTY_SetUint32(data, p);
	},
	glGetFloatv: function (name, data) {
		switch (name) {
			case MTY.gl.COLOR_CLEAR_VALUE:
				const p = MTY.gl.getParameter(name);

				for (let x = 0; x < 4; x++)
					MTY_SetFloat(data + x * 4, p[x]);
				break;
		}
	},
	glBindTexture: function (target, texture) {
		MTY.gl.bindTexture(target, texture ? mty_gl_obj(texture) : null);
	},
	glDeleteTextures: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY.gl.deleteTexture(mty_gl_del(MTY_GetUint32(ids + x * 4)));
	},
	glTexParameteri: function (target, pname, param) {
		MTY.gl.texParameteri(target, pname, param);
	},
	glGenTextures: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_SetUint32(ids + x * 4, mty_gl_new(MTY.gl.createTexture()));
	},
	glTexImage2D: function (target, level, internalformat, width, height, border, format, type, data) {
		MTY.gl.texImage2D(target, level, internalformat, width, height, border, format, type,
			new Uint8Array(mty_mem(), data));
	},
	glTexSubImage2D: function (target, level, xoffset, yoffset, width, height, format, type, pixels) {
		MTY.gl.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
			new Uint8Array(mty_mem(), pixels));
	},
	glDrawElements: function (mode, count, type, indices) {
		MTY.gl.drawElements(mode, count, type, indices);
	},
	glGetAttribLocation: function (program, c_name) {
		return MTY.gl.getAttribLocation(mty_gl_obj(program), MTY_StrToJS(c_name));
	},
	glShaderSource: function (shader, count, c_strings, c_len) {
		let source = '';
		for (let x = 0; x < count; x++)
			source += MTY_StrToJS(MTY_GetUint32(c_strings + x * 4));

		MTY.gl.shaderSource(mty_gl_obj(shader), source);
	},
	glBindBuffer: function (target, buffer) {
		MTY.gl.bindBuffer(target, buffer ? mty_gl_obj(buffer) : null);
	},
	glVertexAttribPointer: function (index, size, type, normalized, stride, pointer) {
		MTY.gl.vertexAttribPointer(index, size, type, normalized, stride, pointer);
	},
	glCreateProgram: function () {
		return mty_gl_new(MTY.gl.createProgram());
	},
	glUniform1i: function (loc, v0) {
		MTY.gl.uniform1i(mty_gl_obj(loc), v0);
	},
	glUniform1f: function (loc, v0) {
		MTY.gl.uniform1f(mty_gl_obj(loc), v0);
	},
	glUniform4i: function (loc, v0, v1, v2, v3) {
		MTY.gl.uniform4i(mty_gl_obj(loc), v0, v1, v2, v3);
	},
	glUniform4f: function (loc, v0, v1, v2, v3) {
		MTY.gl.uniform4f(mty_gl_obj(loc), v0, v1, v2, v3);
	},
	glActiveTexture: function (texture) {
		MTY.gl.activeTexture(texture);
	},
	glDeleteBuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY.gl.deleteBuffer(mty_gl_del(MTY_GetUint32(ids + x * 4)));
	},
	glEnableVertexAttribArray: function (index) {
		MTY.gl.enableVertexAttribArray(index);
	},
	glBufferData: function (target, size, data, usage) {
		MTY.gl.bufferData(target, new Uint8Array(mty_mem(), data, size), usage);
	},
	glDeleteShader: function (shader) {
		MTY.gl.deleteShader(mty_gl_del(shader));
	},
	glGenBuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_SetUint32(ids + x * 4, mty_gl_new(MTY.gl.createBuffer()));
	},
	glCompileShader: function (shader) {
		MTY.gl.compileShader(mty_gl_obj(shader));
	},
	glLinkProgram: function (program) {
		MTY.gl.linkProgram(mty_gl_obj(program));
	},
	glGetUniformLocation: function (program, name) {
		return mty_gl_new(MTY.gl.getUniformLocation(mty_gl_obj(program), MTY_StrToJS(name)));
	},
	glCreateShader: function (type) {
		return mty_gl_new(MTY.gl.createShader(type));
	},
	glAttachShader: function (program, shader) {
		MTY.gl.attachShader(mty_gl_obj(program), mty_gl_obj(shader));
	},
	glUseProgram: function (program) {
		MTY.gl.useProgram(program ? mty_gl_obj(program) : null);
	},
	glGetShaderiv: function (shader, pname, params) {
		if (pname == 0x8B81) {
			let ok = MTY.gl.getShaderParameter(mty_gl_obj(shader), MTY.gl.COMPILE_STATUS);
			MTY_SetUint32(params, ok);

			if (!ok)
				console.warn(MTY.gl.getShaderInfoLog(mty_gl_obj(shader)));

		} else {
			MTY_SetUint32(params, 0);
		}
	},
	glDetachShader: function (program, shader) {
		MTY.gl.detachShader(mty_gl_obj(program), mty_gl_obj(shader));
	},
	glDeleteProgram: function (program) {
		MTY.gl.deleteProgram(mty_gl_del(program));
	},
	glClear: function (mask) {
		MTY.gl.clear(mask);
	},
	glClearColor: function (red, green, blue, alpha) {
		MTY.gl.clearColor(red, green, blue, alpha);
	},
	glGetError: function () {
		return MTY.gl.getError();
	},
	glGetShaderInfoLog: function () {
		// FIXME Logged automatically as part of glGetShaderiv
	},
	glFinish: function () {
		MTY.gl.finish();
	},
	glScissor: function (x, y, width, height) {
		MTY.gl.scissor(x, y, width, height);
	},
	glBlendFunc: function (sfactor, dfactor) {
		MTY.gl.blendFunc(sfactor, dfactor);
	},
	glBlendEquation: function (mode) {
		MTY.gl.blendEquation(mode);
	},
	glUniformMatrix4fv: function (loc, count, transpose, value) {
		MTY.gl.uniformMatrix4fv(mty_gl_obj(loc), transpose, new Float32Array(mty_mem(), value, 4 * 4 * count));
	},
	glBlendEquationSeparate: function (modeRGB, modeAlpha) {
		MTY.gl.blendEquationSeparate(modeRGB, modeAlpha);
	},
	glBlendFuncSeparate: function (srcRGB, dstRGB, srcAlpha, dstAlpha) {
		MTY.gl.blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
	},
	glGetProgramiv: function (program, pname, params) {
		MTY_SetUint32(params, MTY.gl.getProgramParameter(mty_gl_obj(program), pname));
	},
	glPixelStorei: function (pname, param) {
		// GL_UNPACK_ROW_LENGTH is not compatible with WebGL 1
		if (MTY.glver == 'webgl' && pname == 0x0CF2)
			return;

		MTY.gl.pixelStorei(pname, param);
	},
	web_gl_flush: function () {
		MTY.gl.flush();
	},
};


// Audio

function mty_audio_queued_ms() {
	let queued_ms = Math.round((MTY.audio.next_time - MTY.audio.ctx.currentTime) * 1000.0);
	let buffered_ms = Math.round((MTY.audio.offset / 4) / MTY.audio.frames_per_ms);

	return (queued_ms < 0 ? 0 : queued_ms) + buffered_ms;
}

const MTY_AUDIO_API = {
	MTY_AudioCreate: function (sampleRate, minBuffer, maxBuffer) {
		MTY.audio = {};
		MTY.audio.flushing = false;
		MTY.audio.playing = false;
		MTY.audio.sample_rate = sampleRate;

		MTY.audio.frames_per_ms = Math.round(sampleRate / 1000.0);
		MTY.audio.min_buffer = minBuffer * MTY.audio.frames_per_ms;
		MTY.audio.max_buffer = maxBuffer * MTY.audio.frames_per_ms;

		MTY.audio.offset = 0;
		MTY.audio.buf = MTY_Alloc(sampleRate * 4);

		return 0xCDD;
	},
	MTY_AudioDestroy: function (audio) {
		MTY_Free(MTY.audio.buf);
		MTY_SetUint32(audio, 0);
		MTY.audio = null;
	},
	MTY_AudioQueue: function (ctx, frames, count) {
		// Initialize on first queue otherwise the browser may complain about user interaction
		if (!MTY.audio.ctx)
			MTY.audio.ctx = new AudioContext();

		let queued_frames = MTY.audio.frames_per_ms * mty_audio_queued_ms();

		// Stop playing and flush if we've exceeded the maximum buffer
		if (queued_frames > MTY.audio.max_buffer) {
			MTY.audio.playing = false;
			MTY.audio.flushing = true;
		}

		// Stop flushing when the queue reaches zero
		if (queued_frames == 0) {
			MTY.audio.flushing = false;
			MTY.audio.playing = false;
		}

		// Convert PCM int16_t to float
		if (!MTY.audio.flushing) {
			let size = count * 4;
			MTY_Memcpy(MTY.audio.buf + MTY.audio.offset, new Uint8Array(mty_mem(), frames, size));
			MTY.audio.offset += size;
		}

		// Begin playing again if the buffer has accumulated past the min
		if (!MTY.audio.playing && !MTY.audio.flushing && MTY.audio.offset / 4 > MTY.audio.min_buffer) {
			MTY.audio.next_time = MTY.audio.ctx.currentTime;
			MTY.audio.playing = true;
		}

		// Queue the audio if playing
		if (MTY.audio.playing) {
			const src = new Int16Array(mty_mem(), MTY.audio.buf);
			const bcount = MTY.audio.offset / 4;

			const buf = MTY.audio.ctx.createBuffer(2, bcount, MTY.audio.sample_rate);
			const left = buf.getChannelData(0);
			const right = buf.getChannelData(1);

			let offset = 0;
			for (let x = 0; x < bcount * 2; x += 2) {
				left[offset] = src[x] / 32768;
				right[offset] = src[x + 1] / 32768;
				offset++;
			}

			const source = MTY.audio.ctx.createBufferSource();
			source.buffer = buf;
			source.connect(MTY.audio.ctx.destination);
			source.start(MTY.audio.next_time);

			MTY.audio.next_time += buf.duration;
			MTY.audio.offset = 0;
		}
	},
	MTY_AudioReset: function (ctx) {
		MTY.audio.playing = false;
		MTY.audio.flushing = false;
		MTY.audio.offset = 0;
	},
	MTY_AudioGetQueued: function (ctx) {
		if (MTY.audio.ctx)
			return mty_audio_queued_ms();

		return 0;
	},
};


// Net

const MTY_ASYNC_OK = 0;
const MTY_ASYNC_DONE = 1;
const MTY_ASYNC_CONTINUE = 2;
const MTY_ASYNC_ERROR = 3;

const MTY_NET_API = {
	MTY_HttpAsyncCreate: function (num_threads) {
	},
	MTY_HttpAsyncDestroy: function () {
	},
	MTY_HttpSetProxy: function (proxy) {
	},
	MTY_HttpParseUrl: function (url_c, host_c_out, host_size, path_c_out, path_size) {
		const url = MTY_StrToJS(url_c);

		try {
			const url_obj = new URL(url);
			const path = url_obj.pathname + url_obj.search;

			MTY_StrToC(url_obj.host, host_c_out, host_size);
			MTY_StrToC(path, path_c_out, path_size);

			return true;

		} catch (err) {
			console.error(err);
		}

		return false;
	},
	MTY_HttpEncodeUrl: function(src, dst, dst_len) {
		// No-op, automatically converted in fetch
		MTY_StrToC(MTY_StrToJS(src), dst, dst_len);
	},
	MTY_HttpAsyncRequest: function(index, chost, port, secure, cmethod,
		cpath, cheaders, cbody, bodySize, timeout, func)
	{
		const req = ++MTY.reqIndex;
		MTY_SetUint32(index, req);

		MTY.reqs[req] = {
			async: MTY_ASYNC_CONTINUE,
			func: func,
		};

		const jport = port != 0 ? ':' + port.toString() : '';
		const scheme = secure ? 'https' : 'http';
		const method = MTY_StrToJS(cmethod);
		const host = MTY_StrToJS(chost);
		const path = MTY_StrToJS(cpath);
		const headers_str = MTY_StrToJS(cheaders);
		const body = cbody ? MTY_StrToJS(cbody) : undefined;
		const url = scheme + '://' + host + jport + path;

		const headers = {};
		const headers_nl = headers_str.split('\n');
		for (let x = 0; x < headers_nl.length; x++) {
			const pair = headers_nl[x];
			const pair_split = pair.split(':');

			if (pair_split[0] && pair_split[1])
				headers[pair_split[0]] = pair_split[1];
		}

		fetch(url, {
			method: method,
			headers: headers,
			body: body

		}).then((response) => {
			const data = MTY.reqs[req];
			data.status = response.status;

			return response.arrayBuffer();

		}).then((body) => {
			const data = MTY.reqs[req];
			data.response = new Uint8Array(body);
			data.async = MTY_ASYNC_OK;

		}).catch((err) => {
			const data = MTY.reqs[req];
			console.error(err);
			data.status = 0;
			data.async = MTY_ASYNC_ERROR;
		});
	},
	MTY_HttpAsyncPoll: function(index, response, responseSize, code) {
		const data = MTY.reqs[index];

		if (data == undefined || data.async == MTY_ASYNC_DONE)
			return MTY_ASYNC_DONE;

		if (data.async == MTY_ASYNC_CONTINUE)
			return MTY_ASYNC_CONTINUE;

		MTY_SetUint32(code, data.status);

		if (data.response != undefined) {
			MTY_SetUint32(responseSize, data.response.length);

			if (data.buf == undefined) {
				data.buf = MTY_Alloc(data.response.length + 1);
				MTY_Memcpy(data.buf, data.response);
			}

			MTY_SetUint32(response, data.buf);

			if (data.async == MTY_ASYNC_OK && data.func) {
				MTY_CFunc(data.func)(data.status, response, responseSize);
				data.buf = MTY_GetUint32(response);
			}
		}

		const r = data.async;
		data.async = MTY_ASYNC_DONE;

		return r;
	},
	MTY_HttpAsyncClear: function (index) {
		const req = MTY_GetUint32(index);
		const data = MTY.reqs[req];

		if (data == undefined)
			return;

		MTY_Free(data.buf);
		delete MTY.reqs[req];

		MTY_SetUint32(index, 0);
	},
};


// Crypto

const MTY_CRYPTO_API = {
	MTY_CryptoHash: function (algo, input, inputSize, key, keySize, output, outputSize) {
	},
	MTY_GetRandomBytes: function (buf, size) {
		const jbuf = new Uint8Array(mty_mem(), buf, size);
		crypto.getRandomValues(jbuf);
	},
};


// System

const MTY_SYSTEM_API = {
	MTY_HandleProtocol: function (uri, token) {
		MTY_SetAction(() => {
			window.open(MTY_StrToJS(uri), '_blank');
		});
	},
};


// Web API (mostly used in app.c)

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

function mty_scaled(num) {
	return Math.round(num * window.devicePixelRatio);
}

function mty_correct_relative() {
	if (!document.pointerLockElement && MTY.relative)
		MTY.gl.canvas.requestPointerLock();
}

function mty_poll_gamepads(app, controller) {
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
				lt = gp.buttons[6].value;
				rt = gp.buttons[7].value;

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

			MTY_CFunc(controller)(app, x, state, buttons, lx, ly, rx, ry, lt, rt);

		// Disconnected
		} else if (MTY.gps[x]) {
			MTY_CFunc(controller)(app, x, 2, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

			MTY.gps[x] = false;
		}
	}
}

const MTY_WEB_API = {
	web_alert: function (title, msg) {
		alert(MTY_StrToJS(title) + '\n\n' + MTY_StrToJS(msg));
	},
	web_platform: function (platform, size) {
		MTY_StrToC(navigator.platform, platform, size);
	},
	web_set_fullscreen: function (fullscreen) {
		if (fullscreen && !document.fullscreenElement) {
			if (navigator.keyboard)
				navigator.keyboard.lock(["Escape"]);

			document.documentElement.requestFullscreen();

		} else if (!fullscreen && document.fullscreenElement) {
			document.exitFullscreen();

			if (navigator.keyboard)
				navigator.keyboard.unlock();
		}
	},
	web_get_fullscreen: function () {
		return document.fullscreenElement ? true : false;
	},
	web_set_mem_funcs: function (alloc, free) {
		MTY.alloc = alloc;
		MTY.free = free;

		// Global buffers for scratch heap space
		MTY.cbuf = MTY_Alloc(1024);
	},
	web_set_key: function (reverse, code, key) {
		const str = MTY_StrToJS(code);
		MTY.keys[str] = key;

		if (reverse)
			MTY.keysRev[key] = str;
	},
	web_get_key: function (key, cbuf, len) {
		const code = MTY.keysRev[key];

		if (code != undefined) {
			if (MTY.kbMap) {
				const text = MTY.kbMap.get(code);
				if (text) {
					MTY_StrToC(text.toUpperCase(), cbuf, len);
					return true;
				}
			}

			MTY_StrToC(code, cbuf, len);
			return true;
		}

		return false;
	},
	web_wake_lock: async function (enable) {
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
	},
	web_rumble_gamepad: function (id, low, high) {
		const gps = navigator.getGamepads();
		const gp = gps[id];

		if (gp && gp.vibrationActuator)
			gp.vibrationActuator.playEffect('dual-rumble', {
				startDelay: 0,
				duration: 2000,
				weakMagnitude: low,
				strongMagnitude: high,
			});
	},
	web_show_cursor: function (show) {
		MTY.gl.canvas.style.cursor = show ? '': 'none';
	},
	web_get_clipboard: function () {
		MTY.clip.focus();
		MTY.clip.select();
		document.execCommand('paste');

		const size = MTY.clip.value.length * 4;
		const text_c = MTY_Alloc(size);
		MTY_StrToC(MTY.clip.value, text_c, size);

		return text_c;
	},
	web_set_clipboard: function (text_c) {
		MTY.clip.value = MTY_StrToJS(text_c);
		MTY.clip.focus();
		MTY.clip.select();
		document.execCommand('copy');
	},
	web_set_pointer_lock: function (enable) {
		if (enable && !document.pointerLockElement) {
			MTY.gl.canvas.requestPointerLock();

		} else if (!enable && document.pointerLockElement) {
			MTY.synthesizeEsc = false;
			document.exitPointerLock();
		}

		MTY.relative = enable;
	},
	web_get_relative: function () {
		return MTY.relative;
	},
	web_has_focus: function () {
		return document.hasFocus();
	},
	web_is_visible: function () {
		if (document.hidden != undefined) {
			return !document.hidden;

		} else if (document.webkitHidden != undefined) {
			return !document.webkitHidden;
		}

		return true;
	},
	web_get_size: function (c_width, c_height) {
		MTY_SetUint32(c_width, MTY.gl.drawingBufferWidth);
		MTY_SetUint32(c_height, MTY.gl.drawingBufferHeight);
	},
	web_get_position: function (c_x, c_y) {
		MTY_SetInt32(c_x, MTY.lastX);
		MTY_SetInt32(c_y, MTY.lastY);
	},
	web_get_screen_size: function (c_width, c_height) {
		MTY_SetUint32(c_width, screen.width);
		MTY_SetUint32(c_height, screen.height);
	},
	web_set_title: function (title) {
		document.title = MTY_StrToJS(title);
	},
	web_use_default_cursor: function (use_default) {
		if (MTY.cursorClass.length > 0) {
			if (use_default) {
				MTY.gl.canvas.classList.remove(MTY.cursorClass);

			} else {
				MTY.gl.canvas.classList.add(MTY.cursorClass);
			}
		}

		MTY.defaultCursor = use_default;
	},
	web_set_png_cursor: function (buffer, size, hot_x, hot_y) {
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
				MTY.gl.canvas.classList.remove(MTY.cursorClass);

			MTY.cursorClass = MTY.cursorCache[b64_png];

			if (!MTY.defaultCursor)
				MTY.gl.canvas.classList.add(MTY.cursorClass);

		} else {
			if (!MTY.defaultCursor && MTY.cursorClass.length > 0)
				MTY.gl.canvas.classList.remove(MTY.cursorClass);

			MTY.cursorClass = '';
		}
	},
	web_get_pixel_ratio: function () {
		return window.devicePixelRatio;
	},
	web_attach_events: function (app, mouse_motion, mouse_button, mouse_wheel, keyboard, focus, drop, resize) {
		MTY.gl.canvas.addEventListener('mousemove', (ev) => {
			let x = mty_scaled(ev.clientX);
			let y = mty_scaled(ev.clientY);

			if (MTY.relative) {
				x = ev.movementX;
				y = ev.movementY;
			}

			MTY_CFunc(mouse_motion)(app, MTY.relative, x, y);
		});

		document.addEventListener('pointerlockchange', (ev) => {
			// Left relative via the ESC key, which swallows a natural ESC keypress
			if (!document.pointerLockElement && MTY.synthesizeEsc) {
				MTY_CFunc(keyboard)(app, true, MTY.keys['Escape'], 0, 0);
				MTY_CFunc(keyboard)(app, false, MTY.keys['Escape'], 0, 0);
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
			MTY_CFunc(mouse_button)(app, true, ev.button, mty_scaled(ev.clientX), mty_scaled(ev.clientY));
		});

		window.addEventListener('mouseup', (ev) => {
			ev.preventDefault();
			MTY_CFunc(mouse_button)(app, false, ev.button, mty_scaled(ev.clientX), mty_scaled(ev.clientY));
		});

		MTY.gl.canvas.addEventListener('contextmenu', (ev) => {
			ev.preventDefault();
		});

		MTY.gl.canvas.addEventListener('wheel', (ev) => {
			let x = ev.deltaX > 0 ? 120 : ev.deltaX < 0 ? -120 : 0;
			let y = ev.deltaY > 0 ? 120 : ev.deltaY < 0 ? -120 : 0;
			MTY_CFunc(mouse_wheel)(app, x, y);
		}, {passive: true});

		window.addEventListener('keydown', (ev) => {
			mty_correct_relative();
			const key = MTY.keys[ev.code];

			if (key != undefined) {
				const text = ev.key.length == 1 ? MTY_StrToC(ev.key, MTY.cbuf, 1024) : 0;

				if (MTY_CFunc(keyboard)(app, true, key, text, mty_get_mods(ev)))
					ev.preventDefault();
			}
		});

		window.addEventListener('keyup', (ev) => {
			const key = MTY.keys[ev.code];

			if (key != undefined)
				if (MTY_CFunc(keyboard)(app, false, key, 0, mty_get_mods(ev)))
					ev.preventDefault();
		});

		MTY.gl.canvas.addEventListener('dragover', (ev) => {
			ev.preventDefault();
		});

		window.addEventListener('blur', (ev) => {
			MTY_CFunc(focus)(app, false);
		});

		window.addEventListener('focus', (ev) => {
			MTY_CFunc(focus)(app, true);
		});

		window.addEventListener('resize', (ev) => {
			MTY_CFunc(resize)(app);
		});

		MTY.gl.canvas.addEventListener('drop', (ev) => {
			ev.preventDefault();

			if (!ev.dataTransfer.items)
				return;

			for (let x = 0; x < ev.dataTransfer.items.length; x++) {
				if (ev.dataTransfer.items[x].kind == 'file') {
					let file = ev.dataTransfer.items[x].getAsFile();

					const reader = new FileReader();
					reader.addEventListener('loadend', (fev) => {
						if (reader.readyState == 2) {
							let buf = new Uint8Array(reader.result);
							let cmem = MTY_Alloc(buf.length);
							MTY_Memcpy(cmem, buf);
							MTY_CFunc(drop)(app, MTY_StrToC(file.name, MTY.cbuf, 1024), cmem, buf.length);
							MTY_Free(cmem);
						}
					});
					reader.readAsArrayBuffer(file);
					break;
				}
			}
		});
	},
	web_set_swap_interval: function (interval) {
		MTY.swapInterval = interval;
	},
	web_raf: function (app, func, controller, move, opaque) {
		// Init position
		MTY.lastX = window.screenX;
		MTY.lastY = window.screenY;

		const step = () => {
			// Poll gamepads
			if (document.hasFocus())
				mty_poll_gamepads(app, controller);

			// Poll position changes
			if (MTY.lastX != window.screenX || MTY.lastY != window.screenY) {
				MTY.lastX = window.screenX;
				MTY.lastY = window.screenY;
				MTY_CFunc(move)(app);
			}

			// Poll size changes and resize the canvas
			const rect = MTY.gl.canvas.getBoundingClientRect();

			MTY.gl.canvas.width = mty_scaled(rect.width);
			MTY.gl.canvas.height = mty_scaled(rect.height);

			// Keep looping recursively or end based on AppFunc return value
			// Don't call the app func if swap interval is higher than 1
			let cont = true;
			if (MTY.frameCtr++ % MTY.swapInterval == 0)
				cont = MTY_CFunc(func)(opaque);

			if (cont) {
				window.requestAnimationFrame(step);

			} else {
				MTY.endFunc();
			}
		};

		window.requestAnimationFrame(step);
		throw 'MTY_AppRun halted execution';
	},
};


// WASI API

// https://github.com/WebAssembly/WASI/blob/master/phases/snapshot/docs.md

function mty_append_buf_to_b64(b64, buf) {
	// FIXME This is a crude way to handle appending to an open file,
	// complex seek operations will break this

	const cur_buf = mty_b64_to_buf(b64);
	const new_buf = new Uint8Array(cur_buf.length + buf.length);

	new_buf.set(cur_buf);
	new_buf.set(buf, cur_buf.length);

	return mty_buf_to_b64(new_buf);
}

function mty_arg_list() {
	const params = new URLSearchParams(window.location.search);
	const qs = params.toString();

	let plist = [MTY.arg0];

	// TODO This would put each key/val pair as a separate arg
	// for (let p of params)
	// 	plist.push(p[0] + '=' + p[1]);

	//return plist;


	// For now treat the entire query string as argv[1]
	if (qs)
		plist.push(qs);

	return plist;
}

const MTY_WASI_API = {
	// Command line arguments
	args_get: function (argv, argv_buf) {
		const args = mty_arg_list();
		for (let x = 0; x < args.length; x++) {
			MTY_StrToC(args[x], argv_buf, 32 * 1024); // FIXME what is the real size of this buffer
			MTY_SetUint32(argv + x * 4, argv_buf);
			argv_buf += args[x].length + 1;
		}

		return 0;
	},
	args_sizes_get: function (argc, argv_buf_size) {
		const args = mty_arg_list();

		MTY_SetUint32(argc, args.length);
		MTY_SetUint32(argv_buf_size, args.join(' ').length + 1);
		return 0;
	},

	// WASI preopened directory (/)
	fd_prestat_get: function (fd, path) {
		return !MTY.preopen ? 0 : 8;
	},
	fd_prestat_dir_name: function (fd, path, path_len) {
		if (!MTY.preopen) {
			MTY_StrToC('/', path, path_len);
			MTY.preopen = true;

			return 0;
		}

		return 28;
	},

	// Paths
	path_filestat_get: function (fd, flags, cpath, _0, filestat_out) {
		const path = MTY_StrToJS(cpath);
		if (localStorage[path]) {
			// We only need to return the size
			const buf = mty_b64_to_buf(localStorage[path]);
			MTY_SetUint64(filestat_out + 32, buf.byteLength);
		}

		return 0;
	},
	path_open: function (fd, dir_flags, path, o_flags, _0, _1, _2, mode, fd_out) {
		const new_fd = MTY.fdIndex++;
		MTY_SetUint32(fd_out, new_fd);

		MTY.fds[new_fd] = {
			path: MTY_StrToJS(path),
			append: mode == 1,
			offset: 0,
		};

		return 0;
	},
	path_create_directory: function () {
		return 0;
	},
	path_remove_directory: function () {
		return 0;
	},
	path_unlink_file: function () {
		return 0;
	},
	path_readlink: function () {
	},
	path_rename: function () {
		console.log('path_rename', arguments);
		return 0;
	},

	// File descriptors
	fd_close: function (fd) {
		delete MTY.fds[fd];
	},
	fd_fdstat_get: function () {
		return 0;
	},
	fd_fdstat_set_flags: function () {
	},
	fd_readdir: function () {
		return 8;
	},
	fd_seek: function (fd, offset, whence, offset_out) {
		return 0;
	},
	fd_read: function (fd, iovs, iovs_len, nread) {
		const finfo = MTY.fds[fd];

		if (finfo && localStorage[finfo.path]) {
			const full_buf = mty_b64_to_buf(localStorage[finfo.path]);

			let ptr = iovs;
			let cbuf = MTY_GetUint32(ptr);
			let cbuf_len = MTY_GetUint32(ptr + 4);
			let len = cbuf_len < full_buf.length ? cbuf_len : full_buf.length;

			let view = new Uint8Array(mty_mem(), cbuf, cbuf_len);
			let slice = new Uint8Array(full_buf.buffer, 0, len);
			view.set(slice);

			MTY_SetUint32(nread, len);
		}

		return 0;
	},
	fd_write: function (fd, iovs, iovs_len, nwritten) {
		// Calculate full write size
		let len = 0;
		for (let x = 0; x < iovs_len; x++)
			len += MTY_GetUint32(iovs + x * 8 + 4);

		MTY_SetUint32(nwritten, len);

		// Create a contiguous buffer
		let offset = 0;
		let full_buf = new Uint8Array(len);
		for (let x = 0; x < iovs_len; x++) {
			let ptr = iovs + x * 8;
			let cbuf = MTY_GetUint32(ptr);
			let cbuf_len = MTY_GetUint32(ptr + 4);

			full_buf.set(new Uint8Array(mty_mem(), cbuf, cbuf_len), offset);
			offset += cbuf_len;
		}

		// stdout
		if (fd == 1) {
			const str = mty_char_to_js(full_buf);
			if (str != '\n')
				console.log(str);

		// stderr
		} else if (fd == 2) {
			const str = mty_char_to_js(full_buf)
			if (str != '\n')
				console.error(str);

		// Filesystem
		} else if (MTY.fds[fd]) {
			const finfo = MTY.fds[fd];
			const cur_b64 = localStorage[finfo.path];

			if (cur_b64 && finfo.append) {
				localStorage[finfo.path] = mty_append_buf_to_b64(cur_b64, full_buf);

			} else {
				localStorage[finfo.path] = mty_buf_to_b64(full_buf, len);
			}

			finfo.offet += len;
		}

		return 0;
	},

	// Misc
	clock_time_get: function (id, precision, time_out) {
		MTY_SetUint64(time_out, Math.round(performance.now() * 1000.0 * 1000.0));
		return 0;
	},
	poll_oneoff: function (sin, sout, nsubscriptions, nevents) {
		return 0;
	},
	proc_exit: function () {
	},
	environ_get: function () {
	},
	environ_sizes_get: function () {
	},
};


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

async function MTY_Start(bin, userEnv, endFunc, glver) {
	MTY.arg0 = bin;

	if (!mty_supports_wasm() || !mty_supports_web_gl())
		return false;

	if (!userEnv)
		userEnv = {};

	if (endFunc)
		MTY.endFunc = endFunc;

	// Set up full window canvas and webgl context
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

	const canvas = document.createElement('canvas');
	canvas.style.width = '100%';
	canvas.style.height = '100%';
	document.body.appendChild(canvas);

	if (glver)
		MTY.glver = glver;

	MTY.gl = canvas.getContext(MTY.glver, {
		depth: false,
		antialias: false,
		premultipliedAlpha: true,
	});

	// Set up the clipboard
	MTY.clip = document.createElement('textarea');
	MTY.clip.style.position = 'absolute';
	MTY.clip.style.left = '-9999px';
	MTY.clip.autofocus = true;
	document.body.appendChild(MTY.clip);

	// Load keyboard map
	if (navigator.keyboard)
		MTY.kbMap = await navigator.keyboard.getLayoutMap();

	// Fetch the wasm file as an ArrayBuffer
	const res = await fetch(bin);
	const buf = await res.arrayBuffer();

	// Create wasm instance (module) from the ArrayBuffer
	MTY.module = await WebAssembly.instantiate(buf, {
		// Custom imports
		env: {
			...MTY_UNISTD_API,
			...MTY_GL_API,
			...MTY_AUDIO_API,
			...MTY_NET_API,
			...MTY_CRYPTO_API,
			...MTY_SYSTEM_API,
			...MTY_WEB_API,
			...userEnv,
		},

		// Current version of WASI we're compiling against, 'wasi_snapshot_preview1'
		wasi_snapshot_preview1: {
			...MTY_WASI_API,
		},
	});

	// Execute the '_start' entry point, this will fetch args and execute the 'main' function
	try {
		MTY.module.instance.exports._start();

	// We expect to catch the 'MTY_AppRun halted execution' exception
	// Otherwise look for an indication of unsupported WASM features
	} catch (e) {
		estr = e.toString();

		if (estr.search('MTY_AppRun') == -1)
			console.error(e);

		// This probably means the browser does not support WASM 64
		return estr.search('i64 not allowed') == -1;
	}

	return true;
}
