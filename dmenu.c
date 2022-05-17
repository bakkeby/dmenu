/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                             * MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTW(X)              (drw_fontset_getwidth(drw, (X)) + lrpad)
#define OPAQUE 0xffU

/* enums */
enum {
	SchemeNorm,
	SchemeSel,
	SchemeOut,
	SchemeBorder,
	SchemePrompt,
	SchemeAdjacent,
	SchemeNormHighlight,
	SchemeSelHighlight,
	SchemeHp,
	SchemeLast,
}; /* color schemes */

struct item {
	char *text;
	char *stext;
	struct item *left, *right;
	int id; /* for multiselect */
	int hp;
	double distance;
	int index;
};

static char text[BUFSIZ] = "";
static char *embed;
static int bh, mw, mh;
static int dmx = ~0, dmy = ~0, dmw = 0; /* put dmenu at these x and y offsets and w width */
static int dmxp = ~0, dmyp = ~0, dmwp = ~0; /* percentage values for the above */
static int inputw = 0, promptw;
static int lrpad; /* sum of left and right padding */
static size_t cursor;
static struct item *items = NULL;
static struct item *matches, *matchend;
static struct item *prev, *curr, *next, *sel;
static int mon = -1, screen;
static int *selid = NULL;
static unsigned int selidsize = 0;
static unsigned int preselected = 0;

static Atom clip, utf8;
static Atom type, dock;
static Display *dpy;
static Window root, parentwin, win;
static XIC xic;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

static Drw *drw;
static Clr *scheme[SchemeLast];

#include "lib/include.h"
#include "config.h"

static int (*fstrncmp)(const char *, const char *, size_t) = strncmp;
static char *(*fstrstr)(const char *, const char *) = strstr;

static void appenditem(struct item *item, struct item **list, struct item **last);
static void calcoffsets(void);
static void cleanup(void);
static char * cistrstr(const char *s, const char *sub);
static int drawitem(struct item *item, int x, int y, int w);
static void drawmenu(void);
static void grabfocus(void);
static void grabkeyboard(void);
static void match(void);
static void insert(const char *str, ssize_t n);
static size_t nextrune(int inc);
static void movewordedge(int dir);
static void keypress(XKeyEvent *ev);
static void paste(void);
static void xinitvisual(void);
static void readstdin(void);
static void run(void);
static void setup(void);
static unsigned int textw_clamp(const char *str, unsigned int n);
static void usage(void);

#include "lib/include.c"

static void
appenditem(struct item *item, struct item **list, struct item **last)
{
	if (*last)
		(*last)->right = item;
	else
		*list = item;

	item->left = *last;
	item->right = NULL;
	*last = item;
}

static void
calcoffsets(void)
{
	int i, n, rpad = 0;

    if (enabled(ShowNumbers))
        rpad = TEXTW(numbers);

	if (lines > 0)
		if (columns)
			n = lines * columns * bh;
		else
			n = lines * bh;
	else
		n = mw - (promptw + inputw + TEXTW(lsymbol) + TEXTW(rsymbol) + rpad);
	/* calculate which items will begin the next page and previous page */
	for (i = 0, next = curr; next; next = next->right)
		if ((i += (lines > 0) ? bh : textw_clamp(next->text, n)) > n)
			break;
	for (i = 0, prev = curr; prev && prev->left; prev = prev->left)
		if ((i += (lines > 0) ? bh : textw_clamp(prev->left->text, n)) > n)
			break;
}

static void
cleanup(void)
{
	size_t i;

	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	for (i = 0; i < SchemeLast; i++)
		free(scheme[i]);
	for (i = 0; i < hplength; ++i)
		free(hpitems[i]);
	free(hpitems);
	drw_free(drw);
	XSync(dpy, False);
	XCloseDisplay(dpy);
	free(selid);
}

static char *
cistrstr(const char *s, const char *sub)
{
	size_t len;

	for (len = strlen(sub); *s; s++)
		if (!strncasecmp(s, sub, len))
			return (char *)s;
	return NULL;
}

static int
drawitem(struct item *item, int x, int y, int w)
{
	int r;
	char *text = enabled(TabSeparatedValues) ? item->stext : item->text;

	if (item == sel)
		drw_setscheme(drw, scheme[SchemeSel]);
	else if (item->hp)
		drw_setscheme(drw, scheme[SchemeHp]);
	else if (enabled(HighlightAdjacent) && columns < 2 && (item->left == sel || item->right == sel))
		drw_setscheme(drw, scheme[SchemeAdjacent]);
	else if (issel(item->id))
		drw_setscheme(drw, scheme[SchemeOut]);
	else
		drw_setscheme(drw, scheme[SchemeNorm]);

	r = drw_text(drw, x, y, w, bh, lrpad / 2, text, 0);
	drawhighlights(item, x, y, w);
	return r;
}

