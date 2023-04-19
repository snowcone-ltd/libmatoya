// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.


// Worker State

const MTY_W = {
	module: null,
	hostname: '',
	queryString: '',
	cbuf: 0,
	sync: null,

	// Keyboard
	keys: {},
	kbMap: {},
	keysRev: {},

	// Synced state
	fullscreen: false,
	visible: false,
	relative: false,
	hasFocus: false,
	devicePixelRatio: 0,
	screenWidth: 0,
	screenHeight: 0,
	lastX: 0,
	lastY: 0,

	// Exports
	app: 0,
	alloc: 0,
	free: 0,
	mouse_motion: 0,
	mouse_button: 0,
	mouse_wheel: 0,
	keyboard: 0,
	focus: 0,
	drop: 0,
	resize: 0,

	// HTTP
	reqs: {},
	reqIndex: 0,

	// GL
	gl: null,
	glver: '',
	glIndex: 0,
	glObj: {},

	// WASI
	arg0: '',
	fds: {},
	fdIndex: 0,
	preopen: false,
};


// Allocation

function MTY_CFunc(ptr) {
	return MTY_W.module.instance.exports.__indirect_function_table.get(ptr);
}

function MTY_Alloc(size, el) {
	return MTY_CFunc(MTY_W.alloc)(size, el ? el : 1);
}

function MTY_Free(ptr) {
	MTY_CFunc(MTY_W.free)(ptr);
}

function MTY_StrToCD(js_str) {
	const buf = (new TextEncoder()).encode(js_str);
	const ptr = MTY_Alloc(buf.length);
	mty_copy_str(ptr, buf);

	return ptr;
}


// window.localStorage

function mty_get_ls(key) {
	postMessage({
		type: 'get-ls0',
		key: key,
		buf: MTY_W.cbuf,
		sync: MTY_W.sync,
	});

	MTY_Wait(MTY_W.sync);

	const size = MTY_GetUint32(MTY_W.cbuf);
	if (size == 0)
		return 0;

	const cbuf = MTY_Alloc(size);

	postMessage({
		type: 'get-ls1',
		buf: cbuf,
		sync: MTY_W.sync,
	});

	MTY_Wait(MTY_W.sync);

	const buf = new Uint8Array(size);
	buf.set(new Uint8Array(mty_mem(), cbuf, size));
	MTY_Free(cbuf);

	return buf;
}

function mty_set_ls(key, val) {
	postMessage({
		type: 'set-ls',
		key: key,
		val: val,
		sync: MTY_W.sync,
	});

	MTY_Wait(MTY_W.sync);
}


// <unistd.h> stubs

const MTY_UNISTD_API = {
	flock: function (fd, flags) {
		return 0;
	},
};


// GL

function mty_gl_new(obj) {
	MTY_W.glObj[MTY_W.glIndex] = obj;

	return MTY_W.glIndex++;
}

function mty_gl_del(index) {
	let obj = MTY_W.glObj[index];

	MTY_W.glObj[index] = undefined;
	delete MTY_W.glObj[index];

	return obj;
}

function mty_gl_obj(index) {
	return MTY_W.glObj[index];
}

