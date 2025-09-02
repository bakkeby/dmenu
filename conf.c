#include <libconfig.h>

static config_t cfg;
static int config_loaded = 0;

void set_config_path(const char* filename, char *config_path, char *config_file);
int setting_length(const config_setting_t *cfg);
const char *setting_get_string_elem(const config_setting_t *cfg, int i);
int setting_get_int_elem(const config_setting_t *cfg, int i);
int config_lookup_strdup(const config_t *cfg, const char *name, char **strptr);
int config_setting_lookup_strdup(const config_setting_t *cfg, const char *name, char **strptr);
int config_lookup_sloppy_bool(const config_t *cfg, const char *name, int *ptr);
int config_lookup_unsigned_int(const config_t *cfg, const char *name, unsigned int *ptr);
int config_setting_lookup_sloppy_bool(const config_setting_t *cfg, const char *name, int *ptr);
int config_setting_lookup_unsigned_int(const config_setting_t *cfg, const char *name, unsigned int *ptr);
int config_setting_get_sloppy_bool(const config_setting_t *cfg, int *ptr);
int config_setting_get_unsigned_int(const config_setting_t *cfg_item, unsigned int *ptr);

void load_config(void);
void load_fonts(void);
void load_settings(void);
void load_powerline(void);
void load_misc_configs(void);
void load_functionality(void);
void load_colors(void);
void load_alphas(void);
void load_keybindings(void);

int parse_powerline_string(const char *string);
int parse_scheme(const char *string);
unsigned int parse_modifier(const char *string);
ArgFunc parse_function(const char *string);

void add_key_binding(unsigned int mod, KeySym keysym, ArgFunc function, int argument, void *void_argument, float float_argument);

int
setting_length(const config_setting_t *cfg)
{
	if (!cfg)
		return 0;

	switch (config_setting_type(cfg)) {
	case CONFIG_TYPE_GROUP:
	case CONFIG_TYPE_LIST:
	case CONFIG_TYPE_ARRAY:
		return config_setting_length(cfg);
	}

	return 1;
}

const char *
setting_get_string_elem(const config_setting_t *cfg, int i)
{
	if (!cfg)
		return NULL;

	switch (config_setting_type(cfg)) {
	case CONFIG_TYPE_GROUP:
	case CONFIG_TYPE_LIST:
	case CONFIG_TYPE_ARRAY:
		return config_setting_get_string_elem(cfg, i);
	}

	return config_setting_get_string(cfg);
}

int
setting_get_int_elem(const config_setting_t *cfg, int i)
{
	if (!cfg)
		return 0;

	switch (config_setting_type(cfg)) {
	case CONFIG_TYPE_GROUP:
	case CONFIG_TYPE_LIST:
	case CONFIG_TYPE_ARRAY:
		return config_setting_get_int_elem(cfg, i);
	}

	return config_setting_get_int(cfg);
}

const config_setting_t *
setting_get_elem(const config_setting_t *cfg, int i)
{
	if (!cfg)
		return NULL;

	switch (config_setting_type(cfg)) {
	case CONFIG_TYPE_GROUP:
	case CONFIG_TYPE_LIST:
	case CONFIG_TYPE_ARRAY:
		return config_setting_get_elem(cfg, i);
	}

	return cfg;
}

int
config_lookup_strdup(const config_t *cfg, const char *name, char **strptr)
{
	const char *string;
	if (config_lookup_string(cfg, name, &string)) {
		free(*strptr);
		*strptr = strdup(string);
		return 1;
	}

	return 0;
}

int
config_setting_lookup_strdup(const config_setting_t *cfg, const char *name, char **strptr)
{
	const char *string;
	if (config_setting_lookup_string(cfg, name, &string)) {
		free(*strptr);
		*strptr = strdup(string);
		return 1;
	}

	return 0;
}

int
config_lookup_sloppy_bool(const config_t *cfg, const char *name, int *ptr)
{
	return config_setting_get_sloppy_bool(config_lookup(cfg, name), ptr);
}