static void
drawmenu(void)
{
	unsigned int curpos;
	struct item *item;
	int x = 0, y = 0, w, rpad = 0, itw = 0, stw = 0;
	int fh = drw->fonts->h;
	char *censort;

	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, 0, 0, mw, mh, 1, 1);

	if (prompt && *prompt) {
		drw_setscheme(drw, scheme[SchemePrompt]);
		x = drw_text(drw, x, 0, promptw, bh, lrpad / 2, prompt, 0);
	}
	/* draw input field */
	w = (lines > 0 || !matches) ? mw - x : inputw;

	drw_setscheme(drw, scheme[SchemeNorm]);
	if (enabled(PasswordInput)) {
		censort = ecalloc(1, sizeof(text));
		memset(censort, csymbol, strlen(text));
		drw_text(drw, x, 0, w, bh, lrpad / 2, censort, 0);
		free(censort);
	} else
		drw_text(drw, x, 0, w, bh, lrpad / 2, text, 0);

	curpos = TEXTW(text) - TEXTW(&text[cursor]);
	if ((curpos += lrpad / 2 - 1) < w) {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x + curpos, 2 + (bh-fh)/2, 2, fh - 4, 1, 0);
	}

	if (enabled(ShowNumbers)) {
		recalculatenumbers();
		rpad = TEXTW(numbers);
	}
	if (lines > 0) {
		/* draw grid */
		int i = 0, ix = (enabled(PromptIndent) ? x : 0);
		for (item = curr; item != next; item = item->right, i++)
			if (columns) {
				drawitem(
					item,
					ix + ((i / lines) *  ((mw - ix) / columns)),
					y + (((i % lines) + 1) * bh),
					(mw - ix) / columns
				);
			} else {
				drawitem(item, ix, y += bh, mw - ix);
			}
	} else if (matches) {
		/* draw horizontal list */
		x += inputw;
		w = TEXTW(lsymbol);
		if (curr->left) {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_text(drw, x, 0, w, bh, lrpad / 2, lsymbol, 0);
		}
		x += w;
		for (item = curr; item != next; item = item->right) {
			stw = TEXTW(rsymbol);
			itw = textw_clamp(enabled(TabSeparatedValues) ? item->stext : item->text, mw - x - stw - rpad);
			x = drawitem(item, x, 0, itw);
		}
		if (next) {
			w = TEXTW(rsymbol);
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_text(drw, mw - w - rpad, 0, w, bh, lrpad / 2, rsymbol, 0);
		}
	}
	if (enabled(ShowNumbers)) {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_text(drw, mw - TEXTW(numbers), 0, TEXTW(numbers), bh, lrpad / 2, numbers, 0);
	}
	drw_map(drw, win, 0, 0, mw, mh);
}

static void
grabfocus(void)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000  };
	Window focuswin;
	int i, revertwin;

	for (i = 0; i < 100; ++i) {
		XGetInputFocus(dpy, &focuswin, &revertwin);
		if (focuswin == win)
			return;
		XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
		nanosleep(&ts, NULL);
	}
	die("cannot grab focus");
}

static void
grabkeyboard(void)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000  };
	int i;

	if (embed || enabled(Managed))
		return;

	/* try to grab keyboard, we may have to wait for another process to ungrab */
	for (i = 0; i < 1000; i++) {
		if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync,
		                  GrabModeAsync, CurrentTime) == GrabSuccess)
			return;
		nanosleep(&ts, NULL);
	}
	die("cannot grab keyboard");
}

static void
match(void)
{
	if (dynamic && *dynamic)
		refreshoptions();

	if (enabled(FuzzyMatch))
		fuzzymatch();
	else
		exactmatch();
}

static void
insert(const char *str, ssize_t n)
{
	if (strlen(text) + n > sizeof text - 1)
		return;

	static char last[BUFSIZ] = "";
	if (enabled(RejectNoMatch)) {
		/* store last text value in case we need to revert it */
		memcpy(last, text, BUFSIZ);
	}

	/* move existing text out of the way, insert new text, and update cursor */
	memmove(&text[cursor + n], &text[cursor], sizeof text - cursor - MAX(n, 0));
	if (n > 0)
		memcpy(&text[cursor], str, n);
	cursor += n;
	match();

	if (!matches && enabled(RejectNoMatch)) {
		/* revert to last text value if theres no match */
		memcpy(text, last, BUFSIZ);
		cursor -= n;
		match();
	}
}

static size_t
nextrune(int inc)
{
	ssize_t n;

	/* return location of next utf8 rune in the given direction (+1 or -1) */
	for (n = cursor + inc; n + inc >= 0 && (text[n] & 0xc0) == 0x80; n += inc)
		;
	return n;
}

static void
movewordedge(int dir)
{
	if (dir < 0) { /* move cursor to the start of the word*/
		while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
			cursor = nextrune(-1);
		while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
			cursor = nextrune(-1);
	} else { /* move cursor to the end of the word */
		while (text[cursor] && strchr(worddelimiters, text[cursor]))
			cursor = nextrune(+1);
		while (text[cursor] && !strchr(worddelimiters, text[cursor]))
			cursor = nextrune(+1);
	}
}

