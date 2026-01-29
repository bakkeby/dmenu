int
issel(size_t id)
{
	for (int i = 0; i < selidsize; i++)
		if (selid[i] == id)
			return 1;
	return 0;
}

void
printinput(void)
{
	int i;
	struct item *item;

	for (i = 0; i < selidsize; i++) {
		if (selid[i] != -1) {
			item = &items[selid[i]];
			printitem(item);
		}
	}

	printtext(text);
}

void
printselected()
{
	int i;
	struct item *item;

	for (i = 0; i < selidsize; i++) {
		if (selid[i] != -1 && (!sel || sel->id != selid[i])) {
			item = &items[selid[i]];
			printitem(item);
		}
	}

	printitem(sel);
}

void
printitem(struct item *item)
{
	if (!item)
		return;

	addhistoryitem(item);

	if (enabled(PrintIndex)) {
		printf("%d\n", sel->index);
		return;
	}

	if (double_print && separator && item->text != item->text_output) {
		if (separator_reverse) {
			printf("%s%c%s\n", item->text_output, separator, item->text);
		} else {
			printf("%s%c%s\n", item->text, separator, item->text_output);
		}
		return;
	}

	puts(item->text_output);
}

void
printtext(char *text)
{
	if (!text || !strlen(text))
		return;

	addhistory(text);
	puts(text);
}

void
selsel(void)
{
	if (!sel || backup_items != NULL)
		return;
	if (issel(sel->id)) {
		for (int i = 0; i < selidsize; i++)
			if (selid[i] == sel->id)
				selid[i] = -1;
	} else {
		for (int i = 0; i < selidsize; i++)
			if (selid[i] == -1) {
				selid[i] = sel->id;
				return;
			}
		selidsize++;
		selid = realloc(selid, (selidsize + 1) * sizeof(int));
		selid[selidsize - 1] = sel->id;
	}
}
