package group.matoya.lib;

import android.app.Activity;
import android.view.View;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.GestureDetector;
import android.view.ScaleGestureDetector;
import android.view.WindowManager;
import android.view.PointerIcon;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.text.InputType;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.content.Context;
import android.content.Intent;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.res.Configuration;
import android.content.pm.ActivityInfo;
import android.hardware.input.InputManager;
import android.util.Log;
import android.util.DisplayMetrics;
import android.util.Base64;
import android.widget.Scroller;
import android.os.Vibrator;
import android.net.Uri;

public class Matoya extends SurfaceView implements
	SurfaceHolder.Callback,
	InputManager.InputDeviceListener,
	GestureDetector.OnGestureListener,
	GestureDetector.OnDoubleTapListener,
	GestureDetector.OnContextClickListener,
	ScaleGestureDetector.OnScaleGestureListener,
	ClipboardManager.OnPrimaryClipChangedListener
{
	Activity activity;
	KeyCharacterMap kbmap;
	PointerIcon cursor;
	PointerIcon invisCursor;
	GestureDetector detector;
	ScaleGestureDetector sdetector;
	Scroller scroller;
	Vibrator vibrator;
	boolean hiddenCursor;
	boolean defaultCursor;
	boolean kbShowing;
	float displayDensity;
	int scrollY;

	native void gfx_resize(int width, int height);
	native void gfx_set_surface(Surface surface);
	native void gfx_unset_surface();

	native void app_start();
	native void app_stop();

	native boolean app_key(boolean pressed, int code, String text, int mods, boolean soft);
	native boolean app_long_press(float x, float y);
	native void app_unplug(int deviceId);
	native void app_single_tap_up(float x, float y);
	native void app_scroll(float absX, float absY, float x, float y, int fingers);
	native void app_check_scroller(boolean check);
	native void app_mouse_motion(boolean relative, float x, float y);
	native void app_mouse_button(boolean pressed, int button, float x, float y);
	native void app_generic_scroll(float x, float y);
	native void app_button(int deviceId, boolean pressed, int code);
	native void app_axis(int deviceId, float hatX, float hatY, float lX, float lY, float rX, float rY,
		float lT, float rT, float lTalt, float rTalt);
	native void app_unhandled_touch(int action, float x, float y, int fingers);

	static {
		System.loadLibrary("main");
	}

	public Matoya(Activity activity) {
		super(activity);

		this.activity = activity;

		this.kbmap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
		this.vibrator = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
		this.detector = new GestureDetector(activity, this);
		this.sdetector = new ScaleGestureDetector(activity, this);
		this.scroller = new Scroller(activity);

		DisplayMetrics dm = new DisplayMetrics();
		this.activity.getWindowManager().getDefaultDisplay().getMetrics(dm);
		this.displayDensity = dm.density;

		String b64 = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAQAAADZc7J/AAAAH0lEQVR42mNk" +
			"oBAwjhowasCoAaMGjBowasCoAcPNAACOMAAhOO/A7wAAAABJRU5ErkJggg==";

		byte[] iCursorData = Base64.decode(b64, Base64.DEFAULT);
		Bitmap bm = BitmapFactory.decodeByteArray(iCursorData, 0, iCursorData.length, null);
		this.invisCursor = PointerIcon.create(bm, 0, 0);

		activity.setContentView(this);

		ClipboardManager clipboard = (ClipboardManager) this.activity.getSystemService(Context.CLIPBOARD_SERVICE);
		clipboard.addPrimaryClipChangedListener(this);

		InputManager manager = (InputManager) activity.getSystemService(Context.INPUT_SERVICE);
		manager.registerInputDeviceListener(this, null);

		this.getHolder().addCallback(this);
		this.detector.setOnDoubleTapListener(this);
		this.detector.setContextClickListener(this);
		this.setFilterTouchesWhenObscured(true);
		this.setFocusableInTouchMode(true);
		this.setFocusable(true);
		this.requestFocus();

		app_start();
	}

	public void destroy() {
		app_stop();
	}


	/** SurfaceView */


	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
		gfx_resize(w, h);
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		gfx_set_surface(holder.getSurface());
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		gfx_unset_surface();
	}


	// Cursor

	PointerIcon getCursor() {
		return this.defaultCursor ? null : this.hiddenCursor ? this.invisCursor : this.cursor;
	}

	@Override
	public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
		return this.getCursor();
	}


	// InputDevice listener

	@Override
	public void onInputDeviceRemoved(int deviceId) {
		app_unplug(deviceId);
	}

	@Override
	public void onInputDeviceChanged(int deviceId) {
	}

	@Override
	public void onInputDeviceAdded(int deviceId) {
	}


	// InputConnection (for IME keyboard)

	@Override
	public boolean onCheckIsTextEditor() {
		return true;
	}

	@Override
	public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
		outAttrs.inputType = InputType.TYPE_NULL;
		outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;

		return new BaseInputConnection(this, false);
	}


	// Events

	static boolean isMouseEvent(InputEvent event) {
		return
			(event.getSource() & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE ||
			(event.getSource() & InputDevice.SOURCE_TOUCHPAD) == InputDevice.SOURCE_TOUCHPAD;
	}

	static boolean isGamepadEvent(InputEvent event) {
		return
			(event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
			(event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK ||
			(event.getSource() & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD;
	}

	boolean keyEvent(int keyCode, KeyEvent event, boolean down) {
		// Button events fire here (sometimes dpad)
		if (isGamepadEvent(event)) {
			app_button(event.getDeviceId(), down, keyCode);

		// Prevents back buttons etc. from being generated from mice
		} else if (!isMouseEvent(event)) {
			int uc = event.getUnicodeChar();

			return app_key(down, keyCode, uc != 0 ? String.format("%c", uc) : null,
				event.getMetaState(), event.getDeviceId() <= 0);
		}

		return true;
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		return keyEvent(keyCode, event, true);
	}

	@Override
	public boolean onKeyUp(int keyCode, KeyEvent event) {
		return keyEvent(keyCode, event, false);
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		// Mouse motion while buttons are held down fire here
		if (isMouseEvent(event)) {
			if (event.getActionMasked() == MotionEvent.ACTION_MOVE)
				app_mouse_motion(false, event.getX(0), event.getY(0));

		} else {
			this.detector.onTouchEvent(event);
			this.sdetector.onTouchEvent(event);

			app_unhandled_touch(event.getActionMasked(), event.getX(0), event.getY(0), event.getPointerCount());
		}

		return true;
	}

	@Override
	public boolean onDown(MotionEvent event) {
		if (isMouseEvent(event))
			return false;

		this.scroller.forceFinished(true);
		return true;
	}

	@Override
	public boolean onFling(MotionEvent event1, MotionEvent event2, float velocityX, float velocityY) {
		if (isMouseEvent(event1) || isMouseEvent(event2))
			return false;

		this.scroller.forceFinished(true);
		this.scroller.fling(0, 0, Math.round(velocityX), Math.round(velocityY),
			Integer.MIN_VALUE, Integer.MAX_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE);
		app_check_scroller(true);

		return true;
	}

	@Override
	public void onLongPress(MotionEvent event) {
		if (isMouseEvent(event))
			return;

		if (app_long_press(event.getX(0), event.getY(0)))
			this.vibrator.vibrate(10);
	}

	@Override
	public boolean onScroll(MotionEvent event1, MotionEvent event2, float distanceX, float distanceY) {
		if (isMouseEvent(event1) || isMouseEvent(event2))
			return false;

		this.scroller.forceFinished(true);
		app_scroll(event2.getX(0), event2.getY(0), distanceX, distanceY, event2.getPointerCount());
		return true;
	}

	@Override
	public void onShowPress(MotionEvent event) {
	}

	@Override
	public boolean onSingleTapUp(MotionEvent event) {
		if (isMouseEvent(event))
			return false;

		app_single_tap_up(event.getX(0), event.getY(0));
		return true;
	}

	@Override
	public boolean onDoubleTap(MotionEvent event) {
		if (isMouseEvent(event))
			return false;

		app_single_tap_up(event.getX(0), event.getY(0));
		return true;
	}

	@Override
	public boolean onDoubleTapEvent(MotionEvent event) {
		return true;
	}

	@Override
	public boolean onSingleTapConfirmed(MotionEvent event) {
		return true;
	}

	@Override
	public boolean onContextClick(MotionEvent event) {
		return true;
	}

	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
		if (isMouseEvent(event)) {
			switch (event.getActionMasked()) {
				case MotionEvent.ACTION_HOVER_MOVE:
					app_mouse_motion(false, event.getX(0), event.getY(0));
					return true;
				case MotionEvent.ACTION_SCROLL:
					app_generic_scroll(event.getAxisValue(MotionEvent.AXIS_HSCROLL), event.getAxisValue(MotionEvent.AXIS_VSCROLL));
					return true;
				case MotionEvent.ACTION_BUTTON_PRESS:
					app_mouse_button(true, event.getActionButton(), event.getX(0), event.getY(0));
					return true;
				case MotionEvent.ACTION_BUTTON_RELEASE:
					app_mouse_button(false, event.getActionButton(), event.getX(0), event.getY(0));
					return true;
				default:
					break;
			}

		// DPAD and axis events fire here
		} else if (isGamepadEvent(event)) {
			app_axis(
				event.getDeviceId(),
				event.getAxisValue(MotionEvent.AXIS_HAT_X),
				event.getAxisValue(MotionEvent.AXIS_HAT_Y),
				event.getAxisValue(MotionEvent.AXIS_X),
				event.getAxisValue(MotionEvent.AXIS_Y),
				event.getAxisValue(MotionEvent.AXIS_Z),
				event.getAxisValue(MotionEvent.AXIS_RZ),
				event.getAxisValue(MotionEvent.AXIS_LTRIGGER),
				event.getAxisValue(MotionEvent.AXIS_RTRIGGER),
				event.getAxisValue(MotionEvent.AXIS_BRAKE),
				event.getAxisValue(MotionEvent.AXIS_GAS));
		}

		return true;
	}

	@Override
	public boolean onCapturedPointerEvent(MotionEvent event) {
		app_mouse_motion(true, event.getX(0), event.getY(0));

		return this.onGenericMotionEvent(event);
	}

	@Override
	public boolean onScale(ScaleGestureDetector detector) {
		return true;
	}

	@Override
	public boolean onScaleBegin(ScaleGestureDetector sdetector) {
		return true;
	}

	@Override
	public void onScaleEnd(ScaleGestureDetector sdetector) {
	}


	/** NDK (Called from C) */


	// Fullscreen

	static int fullscreenFlags() {
		return
			View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
			View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |  // Prevents hidden stuff from coming back spontaneously
			View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | // Removes space at the top for menu bar
			View.SYSTEM_UI_FLAG_FULLSCREEN |        // Removes menu bar
			View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;    // Hides navigation buttons at the bottom
	}

	public boolean isFullscreen() {
		return (this.activity.getWindow().getDecorView().getSystemUiVisibility() & Matoya.fullscreenFlags()) == Matoya.fullscreenFlags();
	}

	public void enableFullscreen(boolean _enable) {
		final Matoya self = this;
		final boolean enable = _enable;

		this.activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				if (enable) {
					self.activity.getWindow().getDecorView().setSystemUiVisibility(Matoya.fullscreenFlags());

				} else {
					self.activity.getWindow().getDecorView().setSystemUiVisibility(0);
				}
			}
		});
	}


	// Clipboard

	@Override
	public void onPrimaryClipChanged() {
		// TODO Send native notification
	}

	public void setClipboard(String str) {
		ClipboardManager clipboard = (ClipboardManager) this.activity.getSystemService(Context.CLIPBOARD_SERVICE);
		clipboard.setPrimaryClip(ClipData.newPlainText("MTY", str));
	}

	public String getClipboard() {
		ClipboardManager clipboard = (ClipboardManager) this.activity.getSystemService(Context.CLIPBOARD_SERVICE);
		ClipData primary = clipboard.getPrimaryClip();

		if (primary != null) {
			ClipData.Item item = primary.getItemAt(0);
			CharSequence chars = item.getText();

			if (chars != null)
				return chars.toString();
		}

		return null;
	}


	// Cursor

	void setCursor() {
		this.setPointerIcon(this.getCursor());
	}

	public void setCursor(byte[] _data, float _hotX, float _hotY) {
		final Matoya self = this;
		final byte[] data = _data;
		final float hotX = _hotX;
		final float hotY = _hotY;

		this.activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				if (data != null) {
					Bitmap bm = BitmapFactory.decodeByteArray(data, 0, data.length, null);
					self.cursor = PointerIcon.create(bm, hotX, hotY);

				} else {
					self.cursor = null;
				}

				self.setPointerIcon(self.cursor);
			}
		});
	}

	public void showCursor(boolean _show) {
		final Matoya self = this;
		final boolean show = _show;

		this.activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				self.hiddenCursor = !show;
				self.setCursor();
			}
		});
	}

	public void useDefaultCursor(boolean _useDefault) {
		final Matoya self = this;
		final boolean useDefault = _useDefault;

		this.activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				self.defaultCursor = useDefault;
				self.setCursor();
			}
		});
	}

	public void setRelativeMouse(boolean _relative) {
		final Matoya self = this;
		final boolean relative = _relative;

		this.activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				if (relative) {
					self.requestPointerCapture();

				} else {
					self.releasePointerCapture();
				}
			}
		});
	}

	public boolean getRelativeMouse() {
		return this.hasPointerCapture();
	}


	// Misc

	public void openURI(String uri) {
		Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(uri));
		this.activity.startActivity(intent);
	}

	public float getDisplayDensity() {
		return this.displayDensity;
	}

	public String getKey(int keyCode) {
		char label = this.kbmap.getDisplayLabel(keyCode);

		if (label != 0) {
			return String.format("%c", label);

		} else {
			return KeyEvent.keyCodeToString(keyCode).replaceAll("KEYCODE_", "")
				.replaceAll("MOVE_", "");
		}
	}

	public void checkScroller() {
		this.scroller.computeScrollOffset();

		if (!this.scroller.isFinished()) {
			int currY = this.scroller.getCurrY();
			int diff = this.scrollY - currY;

			if (diff != 0)
				app_scroll(-1.0f, -1.0f, 0.0f, diff, 1);

			this.scrollY = currY;

		} else {
			app_check_scroller(false);
			this.scrollY = 0;
		}
	}

	public int keyboardHeight() {
		InputMethodManager imm = (InputMethodManager) this.activity.getSystemService(Context.INPUT_METHOD_SERVICE);

		try {
			java.lang.reflect.Method method = imm.getClass().getMethod("getInputMethodWindowVisibleHeight");
			return (int) method.invoke(imm);

		} catch (Exception e) {
		}

		return -1;
	}

	public boolean keyboardIsShowing() {
		int height = this.keyboardHeight();

		return height == -1 ? this.kbShowing : height > 0;
	}

	public void showKeyboard(boolean show) {
		InputMethodManager imm = (InputMethodManager) this.activity.getSystemService(Context.INPUT_METHOD_SERVICE);

		if (show) {
			imm.showSoftInput(this, 0, null);

		} else {
			imm.hideSoftInputFromWindow(this.getWindowToken(), 0, null);
		}

		this.kbShowing = show;
	}

	public int getOrientation() {
		switch (this.activity.getResources().getConfiguration().orientation) {
			case Configuration.ORIENTATION_LANDSCAPE:
				return 1;
			case Configuration.ORIENTATION_PORTRAIT:
				return 2;
			default:
				return 0;
		}
	}

	public void setOrientation(int _orientation) {
		final Matoya self = this;
		final int orienation = _orientation;

		this.activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				switch (orienation) {
					case 1:
						self.activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
						break;
					case 2:
						self.activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT);
						break;
					default:
						self.activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
						break;
				}
			}
		});
	}

	public void stayAwake(boolean _enable) {
		final Matoya self = this;
		final boolean enable = _enable;

		this.activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				if (enable) {
					self.activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

				} else {
					self.activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				}
			}
		});
	}

	public void finish() {
		this.activity.finishAndRemoveTask();
	}

	public String getExternalFilesDir() {
		return this.activity.getExternalFilesDir(null).getAbsolutePath();
	}

	public String getInternalFilesDir() {
		return this.activity.getFilesDir().getAbsolutePath();
	}

	public int getHardwareIds(int id) {
		int pid = 0;
		int vid = 0;

		InputDevice device = InputDevice.getDevice(id);
		if (device != null) {
			pid = device.getProductId();
			vid = device.getVendorId();
		}

		return (vid << 16) | (pid & 0xFFFF);
	}
}
