#include <libconfig.h>

static char *config_error = NULL;
static config_t cfg;

void set_config_path(const char* filename, char *config_path, char *config_file);
int setting_length(const config_setting_t *cfg);
const char *setting_get_string_elem(const config_setting_t *cfg, int i);
int setting_get_int_elem(const config_setting_t *cfg, int i);

void load_config(void);
void load_fonts(void);

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
			config_error = ecalloc(PATH_MAX + 255, sizeof(char));
			snprintf(config_error, PATH_MAX + 255,
				"Config %s: %s:%d",
				config_error_text(&cfg),
				config_file,
				config_error_line(&cfg));
		}

		fprintf(stderr, "Error reading config at %s\n", config_file);
		fprintf(stderr, "%s:%d - %s\n",
				config_error_file(&cfg),
				config_error_line(&cfg),
				config_error_text(&cfg));

		config_destroy(&cfg);
		return;
	}

	// read_singles(&cfg);
	// read_commands(&cfg);
	// read_autostart(&cfg);
	// read_bar(&cfg);
	// read_button_bindings(&cfg);
	// read_clientrules(&cfg);
	// read_colors(&cfg);
	// read_fonts(&cfg);
	// read_functionality(&cfg);
	// read_indicators(&cfg);
	// read_keybindings(&cfg);
	// read_layouts(&cfg);
	// read_workspace(&cfg);
	//
}

void
cleanup_config(void)
{
	config_destroy(&cfg);
}

void
load_fonts()
{
	int i, num_fonts;
	config_setting_t *fonts;

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