static void
keypress(XKeyEvent *ev)
{
	char buf[32];
	int len;
	KeySym ksym;
	Status status;
	int i;
	struct item *tmpsel;
	int offscreen = 0;

	len = XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
	switch (status) {
	default: /* XLookupNone, XBufferOverflow */
		return;
	case XLookupChars:
		goto insert;
	case XLookupKeySym:
	case XLookupBoth:
		break;
	}

	if (ev->state & ControlMask) {
		switch(ksym) {
		case XK_a: ksym = XK_Home;      break;
		case XK_b: ksym = XK_Left;      break;
		case XK_c: ksym = XK_Escape;    break;
		case XK_d: ksym = XK_Delete;    break;
		case XK_e: ksym = XK_End;       break;
		case XK_f: ksym = XK_Right;     break;
		case XK_g: ksym = XK_Escape;    break;
		case XK_h: ksym = XK_BackSpace; break;
		case XK_i: ksym = XK_Tab;       break;
		case XK_j: /* fallthrough */
		case XK_J: /* fallthrough */
		case XK_m: /* fallthrough */
		case XK_M: ksym = XK_Return; ev->state &= ~ControlMask; break;
		case XK_n: ksym = XK_Down;      break;
		case XK_p: ksym = XK_Up;        break;

		case XK_k: /* delete right */
			text[cursor] = '\0';
			match();
			break;
		case XK_u: /* delete left */
			insert(NULL, 0 - cursor);
			break;
		case XK_w: /* delete word */
			while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
				insert(NULL, nextrune(-1) - cursor);
			while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
				insert(NULL, nextrune(-1) - cursor);
			break;
		case XK_y: /* paste selection */
		case XK_Y:
		case XK_v:
		case XK_V:
			XConvertSelection(dpy, (ev->state & ShiftMask) ? clip : XA_PRIMARY,
			                  utf8, utf8, win, CurrentTime);
			return;
		case XK_Left:
		case XK_KP_Left:
			movewordedge(-1);
			goto draw;
		case XK_Right:
		case XK_KP_Right:
			movewordedge(+1);
			goto draw;
		case XK_Return:
		case XK_KP_Enter:
			if (enabled(RestrictReturn))
				break;
			selsel();
			break;
		case XK_bracketleft:
			cleanup();
			exit(1);
		default:
			return;
		}
	} else if (ev->state & Mod1Mask) {
		switch(ksym) {
		case XK_b:
			movewordedge(-1);
			goto draw;
		case XK_f:
			movewordedge(+1);
			goto draw;
		case XK_g: ksym = XK_Home;  break;
		case XK_G: ksym = XK_End;   break;
		case XK_h: ksym = XK_Up;    break;
		case XK_j: ksym = XK_Next;  break;
		case XK_k: ksym = XK_Prior; break;
		case XK_l: ksym = XK_Down;  break;
		case XK_p:
			navhistory(-1);
			buf[0]=0;
			break;
		case XK_n:
			navhistory(1);
			buf[0]=0;
			break;
		default:
			return;
		}
	}

	switch(ksym) {
	default:
insert:
		if (!iscntrl(*buf))
			insert(buf, len);
		break;
	case XK_Delete:
	case XK_KP_Delete:
		if (text[cursor] == '\0')
			return;
		cursor = nextrune(+1);
		/* fallthrough */
	case XK_BackSpace:
		if (cursor == 0)
			return;
		insert(NULL, nextrune(-1) - cursor);
		break;
	case XK_End:
	case XK_KP_End:
		if (text[cursor] != '\0') {
			cursor = strlen(text);
			break;
		}
		if (next) {
			/* jump to end of list and position items in reverse */
			curr = matchend;
			calcoffsets();
			curr = prev;
			calcoffsets();
			while (next && (curr = curr->right))
				calcoffsets();
		}
		sel = matchend;
		break;
	case XK_Escape:
		cleanup();
		exit(1);
	case XK_Home:
	case XK_KP_Home:
		if (sel == matches) {
			cursor = 0;
			break;
		}
		sel = curr = matches;
		calcoffsets();
		break;
	case XK_Left:
	case XK_KP_Left:
		if (columns > 1) {
			if (!sel)
				return;
			tmpsel = sel;
			for (i = 0; i < lines; i++) {
				if (!tmpsel->left ||  tmpsel->left->right != tmpsel)
					return;
				if (tmpsel == curr)
					offscreen = 0;
				tmpsel = tmpsel->left;
			}
			sel = tmpsel;
			if (offscreen) {
				curr = prev;
				calcoffsets();
			}
			break;
		}
		if (cursor > 0 && (!sel || !sel->left || lines > 0)) {
			cursor = nextrune(-1);
			break;
		}
		if (lines > 0)
			return;
		/* fallthrough */
	case XK_Up:
	case XK_KP_Up:
		if (sel && sel->left && (sel = sel->left)->right == curr) {
			curr = prev;
			calcoffsets();
		}
		break;
	case XK_Next:
	case XK_KP_Next:
		if (!next)
			return;
		sel = curr = next;
		calcoffsets();
		break;
	case XK_Prior:
	case XK_KP_Prior:
		if (!prev)
			return;
		sel = curr = prev;
		calcoffsets();
		break;
	case XK_Return:
	case XK_KP_Enter:
		if (enabled(RestrictReturn) && (!sel || ev->state & (ShiftMask | ControlMask)))
			break;
		if (enabled(PrintInputText))
			puts((sel && ((ev->state & ShiftMask) || !cursor)) ? sel->text : text);
		else if (enabled(ContinuousOutput)) {
			if (enabled(PrintIndex))
				printf("%d\n", (sel && !(ev->state & ShiftMask)) ? sel->index : -1);
			else
				puts((sel && !(ev->state & ShiftMask)) ? sel->text : text);
		}
		if (!(ev->state & ControlMask)) {
			savehistory((sel && !(ev->state & ShiftMask))
				    ? sel->text : text);
			if (disabled(ContinuousOutput) && disabled(PrintInputText))
				printsel(ev->state);
			cleanup();
			exit(0);
		}
		break;
	case XK_Right:
	case XK_KP_Right:
		if (columns > 1) {
			if (!sel)
				return;
			tmpsel = sel;
			for (i = 0; i < lines; i++) {
				if (!tmpsel->right ||  tmpsel->right->left != tmpsel)
					return;
				tmpsel = tmpsel->right;
				if (tmpsel == next)
					offscreen = 1;
			}
			sel = tmpsel;
			if (offscreen) {
				curr = next;
				calcoffsets();
			}
			break;
		}
		if (text[cursor] != '\0') {
			cursor = nextrune(+1);
			break;
		}
		if (lines > 0)
			return;
		/* fallthrough */
	case XK_Down:
	case XK_KP_Down:
		if (sel && sel->right && (sel = sel->right) == next) {
			curr = next;
			calcoffsets();
		}
		break;
	case XK_Tab:
		if (!sel)
			return;
		strncpy(text, sel->text, sizeof text - 1);
		text[sizeof text - 1] = '\0';
		cursor = strlen(text);
		match();
		break;
	}

draw:
	if (enabled(Incremental)) {
		puts(text);
		fflush(stdout);
	}
	drawmenu();
}

static void
paste(void)
{
	char *p, *q;
	int di;
	unsigned long dl;
	Atom da;

	/* we have been given the current selection, now insert it into input */
	if (XGetWindowProperty(dpy, win, utf8, 0, (sizeof text / 4) + 1, False,
	                   utf8, &da, &di, &dl, &dl, (unsigned char **)&p)
	    == Success && p) {
		insert(p, (q = strchr(p, '\n')) ? q - p : (ssize_t)strlen(p));
		XFree(p);
	}
	drawmenu();
}

static void
xinitvisual()
{
	XVisualInfo *infos;
	XRenderPictFormat *fmt;
	int nitems;
	int i;

	XVisualInfo tpl = {
		.screen = screen,
		.depth = 32,
		.class = TrueColor
	};
	long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

	infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
	visual = NULL;
	for (i = 0; i < nitems; i++) {
		fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
		if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
			visual = infos[i].visual;
			depth = infos[i].depth;
			cmap = XCreateColormap(dpy, root, visual, AllocNone);
			useargb = 1;
			break;
		}
	}

	XFree(infos);

	if (!visual || disabled(Alpha)) {
		visual = DefaultVisual(dpy, screen);
		depth = DefaultDepth(dpy, screen);
		cmap = DefaultColormap(dpy, screen);
	}
}

