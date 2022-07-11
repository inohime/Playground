#include <SDL.h>
#include <SDL_syswm.h>
#include <glad.h>
#include <sciter-x-window.hpp>
#include <sciter-x.h>
#include "resources.cpp"
#include <chrono>
#include <format>

namespace fs = std::filesystem;

// using Sciter.lite
static UINT AttachBehavior(LPSCN_ATTACH_BEHAVIOR lpAB);
static UINT OnLoadData(LPSCN_LOAD_DATA lpLoad);
static UINT OnDataLoaded(LPSCN_DATA_LOADED lpLoad);
static UINT SC_CALLBACK HandleNotifications(LPSCITER_CALLBACK_NOTIFICATION callback, LPVOID lpParam);

int main(int, char**)
{
	assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);

	//bool isSet;
	/*
	isSet = SciterSetOption(NULL, SCITER_SET_UX_THEMING, TRUE);
	assert(isSet);

	// these two calls are optional, used here to enable communication with inspector.exe (CTRL+SHIFT+I with running inspector) 
	isSet = SciterSetOption(NULL, SCITER_SET_SCRIPT_RUNTIME_FEATURES,
		ALLOW_FILE_IO |
		ALLOW_SOCKET_IO |
		ALLOW_EVAL |
		ALLOW_SYSINFO);
	assert(isSet);

	isSet = SciterSetOption(NULL, SCITER_SET_DEBUG_MODE, TRUE);
	assert(isSet);
	*/

	constexpr int W = 1024, H = 768;

	std::string wndStr = "Application";
	SDL_Window *window = SDL_CreateWindow(wndStr.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_bool isValid = SDL_GetWindowWMInfo(window, &wmInfo);
	assert(isValid);
	HWND hwnd = wmInfo.info.win.window;

	SDL_GLContext context = SDL_GL_CreateContext(window);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
	printf("Vendor:  %s\n", glGetString(GL_VENDOR));
	printf("GPU:  %s\n", glGetString(GL_RENDERER));
	printf("Version:  %d.%d\n", GLVersion.major, GLVersion.minor);

	SDL_GL_SetSwapInterval(1);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	int vw = 0, vh = 0;
	SDL_GetWindowSize(window, &vw, &vh);
	glViewport(0, 0, vw, vh);

	auto begin = std::chrono::system_clock::now();
	SDL_Event ev;
	std::chrono::duration<double, std::milli> dt;

	auto end = std::chrono::system_clock::now();
	dt = std::chrono::duration<double, std::milli>(end - begin);
	begin = end;

	// start sciter after everything else is loaded
	auto isOK = SciterProcX(hwnd, SCITER_X_MSG_CREATE(GFX_LAYER_SKIA_OPENGL, FALSE));
	assert(isOK);

	SciterProcX(hwnd, SCITER_X_MSG_RESOLUTION(96));
	SciterProcX(hwnd, SCITER_X_MSG_SIZE(vw, vh));

	SciterSetCallback(hwnd, HandleNotifications, NULL);

	// load file
	sciter::archive::instance().open(aux::elements_of(resources)); // bind resources[] (defined in "facade-resources.cpp") with the archive
	//SBOOL isLoaded = SciterLoadFile(hwnd, L"assets/minimal.htm"); // assets in the sciter_test/sciter_test folder
	SBOOL isLoaded = SciterLoadFile(hwnd, WSTR("this://app/minimal.htm")); // it finds the file on its own (relative)
	if (!isLoaded)
		printf("not loaded\n");

	MOUSE_BUTTONS mb = MOUSE_BUTTONS(0);
	MOUSE_EVENTS me = {};
	SDL_Point mouse = {};

	const auto &mbCallback = [&](SDL_Event *ev) {
		switch (ev->type) {
			case SDL_MOUSEBUTTONDOWN: {
				me = MOUSE_DOWN;
			} break;

			case SDL_MOUSEBUTTONUP: {
				me = MOUSE_UP;
			} break;
		}

		switch (ev->button.button) {
			case SDL_BUTTON_LEFT:
				mb = MAIN_MOUSE_BUTTON;
				break;

			case SDL_BUTTON_RIGHT:
				mb = PROP_MOUSE_BUTTON;
				break;

			case SDL_BUTTON_MIDDLE:
				mb = MIDDLE_MOUSE_BUTTON;
				break;
		}

		int x, y;
		SDL_GetMouseState(&x, &y);
		POINT pos = {x, y};

		SciterProcX(hwnd, SCITER_X_MSG_MOUSE(me, mb, KEYBOARD_STATES(0), pos));
	};

	const int FPS = 72;
	double delay = 1000.0 / FPS;
	
	bool quit = false;

	auto start = std::chrono::system_clock::now();
	while (!quit) {
		SciterProcX(hwnd, SCITER_X_MSG_HEARTBIT(dt.count()));
		SDL_SetWindowTitle(window, std::format("{} FPS: {}", wndStr.c_str(), (int)(1000 / dt.count())).c_str());

		while (SDL_PollEvent(&ev) != 0) {
			switch (ev.type) {
				case SDL_QUIT:
					quit = true;
					break;

				case SDL_MOUSEBUTTONUP:
					[[fallthrough]];
				case SDL_MOUSEBUTTONDOWN:
					mbCallback(&ev);
					break;

				case SDL_MOUSEMOTION: {
					MOUSE_EVENTS me = MOUSE_MOVE;
					SDL_GetMouseState(&mouse.x, &mouse.y);
					POINT pos = { mouse.x, mouse.y };
					SciterProcX(hwnd, SCITER_X_MSG_MOUSE(me, mb, KEYBOARD_STATES(0), pos));
				}
				break;
			}
		}
		auto now = std::chrono::system_clock::now();
		dt = std::chrono::duration<double, std::milli>(now - start);
		start = now;

		glViewport(0, 0, vw, vh);
		glClearColor(0.4f, 0.25f, 0.5f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		SciterProcX(hwnd, SCITER_X_MSG_PAINT(0, true));

		SDL_GL_SwapWindow(window);

		if (delay > dt.count())
			SDL_Delay(delay - dt.count());
	}

	SciterProcX(hwnd, SCITER_X_MSG_DESTROY());

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

UINT AttachBehavior(LPSCN_ATTACH_BEHAVIOR lpAB) {
	sciter::event_handler* evHandler = sciter::behavior_factory::create(lpAB->behaviorName, lpAB->element);

	if (evHandler) {
		lpAB->elementTag = evHandler;
		lpAB->elementProc = sciter::event_handler::element_proc;

		return TRUE;
	}

	return FALSE;
}

UINT OnLoadData(LPSCN_LOAD_DATA lpLoad) {
	LPCBYTE lpb = 0;
	UINT cb = 0;

	aux::wchars chars = aux::chars_of(lpLoad->uri);

	if (chars.like(WSTR("this://app/*"))) {
		aux::bytes data = sciter::archive::instance().get(chars.start + 11);
		if (data.length)
			SciterDataReady(lpLoad->hwnd, lpLoad->uri, data.start, data.length);
	}

	return LOAD_OK;
}

UINT OnDataLoaded(LPSCN_DATA_LOADED lpLoad) {
	return 0;
}

UINT SC_CALLBACK HandleNotifications(LPSCITER_CALLBACK_NOTIFICATION callback, LPVOID lpParam) {
	switch (callback->code) {
	case SC_LOAD_DATA:
		return OnLoadData((LPSCN_LOAD_DATA)callback);

	case SC_DATA_LOADED:
		return OnDataLoaded((LPSCN_DATA_LOADED)callback);

	case SC_ATTACH_BEHAVIOR:
		return AttachBehavior((LPSCN_ATTACH_BEHAVIOR)callback);

	case SC_ENGINE_DESTROYED:
		break;
	}
}