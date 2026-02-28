/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

uint64_t settings = 0;

void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("calloc:");
	return p;
}

int
enabled(const long functionality)
{
	return (settings & functionality) > 0;
}

int
disabled(const long functionality)
{
	return !(settings & functionality);
}

void
enablefunc(const long functionality)
{
	settings |= functionality;
}

void
disablefunc(const long functionality)
{
	settings &= ~functionality;
}

void
togglefunc(const long functionality)
{
	settings ^= functionality;
}

/* Like sprintf but allocates memory as needed */
char *
xasprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	if (n < 0)
		return NULL;

	char *buf = malloc(n + 1);
	if (!buf)
		return NULL;

	va_start(ap, fmt);
	vsnprintf(buf, n + 1, fmt, ap);
	va_end(ap);

	return buf;
}

char *
path_dirname(const char *path)
{
	if (!path || !*path)
		return strdup(".");

	const char *slash = strrchr(path, '/');

	if (!slash)
		return strdup(".");

	/* handle root */
	if (slash == path)
		return strdup("/");

	size_t len = slash - path;

	char *out = malloc(len + 1);
	if (!out)
		return NULL;

	memcpy(out, path, len);
	out[len] = '\0';

	return out;
}

#ifdef __linux__
/*
 * Copy string src to buffer dst of size dsize.  At most dsize-1
 * chars will be copied.  Always NUL terminates (unless dsize == 0).
 * Returns strlen(src); if retval >= dsize, truncation occurred.
 *
 * From:
 * https://github.com/freebsd/freebsd-src/blob/master/sys/libkern/strlcpy.c
 */
size_t
strlcpy(char * __restrict dst, const char * __restrict src, size_t dsize)
{
	const char *osrc = src;
	size_t nleft = dsize;

	/* Copy as many bytes as will fit. */
	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src. */
	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0';		/* NUL-terminate dst */
		while (*src++)
			;
	}

	return (src - osrc - 1);	/* count does not include NUL */
}
#endif /* __linux__ */
