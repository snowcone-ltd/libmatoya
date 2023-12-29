// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

struct dxgi_sync {
	HANDLE event;
	MTY_Thread *thread;
	MTY_Atomic32 done;
	MTY_Atomic64 count;
};

static void dxgi_sync_destroy(struct dxgi_sync **dxgi_sync)
{
	if (!dxgi_sync || !*dxgi_sync)
		return;

	struct dxgi_sync *ctx = *dxgi_sync;

	MTY_Atomic32Set(&ctx->done, 1);
	MTY_ThreadDestroy(&ctx->thread);

	if (ctx->event)
		CloseHandle(ctx->event);

	MTY_Free(ctx);
	*dxgi_sync = NULL;
}

static void *dxgi_sync_thread(void *opaque)
{
	struct dxgi_sync *ctx = opaque;

	IDXGIFactory2 *factory2 = NULL;
	IDXGIAdapter *adapter = NULL;
	IDXGIOutput *output = NULL;

	HRESULT e = CreateDXGIFactory2(0, &IID_IDXGIFactory2, &factory2);
	if (e != S_OK) {
		MTY_Log("'CreateDXGIFactory2' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIFactory2_EnumAdapters(factory2, 0, &adapter);
	if (e != S_OK) {
		MTY_Log("'IDXGIFactory2_EnumAdapters' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIAdapter_EnumOutputs(adapter, 0, &output);
	if (e != S_OK) {
		MTY_Log("'IDXGIAdapter_EnumOutputs' failed with HRESULT 0x%X", e);
		goto except;
	}

	while (MTY_Atomic32Get(&ctx->done) == 0) {
		e = IDXGIOutput_WaitForVBlank(output);
		if (e != S_OK) {
			MTY_Log("'IDXGIOutput_WaitForVBlank' failed with HRESULT 0x%X", e);
			break;
		}

		MTY_Atomic64Add(&ctx->count, 1);

		SetEvent(ctx->event);
		ResetEvent(ctx->event);
	}

	except:

	if (output)
		IDXGIOutput_Release(output);

	if (adapter)
		IDXGIAdapter_Release(adapter);

	if (factory2)
		IDXGIFactory2_Release(factory2);

	return NULL;
}

static struct dxgi_sync *dxgi_sync_create(HWND hwnd)
{
	struct dxgi_sync *ctx = MTY_Alloc(1, sizeof(struct dxgi_sync));

	ctx->event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!ctx->event) {
		MTY_Log("'CreateEvent' failed with error 0x%X", GetLastError());
		goto except;
	}

	ctx->thread = MTY_ThreadCreate(dxgi_sync_thread, ctx);

	except:

	if (!ctx->event)
		dxgi_sync_destroy(&ctx);

	return ctx;
}

static int64_t dxgi_sync_get_count(struct dxgi_sync *ctx)
{
	if (!ctx)
		return 0;

	return MTY_Atomic64Get(&ctx->count);
}

static void dxgi_sync_wait(struct dxgi_sync *ctx, DWORD timeout)
{
	if (!ctx)
		return;

	DWORD e = WaitForSingleObjectEx(ctx->event, timeout, FALSE);

	if (e != WAIT_OBJECT_0)
		MTY_Log("'WaitForSingleObjectEx' failed with error 0x%X", e);
}