int
config_setting_lookup_sloppy_bool(const config_setting_t *cfg, const char *name, int *ptr)
{
	return config_setting_get_sloppy_bool(config_setting_lookup(cfg, name), ptr);
}

int
config_setting_get_sloppy_bool(const config_setting_t *cfg_item, int *ptr)
{
	const char *string;

	if (!cfg_item)
		return 0;

	switch (config_setting_type(cfg_item)) {
	case CONFIG_TYPE_INT:
		*ptr = config_setting_get_int(cfg_item);
		return 1;
	case CONFIG_TYPE_STRING:
		string = config_setting_get_string(cfg_item);

		if (string && strlen(string)) {
			char a = tolower(string[0]);
			/* Match for positives like "true", "yes" and "on" */
			*ptr = (a == 't' || a == 'y' || !strcasecmp(string, "on"));
			return 1;
		}
		break;
	case CONFIG_TYPE_BOOL:
		*ptr = config_setting_get_bool(cfg_item);
		return 1;
	}

	return 0;
}

int
config_lookup_unsigned_int(const config_t *cfg, const char *name, unsigned int *ptr)
{
	return config_setting_get_unsigned_int(config_lookup(cfg, name), ptr);
}

int
config_setting_lookup_unsigned_int(const config_setting_t *cfg, const char *name, unsigned int *ptr)
{
	return config_setting_get_unsigned_int(config_setting_lookup(cfg, name), ptr);
}

int
config_setting_get_unsigned_int(const config_setting_t *cfg_item, unsigned int *ptr)
{
	if (!cfg_item)
		return 0;

	int integer = config_setting_get_int(cfg_item);

	if (integer >= 0) {
		*ptr = (unsigned int)integer;
		return 1;
	}

	return 1;
}

void
set_config_path(const char* filename, char *config_path, char *config_file)
{
	const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
	const char* home = getenv("HOME");

	if (xdg_config_home && xdg_config_home[0] != '\0') {
		snprintf(config_path, PATH_MAX, "%s/dmenu/", xdg_config_home);
	} else if (home) {
		snprintf(config_path, PATH_MAX, "%s/.config/dmenu/", home);
	} else {
		return;
	}

	snprintf(config_file, PATH_MAX, "%s/%s", config_path, filename);
}

void
load_config(void)
{
	char config_path[PATH_MAX] = {0};
	char config_file[PATH_MAX] = {0};

	set_config_path("dmenu.cfg", config_path, config_file);
	config_init(&cfg);
	config_set_include_dir(&cfg, config_path);
	if (!config_read_file(&cfg, config_file)) {
		if (strcmp(config_error_text(&cfg), "file I/O error")) {
			fprintf(stderr, "Error reading config at %s\n", config_file);
			fprintf(stderr, "%s:%d - %s\n",
					config_error_file(&cfg),
					config_error_line(&cfg),
					config_error_text(&cfg));
		}

		config_destroy(&cfg);
		return;
	}

	config_loaded = 1;
}

void
cleanup_config(void)
{
	if (!config_loaded)
		return;

	config_destroy(&cfg);
}

void
load_fonts()
{
	int i, num_fonts;
	config_setting_t *fonts;

	if (!config_loaded)
		return;

	if (!normal_fonts) {
		fonts = config_lookup(&cfg, "fonts.normal");
		num_fonts = setting_length(fonts);
		for (i = 0; i < num_fonts; i++) {
			drw_font_add(drw, &normal_fonts, setting_get_string_elem(fonts, i));
		}
	}

	if (!selected_fonts) {
		fonts = config_lookup(&cfg, "fonts.selected");
		num_fonts = setting_length(fonts);
		for (i = 0; i < num_fonts; i++) {
			drw_font_add(drw, &selected_fonts, setting_get_string_elem(fonts, i));
		}
	}

	if (!output_fonts) {
		fonts = config_lookup(&cfg, "fonts.output");
		num_fonts = setting_length(fonts);
		for (i = 0; i < num_fonts; i++) {
			drw_font_add(drw, &output_fonts, setting_get_string_elem(fonts, i));
		}
	}

	if (!selected_fonts)
		selected_fonts = normal_fonts;

	if (!output_fonts)
		output_fonts = normal_fonts;

	drw->fonts = normal_fonts;
}