static void
readstdin(void)
{
	char buf[sizeof text], *p;
	size_t i, size = 0;

	if (enabled(PasswordInput)) {
		inputw = lines = 0;
		return;
	}

	/* read each line from stdin and add it to the item list */
	for (i = 0; fgets(buf, sizeof buf, stdin); i++)	{
		if (i + 1 >= size / sizeof *items)
			if (!(items = realloc(items, (size += BUFSIZ))))
				die("cannot realloc %u bytes:", size);
		if ((p = strchr(buf, '\n')))
			*p = '\0';
		if (!(items[i].text = strdup(buf)))
			die("cannot strdup %u bytes:", strlen(buf) + 1);
		if (enabled(TabSeparatedValues)) {
			if ((p = strchr(buf, '\t')))
				*p = '\0';
			if (!(items[i].stext = strdup(buf)))
				die("cannot strdup %u bytes:", strlen(buf) + 1);
		}

		items[i].id = i; /* for multiselect */
		items[i].index = i;
		items[i].hp = arrayhas(hpitems, hplength, items[i].text);
	}
	if (items)
		items[i].text = NULL;
	lines = MIN(lines, i);
}

static void
run(void)
{
	XEvent ev;
	int i;

	while (!XNextEvent(dpy, &ev)) {
		if (preselected) {
			for (i = 0; i < preselected; i++) {
				if (sel && sel->right && (sel = sel->right) == next) {
					curr = next;
					calcoffsets();
				}
			}
			drawmenu();
			preselected = 0;
		}
		if (XFilterEvent(&ev, win))
			continue;
		switch(ev.type) {
		case ButtonPress:
			buttonpress(&ev);
			break;
		case DestroyNotify:
			if (ev.xdestroywindow.window != win)
				break;
			cleanup();
			exit(1);
		case Expose:
			if (ev.xexpose.count == 0)
				drw_map(drw, win, 0, 0, mw, mh);
			break;
		case FocusIn:
			/* regrab focus from parent window */
			if (ev.xfocus.window != win)
				grabfocus();
			break;
		case KeyPress:
			keypress(&ev.xkey);
			break;
		case SelectionNotify:
			if (ev.xselection.property == utf8)
				paste();
			break;
		case VisibilityNotify:
			if (ev.xvisibility.state != VisibilityUnobscured)
				XRaiseWindow(dpy, win);
			break;
		}
	}
}

static void
setup(void)
{
	int x, y, i, j, max_h, max_w;
	int minstrlen = 0, curstrlen = 0;
	int numwidthchecks = 100;
	int xoffset = 0, yoffset = 0, height = 0, width = 0;
	unsigned int du, tmp;
	XSetWindowAttributes swa;
	XIM xim;
	Window w, dw, *dws;
	XWindowAttributes wa;
	XClassHint ch = {"dmenu", "dmenu"};
	struct item *item;
#ifdef XINERAMA
	XineramaScreenInfo *info;
	Window pw;
	int a, di, n, area = 0;
#endif

	/* init appearance */
	for (j = 0; j < SchemeLast; j++)
		scheme[j] = drw_scm_create(drw, (const char**)colors[j], alphas[j], 2);

	clip = XInternAtom(dpy, "CLIPBOARD",   False);
	utf8 = XInternAtom(dpy, "UTF8_STRING", False);
	type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);

