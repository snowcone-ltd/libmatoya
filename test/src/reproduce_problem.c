#include <unistd.h>
#include "matoya.h"
// Your top level application context
struct context {
	MTY_App *app;
	bool quit;
};

// This function will fire for each event
static void event_func(const MTY_Event *evt, void *opaque)
{
	struct context *ctx = opaque;

	MTY_PrintEvent(evt);

	if (evt->type == MTY_EVENT_CLOSE)
		ctx->quit = true;
}

// This function fires once per "cycle", either blocked by a
// call to MTY_WindowPresent or limited by MTY_AppSetTimeout
static bool app_func(void *opaque)
{
	struct context *ctx = opaque;

	MTY_WindowPresent(ctx->app, 0);
	usleep(1000); /* simulate the 1ms sleep in parsec app loop */

	return !ctx->quit;
}

int main(int argc, char **argv)
{
	// Set up the application object and attach it to your context
	struct context ctx = {0};
	ctx.app = MTY_AppCreate(0, app_func, event_func, &ctx);
	if (!ctx.app)
		return 1;

	// Create a window
	MTY_WindowCreate(ctx.app, "My Window", NULL, 0);

	// Set the graphics API
	MTY_WindowSetGFX(ctx.app, 0, MTY_GetDefaultGFX(), true);

	// Run the app -- blocks until your app_func returns false
	MTY_AppRun(ctx.app);
	MTY_AppDestroy(&ctx.app);

	return 0;
}
