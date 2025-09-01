#include <X11/Xresource.h>

void
readxresources(void)
{
	XrmInitialize();
	int s;
	int len = 40;
	char resource[len];
	char *pattern = "dmenu.%s.%s.color";

	char* xrm;
	if ((xrm = XResourceManagerString(drw->dpy))) {
		char *type;
		XrmDatabase xdb = XrmGetStringDatabase(xrm);
		XrmValue xval;

		if (!normal_fonts && XrmGetResource(xdb, "dmenu.font", "*", &type, &xval)) {
			drw_font_add(drw, &normal_fonts, xval.addr);
		}

		if (!selected_fonts && XrmGetResource(xdb, "dmenu.selfont", "*", &type, &xval)) {
			drw_font_add(drw, &selected_fonts, xval.addr);
		}

		if (!output_fonts && XrmGetResource(xdb, "dmenu.outfont", "*", &type, &xval)) {
			drw_font_add(drw, &output_fonts, xval.addr);
		}

		for (s = 0; s < SchemeLast; s++) {
			if (!scheme[s][ColFg].pixel) {
				snprintf(resource, len, pattern, colors[s][ColResource], "fg");
				if (XrmGetResource(xdb, resource, "*", &type, &xval))
					drw_clr_create(drw, &scheme[s][ColFg], xval.addr, alphas[s][ColFg]);
			}

			if (!scheme[s][ColBg].pixel) {
				snprintf(resource, len, pattern, colors[s][ColResource], "bg");
				if (XrmGetResource(xdb, resource, "*", &type, &xval))
					drw_clr_create(drw, &scheme[s][ColBg], xval.addr, alphas[s][ColBg]);
			}
		}

		XrmDestroyDatabase(xdb);
	}
}