#ifdef XINERAMA
	i = 0;
	if (parentwin == root && (info = XineramaQueryScreens(dpy, &n))) {
		XGetInputFocus(dpy, &w, &di);
		if (mon >= 0 && mon < n)
			i = mon;
		else if (w != root && w != PointerRoot && w != None) {
			/* find top-level window containing current input focus */
			do {
				if (XQueryTree(dpy, (pw = w), &dw, &w, &dws, &du) && dws)
					XFree(dws);
			} while (w != root && w != pw);
			/* find xinerama screen with which the window intersects most */
			if (XGetWindowAttributes(dpy, pw, &wa))
				for (j = 0; j < n; j++)
					if ((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
						area = a;
						i = j;
					}
		}
		/* no focused window is on screen, so use pointer location instead */
		if (mon < 0 && !area && XQueryPointer(dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
			for (i = 0; i < n; i++)
				if (INTERSECT(x, y, 1, 1, info[i]) != 0)
					break;

		xoffset = info[i].x_org;
		yoffset = info[i].y_org;
		width = info[i].width;
		height = info[i].height;

		XFree(info);
	} else
#endif
	{
		if (!XGetWindowAttributes(dpy, parentwin, &wa))
			die("could not get embedding window attributes: 0x%lx",
			    parentwin);

		width = wa.width;
		height = wa.height;
	}

	/* calculate menu geometry */
	bh = MAX(drw->fonts->h + 2, lineheight);
	lines = MAX(lines, 0);
	mh = (lines + 1) * bh;
	promptw = (prompt && *prompt) ? TEXTW(prompt) - lrpad / 4 : 0;
 	max_h = height - vertpad * 2 - border_width * 2;
 	max_w = width - sidepad * 2;

	if (mh > max_h) {
		mh = max_h;
		bh = max_h / (lines + 1);
	}

	if (enabled(Centered)) {
		dmxp = 50;
		dmyp = 50;
		if (dmw <= 0 && dmwp == ~0)
			dmw = min_width + dmw;
	}

	if (dmw <= 0)
		dmw = max_w + dmw; // dmw can be a negative adjustment
	if (dmwp != ~0)
		dmw = dmw * dmwp / 100;

	if (dmxp != ~0)
		dmx = sidepad + (max_w - dmw) * dmxp / 100;
	else if (dmx == ~0)
		dmx = sidepad;

	if (dmyp != ~0) {
		dmy = vertpad + (max_h - mh) * dmyp / 100;
	} else if (dmy == ~0)
		dmy = enabled(TopBar) ? vertpad : max_h - mh;

	if (enabled(Centered))
		dmw = MIN(MAX(max_textw() + promptw, dmw), width);

	mw = dmw - border_width * 2;
	x = xoffset + dmx;
	y = yoffset + dmy;

	for (item = items; !lines && item && item->text; ++item) {
		curstrlen = strlen(item->text);
		if (numwidthchecks || minstrlen < curstrlen) {
			numwidthchecks = MAX(numwidthchecks - 1, 0);
			minstrlen = MAX(minstrlen, curstrlen);
			if ((tmp = textw_clamp(item->text, mw/3)) > inputw) {
				inputw = tmp;
				if (tmp == mw/3)
					break;
			}
		}
	}
	match();

	/* create menu window */
	swa.override_redirect = enabled(Managed) ? False : True;
	swa.background_pixel = 0;
	swa.colormap = cmap;
	swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask | ButtonPressMask;
	win = XCreateWindow(dpy, parentwin, x, y, mw, mh, border_width,
		depth, InputOutput, visual,
		CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &swa
	);

	if (border_width)
		XSetWindowBorder(dpy, win, scheme[SchemeBorder][ColBg].pixel);

	XSetClassHint(dpy, win, &ch);
	XChangeProperty(dpy, win, type, XA_ATOM, 32, PropModeReplace, (unsigned char *) &dock, 1);

	/* input methods */
	if ((xim = XOpenIM(dpy, NULL, NULL, NULL)) == NULL)
		die("XOpenIM failed: could not open input device");

	xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
	                XNClientWindow, win, XNFocusWindow, win, NULL);

	if (enabled(Managed)) {
		XTextProperty prop;
		char *windowtitle = prompt != NULL ? prompt : "dmenu";
		Xutf8TextListToTextProperty(dpy, &windowtitle, 1, XUTF8StringStyle, &prop);
		XSetWMName(dpy, win, &prop);
		XSetTextProperty(dpy, win, &prop, XInternAtom(dpy, "_NET_WM_NAME", False));
		XFree(prop.value);
	}

	XMapRaised(dpy, win);
	if (embed || enabled(Managed)) {
		XSelectInput(dpy, parentwin, FocusChangeMask | SubstructureNotifyMask);
		if (XQueryTree(dpy, parentwin, &dw, &w, &dws, &du) && dws) {
			for (i = 0; i < du && dws[i] != win; ++i)
				XSelectInput(dpy, dws[i], FocusChangeMask);
			XFree(dws);
		}
		grabfocus();
	}
	drw_resize(drw, mw, mh);
	drawmenu();
}

static unsigned int
textw_clamp(const char *str, unsigned int n)
{
	unsigned int w = drw_fontset_getwidth_clamp(drw, str, n) + lrpad;
	return MIN(w, n);
}

static void
usage(void)
{
	fputs("usage: dmenu [-bcfiIjJkKnNrRsStuUvxyz]"
		" [-bw width]"
		" [-g columns]"
		" [-l lines]"
		" [-p prompt]"
		"\n            "
		" [-dy command]"
		" [-it text]"
		" [-hp items]"
		" [-H histfile]"
		" [-h height]"
		" [-ps index]"
		"\n            "
		" [-x offset]"
		" [-y offset]"
		" [-z width]"
		" [-m monitor]"
		" [-w windowid]"
		"\n            "
		" [-fn font] [-nb color] [-nf color] [-sb color] [-sf color] ..."
		"\n\n", stderr);

	fputs("dmenu is a dynamic menu for X, which reads a list of newline-separated items from stdin."
		"\nWhen the user selects an item and presses Return, their choice is printed to stdout and"
		"\ndmenu terminates. Entering text will narrow the items to those matching the tokens in the"
		"\ninput.\n\n", stderr);

	char ofmt[] = "   %-30s%s%s\n";
	fprintf(stderr, "Placement options:\n");
	fprintf(stderr, ofmt, "-t", "dmenu is shown at the top of the screen", enabled(TopBar) ? " (default)" : "");
	fprintf(stderr, ofmt, "-b", "dmenu is shown at the bottom of the screen", disabled(TopBar) ? " (default)" : "");
	fprintf(stderr, ofmt, "-c", "dmenu is shown in the center of the screen", enabled(Centered) ? " (default)" : "");
	fprintf(stderr, ofmt, "-e <windowid>", "embed into the given window ID", "");
	fprintf(stderr, ofmt, "-m <monitor>", "specifies the monitor index to place dmenu on (starts at 0)", "");
	fprintf(stderr, ofmt, "-x <offset>", "specifies the x position relative to monitor", "");
	fprintf(stderr, ofmt, "-y <offset>", "specifies the y position relative to monitor", "");
	fprintf(stderr, ofmt, "-w <width>", "specifies the width of dmenu", "");

	fprintf(stderr, "\nAppearance:\n");
	fprintf(stderr, ofmt, "-bw <width>", "specifies the border width", "");
	fprintf(stderr, ofmt, "-h <height>", "specifies the height of dmenu lines", "");
	fprintf(stderr, ofmt, "-p <text>, -prompt <text>", "defines the prompt to be displayed to the left of the input field", "");
	fprintf(stderr, ofmt, "-l <lines>", "specifies the number of lines for grid presentation", "");
	fprintf(stderr, ofmt, "-g <columns>", "specifies the number of columns for grid presentation", "");

	fprintf(stderr, "\nColors:\n");
	fprintf(stderr, ofmt, "-fn <font>", "defines the font or font set used", "");
	fprintf(stderr, ofmt, "-nb <color>", "defines the normal background color", "");
	fprintf(stderr, ofmt, "-nf <color>", "defines the normal foreground color", "");
	fprintf(stderr, ofmt, "-sb <color>", "defines the selected background color", "");
	fprintf(stderr, ofmt, "-sf <color>", "defines the selected foreground color", "");
	fprintf(stderr, ofmt, "-bb <color>", "defines the border background color", "");
	fprintf(stderr, ofmt, "-bf <color>", "defines the border foreground color", "");
	fprintf(stderr, ofmt, "-pb <color>", "defines the prompt background color", "");
	fprintf(stderr, ofmt, "-pf <color>", "defines the prompt foreground color", "");
	fprintf(stderr, ofmt, "-ab <color>", "defines the adjacent background color", "");
	fprintf(stderr, ofmt, "-af <color>", "defines the adjacent foreground color", "");
	fprintf(stderr, ofmt, "-ob <color>", "defines the out(put) background color", "");
	fprintf(stderr, ofmt, "-of <color>", "defines the out(put) foreground color", "");
	fprintf(stderr, ofmt, "-hb <color>", "defines the high priority background color", "");
	fprintf(stderr, ofmt, "-hf <color>", "defines the high priority foreground color", "");
	fprintf(stderr, ofmt, "-nhb <color>", "defines the normal highlight background color", "");
	fprintf(stderr, ofmt, "-nhf <color>", "defines the normal highlight foreground color", "");
	fprintf(stderr, ofmt, "-shb <color>", "defines the selected highlight background color", "");
	fprintf(stderr, ofmt, "-shf <color>", "defines the selected highlight foreground color", "");

	fprintf(stderr, "\nFor color settings #RGB, #RRGGBB, and X color names are supported.\n");

	fprintf(stderr, "\nFunctionality:\n");
	fprintf(stderr, ofmt, "-dy <command>", "a command used to dynamically change the dmenu options", "");
	fprintf(stderr, ofmt, "-hp <items>", "comma separated list of high priority items", "");
	fprintf(stderr, ofmt, "-it <text>", "starts dmenu with initial text already typed in", "");
	fprintf(stderr, ofmt, "-ps <index>", "preselect the item with the given index", "");
	fprintf(stderr, ofmt, "-f", "dmenu grabs the keyboard before reading stdin if not reading from a tty", "");
	fprintf(stderr, ofmt, "-H <histfile>", "specifies the history file to use", "");
	fprintf(stderr, ofmt, "-i, -NoCaseSensitive", "makes dmenu case-insensitive", disabled(CaseSensitive) ? " (default)" : "");
	fprintf(stderr, ofmt, "-I, -CaseSensitive", "makes dmenu case-sensitive", enabled(CaseSensitive) ? " (default)" : "");
	fprintf(stderr, ofmt, "-j, -RejectNoMatch", "makes dmenu reject input if it would result in no matching item", enabled(RejectNoMatch) ? " (default)" : "");
	fprintf(stderr, ofmt, "-J, -NoRejectNoMatch", "input can be entered into dmenu freely", disabled(RejectNoMatch) ? " (default)" : "");
	fprintf(stderr, ofmt, "-k, -PrintIndex", "makes dmenu print out the 0-based index instead of the matched text", enabled(PrintIndex) ? " (default)" : "");
	fprintf(stderr, ofmt, "-K, -NoPrintIndex", "makes dmenu print out matched text itself", disabled(PrintIndex) ? " (default)" : "");
	fprintf(stderr, ofmt, "-n, -InstantReturn", "makes dmenu select an item immediately if there is only one matching option left", enabled(InstantReturn) ? " (default)" : "");
	fprintf(stderr, ofmt, "-N, -NoInstantReturn", "user must press enter to select an item (disables auto-select)", disabled(InstantReturn) ? " (default)" : "");
	fprintf(stderr, ofmt, "-u, -PasswordInput", "indicates that the input is a password and should be masked", enabled(PasswordInput) ? " (default)" : "");
	fprintf(stderr, ofmt, "-U, -NoPasswordInput", "indicates that the input is not a password", disabled(PasswordInput) ? " (default)" : "");
	fprintf(stderr, ofmt, "-s, -Sort", "enables sorting of menu items after matching", enabled(Sort) ? " (default)" : "");
	fprintf(stderr, ofmt, "-S, -NoSort", "disables sorting of menu items after matching", disabled(Sort) ? " (default)" : "");
	fprintf(stderr, ofmt, "-r, -RestrictReturn", "disables Shift-Return and Ctrl-Return to restrict dmenu to only output one item", enabled(RestrictReturn) ? " (default)" : "");
	fprintf(stderr, ofmt, "-R, -NoRestrictReturn", "enables Shift-Return and Ctrl-Return to allow dmenu to output more than one item", disabled(RestrictReturn) ? " (default)" : "");
	fprintf(stderr, ofmt, "-v", "prints version information to stdout, then exits", "");
	fprintf(stderr, ofmt, "-xpad <offset>", "sets the horizontal padding value for dmenu", "");
	fprintf(stderr, ofmt, "-ypad <offset>", "sets the vertical padding value for dmenu", "");
	fprintf(stderr, ofmt, "    -Alpha", "enables transparency", enabled(Alpha) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoAlpha", "disables transparency", disabled(Alpha) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -ColorEmoji", "enables color emoji in dmenu (requires libxft-bgra)", enabled(ColorEmoji) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoColorEmoji", "disables color emoji", disabled(ColorEmoji) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -ContinuousOutput", "makes dmenu print out selected items immediately rather than at the end", enabled(ContinuousOutput) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoContinuousOutput", "dmenu prints out the selected items when enter is pressed", disabled(ContinuousOutput) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -FuzzyMatch", "allows fuzzy-matching of items in dmenu", enabled(FuzzyMatch) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoFuzzyMatch", "enables exact matching of items in dmenu", disabled(FuzzyMatch) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -HighlightAdjacent", "makes dmenu highlight items adjacent to the selected item", enabled(HighlightAdjacent) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoHighlightAdjacent", "only the selected item is highlighted", disabled(HighlightAdjacent) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -Incremental", "makes dmenu print out the current text each time a key is pressed", enabled(Incremental) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoIncremental", "dmenu will not print out the current text each time a key is pressed", disabled(Incremental) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -Managed", "allows dmenu to be managed by a window manager", enabled(Managed) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoManaged", "dmenu manages itself, window manager not to interfere", disabled(Managed) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -PrintInputText", "makes dmenu print the input text instead of the selected item", enabled(PrintInputText) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoPrintInputText", "dmenu to print the text of the selected item", disabled(PrintInputText) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -PromptIndent", "makes dmenu indent items at the same level as the prompt on multi-line views", enabled(PromptIndent) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoPromptIndent", "items on multi-line views are not indented", disabled(PromptIndent) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -ShowNumbers", "makes dmenu display the number of matched and total items in the top right corner", enabled(ShowNumbers) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoShowNumbers", "dmenu will not show item count", disabled(ShowNumbers) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -TabSeparatedValues", "makes dmenu hide values following a tab character", enabled(TabSeparatedValues) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoTabSeparatedValues", "dmenu will not split items containing tabs", disabled(TabSeparatedValues) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -Xresources", "makes dmenu read X resources at startup", enabled(Xresources) ? " (default)" : "");
	fprintf(stderr, ofmt, "    -NoXresources", "dmenu will not read X resources", disabled(Xresources) ? " (default)" : "");

	fprintf(stderr, "\n");
	exit(1);
}

#define arg(A) (!strcmp(argv[i], A))

int
main(int argc, char *argv[])
{
	XWindowAttributes wa;
	int i, val;
	char ch;
	int fast = 0;

	enablefunc(functionality);

	if (disabled(CaseSensitive)) {
		fstrncmp = strncasecmp;
		fstrstr = cistrstr;
	}

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("cannot open display");

	/* These need to be checked before we init the visuals and read X resources. */
	for (i = 1; i < argc; i++) {
		if arg("-v") { /* prints version information */
			puts("dmenu-"VERSION);
			exit(0);
		} else if arg("--help") {
			usage();
		} else if arg("-Alpha") { /* enables transparency (alpha, opacity) */
			enablefunc(Alpha);
		} else if arg("-NoAlpha") { /* disables transparency (alpha, opacity) */
			disablefunc(Alpha);
		} else if arg("-Xresources") {
			enablefunc(Xresources);
		} else if arg("-NoXresources") {
			disablefunc(Xresources);
		} else if (arg("-e") && i + 1 < argc) { /* embedding window id */
			argv[i][0] = '\0';
			embed = argv[++i];
		} else
			continue;
		argv[i][0] = '\0'; // mark as used
	}

	/* Set up the X window */
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	if (!embed || !(parentwin = strtol(embed, NULL, 0)))
		parentwin = root;
	if (!XGetWindowAttributes(dpy, parentwin, &wa))
		die("could not get embedding window attributes: 0x%lx", parentwin);
	xinitvisual();
	drw = drw_create(dpy, screen, root, wa.width, wa.height, visual, depth, cmap);

	/* X resources are loaded before the rest of the parameter are read. This is
	 * to allow for command line arguments to override the X resource colours. */
	if (enabled(Xresources))
		readxresources();

	/* Parse remaining options */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '\0')
			continue;

		/* Placement */
		if (arg("-TopBar") || arg("-t")) { /* dmenu appears at the top of the screen */
			enablefunc(TopBar);
			disablefunc(Centered);
		} else if (arg("-BottomBar") || arg("-b")) { /* dmenu appears at the bottom of the screen */
			disablefunc(TopBar);
			disablefunc(Centered);
		} else if (arg("-Centered") || arg("-c")) { /* centers dmenu window on the screen */
			enablefunc(Centered);
			disablefunc(TopBar);
		} else if arg("-x") { /* window x offset */
			switch (sscanf(argv[++i], "%d%c", &val, &ch)) {
				case 2:
					if (ch == '%') {
						dmxp = val;
						break;
					}
					/* falls through */
				case 1:
					dmx = val;
					break;
			}
			disablefunc(Centered);
		} else if arg("-y") { /* window y offset (from bottom up if -b) */
			switch (sscanf(argv[++i], "%d%c", &val, &ch)) {
				case 2:
					if (ch == '%') {
						dmyp = val;
						break;
					}
					/* falls through */
				case 1:
					dmy = val;
					break;
			}
			disablefunc(Centered);
		} else if (arg("-w") || arg("-z")) { /* make dmenu this wide, -z for compatibility reasons */
			switch (sscanf(argv[++i], "%d%c", &val, &ch)) {
				case 2:
					if (ch == '%') {
						dmwp = val;
						break;
					}
					/* falls through */
				case 1:
					dmw = val;
					break;
			}
		/* Functionality toggles */
		} else if (arg("-CaseSensitive") || arg("-I")) { /* case-sensitive item matching */
			fstrncmp = strncmp;
			fstrstr = strstr;
		} else if (arg("-NoCaseSensitive") || arg("-i")) { /* case-insensitive item matching */
			fstrncmp = strncasecmp;
			fstrstr = cistrstr;
		} else if (arg("-InstantReturn") || arg("-n")) { /* instant select only match */
			enablefunc(InstantReturn);
		} else if (arg("-NoInstantReturn") || arg("-N")) { /* instant select only match */
			disablefunc(InstantReturn);
		} else if (arg("-PasswordInput") || arg("-u")) { /* indicates that the input is a password and should be masked */
			enablefunc(PasswordInput);
		} else if (arg("-NoPasswordInput") || arg("-U")) { /* indicates that the input is not a password */
			disablefunc(PasswordInput);
		} else if (arg("-Sort") || arg("-s")) { /* sort matches */
			enablefunc(Sort);
		} else if (arg("-NoSort") || arg("-S")) { /* do not sort matches */
			disablefunc(Sort);
		} else if arg("-Incremental") { /* incremental, print out the current text each time a key is pressed */
			enablefunc(Incremental);
		} else if arg("-NoIncremental") {
			disablefunc(Incremental);
		} else if (arg("-PrintIndex") || arg("-k")) { /* makes dmenu print out the 0-based index instead of the matched text itself */
			enablefunc(PrintIndex);
		} else if (arg("-NoPrintIndex") || arg("-K")) { /* makes dmenu print out the matched text itself */
			disablefunc(PrintIndex);
		} else if (arg("-RejectNoMatch") || arg("-j")) { /* makes dmenu reject input if it would result in no matching item */
			enablefunc(RejectNoMatch);
		} else if (arg("-NoRejectNoMatch") || arg("-J")) { /* allow input to be added freely into dmenu */
			disablefunc(RejectNoMatch);
		} else if (arg("-RestrictReturn") || arg("-r")) { /* disallow Shift-Return and Ctrl-Return */
			enablefunc(RestrictReturn);
		} else if (arg("-NoRestrictReturn") || arg("-R")) { /* allow Shift-Return and Ctrl-Return */
			disablefunc(RestrictReturn);
		} else if arg("-Managed") { /* display as managed wm window */
			enablefunc(Managed);
		} else if arg("-NoManaged") { /* dmenu manages itself, window manager not to interfere */
			disablefunc(Managed);
		} else if arg("-ColorEmoji") { /* enables color emoji in dmenu (requires libxft-bgra) */
			enablefunc(ColorEmoji);
		} else if arg("-NoColorEmoji") {
			disablefunc(ColorEmoji);
		} else if arg("-ContinuousOutput") { /* makes dmenu print out selected items immediately rather than at the end */
			enablefunc(ContinuousOutput);
		} else if arg("-NoContinuousOutput") {
			disablefunc(ContinuousOutput);
		} else if arg("-FuzzyMatch") { /* allows fuzzy-matching of items in dmenu */
			enablefunc(FuzzyMatch);
		} else if arg("-NoFuzzyMatch") {
			disablefunc(FuzzyMatch);
		} else if arg("-HighlightAdjacent") {
			enablefunc(HighlightAdjacent);
		} else if arg("-NoHighlightAdjacent") {
			disablefunc(HighlightAdjacent);
		} else if arg("-PrintInputText") {
			enablefunc(PrintInputText);
		} else if arg("-NoPrintInputText") {
			disablefunc(PrintInputText);
		} else if arg("-PromptIndent") {
			enablefunc(PromptIndent);
		} else if arg("-NoPromptIndent") {
			disablefunc(PromptIndent);
		} else if arg("-ShowNumbers") {
			enablefunc(ShowNumbers);
		} else if arg("-NoShowNumbers") {
			disablefunc(ShowNumbers);
		} else if arg("-TabSeparatedValues") {
			enablefunc(TabSeparatedValues);
		} else if arg("-NoTabSeparatedValues") {
			disablefunc(TabSeparatedValues);

		/* These options take one argument */
		} else if (i + 1 == argc) {
			usage();
		} else if arg("-m") {
			mon = atoi(argv[++i]);
		} else if arg("-bw") { /* border width around dmenu */
			border_width = atoi(argv[++i]);
		} else if arg("-l") { /* number of lines in vertical list */
			lines = atoi(argv[++i]);
		} else if arg("-g") {   /* number of columns in grid */
			columns = atoi(argv[++i]);
			if (columns && lines == 0)
				lines = 1;
		} else if arg("-f") { /* grabs keyboard before reading stdin */
			fast = 1;
		} else if arg("-H") {
			histfile = argv[++i];
		} else if (arg("-p") || arg("-prompt")) { /* adds prompt to left of input field */
			prompt = argv[++i];
		} else if arg("-h") { /* minimum height of one menu line */
			lineheight = atoi(argv[++i]);
		} else if arg("-it") { /* initial text */
		    const char * text = argv[++i];
		    insert(text, strlen(text));
		} else if arg("-ps") { /* preselected item */
			preselected = atoi(argv[++i]);
		} else if arg("-dy") { /* dynamic command to run */
			dynamic = argv[++i];
		} else if arg("-hp") { /* high priority items */
			hpitems = tokenize(argv[++i], ",", &hplength);
		} else if arg("-xpad") { /* sets horizontal padding */
			sidepad = atoi(argv[++i]);
		} else if arg("-ypad") { /* sets vertical padding */
			vertpad = atoi(argv[++i]);
		/* Color arguments */
		} else if arg("-fn") { /* font or font set */
			fonts[0] = argv[++i];
		} else if arg("-ab") { /* adjacent background color */
			colors[SchemeAdjacent][ColBg] = argv[++i];
		} else if arg("-af") { /* adjacent foreground color */
			colors[SchemeAdjacent][ColFg] = argv[++i];
		} else if arg("-bb") { /* border background color */
			colors[SchemeBorder][ColBg] = argv[++i];
		} else if arg("-bf") { /* border foreground color */
			colors[SchemeBorder][ColFg] = argv[++i];
		} else if arg("-nb") { /* normal background color */
			colors[SchemeNorm][ColBg] = argv[++i];
		} else if arg("-nf") { /* normal foreground color */
			colors[SchemeNorm][ColFg] = argv[++i];
		} else if arg("-ob") { /* out(put) background color */
			colors[SchemeOut][ColBg] = argv[++i];
		} else if arg("-of") { /* out(put) foreground color */
			colors[SchemeOut][ColFg] = argv[++i];
		} else if arg("-sb") { /* selected background color */
			colors[SchemeSel][ColBg] = argv[++i];
		} else if arg("-sf") { /* selected foreground color */
			colors[SchemeSel][ColFg] = argv[++i];
		} else if arg("-pb") { /* prompt background color */
			colors[SchemePrompt][ColBg] = argv[++i];
		} else if arg("-pf") { /* prompt foreground color */
			colors[SchemePrompt][ColFg] = argv[++i];
		} else if arg("-hb") { /* high priority background color */
			colors[SchemeHp][ColBg] = argv[++i];
		} else if arg("-hf") { /* high priority foreground color */
			colors[SchemeHp][ColFg] = argv[++i];
		} else if arg("-nhb") { /* normal hi background color */
			colors[SchemeNormHighlight][ColBg] = argv[++i];
		} else if arg("-nhf") { /* normal hi foreground color */
			colors[SchemeNormHighlight][ColFg] = argv[++i];
		} else if arg("-shb") { /* selected hi background color */
			colors[SchemeSelHighlight][ColBg] = argv[++i];
		} else if arg("-shf") { /* selected hi foreground color */
			colors[SchemeSelHighlight][ColFg] = argv[++i];
		} else { /* a catch all for any argument that is not recognised */
			usage();
		}
	}

	if (!drw_fontset_create(drw, (const char**)fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");

	lrpad = drw->fonts->h;

	if (lineheight == -1)
		lineheight = drw->fonts->h * 2.5;

#ifdef __OpenBSD__
	if (pledge("stdio rpath", NULL) == -1)
		die("pledge");
#endif
	loadhistory();

	if (fast && !isatty(0)) {
		grabkeyboard();
		if (!(dynamic && *dynamic))
			readstdin();
	} else {
		if (!(dynamic && *dynamic))
			readstdin();
		grabkeyboard();
	}
	setup();
	run();

	return 1; /* unreachable */
}
