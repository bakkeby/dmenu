/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

/* -fn option overrides fonts[0]; default X11 font or font set */
static char *fonts[] =
{
	"monospace:size=10"
};

static char *prompt        = NULL; /* -p  option; prompt to the left of input field */
static const char *dynamic = NULL; /* -dy option; dynamic command to run on input change */
static const char *lsymbol = "<";  /* shown when there are more items on the left */
static const char *rsymbol = ">";  /* shown when there are more items on the right */
static const char csymbol = '*';   /* shown when password input is enabled */

/* Functionality that is enabled by default, see util.h for options */
static unsigned long functionality = 0
	|Alpha // enables transparency
	|CaseSensitive // makes dmenu case sensitive by default
//	|Centered // dmenu appears in the center of the screen
//	|ColorEmoji // enables color emoji support (removes Xft workaround)
//	|ContinuousOutput // makes dmenu print out selected items immediately rather than at the end
	|FuzzyMatch // allows fuzzy-matching of items in dmenu
//	|HighlightAdjacent // makes dmenu highlight items adjacent to the selected item
//	|Incremental // makes dmenu print out the current text each time a key is pressed
//	|InstantReturn // makes dmenu select an item immediately if there is only one matching option left
//	|Managed // allow dmenu to be managed by window managers (disables override_redirect)
//	|PasswordInput // indicates that the input is a password and should be masked
//	|PrintIndex // makes dmenu print out the 0-based index instead of the matched text itself
//	|PrintInputText // makes dmenu print the input text instead of the selected item
	|PromptIndent // makes dmenu indent items at the same level as the prompt on multi-line views
//	|RejectNoMatch // makes dmenu reject input if it would result in no matching item
//	|RestrictReturn // disables Shift-Return and Ctrl-Return to restrict dmenu to only output one item
//	|ShowNumbers // makes dmenu display the number of matched and total items in the top right corner
	|Sort // allow dmenu to sort menu items after matching
	|TabSeparatedValues // makes dmenu hide values following a tab character
	|TopBar // dmenu appears at the top of the screen
	|Xresources // makes dmenu read X resources at startup
;

/* Alpha values. You only need to add colour schemes here if you want different levels of
 * transparency per scheme. The default values are defined in the alpha_default array in drw.c. */
static const unsigned int alphas[SchemeLast][2] = {
	/*               fg      bg   */
	[SchemeNorm] = { OPAQUE, 0xd0 },
};

static char *colors[SchemeLast][ColCount] = {
	/*                        fg         bg         resource prefix */
	[SchemeNorm]          = { "#bbbbbb", "#222222", "norm" },
	[SchemeSel]           = { "#eeeeee", "#005577", "sel" },
	[SchemeOut]           = { "#000000", "#00ffff", "out" },
	[SchemeBorder]        = { "#000000", "#005577", "border" },
	[SchemePrompt]        = { "#eeeeee", "#005577", "prompt" },
	[SchemeAdjacent]      = { "#eeeeee", "#770000", "adjacent" },
	[SchemeSelHighlight]  = { "#ffc978", "#005577", "selhl" },
	[SchemeNormHighlight] = { "#ffc978", "#222222", "normhl" },
	[SchemeHp]            = { "#bbbbbb", "#333333", "hp" },
};
/* -l option; if nonzero, dmenu uses vertical list with given number of lines */
static unsigned int lines      = 0;
/* -g option; if nonzero, dmenu uses a grid comprised of columns and lines */
static unsigned int columns    = 0;
static unsigned int lineheight = 0; /* -h option; minimum height of a menu line */
static unsigned int min_width  = 500; /* minimum width when centered */
static unsigned int maxhist    = 15;
static int histnodup           = 1;	/* if 0, record repeated histories */

/*
 * Characters not considered part of a word while deleting words
 * for example: " /?\"&[]"
 */
static const char worddelimiters[] = " ";

/* Default size of the window border */
static unsigned int border_width = 0;

/* Vertical and horizontal padding of dmenu in relation to monitor border */
static int vertpad = 0;
static int sidepad = 0;
