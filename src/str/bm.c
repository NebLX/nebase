
#include <nebase/str/bm.h>

#include <stdlib.h>

/**
 * Reference: http://igm.univ-mlv.fr/~lecroq/string/node14.html
 */

#define ASIZE 256

#define MAX(a, b) \
	((a) > (b)) ? (a) : (b)

struct neb_str_bm_ctx {
	const u_char *pattern;
	int pattern_len;
	int bmBc[ASIZE];
	int bmGs[];
};

static void preBmBc(const u_char *x, int m, int bmBc[])
{
	for (int i = 0; i < ASIZE; ++i)
		bmBc[i] = m;
	for (int i = 0; i < m - 1; ++i)
		bmBc[x[i]] = m - i - 1;
}

static void suffixes(const u_char *x, int m, int *suff)
{
	int f = 0, g = m - 1;

	suff[m - 1] = m;
	for (int i = m - 2; i >= 0; --i) {
		if (i > g && suff[i + m - 1 - f] < i - g)
			suff[i] = suff[i + m - 1 - f];
		else {
			if (i < g)
				g = i;
			f = i;
			while (g >= 0 && x[g] == x[g + m - 1 - f])
				--g;
			suff[i] = f - g;
		}
	}
}

static void preBmGs(const u_char *x, int m, int bmGs[])
{
	int suff[m];

	suffixes(x, m, suff);

	for (int i = 0; i < m; ++i)
		bmGs[i] = m;
	for (int i = m - 1, j = 0; i >= 0; --i)
		if (suff[i] == i + 1)
			for (; j < m - 1 - i; ++j)
				if (bmGs[j] == m)
					bmGs[j] = m - 1 - i;
	for (int i = 0; i <= m - 2; ++i)
		bmGs[m - 1 - suff[i]] = m - 1 - i;
}

const u_char *neb_str_bm_search(const u_char *p, int pl, const u_char *t, int64_t tl)
{
	int i, bmGs[pl], bmBc[ASIZE];

	if (pl > tl)
		return NULL;

	/* Preprocessing */
	preBmGs(p, pl, bmGs);
	preBmBc(p, pl, bmBc);

	/* Searching */
	int64_t j = 0;
	while (j <= tl - pl) {
		for (i = pl - 1; i >= 0 && p[i] == t[i + j]; --i);
		if (i < 0)
			return t + j;
		else
			j += MAX(bmGs[i], bmBc[t[i + j]] - pl + 1 + i);
	}

	return NULL;
}

neb_str_bm_ctx_t neb_str_bm_ctx_create(const u_char *p, int pl)
{
	struct neb_str_bm_ctx *c = malloc(sizeof(struct neb_str_bm_ctx) + sizeof(int) * pl);
	if (!c)
		return NULL;

	c->pattern = p;
	c->pattern_len = pl;
	preBmGs(p, pl, c->bmGs);
	preBmBc(p, pl, c->bmBc);

	return c;
}

void neb_str_bm_ctx_destroy(neb_str_bm_ctx_t c)
{
	free(c);
}

const u_char *neb_str_bm_ctx_search(neb_str_bm_ctx_t c, const u_char *t, int64_t tl)
{
	int i;
	int64_t j = 0;

	if (c->pattern_len > tl)
		return NULL;

	while (j <= tl - c->pattern_len) {
		for (i = c->pattern_len - 1; i >= 0 && c->pattern[i] == t[i + j]; --i);
		if (i < 0)
			return t + j;
		else
			j += MAX(c->bmGs[i], c->bmBc[t[i + j]] - c->pattern_len + 1 + i);
	}

	return NULL;
}
