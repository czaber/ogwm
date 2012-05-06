#ifndef _UWM_H_
#define _UWM_H_
// Standard
#include <string.h>
#include <stdio.h>
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
// Macros
#define MAKE_GL_VERSION(major, minor, release) (((major) << 16 ) | ((minor) << 8 ) | (release))

void draw(void);
void print(Window w);
void for_windows(Window w, void (*f)(Window w));
void on_clientmessage(XEvent* e);
void on_keypress(XEvent* e);
void on_expose(XEvent* e) {};
void on_maprequest(XEvent* e);
void on_mapnotify(XEvent* e);
void on_enternotify(XEvent* e) {};
void on_unmapnotify(XEvent* e) {};
void on_buttonpressed(XEvent* e) {};
void on_destroynotify(XEvent* e);
void on_visibilitynotify(XEvent* e) {};
void on_createnotify(XEvent* e);
void on_configurenotify(XEvent* e) {};
void on_configurerequest(XEvent* e) {};
void on_signal(int);
Window create_overlay(void);
Window create_backplane(void);
Window create_frontplane(void);
void ignore_input(Window w);
void check_gl(int line);
void check_features(void);
void setup_app(void);
void setup_x(void);
void setup_gl(void);
void start(void);
void clean(void);
GLXFBConfig choose_fbconfig();

typedef struct _Client {
	Window window;
	XWindowAttributes geom;
	GLXPixmap glxpixmap;
	GLenum target;
	GLuint texture;
} Client;

// Events array
static void (*events[EVENTS_SIZE])(XEvent* e) = {
	//[KeyPress] = on_keypress,
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
	[ConfigureRequest] = on_configurerequest,
	[ClientMessage] = on_clientmessage
};

static char* event_names[EVENTS_SIZE] = {
	[KeyPress] = "KeyPress",
	//[MotionNotify] = "MotionNotify",
	//[Expose] = "Expose",
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
	//[ClientMessage] = "ClientMessage"
};

//static int gl_attr[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
//static int pixmap_attr[] = {GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT, 
//			    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_RECTANGLE_EXT, None};
static int fbconfig_attr[] = {
	GLX_BIND_TO_TEXTURE_RGBA_EXT, True,
	GLX_BIND_TO_MIPMAP_TEXTURE_EXT, True,
    GLX_DOUBLEBUFFER, True, // We want double-buffering
    GLX_DEPTH_SIZE, 24, // Highest non-RGBA depth possible
    GLX_RENDER_TYPE, GLX_RGBA_BIT, // Don't consider indexed color configs
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, // Must be a window config
    GLX_X_RENDERABLE, True, // Only consider configs that have an associated X visual
    None
};
static int pixmap_attr[] = {
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT, 
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_RECTANGLE_EXT,
    None
};
#endif // _UWM_H_
