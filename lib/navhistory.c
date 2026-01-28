static char *histfile;
static char **history;
static size_t histsz, histpos;
static size_t cap = 0;
static struct item *backup_items = NULL;

void
cleanhistory(void)
{
	int i;

	for (i = 0; i < histsz; i++) {
		free(history[i]);
	}
	free(history);
}

void
loadhistory(void)
{
	FILE *fp = NULL;
	size_t llen;
	char *line;

	if (!histfile) {
		return;
	}

	fp = fopen(histfile, "r");
	if (!fp) {
		return;
	}

	for (;;) {
		line = NULL;
		llen = 0;
		if (-1 == getline(&line, &llen, fp)) {
			if (ferror(fp)) {
				die("failed to read history");
			}
			free(line);
			break;
		}

		addhistory(line);
		free(line);
	}
	histpos = histsz;

	if (fclose(fp)) {
		die("failed to close file %s", histfile);
	}
}

static void
navhistory(const Arg *arg)
{
	static char def[BUFSIZ];
	char *p = NULL;
	size_t len = 0;

	if (!history || histpos + 1 == 0)
		return;

	if (histsz == histpos) {
		strncpy(def, text, sizeof(def));
	}

	switch(arg->i) {
	case 1:
		if (histpos < histsz - 1) {
			p = history[++histpos];
		} else if (histpos == histsz - 1) {
			p = def;
			histpos++;
		}
		break;
	case -1:
		if (histpos > 0) {
			p = history[--histpos];
		}
		break;
	}
	if (p == NULL) {
		return;
	}

	len = MIN(strlen(p), BUFSIZ - 1);
	strncpy(text, p, len);
	text[len] = '\0';
	cursor = len;
	match();
}

void
searchnavhistory(const Arg *arg)
{
	togglehistoryitems();
	match();
}

void
addhistory(char *input)
{
	unsigned int i;

	if (!histfile ||
	    0 == maxhist ||
	    0 == strlen(input)) {
		return;
	}

	strtok(input, "\n");

	if (histnodup) {
		for (i = 0; i < histsz; i++) {
			if (!strcmp(input, history[i])) {
				return;
			}
		}
	}

	if (cap == histsz) {
		reallochistory();
	}

	history[histsz] = strdup(input);
	histsz++;
}

void
addhistoryitem(struct item *item)
{
	if (separator && item->text_output && item->text != item->text_output) {
		int histlen = strlen(item->text) + strlen(item->text_output) + 2;
		char *histitem = ecalloc(histlen + 1, sizeof(char *));
		snprintf(histitem, histlen, "%s%c%s", item->text, separator, item->text_output);
		addhistory(histitem);
		free(histitem);
	} else {
		addhistory(item->text);
	}
}

void
reallochistory(void)
{
	size_t oldcap = cap;
	cap += 64;
	char **newhistory = realloc(history, cap * sizeof *history);
	if (!newhistory) {
		die("failed to realloc memory");
	}

	history = newhistory;
	memset(history + oldcap, 0, (cap - oldcap) * sizeof *history);
}

void
togglehistoryitems(void)
{
	int i;
	char *p;

	if (!histfile)
		return;

	if (backup_items) {
		restorebackupitems();
		return;
	}

	backup_items = items;
	items = calloc(histsz + 1, sizeof(struct item));
	if (!items) {
		die("cannot allocate memory");
	}

	for (i = 0; i < histsz; i++) {
		items[i].text = strdup(history[i]);
		if (separator && (p = sepchr(items[i].text, separator))) {
			*p = '\0';
			items[i].text_output = ++p;
		} else {
			items[i].text_output = items[i].text;
		}
	}
}

void
restorebackupitems(void)
{
	size_t i;

	if (!backup_items)
		return;

	for (i = 0; items && items[i].text; ++i) {
		free(items[i].text);
	}
	free(items);

	items = backup_items;
	backup_items = NULL;
}

void
savehistory(void)
{
	unsigned int i;
	FILE *fp;

	if (!histfile || 0 == maxhist)
		return;

	fp = fopen(histfile, "w");
	if (!fp) {
		die("failed to open %s", histfile);
	}

	for (i = histsz < maxhist ? 0 : histsz - maxhist; i < histsz; i++) {
		if (0 >= fprintf(fp, "%s\n", history[i])) {
			die("failed to write to %s", histfile);
		}
	}

	if (fclose(fp)) {
		die("failed to close file %s", histfile);
	}
}
