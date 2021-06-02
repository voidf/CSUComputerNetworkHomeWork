#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define min(_lhs, _rhs) (_lhs < _rhs ? _lhs : _rhs)
#define endl '\n'

#define BUFFER_SIZE (1 << 14)

typedef char vector_element;
typedef struct
{
	vector_element *begin, *end;
	size_t real_size;
} vector_t;

size_t vector_size(vector_t *V) { return V->end - V->begin; }

void vector_migrate(vector_t *V)
{
	V->real_size <<= 1;
	vector_element *tmp = (vector_element *)malloc(sizeof(vector_element) * V->real_size);
	size_t siz = vector_size(V);
	memcpy(tmp, V->begin, sizeof(vector_element) * siz);
	free(V->begin);
	V->begin = tmp;
	V->end = V->begin + siz;
}

void vector_resize(vector_t *V, size_t newsize)
{
	V->real_size = newsize;
	vector_element *tmp = (vector_element *)malloc(sizeof(vector_element) * V->real_size);
	size_t siz = min(newsize, vector_size(V));
	memcpy(tmp, V->begin, sizeof(vector_element) * siz);
	free(V->begin);
	V->begin = tmp;
	V->end = V->begin + siz;
}

void vector_concat_n(vector_t *V, vector_element *Es, size_t n)
{
	if (vector_size(V) + n > V->real_size)
		vector_resize(V, vector_size(V) + n << 1);
	while (n--)
	{
		*V->end = *Es;
		Es++;
		V->end++;
	}
}

void vector_emplace_back(vector_t *V, vector_element *E)
{
	size_t siz = vector_size(V);
	if (siz + 1 >= V->real_size)
		vector_migrate(V);
	*V->end = *E;
	V->end++;
}

void vector_concat(vector_t *V, vector_element *Es)
{
	while (*Es)
	{
		vector_emplace_back(V, Es);
		Es++;
	}
}
void vector_init(vector_t *V, size_t siz)
{
	V->real_size = siz;
	V->begin = (vector_element *)malloc(sizeof(vector_element) * V->real_size);
	V->end = V->begin;
}

void vector_clear(vector_t *V) { V->end = V->begin; }

void vector_destroy(vector_t *V) { free(V->begin); }

void vector_connect(vector_t *original, vector_t *another)
{
	if (original->real_size <
		vector_size(original) + vector_size(another))
	{
		vector_resize(original, vector_size(original) + vector_size(another));
	}
	for (vector_element *p = another->begin; p != another->end; p++)
	{
		*original->end = *p;
		original->end++;
	}
}

signed main()
{
	vector_t VV;
	vector_init(&VV, 10);
	vector_concat_n(&VV, "HWIOHOA\0JIJO", 12);
	vector_concat(&VV, "\r\n");
	vector_concat(&VV, "HTTP/1.1");
	vector_emplace_back(&VV, "\0");
	printf("size:%zu, real:%zu\n", vector_size(&VV), VV.real_size);
	for (vector_element *p = VV.begin; p != VV.end; p++)
	{
		putchar(*p);
	}
	putchar('\n');
	puts(VV.begin);
	vector_destroy(&VV);

	return 0;
}