const MTY_GL_API = {
	glGenFramebuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_SetUint32(ids + x * 4, mty_gl_new(MTY_W.gl.createFramebuffer()));
	},
	glDeleteFramebuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_W.gl.deleteFramebuffer(mty_gl_del(MTY_GetUint32(ids + x * 4)));
	},
	glBindFramebuffer: function (target, fb) {
		MTY_W.gl.bindFramebuffer(target, fb ? mty_gl_obj(fb) : null);
	},
	glBlitFramebuffer: function (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter) {
		MTY_W.gl.blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
	},
	glFramebufferTexture2D: function (target, attachment, textarget, texture, level) {
		MTY_W.gl.framebufferTexture2D(target, attachment, textarget, mty_gl_obj(texture), level);
	},
	glEnable: function (cap) {
		MTY_W.gl.enable(cap);
	},
	glIsEnabled: function (cap) {
		return MTY_W.gl.isEnabled(cap);
	},
	glDisable: function (cap) {
		MTY_W.gl.disable(cap);
	},
	glViewport: function (x, y, width, height) {
		MTY_W.gl.viewport(x, y, width, height);
	},
	glGetIntegerv: function (name, data) {
		const p = MTY_W.gl.getParameter(name);

		switch (name) {
			// object
			case MTY_W.gl.READ_FRAMEBUFFER_BINDING:
			case MTY_W.gl.DRAW_FRAMEBUFFER_BINDING:
			case MTY_W.gl.ARRAY_BUFFER_BINDING:
			case MTY_W.gl.TEXTURE_BINDING_2D:
			case MTY_W.gl.CURRENT_PROGRAM:
				MTY_SetUint32(data, mty_gl_new(p));
				break;

			// int32[4]
			case MTY_W.gl.VIEWPORT:
			case MTY_W.gl.SCISSOR_BOX:
				for (let x = 0; x < 4; x++)
					MTY_SetUint32(data + x * 4, p[x]);
				break;

			// int
			case MTY_W.gl.ACTIVE_TEXTURE:
			case MTY_W.gl.BLEND_SRC_RGB:
			case MTY_W.gl.BLEND_DST_RGB:
			case MTY_W.gl.BLEND_SRC_ALPHA:
			case MTY_W.gl.BLEND_DST_ALPHA:
			case MTY_W.gl.BLEND_EQUATION_RGB:
			case MTY_W.gl.BLEND_EQUATION_ALPHA:
				MTY_SetUint32(data, p);
				break;
		}

		MTY_SetUint32(data, p);
	},
	glGetFloatv: function (name, data) {
		switch (name) {
			case MTY_W.gl.COLOR_CLEAR_VALUE:
				const p = MTY_W.gl.getParameter(name);

				for (let x = 0; x < 4; x++)
					MTY_SetFloat(data + x * 4, p[x]);
				break;
		}
	},
	glBindTexture: function (target, texture) {
		MTY_W.gl.bindTexture(target, texture ? mty_gl_obj(texture) : null);
	},
	glDeleteTextures: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_W.gl.deleteTexture(mty_gl_del(MTY_GetUint32(ids + x * 4)));
	},
	glTexParameteri: function (target, pname, param) {
		MTY_W.gl.texParameteri(target, pname, param);
	},
	glGenTextures: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_SetUint32(ids + x * 4, mty_gl_new(MTY_W.gl.createTexture()));
	},
	glTexImage2D: function (target, level, internalformat, width, height, border, format, type, data) {
		MTY_W.gl.texImage2D(target, level, internalformat, width, height, border, format, type,
			new Uint8Array(mty_mem(), data));
	},
	glTexSubImage2D: function (target, level, xoffset, yoffset, width, height, format, type, pixels) {
		MTY_W.gl.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
			new Uint8Array(mty_mem(), pixels));
	},
	glDrawElements: function (mode, count, type, indices) {
		MTY_W.gl.drawElements(mode, count, type, indices);
	},
	glGetAttribLocation: function (program, c_name) {
		return MTY_W.gl.getAttribLocation(mty_gl_obj(program), MTY_StrToJS(c_name));
	},
	glShaderSource: function (shader, count, c_strings, c_len) {
		let source = '';
		for (let x = 0; x < count; x++)
			source += MTY_StrToJS(MTY_GetUint32(c_strings + x * 4));

		MTY_W.gl.shaderSource(mty_gl_obj(shader), source);
	},
	glBindBuffer: function (target, buffer) {
		MTY_W.gl.bindBuffer(target, buffer ? mty_gl_obj(buffer) : null);
	},
	glVertexAttribPointer: function (index, size, type, normalized, stride, pointer) {
		MTY_W.gl.vertexAttribPointer(index, size, type, normalized, stride, pointer);
	},
	glCreateProgram: function () {
		return mty_gl_new(MTY_W.gl.createProgram());
	},
	glUniform1i: function (loc, v0) {
		MTY_W.gl.uniform1i(mty_gl_obj(loc), v0);
	},
	glUniform1f: function (loc, v0) {
		MTY_W.gl.uniform1f(mty_gl_obj(loc), v0);
	},
	glUniform4i: function (loc, v0, v1, v2, v3) {
		MTY_W.gl.uniform4i(mty_gl_obj(loc), v0, v1, v2, v3);
	},
	glUniform4f: function (loc, v0, v1, v2, v3) {
		MTY_W.gl.uniform4f(mty_gl_obj(loc), v0, v1, v2, v3);
	},
	glActiveTexture: function (texture) {
		MTY_W.gl.activeTexture(texture);
	},
	glDeleteBuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_W.gl.deleteBuffer(mty_gl_del(MTY_GetUint32(ids + x * 4)));
	},
	glEnableVertexAttribArray: function (index) {
		MTY_W.gl.enableVertexAttribArray(index);
	},
	glBufferData: function (target, size, data, usage) {
		MTY_W.gl.bufferData(target, new Uint8Array(mty_mem(), data, size), usage);
	},
	glDeleteShader: function (shader) {
		MTY_W.gl.deleteShader(mty_gl_del(shader));
	},
	glGenBuffers: function (n, ids) {
		for (let x = 0; x < n; x++)
			MTY_SetUint32(ids + x * 4, mty_gl_new(MTY_W.gl.createBuffer()));
	},
	glCompileShader: function (shader) {
		MTY_W.gl.compileShader(mty_gl_obj(shader));
	},
	glLinkProgram: function (program) {
		MTY_W.gl.linkProgram(mty_gl_obj(program));
	},
	glGetUniformLocation: function (program, name) {
		return mty_gl_new(MTY_W.gl.getUniformLocation(mty_gl_obj(program), MTY_StrToJS(name)));
	},
	glCreateShader: function (type) {
		return mty_gl_new(MTY_W.gl.createShader(type));
	},
	glAttachShader: function (program, shader) {
		MTY_W.gl.attachShader(mty_gl_obj(program), mty_gl_obj(shader));
	},
	glUseProgram: function (program) {
		MTY_W.gl.useProgram(program ? mty_gl_obj(program) : null);
	},
	glGetShaderiv: function (shader, pname, params) {
		if (pname == 0x8B81) {
			let ok = MTY_W.gl.getShaderParameter(mty_gl_obj(shader), MTY_W.gl.COMPILE_STATUS);
			MTY_SetUint32(params, ok);

			if (!ok)
				console.warn(MTY_W.gl.getShaderInfoLog(mty_gl_obj(shader)));

		} else {
			MTY_SetUint32(params, 0);
		}
	},
	glDetachShader: function (program, shader) {
		MTY_W.gl.detachShader(mty_gl_obj(program), mty_gl_obj(shader));
	},
	glDeleteProgram: function (program) {
		MTY_W.gl.deleteProgram(mty_gl_del(program));
	},
	glClear: function (mask) {
		MTY_W.gl.clear(mask);
	},
	glClearColor: function (red, green, blue, alpha) {
		MTY_W.gl.clearColor(red, green, blue, alpha);
	},
	glGetError: function () {
		return MTY_W.gl.getError();
	},
	glGetShaderInfoLog: function () {
		// FIXME Logged automatically as part of glGetShaderiv
	},
	glFinish: function () {
		MTY_W.gl.finish();
	},
	glScissor: function (x, y, width, height) {
		MTY_W.gl.scissor(x, y, width, height);
	},
	glBlendFunc: function (sfactor, dfactor) {
		MTY_W.gl.blendFunc(sfactor, dfactor);
	},
	glBlendEquation: function (mode) {
		MTY_W.gl.blendEquation(mode);
	},
	glUniformMatrix4fv: function (loc, count, transpose, value) {
		MTY_W.gl.uniformMatrix4fv(mty_gl_obj(loc), transpose, new Float32Array(mty_mem(), value, 4 * 4 * count));
	},
	glBlendEquationSeparate: function (modeRGB, modeAlpha) {
		MTY_W.gl.blendEquationSeparate(modeRGB, modeAlpha);
	},
	glBlendFuncSeparate: function (srcRGB, dstRGB, srcAlpha, dstAlpha) {
		MTY_W.gl.blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
	},
	glGetProgramiv: function (program, pname, params) {
		MTY_SetUint32(params, MTY_W.gl.getProgramParameter(mty_gl_obj(program), pname));
	},
	glPixelStorei: function (pname, param) {
		// GL_UNPACK_ROW_LENGTH is not compatible with WebGL 1
		if (MTY_W.glver == 'webgl' && pname == 0x0CF2)
			return;

		MTY_W.gl.pixelStorei(pname, param);
	},
	web_gl_flush: function () {
		MTY_W.gl.flush();
	},
};


