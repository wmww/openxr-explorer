#include "imgui_skg.h"
#include "imgui_impl_skg.h"

#if defined(_WIN32)
#include "imgui_impl_win32.h"
#include <windows.h>

#elif defined(__linux__)
// libxcb-keysyms1-dev, libxcb1-dev, libxcb-xfixes0-dev libxcb-cursor-dev libxcb-xkb-dev
// libxcb, libxcb-xfixes, libxcb-xkb1 libxcb-cursor0, libxcb-keysyms1 and libxcb-randr0
#include "imgui_impl_x11.h"

#include<X11/X.h>
#include<X11/Xlib.h>
#include<X11/Xlib-xcb.h>
#include<GL/gl.h>
#include<GL/glx.h>
#include<GL/glu.h>
#endif

#include <stdio.h>

#include "sk_gpu.h"
#define SOKOL_TIME_IMPL
#include "sokol_time.h"

skg_swapchain_t sk_swapchain = {};
int32_t         sk_width     = 1280;
int32_t         sk_height    = 800;

ImVec4 shell_clear_color = ImVec4(0, 0, 0, 1.00f);

bool shell_create_window();
void shell_destroy_window();
void shell_loop(void (*step)());

//int WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
int main(int arg_count, const char **args) {
	if (!app_args(arg_count, args))
		return 0;

	shell_create_window();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	if (!app_init())
		return 2;

	stm_setup();

	shell_loop([]() {
		// Start the Dear ImGui frame
		skg_draw_begin();
		ImGui::NewFrame();

		app_step({(float)sk_width, (float)sk_height});

		// Rendering
		ImGui::Render();
		skg_swapchain_bind(&sk_swapchain);
		skg_target_clear(true, (float *)&shell_clear_color);
		ImGui_ImplSkg_RenderDrawData(ImGui::GetDrawData());

		skg_swapchain_present(&sk_swapchain);
	});

	// Cleanup
	app_shutdown();
	ImGui_ImplSkg_Shutdown();
	shell_destroy_window();
	ImGui::DestroyContext();

	return 0;
}

#if defined(_WIN32)

HWND shell_hwnd;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
			sk_width  = LOWORD(lParam);
			sk_height = HIWORD(lParam);
			skg_swapchain_resize(&sk_swapchain, (UINT)sk_width, (UINT)sk_height);
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool shell_create_window() {
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "ImGui sk_gpu shell", NULL };
	RegisterClassEx(&wc);
	shell_hwnd = CreateWindow(wc.lpszClassName, app_name, WS_OVERLAPPEDWINDOW, 100, 100, sk_width, sk_height, NULL, NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	skg_callback_log([](skg_log_ level, const char *text) { 
		if (level != skg_log_info)
			printf("[%d] %s\n", level, text); 
		});
	if (!skg_init(app_name, nullptr)) {
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}
	sk_swapchain = skg_swapchain_create(shell_hwnd, skg_tex_fmt_rgba32_linear, skg_tex_fmt_depth16, 1280, 800);

	// Show the window
	ShowWindow  (shell_hwnd, SW_SHOWDEFAULT);
	UpdateWindow(shell_hwnd);

	// Setup Platform/Renderer backends
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(shell_hwnd);
	ImGui_ImplSkg_Init();

	return true;
}

void shell_destroy_window() {
	skg_swapchain_destroy(&sk_swapchain);
	skg_shutdown();

	ImGui_ImplWin32_Shutdown();
	DestroyWindow(shell_hwnd);
	UnregisterClass("ImGui sk_gpu shell", nullptr);
}

void shell_loop(void (*step)()) {
	// Main loop
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	bool    evts             = true;
	bool    run              = true;
	int64_t last_update_time = 0;
	while (run) {
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			evts = true;
			if (msg.message == WM_QUIT) run = false;
		}
		if (!evts && stm_sec(stm_since(last_update_time)) > 0.4f) evts = true;
		if (!evts) { Sleep(3); continue; }
		evts = false;
		last_update_time = stm_now();

		ImGui_ImplWin32_NewFrame();

		step();
	}
}

#elif defined(__linux__)

Display *x_display = nullptr;

GLXDrawable glx_drawable     = {};
GLint       glx_attributes[] = {
	GLX_DOUBLEBUFFER,  true,
	GLX_RED_SIZE,      8,
	GLX_GREEN_SIZE,    8,
	GLX_BLUE_SIZE,     8,
	GLX_ALPHA_SIZE,    8,
	GLX_DEPTH_SIZE,    16,
	GLX_RENDER_TYPE,   GLX_RGBA_BIT,
	GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
	GLX_X_RENDERABLE,  true,
	None
};

xcb_connection_t *xcb_connection = nullptr;
xcb_screen_t     *xcb_screen     = nullptr;
xcb_drawable_t    xcb_window     = {};

