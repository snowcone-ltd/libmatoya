// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.


// Worker State

const MTY_W = {
	// Keyboard
	keys: {},
	keysRev: {},

	// GL
	glIndex: 0,
	glObj: {},

	// WASI
	fds: {},
	fdIndex: 0,
};


// Allocation

function mty_cfunc(ptr) {
	return MTY_W.exports.__indirect_function_table.get(ptr);
}

function mty_alloc(size, el) {
	return MTY_W.exports.system_alloc(size, el ? el : 1);
}

function mty_free(ptr) {
	MTY_W.exports.system_free(ptr);
}


// window.localStorage

function mty_get_ls(key) {
	postMessage({
		type: 'get-ls',
		key: key,
		sab: MTY_W.sab,
		sync: MTY_W.sync,
	});

	mty_wait(MTY_W.sync);

	const size = MTY_W.sab[0];
	if (size == 0)
		return 0;

	const sab8 = new Uint8Array(new SharedArrayBuffer(size));

	postMessage({
		type: 'async-copy',
		sab8: sab8,
		sync: MTY_W.sync,
	});

	mty_wait(MTY_W.sync);

	return sab8;
}

function mty_set_ls(key, val) {
	postMessage({
		type: 'set-ls',
		key: key,
		val: val,
		sync: MTY_W.sync,
	});

	mty_wait(MTY_W.sync);
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
	glFramebufferTexture2D: function (target, attachment, textarget, texture, level) {
		MTY_W.gl.framebufferTexture2D(target, attachment, textarget, mty_gl_obj(texture), level);
	},
	glEnable: function (cap) {
		MTY_W.gl.enable(cap);
	},
	glDisable: function (cap) {
		MTY_W.gl.disable(cap);
	},
	glViewport: function (x, y, width, height) {
		MTY_W.gl.viewport(x, y, width, height);
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
			new Uint8Array(MTY_MEMORY.buffer, data));
	},
	glTexSubImage2D: function (target, level, xoffset, yoffset, width, height, format, type, pixels) {
		MTY_W.gl.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
			new Uint8Array(MTY_MEMORY.buffer, pixels));
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
		MTY_W.gl.bufferData(target, new Uint8Array(MTY_MEMORY.buffer, data, size), usage);
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
	glGetShaderInfoLog: function (shader, maxLength, length, infoLog) {
		const log = gl.getShaderInfoLog(mty_gl_obj(shader));
		const buf = new TextEncoder().encode();

		if (buf.length < maxLength) {
			MTY_SetUint32(length);
			MTY_Strcpy(infoLog, buf);
		}
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
		MTY_W.gl.uniformMatrix4fv(mty_gl_obj(loc), transpose, new Float32Array(MTY_MEMORY.buffer, value, 4 * 4 * count));
	},
	glGetProgramiv: function (program, pname, params) {
		MTY_SetUint32(params, MTY_W.gl.getProgramParameter(mty_gl_obj(program), pname));
	},
	glPixelStorei: function (pname, param) {
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
		MTY.audio.buf = mty_alloc(sampleRate * 2 * MTY.audio.channels);

		return 0xCDD;
	},
	MTY_AudioDestroy: function (audio) {
		if (!MTY.audio)
			return;

		mty_free(MTY.audio.buf);
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
			MTY_Memcpy(MTY.audio.buf + MTY.audio.offset, new Uint8Array(MTY_MEMORY.buffer, frames, size));
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
			const src = new Int16Array(MTY_MEMORY.buffer, MTY.audio.buf);
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

function mty_net_args(base_scheme, chost, port, secure, cpath, cheaders) {
	const jport = port != 0 ? ':' + port.toString() : '';
	const scheme = secure ? base_scheme + 's' : base_scheme;
	const host = MTY_StrToJS(chost);
	const path = MTY_StrToJS(cpath);
	const headers_str = MTY_StrToJS(cheaders);
	const url = scheme + '://' + host + jport + path;

	const headers = {};
	const headers_nl = headers_str.split('\n');
	for (let x = 0; x < headers_nl.length; x++) {
		const pair = headers_nl[x];
		const pair_split = pair.split(':');

		if (pair_split[0] && pair_split[1])
			headers[pair_split[0]] = pair_split[1];
	}

	return {
		url,
		headers,
	};
}

const MTY_NET_API = {
	MTY_HttpRequest: function (chost, port, secure, cmethod, cpath, cheaders, cbody, bodySize,
		timeout, response, responseSize, cstatus)
	{
		// FIXME timeout is currently ignored

		let body = null;
		if (cbody) {
			body = new Uint8Array(bodySize);
			body.set(new Uint8Array(MTY_MEMORY.buffer, cbody, bodySize));
		}

		const method = MTY_StrToJS(cmethod);
		const args = mty_net_args('http', chost, port, secure, cpath, cheaders);

		postMessage({
			type: 'http',
			url: args.url,
			method: method,
			headers: args.headers,
			body: body,
			sync: MTY_W.sync,
			sab: MTY_W.sab,
		});

		mty_wait(MTY_W.sync);

		const error = MTY_W.sab[0];
		if (error)
			return false;

		const size = MTY_W.sab[1];
		MTY_SetUint32(responseSize, size);

		const status = MTY_W.sab[2];
		MTY_SetUint16(cstatus, status);

		if (size > 0) {
			const buf = mty_alloc(size + 1);
			MTY_SetUint32(response, buf);

			postMessage({
				type: 'async-copy',
				sync: MTY_W.sync,
				sab8: new Uint8Array(MTY_MEMORY.buffer, buf, size + 1),
			});

			mty_wait(MTY_W.sync);
		}

		return true;
	},
	MTY_WebSocketConnect: function (chost, port, secure, cpath, cheaders, timeout, upgrade_status_out) {
		// FIXME timeout is currently ignored
		// FIXME headers are currently ignored
		// FIXME upgrade_status_out currently unsupported

		const args = mty_net_args('ws', chost, port, secure, cpath, cheaders);

		postMessage({
			type: 'ws',
			url: args.url,
			sync: MTY_W.sync,
			sab: MTY_W.sab,
		});

		mty_wait(MTY_W.sync);

		return MTY_W.sab[0];
	},
	MTY_WebSocketDestroy: function (ctx_out) {
		if (!ctx_out)
			return;

		postMessage({
			type: 'ws-close',
			ctx: MTY_GetUint32(ctx_out),
		});
	},
	MTY_WebSocketRead: function (ctx, timeout, msg_out, size) {
		postMessage({
			type: 'ws-read',
			ctx: ctx,
			timeout: timeout,
			buf: msg_out,
			size: size,
			sab: MTY_W.sab,
			sync: MTY_W.sync,
		});

		mty_wait(MTY_W.sync);

		return MTY_W.sab[0]; // MTY_Async
	},
	MTY_WebSocketWrite: function (ctx, msg_c) {
		postMessage({
			type: 'ws-write',
			ctx: ctx,
			text: MTY_StrToJS(msg_c),
		});

		return true;
	},
	MTY_WebSocketGetCloseCode: function (ctx) {
		postMessage({
			type: 'ws-code',
			ctx: ctx,
			sab: MTY_W.sab,
			sync: MTY_W.sync,
		});

		mty_wait(MTY_W.sync);

		return MTY_W.sab[0];
	},
};


// Image

const MTY_IMAGE_API = {
	MTY_DecompressImage: function (input, size, cwidth, cheight) {
		const jinput = new ArrayBuffer(size);
		new Uint8Array(jinput).set(new Uint8Array(MTY_MEMORY.buffer, input, size));

		postMessage({
			type: 'image',
			input: jinput,
			sync: MTY_W.sync,
			sab: MTY_W.sab,
		}, [jinput]);

		mty_wait(MTY_W.sync);

		const width = MTY_W.sab[0];
		const height = MTY_W.sab[1];

		const buf_size = width * height * 4;
		const buf = mty_alloc(buf_size);

		postMessage({
			type: 'async-copy',
			sync: MTY_W.sync,
			sab8: new Uint8Array(MTY_MEMORY.buffer, buf, buf_size),
		});

		mty_wait(MTY_W.sync);

		MTY_SetUint32(cwidth, width);
		MTY_SetUint32(cheight, height);

		return buf;
	},
	MTY_CompressImage: function (method, input, width, height, outputSize) {
	},
	MTY_GetProgramIcon: function (path, width, height) {
	},
};


// Crypto

const MTY_CRYPTO_API = {
	MTY_CryptoHash: function (algo, input, inputSize, key, keySize, output, outputSize) {
	},
	MTY_GetRandomBytes: function (buf, size) {
		const cpy = new Uint8Array(size);
		crypto.getRandomValues(cpy);

		const jbuf = new Uint8Array(MTY_MEMORY.buffer, buf, size);
		jbuf.set(cpy);
	},
};


// System

const MTY_SYSTEM_API = {
	MTY_HandleProtocol: function (uri, token) {
		postMessage({type: 'uri', uri});
	},
};


// Web API (mostly used in app.c)

function mty_update_window(app, info) {
	MTY_W.exports.window_update_position(app, info.posX, info.posY);
	MTY_W.exports.window_update_screen(app, info.screenWidth, info.screenHeight);
	MTY_W.exports.window_update_size(app, info.canvasWidth, info.canvasHeight);
	MTY_W.exports.window_update_focus(app, info.hasFocus);
	MTY_W.exports.window_update_fullscreen(app, info.fullscreen);
	MTY_W.exports.window_update_visibility(app, info.visible);
	MTY_W.exports.window_update_pixel_ratio(app, info.devicePixelRatio);
	MTY_W.exports.window_update_relative_mouse(app, info.relative);
}

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
		postMessage({type: 'get-clip', sync: MTY_W.sync, sab: MTY_W.sab});
		mty_wait(MTY_W.sync);

		const size = MTY_W.sab[0];
		const buf = mty_alloc(size + 1);

		if (size > 0) {
			postMessage({
				type: 'async-copy',
				sync: MTY_W.sync,
				sab8: new Uint8Array(MTY_MEMORY.buffer, buf, size + 1),
			});

			mty_wait(MTY_W.sync);
		}

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
	web_set_kb_grab: function (grab) {
		postMessage({type: 'kb-grab', grab});
	},
	web_get_hostname: function () {
		const buf = new TextEncoder().encode(MTY_W.hostname);
		const ptr = mty_alloc(buf.length);
		MTY_Strcpy(ptr, buf);

		return ptr;
	},
	web_platform: function (platform, size) {
		MTY_StrToC(navigator.platform, platform, size);
	},
	web_set_key: function (reverse, code, key) {
		const str = MTY_StrToJS(code);
		MTY_W.keys[str] = key;

		if (reverse)
			MTY_W.keysRev[key] = str;
	},
	web_get_key: function (key, buf, len) {
		const code = MTY_W.keysRev[key];

		if (code != undefined) {
			const text = MTY_W.kbMap[code];
			if (text) {
				MTY_StrToC(text.toUpperCase(), buf, len);

			} else {
				MTY_StrToC(code, buf, len);
			}

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
	web_set_gfx: function () {
		const info = MTY_W.initWindowInfo;
		const canvas = new OffscreenCanvas(info.canvasWidth, info.canvasHeight);

		MTY_W.gl = canvas.getContext('webgl2', {
			depth: false,
			antialias: false,
		});
	},
	web_set_canvas_size: function (width, height) {
		MTY_W.gl.canvas.width = width;
		MTY_W.gl.canvas.height = height;
	},
	web_present: function (wait) {
		const image = MTY_W.gl.canvas.transferToImageBitmap();

		postMessage({
			type: 'present',
			image: image,
		}, [image]);

		if (wait)
			mty_wait(MTY_W.psync);
	},

	// Synchronization from C
	MTY_WaitPtr: function (csync) {
		mty_wait(new Int32Array(MTY_MEMORY.buffer, csync, 1));
	},

	// Should be called on main thread only
	web_set_app: function (app) {
		MTY_W.app = app;
		mty_update_window(app, MTY_W.initWindowInfo);
	},
	web_run_and_yield: function (iter, opaque) {
		MTY_W.exports.app_set_keys();

		const step = () => {
			if (mty_cfunc(iter)(opaque))
				setTimeout(step, 0);
		};

		setTimeout(step, 0);
		throw 'MTY_RunAndYield halted execution';
	},
};


// WASI API

// github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/headers/public/wasi/api.h

function mty_append_buf_to_b64(cur_buf, buf) {
	// FIXME This is a crude way to handle appending to an open file,
	// complex seek operations will break this

	const new_buf = new Uint8Array(cur_buf.length + buf.length);

	new_buf.set(cur_buf);
	new_buf.set(buf, cur_buf.length);

	return mty_buf_to_b64(new_buf);
}

function mty_arg_list(bin, args) {
	let plist = [new TextEncoder().encode(bin)];

	const params = new URLSearchParams(args);
	const qs = params.toString();

	if (qs)
		plist.push(new TextEncoder().encode(qs));

	return plist;
}

const MTY_WASI_SNAPSHOT_PREVIEW1_API = {
	// Command line arguments
	args_get: function (argv, argv_buf) {
		const args = mty_arg_list(MTY_W.bin, MTY_W.queryString);

		for (let x = 0; x < args.length; x++) {
			MTY_Strcpy(argv_buf, args[x]);
			MTY_SetUint32(argv + x * 4, argv_buf);
			argv_buf += args[x].length + 1;
		}

		return 0;
	},
	args_sizes_get: function (retptr0, retptr1) {
		const args = mty_arg_list(MTY_W.bin, MTY_W.queryString);

		let total_len = 0;
		for (let x = 0; x < args.length; x++)
			total_len += args[x].length + 1;

		MTY_SetUint32(retptr0, args.length);
		MTY_SetUint32(retptr1, total_len);
		return 0;
	},

	// WASI preopened directory (/)
	fd_prestat_get: function (fd, retptr0) {
		if (MTY_W.preopen == undefined) {
			MTY_SetInt8(retptr0, 0);
			MTY_SetUint64(retptr0 + 4, 1);
			MTY_W.preopen = fd;
			return 0;
		}

		return 8;
	},
	fd_prestat_dir_name: function (fd, path, path_len) {
		if (MTY_W.preopen == fd) {
			MTY_Strcpy(path, new TextEncoder().encode('/'));
			return 0;
		}

		return 28;
	},

	// Paths
	path_filestat_get: function (fd, flags, path, path_size, retptr0) {
		const jpath = MTY_StrToJS(path);
		const buf = mty_get_ls(jpath);

		if (buf) {
			// We only need to return the size
			MTY_SetUint64(retptr0 + 32, buf.byteLength);
		}

		return 0;
	},
	path_open: function (fd, dirflags, path, path_size, oflags, fs_rights_base, fs_rights_inheriting, fdflags, retptr0) {
		const new_fd = MTY_W.fdIndex++;
		MTY_SetUint32(retptr0, new_fd);

		MTY_W.fds[new_fd] = {
			path: MTY_StrToJS(path),
			append: fdflags == 1,
			offset: 0,
		};

		return 0;
	},
	path_create_directory: function (fd, path) {
		return 0;
	},
	path_remove_directory: function (fd, path) {
		return 0;
	},
	path_unlink_file: function (fd, path) {
		return 0;
	},
	path_readlink: function (fd, path, buf, buf_len, retptr0) {
	},
	path_rename: function (fd, old_path, new_fd, new_path) {
		return 0;
	},

	// File descriptors
	fd_close: function (fd) {
		delete MTY_W.fds[fd];
	},
	fd_fdstat_get: function (fd, retptr0) {
		return 0;
	},
	fd_fdstat_set_flags: function (fd, flags) {
	},
	fd_readdir: function (fd, buf, buf_len, cookie, retptr0) {
		return 8;
	},
	fd_seek: function (fd, offset, whence, retptr0) {
		return 0;
	},
	fd_read: function (fd, iovs, iovs_len, retptr0) {
		const finfo = MTY_W.fds[fd];
		const full_buf = mty_get_ls(finfo.path);

		if (finfo && full_buf) {
			let total = 0;

			for (let x = 0; x < iovs_len; x++) {
				let ptr = iovs + x * 8;
				let buf = MTY_GetUint32(ptr);
				let buf_len = MTY_GetUint32(ptr + 4);
				let len = buf_len < full_buf.length - total ? buf_len : full_buf.length - total;

				let view = new Uint8Array(MTY_MEMORY.buffer, buf, buf_len);
				let slice = new Uint8Array(full_buf.buffer, total, len);
				view.set(slice);

				total += len;
			}

			MTY_SetUint32(retptr0, total);
		}

		return 0;
	},
	fd_write: function (fd, iovs, iovs_len, retptr0) {
		// Calculate full write size
		let len = 0;
		for (let x = 0; x < iovs_len; x++)
			len += MTY_GetUint32(iovs + x * 8 + 4);

		MTY_SetUint32(retptr0, len);

		// Create a contiguous buffer
		let offset = 0;
		let full_buf = new Uint8Array(len);
		for (let x = 0; x < iovs_len; x++) {
			let ptr = iovs + x * 8;
			let buf = MTY_GetUint32(ptr);
			let buf_len = MTY_GetUint32(ptr + 4);

			full_buf.set(new Uint8Array(MTY_MEMORY.buffer, buf, buf_len), offset);
			offset += buf_len;
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
	clock_time_get: function (id, precision, retptr0) {
		MTY_SetUint64(retptr0, Math.round(performance.now() * 1000.0 * 1000.0));
		return 0;
	},
	poll_oneoff: function (_in, out, nsubscriptions, retptr0) {
		// __WASI_EVENTTYPE_CLOCK
		if (MTY_GetUint8(_in + 8) == 0)
			Atomics.wait(MTY_W.sleeper, 0, 0, Number(MTY_GetUint64(_in + 24)) / 1000000);

		MTY_SetUint32(out + 8, 0);
		return 0;
	},
	proc_exit: function (rval) {
	},
	environ_get: function (environ, environ_buf) {
	},
	environ_sizes_get: function (retptr0, retptr1) {
	},
	sched_yield: function () {
	},
};

const MTY_WASI_API = {
	'thread-spawn': function (start_arg) {
		postMessage({
			type: 'thread',
			startArg: start_arg,
			sab: MTY_W.sab,
			sync: MTY_W.sync,
		});

		mty_wait(MTY_W.sync);

		return MTY_W.sab[0];
	},
};


// Entry

async function mty_instantiate_wasm(wasmBuf, userEnv) {
	if (!userEnv)
		userEnv = [];

	// Imports
	const imports = {
		// Custom imports
		env: {
			memory: MTY_MEMORY,
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
				sab: MTY_W.sab,
				sync: MTY_W.sync,
			});

			mty_wait(MTY_W.sync);

			return MTY_W.sab[0];
		};
	}

	// Create wasm instance (module) from the ArrayBuffer
	return await WebAssembly.instantiate(wasmBuf, imports);
}

onmessage = async (ev) => {
	const msg = ev.data;

	switch (msg.type) {
		case 'init':
			importScripts(msg.file);

			MTY_MEMORY = msg.memory;

			MTY_W.queryString = msg.args;
			MTY_W.hostname = msg.hostname;
			MTY_W.bin = msg.bin;
			MTY_W.fdIndex = 64;
			MTY_W.kbMap = msg.kbMap;
			MTY_W.psync = msg.psync;
			MTY_W.initWindowInfo = msg.windowInfo;
			MTY_W.sync = new Int32Array(new SharedArrayBuffer(4));
			MTY_W.sleeper = new Int32Array(new SharedArrayBuffer(4));
			MTY_W.module = await mty_instantiate_wasm(msg.wasmBuf, msg.userEnv);
			MTY_W.exports = MTY_W.module.instance.exports;
			MTY_W.sab = new Uint32Array(new SharedArrayBuffer(128));

			MTY_W.exports.system_setbuf();

			try {
				// Secondary thread
				if (msg.startArg) {
					MTY_W.exports.wasi_thread_start(msg.threadId, msg.startArg);

				// Main thread
				} else {
					MTY_W.exports._start();
				}

				close();

			} catch (e) {
				if (e.toString().search('MTY_RunAndYield') == -1)
					console.error(e);
			}
			break;

		// Main thread only
		case 'window-update':
			if (MTY_W.app)
				mty_update_window(MTY_W.app, msg.windowInfo);
			break;
		case 'key': {
			if (!MTY_W.app)
				return;

			const key = MTY_W.keys[msg.code];

			if (key != undefined) {
				let packed = 0;

				if (msg.key.length == 1) {
					const buf = new TextEncoder().encode(msg.key);

					for (let x = 0; x < buf.length; x++)
						packed |= buf[x] << x * 8;
				}

				MTY_W.exports.window_keyboard(MTY_W.app, msg.pressed, key, packed, msg.mods);
			}
			break;
		}
		case 'motion':
			if (MTY_W.app)
				MTY_W.exports.window_motion(MTY_W.app, msg.relative, msg.x, msg.y);
			break;
		case 'button':
			if (MTY_W.app)
				MTY_W.exports.window_button(MTY_W.app, msg.pressed, msg.button, msg.x, msg.y);
			break;
		case 'wheel':
			if (MTY_W.app)
				MTY_W.exports.window_scroll(MTY_W.app, msg.x, msg.y);
			break;
		case 'move':
			if (MTY_W.app)
				MTY_W.exports.window_move(MTY_W.app);
			break;
		case 'resize':
			if (MTY_W.app) {
				MTY_W.exports.window_update_size(MTY_W.app, msg.width, msg.height);
				MTY_W.exports.window_resize(MTY_W.app);
			}
			break;
		case 'focus':
			if (MTY_W.app) {
				MTY_W.exports.window_update_focus(MTY_W.app, msg.focus);
				MTY_W.exports.window_focus(MTY_W.app, msg.focus);
			}
			break;
		case 'gamepad':
			if (MTY_W.app)
				MTY_W.exports.window_controller(MTY_W.app, msg.id, msg.state, msg.buttons, msg.lx,
					msg.ly, msg.rx, msg.ry, msg.lt, msg.rt);
			break;
		case 'disconnect':
			if (MTY_W.app)
				MTY_W.exports.window_controller(MTY_W.app, msg.id, msg.state, 0, 0, 0, 0, 0, 0, 0);
			break;
		case 'drop': {
			if (!MTY_W.app)
				return;

			const buf = new Uint8Array(msg.data);
			const cmem = mty_alloc(buf.length);
			MTY_Memcpy(cmem, buf);

			const name = new TextEncoder().encode(msg.name);
			const cname = mty_alloc(buf.length);
			MTY_Strcpy(cname, name);

			MTY_W.exports.window_drop(MTY_W.app, cname, cmem, buf.length);

			mty_free(cname);
			mty_free(cmem);
			break;
		}
	}
};