// Audio

function mty_audio_queued_ms() {
	let queued_ms = Math.round((MTY.audio.next_time - MTY.audio.ctx.currentTime) * 1000.0);
	let buffered_ms = Math.round((MTY.audio.offset / 4) / MTY.audio.frames_per_ms);

	return (queued_ms < 0 ? 0 : queued_ms) + buffered_ms;
}

const MTY_AUDIO_API = {
	MTY_AudioCreate: function (sampleRate, minBuffer, maxBuffer, channels, deviceID, fallback) {
		MTY.audio = {};
		MTY.audio.flushing = false;
		MTY.audio.playing = false;
		MTY.audio.sample_rate = sampleRate;
		MTY.audio.channels = channels;

		MTY.audio.frames_per_ms = Math.round(sampleRate / 1000.0);
		MTY.audio.min_buffer = minBuffer * MTY.audio.frames_per_ms;
		MTY.audio.max_buffer = maxBuffer * MTY.audio.frames_per_ms;

		MTY.audio.offset = 0;
		MTY.audio.buf = MTY_Alloc(sampleRate * 2 * MTY.audio.channels);

		return 0xCDD;
	},
	MTY_AudioDestroy: function (audio) {
		if (!MTY.audio)
			return;

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
			let size = count * 2 * MTY.audio.channels;
			MTY_Memcpy(MTY.audio.buf + MTY.audio.offset, new Uint8Array(mty_mem(), frames, size));
			MTY.audio.offset += size;
		}

		// Begin playing again if the buffer has accumulated past the min
		if (!MTY.audio.playing && !MTY.audio.flushing &&
			MTY.audio.offset / (2 * MTY.audio.channels) > MTY.audio.min_buffer)
		{
			MTY.audio.next_time = MTY.audio.ctx.currentTime;
			MTY.audio.playing = true;
		}

		// Queue the audio if playing
		if (MTY.audio.playing) {
			const src = new Int16Array(mty_mem(), MTY.audio.buf);
			const bcount = MTY.audio.offset / (2 * MTY.audio.channels);

			const buf = MTY.audio.ctx.createBuffer(MTY.audio.channels, bcount, MTY.audio.sample_rate);

			const chans = [];
			for (let x = 0; x < MTY.audio.channels; x++)
				chans[x] = buf.getChannelData(x);

			let offset = 0;
			for (let x = 0; x < bcount * MTY.audio.channels; x += MTY.audio.channels) {
				for (y = 0; y < MTY.audio.channels; y++) {
					chans[y][offset] = src[x + y] / 32768;
					offset++;
				}
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
		cpath, cheaders, cbody, bodySize, timeout, image)
	{
		const req = ++MTY_W.reqIndex;
		MTY_SetUint32(index, req);

		MTY_W.reqs[req] = {
			async: MTY_ASYNC_CONTINUE,
			image: image,
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
			const data = MTY_W.reqs[req];
			data.status = response.status;

			return response.arrayBuffer();

		}).then((body) => {
			const data = MTY_W.reqs[req];
			data.response = new Uint8Array(body);
			data.async = MTY_ASYNC_OK;

		}).catch((err) => {
			const data = MTY_W.reqs[req];
			console.error(err);
			data.status = 0;
			data.async = MTY_ASYNC_ERROR;
		});
	},
	MTY_HttpAsyncPoll: function(index, response, responseSize, code) {
		const data = MTY_W.reqs[index];

		// Unknown index or request has already been polled
		if (data == undefined || data.async == MTY_ASYNC_DONE)
			return MTY_ASYNC_DONE;

		// Request is in progress
		if (data.async == MTY_ASYNC_CONTINUE)
			return MTY_ASYNC_CONTINUE;

		// Request is has completed asynchronously, check if there is a response
		if (data.response != undefined) {

			// Optionally decompress an image on a successful response
			const res_ok = data.status >= 200 && data.status < 300;
			const req_ok = data.async == MTY_ASYNC_OK;

			if (data.image && req_ok && res_ok) {
				data.async = MTY_ASYNC_CONTINUE;
				data.image = false;

				const size = data.response.length;
				const buf = MTY_Alloc(size);
				MTY_Memcpy(buf, data.response);

				const cwidth = MTY_W.cbuf;
				const cheight = MTY_W.cbuf + 4;
				const cimage = MTY_DecompressImage(buf, size, cwidth, cheight);

				data.width = MTY_GetUint32(cwidth);
				data.height = MTY_GetUint32(cheight);
				data.response = new Uint8Array(mty_mem(), cimage, data.width * data.height * 4);
				data.async = MTY_ASYNC_OK;

				return MTY_ASYNC_CONTINUE;
			}

			// Set C status code
			MTY_SetUint32(code, data.status);

			// Set C response size
			if (data.width && data.height) {
				MTY_SetUint32(responseSize, data.width | data.height << 16);

			} else {
				MTY_SetUint32(responseSize, data.response.length);
			}

			// Allocate C buffer and set return pointer
			if (data.buf == undefined) {
				data.buf = MTY_Alloc(data.response.length + 1);
				MTY_Memcpy(data.buf, data.response);
			}

			MTY_SetUint32(response, data.buf);
		}

		const r = data.async;
		data.async = MTY_ASYNC_DONE;

		return r;
	},
	MTY_HttpAsyncClear: function (index) {
		const req = MTY_GetUint32(index);
		const data = MTY_W.reqs[req];

		if (data == undefined)
			return;

		MTY_Free(data.buf);
		delete MTY_W.reqs[req];

		MTY_SetUint32(index, 0);
	},
};