void
load_misc_configs()
{
	const char *string;

	if (!config_loaded)
		return;

	if (!prompt_string)
		config_lookup_strdup(&cfg, "prompt", &prompt_string);

	config_lookup_strdup(&cfg, "symbols.left", &left_symbol);
	config_lookup_strdup(&cfg, "symbols.right", &right_symbol);

	if (config_lookup_string(&cfg, "symbols.censored", &string))
		censored_symbol = string[0];
}

void
load_settings(void)
{
	if (!config_loaded)
		return;

	load_powerline();
	config_lookup_unsigned_int(&cfg, "lines", &lines);
	config_lookup_unsigned_int(&cfg, "columns", &columns);
	config_lookup_unsigned_int(&cfg, "lineheight", &lineheight);
	config_lookup_unsigned_int(&cfg, "min_width", &min_width);
	config_lookup_unsigned_int(&cfg, "maxhist", &maxhist);
	config_lookup_unsigned_int(&cfg, "border_width", &border_width);
	config_lookup_int(&cfg, "vertpad", &vertpad);
	config_lookup_int(&cfg, "sidepad", &sidepad);
	config_lookup_sloppy_bool(&cfg, "histnodup", &histnodup);
	config_lookup_strdup(&cfg, "", &word_delimiters);
}

void
load_powerline()
{
	config_setting_t *pwl, *separator;

	pwl = config_lookup(&cfg, "powerline");
	if (!pwl)
		return;

	config_setting_lookup_int(pwl, "size_reduction", &powerline_size_reduction_pixels);

	separator = config_setting_lookup(pwl, "separator");
	if (!separator)
		return;

	switch (config_setting_type(separator)) {
	case CONFIG_TYPE_INT:
		powerline = config_setting_get_int(separator);
		break;
	case CONFIG_TYPE_STRING:
		powerline = parse_powerline_string(config_setting_get_string(separator));
		break;
	}
}

#define readfunc(F) if (config_lookup_sloppy_bool(&cfg, "functionality." #F, &enabled)) { enabled ? enablefunc(F) : disablefunc(F); }

void
load_functionality(void)
{
	int enabled;

	/* Start by initialising defaults from hardcoded config */
	enablefunc(functionality);

	if (!config_loaded)
		return;

	readfunc(Alpha);
	readfunc(CaseSensitive);
	readfunc(Centered);
	readfunc(ColorEmoji);
	readfunc(ContinuousOutput);
	readfunc(FuzzyMatch);
	readfunc(HighlightAdjacent);
	readfunc(Incremental);
	readfunc(InstantReturn);
	readfunc(Managed);
	readfunc(NoInput);
	readfunc(PasswordInput);
	readfunc(PrintIndex);
	readfunc(PrintInputText);
	readfunc(RejectNoMatch);
	readfunc(RestrictReturn);
	readfunc(ShowNumbers);
	readfunc(Sort);
	readfunc(TopBar);
	readfunc(Xresources);
}

#undef readfunc

void
load_colors(void)
{
	int i, s, num_cols;
	const char *string;
	config_setting_t *cols, *c;

	cols = config_lookup(&cfg, "colors");
	if (!cols || !config_setting_is_group(cols))
		return;

	num_cols = config_setting_length(cols);
	if (!num_cols)
		return;

	/* Parse and set the colors based on config */
	for (i = 0; i < num_cols; i++) {
		c = config_setting_get_elem(cols, i);
		s = parse_scheme(config_setting_name(c));

		if (!scheme[s][ColFg].pixel && config_setting_lookup_string(c, "fg", &string))
			drw_clr_create(drw, &scheme[s][ColFg], string, alphas[s][ColFg]);

		if (!scheme[s][ColBg].pixel && config_setting_lookup_string(c, "bg", &string))
			drw_clr_create(drw, &scheme[s][ColBg], string, alphas[s][ColBg]);
	}
}