bool shell_create_window() {
	x_display = XOpenDisplay(nullptr);
	if (!x_display)
		return false;

	xcb_connection = XGetXCBConnection(x_display);
	if (!xcb_connection)
		return false;

	int32_t               default_screen = DefaultScreen(x_display);
	xcb_screen_iterator_t screen_iter    = xcb_setup_roots_iterator(xcb_get_setup(xcb_connection));
	for (int32_t i=default_screen; screen_iter.rem && i>0; i--) {
		xcb_screen_next(&screen_iter);
	}
	xcb_screen = screen_iter.data;

	GLXFBConfig *fb_configs = 0;
	int          config_ct  = 0;
	fb_configs = glXChooseFBConfig(x_display, default_screen, glx_attributes, &config_ct);
	if(!fb_configs || config_ct == 0)
		return false;

	int32_t     visual_id = 0;
	GLXFBConfig fb_config = fb_configs[0];
	glXGetFBConfigAttrib(x_display, fb_config, GLX_VISUAL_ID , &visual_id);

	GLXContext glx_context = glXCreateNewContext(x_display, fb_config, GLX_RGBA_TYPE, 0, true);
	if(!glx_context)
		return false;

	xcb_colormap_t xcb_colormap = xcb_generate_id(xcb_connection);
	xcb_window                  = xcb_generate_id(xcb_connection);

	xcb_create_colormap( xcb_connection, XCB_COLORMAP_ALLOC_NONE, xcb_colormap, xcb_screen->root, visual_id );
	uint32_t eventmask = 
		XCB_EVENT_MASK_EXPOSURE       | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_KEY_PRESS      | XCB_EVENT_MASK_KEY_RELEASE      |
		XCB_EVENT_MASK_BUTTON_PRESS   | XCB_EVENT_MASK_BUTTON_RELEASE   |
		XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION    |
		XCB_EVENT_MASK_ENTER_WINDOW   | XCB_EVENT_MASK_LEAVE_WINDOW;
	uint32_t valuelist[] = { eventmask, xcb_colormap, 0 };
	uint32_t valuemask   = XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;

	xcb_create_window(
		xcb_connection, XCB_COPY_FROM_PARENT, xcb_window, xcb_screen->root,
		0, 0, sk_width, sk_height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
		visual_id, valuemask, valuelist );

	xcb_map_window(xcb_connection, xcb_window);

	GLXWindow glx_window = glXCreateWindow( x_display, fb_config, xcb_window, 0 );
	if (!glx_window) {
		xcb_destroy_window(xcb_connection, xcb_window);
		glXDestroyContext (x_display, glx_context);
		return false;
	}
	glx_drawable = glx_window;

	XVisualInfo *vi =  glXGetVisualFromFBConfig(x_display, fb_config);
	skg_setup_xlib(x_display, vi, &fb_config, &glx_drawable);
	skg_callback_log([](skg_log_ level, const char *text) { 
		if (level != skg_log_info)
			printf("[%d] %s\n", level, text); 
	});
	if (!skg_init(app_name, nullptr)) {
		return false;
	}

	sk_swapchain = skg_swapchain_create(&glx_drawable, skg_tex_fmt_rgba32_linear, skg_tex_fmt_depth16, sk_width, sk_height);

	// Setup Platform/Renderer backends
	ImGui::CreateContext();
	ImGui_ImplX11_Init(xcb_window);
	ImGui_ImplSkg_Init();

	return true;
}

void shell_destroy_window() {
	ImGui_ImplX11_Shutdown();
	ImGui::DestroyContext();

	xcb_disconnect(xcb_connection);
	XCloseDisplay(x_display);
}

void shell_loop(void (*step)()) {
	// Main loop
	bool done = false;
	xcb_generic_error_t* x_Err = nullptr;
	xcb_atom_t wm_protocols = xcb_intern_atom_reply(xcb_connection,
		xcb_intern_atom(xcb_connection, 1, 12, "WM_PROTOCOLS"),
		&x_Err)->atom;
	xcb_atom_t wm_delete_window = xcb_intern_atom_reply(xcb_connection,
		xcb_intern_atom(xcb_connection, 0, 16, "WM_DELETE_WINDOW"),
		&x_Err)->atom;
	xcb_change_property(xcb_connection, XCB_PROP_MODE_REPLACE, xcb_window,
		wm_protocols, 4, 32, 1, &wm_delete_window);

	while (!done)
	{
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		xcb_generic_event_t* event = xcb_poll_for_event(xcb_connection);
		if (event)
		{
			if (!ImGui_ImplX11_ProcessEvent(event))
			{
				switch (event->response_type & ~0x80)
				{
				case XCB_EXPOSE:
				{
					xcb_flush(xcb_connection);
					break;
				}
				case XCB_CLIENT_MESSAGE:
				{
					if (((xcb_client_message_event_t*)event)->data.data32[0] == wm_delete_window)
						done = true;
					break;
				}
				case XCB_CONFIGURE_NOTIFY:
				{
					xcb_configure_notify_event_t* config = (xcb_configure_notify_event_t*)event;
					// Set DisplaySize here instead of checking in X11 NewFrame
					// Checking window size is request/response
					ImGui::GetIO().DisplaySize = ImVec2(config->width, config->height); // FIXME: Why isn't this processed in ImGui_ImplX11_ProcessEvent?

					sk_width  = config->width;
					sk_height = config->height;
					skg_swapchain_resize(&sk_swapchain, sk_width, sk_height);
					break;
				}
				default:
					break;
				}
			}

			// xcb allocates the memory for the event and specifies the user to free it
			free(event);
			continue;
		}

		// Start the Dear ImGui frame
		ImGui_ImplX11_NewFrame();

		step();

		// Update and Render additional Platform Windows
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
}

#endif