static void
refreshoptions()
{
	int dynlen = strlen(dynamic);
	char* cmd= malloc(dynlen + strlen(text) + 2);

	/* Clear selections on refresh */
	for (int i = 0; i < selidsize; i++)
		selid[i] = -1;

	if (cmd == NULL)
		die("malloc:");
	sprintf(cmd, "%s %s", dynamic, text);
	FILE *stream = popen(cmd, "r");
	if (!stream)
		die("popen(%s):", cmd);
	readstream(stream);
	int pc = pclose(stream);
	if (pc == -1)
		die("pclose:");
	free(cmd);
	curr = sel = items;
}

static void
readstream(FILE* stream)
{
	char buf[sizeof text], *p;
	size_t i, imax = 0, size = 0;
	XGlyphInfo ext;

	/* read each line from stdin and add it to the item list */
	for (i = 0; fgets(buf, sizeof buf, stream); i++) {
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
		items[i].id = i;
		items[i].hp = arrayhas(hpitems, hplength, items[i].text);
		XftTextExtentsUtf8(drw->fonts->dpy, drw->fonts->xfont, (XftChar8 *)buf, strlen(buf), &ext);
		if (ext.xOff > inputw) {
			inputw = ext.xOff;
			imax = i;
		}
	}

	/* If the command did not give any output at all, then do not clear the existing items */
	if (!i)
		return;

	if (items)
		items[i].text = NULL;
	inputw = items ? TEXTW(items[imax].text) : 0;
	if (!dynamic || !*dynamic)
		lines = MIN(lines, i);
	else {
		text[0] = '\0';
		cursor = 0;
	}
}