void
load_alphas(void)
{
	int i, s, num_alphas;
	config_setting_t *alpha_t, *a;

	alpha_t = config_lookup(&cfg, "alphas");
	if (!alpha_t || !config_setting_is_group(alpha_t))
		return;

	num_alphas = config_setting_length(alpha_t);
	if (!num_alphas)
		return;

	/* Parse and set the colors based on config */
	for (i = 0; i < num_alphas; i++) {
		a = config_setting_get_elem(alpha_t, i);
		s = parse_scheme(config_setting_name(a));

		config_setting_lookup_unsigned_int(a, "fg", &alphas[s][ColFg]);
		config_setting_lookup_unsigned_int(a, "bg", &alphas[s][ColBg]);
	}
}

void
load_keybindings(void)
{
	int i, j, k, length, num_modifiers, num_keys, num_functions, num_arguments;
	int num_bindings, num_expanded_bindings;
	unsigned int modifier_arr[20] = {0};
	KeySym key_arr[20] = {0};
	ArgFunc function_arr[20] = {0};
	const char *string;
	int argument_arr[20] = {0};
	void *void_argument_arr[20] = {0};
	float float_argument_arr[20] = {0};

	const config_setting_t *bindings, *binding, *modifier, *key, *function, *argument, *arg;

	bindings = config_lookup(&cfg, "keybindings");
	if (!bindings || !config_setting_is_list(bindings))
		return;

	num_bindings = config_setting_length(bindings);
	if (!num_bindings)
		return;

	keybindings = ecalloc(MAX(num_bindings * 2, 200), sizeof(Key));

	/* Parse and set the key bindings based on config */
	for (i = 0; i < num_bindings; i++) {
		binding = config_setting_get_elem(bindings, i);
		modifier = config_setting_lookup(binding, "modifier");
		key = config_setting_lookup(binding, "key");
		function = config_setting_lookup(binding, "function");
		argument = config_setting_lookup(binding, "argument");

		modifier_arr[0] = 0;
		function_arr[0] = NULL;
		argument_arr[0] = 0;
		float_argument_arr[0] = 0;
		void_argument_arr[0] = NULL;
		key_arr[0] = 0;

		length = setting_length(modifier);
		num_modifiers = MAX(length, 1);
		for (j = 0; j < length; j++) {
			modifier_arr[j] = parse_modifier(setting_get_string_elem(modifier, j));
		}

		length = setting_length(function);
		num_functions = MAX(length, 1);
		for (j = 0; j < length; j++) {
			function_arr[j] = parse_function(setting_get_string_elem(function, j));
		}

		length = setting_length(argument);
		num_arguments = MAX(length, 1);
		for (j = 0; j < length; j++) {
			arg = setting_get_elem(argument, j);
			argument_arr[j] = 0;
			float_argument_arr[j] = 0;
			void_argument_arr[j] = NULL;
			switch (config_setting_type(arg)) {
			case CONFIG_TYPE_INT:
				argument_arr[j] = config_setting_get_int(arg);
				break;
			case CONFIG_TYPE_STRING:
				string = config_setting_get_string(arg);
				argument_arr[j] = atoi(string);
				break;
			case CONFIG_TYPE_FLOAT:
				float_argument_arr[j] = config_setting_get_float(arg);
				break;
			}
		}

		length = setting_length(key);
		num_keys = MAX(length, 1);
		for (j = 0; j < length; j++) {
			string = setting_get_string_elem(key, j);

			key_arr[j] = XStringToKeysym(string);
			if (key_arr[j] == NoSymbol)
				fprintf(stderr, "Warning: config could not look up keysym for key %s\n", string);
		}

		/* Figure out the maximum number of expanded keybindings */
		int counts[4] = {num_modifiers, num_functions, num_arguments, num_keys};
		num_expanded_bindings = counts[0];
		for (k = 1; k < 4; k++)
			if (counts[k] > num_expanded_bindings)
				num_expanded_bindings = counts[k];

		/* Finally add each keybinding */
		for (j = 0; j < num_expanded_bindings; j++) {
			add_key_binding(
				modifier_arr[j % num_modifiers],
				key_arr[j % num_keys],
				function_arr[j % num_functions],
				argument_arr[j % num_arguments],
				void_argument_arr[j % num_arguments],
				float_argument_arr[j % num_arguments]
			);
		}
	}
}

