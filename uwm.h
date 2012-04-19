#ifndef _UWM_H_
#define _UWM_H_
// Standard
#include <string.h>
// Pure X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
// Extensions
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
// OpenGL (GLX)
#include <GL/glx.h>
#include <GL/glxext.h>
// Defines
#define EVENTS_SIZE LASTEvent
void on_keypress(XEvent* e);
void on_expose(XEvent* e);
void on_maprequest(XEvent* e);
void on_mapnotify(XEvent* e);
void on_enternotify(XEvent* e);
void on_unmapnotify(XEvent* e);
void on_buttonpressed(XEvent* e);
void on_destroynotify(XEvent* e);
void on_visibilitynotify(XEvent* e);
void on_createnotify(XEvent* e);
void on_configurenotify(XEvent* e);
void on_configurerequest(XEvent* e);

Window create_overlay(void);
Window create_backplane(void);
Window create_frontplane(void);
void ignore_input(Window w);
void setup_x(void);
void setup_gl(void);
void start(void);
void clean(void);

// Events array
static void (*events[EVENTS_SIZE])(XEvent* e) = {
	[KeyPress] = on_keypress,
	[Expose] = on_expose,
	[MapRequest] = on_maprequest,
	[MapNotify] = on_mapnotify,
	[EnterNotify] = on_enternotify,
	[UnmapNotify] = on_unmapnotify,
	[ButtonPress] = on_buttonpressed,
	[DestroyNotify] = on_destroynotify,
	[VisibilityNotify] = on_visibilitynotify,
	[CreateNotify] = on_createnotify,
	[ConfigureNotify] = on_configurenotify,
	[ConfigureRequest] = on_configurerequest
};

static char* event_names[EVENTS_SIZE] = {
	//[KeyPress] = "KeyPress",
	//[MotionNotify] = "MotionNotify",
	[Expose] = "Expose",
	//[FocusIn] = "FocusIn",
	//[FocusOut] = "FocusOut",
	[MapRequest] = "MapRequest",
	[MapNotify] = "MapNotify",
	[CreateNotify] = "CreateNotify",
	//[EnterNotify] = "EnterNotify",
	[UnmapNotify] = "UnmapNotify",
	//[ButtonPress] = "ButtonPress",
	//[ReparentNotify] = "ReparentNotify",
	[DestroyNotify] = "DestroyNotify",
	[VisibilityNotify] = "VisibilityNotify",
	[ConfigureNotify] = "ConfigureNotify",
	[ConfigureRequest] = "ConfigureRequest"
};

#endif // _UWM_H_