// Image

function MTY_DecompressImage(input, size, cwidth, cheight) {
	postMessage({
		type: 'image0',
		input: input,
		size: size,
		buf: MTY_W.cbuf,
		sync: MTY_W.sync,
	});

	MTY_Wait(MTY_W.sync);

	const width = MTY_GetUint32(MTY_W.cbuf);
	const height = MTY_GetUint32(MTY_W.cbuf + 4);
	const cimage = MTY_Alloc(width * height * 4);

	postMessage({
		type: 'image1',
		buf: cimage,
		sync: MTY_W.sync,
	});

	MTY_Wait(MTY_W.sync);

	MTY_SetUint32(cwidth, width);
	MTY_SetUint32(cheight, height);

	return cimage;
}

const MTY_IMAGE_API = {
	MTY_DecompressImage: MTY_DecompressImage,
};


// Crypto

const MTY_CRYPTO_API = {
	MTY_CryptoHash: function (algo, input, inputSize, key, keySize, output, outputSize) {
	},
	MTY_GetRandomBytes: function (buf, size) {
		const cpy = new Uint8Array(size);
		crypto.getRandomValues(cpy);

		const jbuf = new Uint8Array(mty_mem(), buf, size);
		jbuf.set(cpy);
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

const MTY_WEB_API = {
	web_alert: function (title, msg) {
		postMessage({type: 'alert', title, msg});
	},
	web_set_fullscreen: function (fullscreen) {
		postMessage({type: 'fullscreen', fullscreen});
	},
	web_wake_lock: function (enable) {
		postMessage({type: 'wake-lock', enable});
	},
	web_rumble_gamepad: function (id, low, high) {
		postMessage({type: 'rumble', id, low, high});
	},
	web_show_cursor: function (show) {
		postMessage({type: 'show-cursor', show});
	},
	web_get_clipboard: function () {
		postMessage({type: 'get-clip0', sync: MTY_W.sync, buf: MTY_W.cbuf});
		MTY_Wait(MTY_W.sync);

		const size = MTY_GetUint32(MTY_W.cbuf);
		const buf = MTY_Alloc(size + 1);

		postMessage({type: 'get-clip1', sync: MTY_W.sync, buf: buf});
		MTY_Wait(MTY_W.sync);

		return buf;
	},
	web_set_clipboard: function (text) {
		postMessage({type: 'set-clip', text});
	},
	web_set_pointer_lock: function (enable) {
		postMessage({type: 'pointer-lock', enable});
	},
	web_use_default_cursor: function (use_default) {
		postMessage({type: 'cursor-default', use_default});
	},
	web_set_png_cursor: function (buffer, size, hot_x, hot_y) {
		postMessage({type: 'cursor', buffer, size, hot_x, hot_y});
	},
	web_get_relative: function () {
		return MTY_W.relative;
	},
	web_get_hostname: function () {
		return MTY_StrToCD(MTY_W.hostname);
	},
	web_get_fullscreen: function () {
		return MTY_W.fullscreen;
	},
	web_platform: function (platform, size) {
		MTY_StrToC(navigator.platform, platform, size);
	},
	web_set_mem_funcs: function (alloc, free) {
		MTY_W.alloc = alloc;
		MTY_W.free = free;

		const csync = MTY_Alloc(4);
		MTY_W.sync = new Int32Array(mty_mem(), csync, 1);

		// Global buffer for scratch heap space
		MTY_W.cbuf = MTY_Alloc(2048);
	},
	web_set_key: function (reverse, code, key) {
		const str = MTY_StrToJS(code);
		MTY_W.keys[str] = key;

		if (reverse)
			MTY_W.keysRev[key] = str;
	},
	web_get_key: function (key, cbuf, len) {
		const code = MTY_W.keysRev[key];

		if (code != undefined) {
			const text = MTY_W.kbMap[code];
			if (text) {
				MTY_StrToC(text.toUpperCase(), cbuf, len);
				return true;
			}

			MTY_StrToC(code, cbuf, len);
			return true;
		}

		return false;
	},
	web_set_title: function (title) {
		postMessage({
			type: 'title',
			title: MTY_StrToJS(title),
		});
	},
	web_has_focus: function () {
		return MTY_W.hasFocus;
	},
	web_is_visible: function () {
		return MTY_W.visible;
	},
	web_get_size: function (c_width, c_height) {
		MTY_SetUint32(c_width, MTY_W.gl.drawingBufferWidth);
		MTY_SetUint32(c_height, MTY_W.gl.drawingBufferHeight);
	},
	web_get_position: function (c_x, c_y) {
		MTY_SetInt32(c_x, MTY_W.lastX);
		MTY_SetInt32(c_y, MTY_W.lastY);
	},
	web_get_screen_size: function (c_width, c_height) {
		MTY_SetUint32(c_width, MTY_W.screenWidth);
		MTY_SetUint32(c_height, MTY_W.screenHeight);
	},
	web_get_pixel_ratio: function () {
		return MTY_W.devicePixelRatio;
	},
	web_attach_events: function (app, mouse_motion, mouse_button, mouse_wheel, keyboard,
		focus, drop, resize, controller, move)
	{
		MTY_W.app = app;
		MTY_W.mouse_motion = mouse_motion;
		MTY_W.mouse_button = mouse_button;
		MTY_W.mouse_wheel = mouse_wheel;
		MTY_W.keyboard = keyboard;
		MTY_W.focus = focus;
		MTY_W.drop = drop;
		MTY_W.resize = resize;
		MTY_W.controller = controller;
		MTY_W.move = move;
	},
	web_raf: function (func, opaque) {
		const step = () => {
			// Keep looping recursively or end based on AppFunc return value
			if (MTY_CFunc(func)(opaque))
				requestAnimationFrame(step);
		};

		requestAnimationFrame(step);
		throw 'MTY_AppRun halted execution';
	},
};


// WASI API

// https://github.com/WebAssembly/WASI/blob/master/phases/snapshot/docs.md

function mty_append_buf_to_b64(cur_buf, buf) {
	// FIXME This is a crude way to handle appending to an open file,
	// complex seek operations will break this

	const new_buf = new Uint8Array(cur_buf.length + buf.length);

	new_buf.set(cur_buf);
	new_buf.set(buf, cur_buf.length);

	return mty_buf_to_b64(new_buf);
}

function mty_arg_list(args) {
	const params = new URLSearchParams(args);
	const qs = params.toString();

	let plist = [MTY_W.arg0];

	// TODO This would put each key/val pair as a separate arg
	// for (let p of params)
	// 	plist.push(p[0] + '=' + p[1]);

	//return plist;

	// For now treat the entire query string as argv[1]
	if (qs)
		plist.push(qs);

	return plist;
}

const MTY_WASI_SNAPSHOT_PREVIEW1_API = {
	// Command line arguments
	args_get: function (argv, argv_buf) {
		const args = mty_arg_list(MTY_W.queryString);
		for (let x = 0; x < args.length; x++) {
			MTY_StrToC(args[x], argv_buf, 32 * 1024); // FIXME what is the real size of this buffer
			MTY_SetUint32(argv + x * 4, argv_buf);
			argv_buf += args[x].length + 1;
		}

		return 0;
	},
	args_sizes_get: function (argc, argv_buf_size) {
		const args = mty_arg_list(MTY_W.queryString);

		MTY_SetUint32(argc, args.length);
		MTY_SetUint32(argv_buf_size, args.join(' ').length + 1);
		return 0;
	},

	// WASI preopened directory (/)
	fd_prestat_get: function (fd, path) {
		return !MTY_W.preopen ? 0 : 8;
	},
	fd_prestat_dir_name: function (fd, path, path_len) {
		if (!MTY_W.preopen) {
			MTY_StrToC('/', path, path_len);
			MTY_W.preopen = true;

			return 0;
		}

		return 28;
	},

	// Paths
	path_filestat_get: function (fd, flags, cpath, _0, filestat_out) {
		const path = MTY_StrToJS(cpath);
		const buf = mty_get_ls(path);

		if (buf) {
			// We only need to return the size
			MTY_SetUint64(filestat_out + 32, buf.byteLength);
		}

		return 0;
	},
	path_open: function (fd, dir_flags, path, o_flags, _0, _1, _2, mode, fd_out) {
		const new_fd = MTY_W.fdIndex++;
		MTY_SetUint32(fd_out, new_fd);

		MTY_W.fds[new_fd] = {
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
		delete MTY_W.fds[fd];
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
		const finfo = MTY_W.fds[fd];
		const full_buf = mty_get_ls(finfo.path);

		if (finfo && full_buf) {
			let total = 0;

			for (let x = 0; x < iovs_len; x++) {
				let ptr = iovs + x * 8;
				let cbuf = MTY_GetUint32(ptr);
				let cbuf_len = MTY_GetUint32(ptr + 4);
				let len = cbuf_len < full_buf.length - total ? cbuf_len : full_buf.length - total;

				let view = new Uint8Array(mty_mem(), cbuf, cbuf_len);
				let slice = new Uint8Array(full_buf.buffer, total, len);
				view.set(slice);

				total += len;
			}

			MTY_SetUint32(nread, total);
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
			const str = mty_buf_to_js_str(full_buf);
			if (str != '\n')
				console.log(str);

		// stderr
		} else if (fd == 2) {
			const str = mty_buf_to_js_str(full_buf)
			if (str != '\n')
				console.error(str);

		// Filesystem
		} else if (MTY_W.fds[fd]) {
			const finfo = MTY_W.fds[fd];
			const cur_buf = mty_get_ls(finfo.path);

			if (cur_buf && finfo.append) {
				mty_set_ls(finfo.path, mty_append_buf_to_b64(cur_buf, full_buf));

			} else {
				mty_set_ls(finfo.path, mty_buf_to_b64(full_buf, len));
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
		MTY_SetUint32(sout + 8, 0);
		return 0;
	},
	proc_exit: function () {
	},
	environ_get: function () {
	},
	environ_sizes_get: function () {
	},
	sched_yield: function () {
	},
};

const MTY_WASI_API = {
	'thread-spawn': function () {
		console.log(arguments);
	},
};


// Entry

async function mty_start(bin, userEnv) {
	if (!userEnv)
		userEnv = [];

	// Fetch the wasm file as an ArrayBuffer
	const res = await fetch(bin);
	const buf = await res.arrayBuffer();

	// Imports
	const imports = {
		// Custom imports
		env: {
			memory: MTY.memory,
			...MTY_UNISTD_API,
			...MTY_GL_API,
			...MTY_AUDIO_API,
			...MTY_NET_API,
			...MTY_IMAGE_API,
			...MTY_CRYPTO_API,
			...MTY_SYSTEM_API,
			...MTY_WEB_API,
		},

		// Current version of WASI we're compiling against, 'wasi_snapshot_preview1'
		wasi_snapshot_preview1: {
			...MTY_WASI_SNAPSHOT_PREVIEW1_API,
		},

		wasi: {
			...MTY_WASI_API,
		},
	}

	// Add userEnv to imports
	for (let x = 0; x < userEnv.length; x++) {
		const key = userEnv[x];

		imports.env[key] = function () {
			const args = [];
			for (let y = 0; y < arguments.length; y++)
				args.push(arguments[y]);

			postMessage({
				type: 'user-env',
				name: key,
				args: args,
				rbuf: MTY_W.cbuf,
				sync: MTY_W.sync,
			});

			MTY_Wait(MTY_W.sync);

			return MTY_GetInt32(MTY_W.cbuf);
		};
	}

	// Create wasm instance (module) from the ArrayBuffer
	MTY_W.module = await WebAssembly.instantiate(buf, imports);

	// Execute the '_start' entry point, this will fetch args and execute the 'main' function
	try {
		MTY_W.module.instance.exports._start();

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

onmessage = (ev) => {
	const msg = ev.data;

	switch (msg.type) {
		case 'init':
			importScripts('/lib/matoya.js');

			MTY.memory = msg.memory;

			MTY_W.queryString = msg.args;
			MTY_W.hostname = msg.hostname;
			MTY_W.arg0 = msg.bin;
			MTY_W.fdIndex = 64;
			MTY_W.kbMap = msg.kbMap;
			MTY_W.glver = msg.glver ? msg.glver : 'webgl';

			MTY_W.gl = msg.canvas.getContext(MTY_W.glver, {
				depth: false,
				antialias: false,
				premultipliedAlpha: true,
			});

			mty_start(msg.bin, msg.userEnv);
			break;
		case 'raf':
			MTY_W.lastX = msg.lastX;
			MTY_W.lastY = msg.lastY;
			MTY_W.hasFocus = msg.hasFocus;
			MTY_W.screenWidth = msg.screenWidth;
			MTY_W.screenHeight = msg.screenHeight;
			MTY_W.fullscreen = msg.fullscreen;
			MTY_W.devicePixelRatio = msg.devicePixelRatio;
			MTY_W.visible = msg.visible;
			MTY_W.relative = msg.relative;

			if (MTY_W.gl) {
				MTY_W.gl.canvas.width = msg.canvasWidth;
				MTY_W.gl.canvas.height = msg.canvasHeight;
			}
			break;
		case 'key':
			if (!MTY_W.keyboard)
				break;

			const key = MTY_W.keys[msg.code];

			if (key != undefined) {
				const text = msg.key.length == 1 ? MTY_StrToC(msg.key, MTY_W.cbuf, 2048) : 0;
				MTY_CFunc(MTY_W.keyboard)(MTY_W.app, msg.pressed, key, text, msg.mods);
			}
			break;
		case 'motion':
			if (MTY_W.mouse_motion)
				MTY_CFunc(MTY_W.mouse_motion)(MTY_W.app, msg.relative, msg.x, msg.y);
			break;
		case 'button':
			if (MTY_W.mouse_button)
				MTY_CFunc(MTY_W.mouse_button)(MTY_W.app, msg.pressed, msg.button, msg.x, msg.y);
			break;
		case 'wheel':
			if (MTY_W.mouse_wheel)
				MTY_CFunc(MTY_W.mouse_wheel)(MTY_W.app, msg.x, msg.y);
			break;
		case 'move':
			if (MTY_W.move)
				MTY_CFunc(MTY_W.move)(MTY_W.app);
			break;
		case 'resize':
			if (MTY_W.resize)
				MTY_CFunc(MTY_W.resize)(MTY_W.app);
			break;
		case 'focus':
			if (MTY_W.focus)
				MTY_CFunc(MTY_W.focus)(MTY_W.app, msg.focus);
			break;
		case 'gamepad':
			if (MTY_W.controller)
				MTY_CFunc(MTY_W.controller)(MTY_W.app, msg.id, msg.state, msg.buttons, msg.lx,
					msg.ly, msg.rx, msg.ry, msg.lt, msg.rt);
			break;
		case 'disconnect':
			if (MTY_W.controller)
				MTY_CFunc(MTY_W.controller)(MTY_W.app, msg.id, msg.state, 0, 0, 0, 0, 0, 0, 0);
			break;
		case 'drop':
			if (!MTY_W.drop)
				break;

			const buf = new Uint8Array(msg.data);
			const cmem = MTY_Alloc(buf.length);
			MTY_Memcpy(cmem, buf);

			const name_c = MTY_StrToC(msg.name, MTY_W.cbuf, 2048);

			MTY_CFunc(MTY_W.drop)(MTY_W.app, name_c, cmem, buf.length);

			MTY_Free(cmem);
			break;
	}
};