void add_key_binding(
	unsigned int mod,
	KeySym keysym,
	ArgFunc function,
	int argument,
	void *void_argument,
	float float_argument
) {
	keybindings[num_keybindings].mod = mod;
	keybindings[num_keybindings].keysym = keysym;
	keybindings[num_keybindings].func = function;
	if (void_argument != NULL) {
		keybindings[num_keybindings].arg.v = void_argument;
	} else if (float_argument != 0) {
		keybindings[num_keybindings].arg.f = float_argument;
	} else {
		keybindings[num_keybindings].arg.i = argument;
	}

	num_keybindings++;
}

#define map(S, I) if (!strcasecmp(string, S)) return I;

int
parse_powerline_string(const char *string)
{
	if (startswith("Pwrl", string))
		string += 4;

	map("None", PwrlNone);
	map("RightArrow", PwrlRightArrow);
	map("LeftArrow", PwrlLeftArrow);
	map("ForwardSlash", PwrlForwardSlash);
	map("Backslash", PwrlBackslash);

	fprintf(stderr, "Warning: config could not find powerline option with name %s\n", string);
	return PwrlNone;
}

int
parse_scheme(const char *string)
{
	if (startswith("Scheme", string))
		string += 6;

	map("Norm", SchemeNorm);
	map("Sel", SchemeSel);
	map("Out", SchemeOut);
	map("Border", SchemeBorder);
	map("Prompt", SchemePrompt);
	map("Adjacent", SchemeAdjacent);
	map("SelHighlight", SchemeSelHighlight);
	map("NormHighlight", SchemeNormHighlight);
	map("Hp", SchemeHp);

	fprintf(stderr, "Warning: config could not find color scheme with name %s\n", string);
	return SchemeNorm;
}

ArgFunc
parse_function(const char *string)
{
	map("backspace", backspace);
	map("complete", complete);
	map("delete", delete);
	map("deleteleft", deleteleft);
	map("deleteright", deleteright);
	map("deleteword", deleteword);
	map("movewordedge", movewordedge);
	map("movestart", movestart);
	map("moveend", moveend);
	map("movenext", movenext);
	map("moveprev", moveprev);
	map("movedown", movedown);
	map("moveup", moveup);
	map("moveleft", moveleft);
	map("moveright", moveright);
	map("navhistory", navhistory);
	map("paste", paste);
	map("quit", quit);
	map("selectandresume", selectandresume);
	map("selectinput", selectinput);
	map("selectandexit", selectandexit);

	fprintf(stderr, "Warning: config could not find function with name %s\n", string);
	return 0;
}

#undef map

unsigned int
parse_modifier(const char *string)
{
	int i, len;
	unsigned int mask = 0;
	len = strlen(string) + 1;
	char buffer[len];
	strlcpy(buffer, string, len);
	const char *delims = "+-|:;, ";
	const char *modifier_strings[] = {
		"Super",
		"AltGr",
		"Alt",
		"ShiftGr",
		"Shift",
		"Ctrl",
		NULL
	};
	const unsigned int modifier_values[] = {
		Mod4Mask,
		Mod3Mask,
		Mod1Mask,
		Mod5Mask,
		ShiftMask,
		ControlMask
	};

	char *token = strtok(buffer, delims);
	while (token) {
		for (i = 0; modifier_strings[i]; i++) {
			if (!strcasecmp(token, modifier_strings[i])) {
				mask |= modifier_values[i];
				break;
			}
		}
		token = strtok(NULL, delims);
	}

	return mask;
}
