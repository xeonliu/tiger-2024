#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int tigermain(long long rsp, long long sl);

// seven arguments testcase
int sum_seven(int v1, int v2, int v3, int v4, int v5, int v6, int v7) {
  return v1 + v2 + v3 + v4 + v5 + v6 + v7;
}

long long init_array(int size, long init) {
  int i;
  long *a = (long *)malloc(size * sizeof(long));
  for (i = 0; i < size; i++) a[i] = init;
  return (long long)a;
}

long long alloc_record(int size) {
  int i;
  int *p, *a;
  p = a = (int *)malloc(size);
  for (i = 0; i < size; i += sizeof(int)) *p++ = 0;
  return (long long)a;
}

struct LLVMString {
    int length;          // length of string
    char padding[4];     // padding for alignment
    char *chars;        // actual string content
};

int string_equal(struct LLVMString *s, struct LLVMString *t) {
  int i;
  if (s == t) return 1;
  if (s->length != t->length) return 0;
  for (i = 0; i < s->length; i++)
    if (s->chars[i] != t->chars[i]) return 0;
  return 1;
}

void print(struct LLVMString *s) {
  int i;
  char *p = s->chars;
  for (i = 0; i < s->length; i++, p++) putchar(*p);
}

void printi(int k) { printf("%d", k); }

void flush() { fflush(stdout); }

struct LLVMString consts[256];
struct LLVMString empty = {
    .length = 0,         // 初始化 value
    .padding = {0},      // 初始化 padding，默认全为 0
    .chars = ""   // 将字符串常量的地址赋值给 ptr
};


int main() {
  int i;
  for (i = 0; i < 256; i++) {
    consts[i].length = 1;
    consts[i].chars = (char*)malloc(1);
    consts[i].chars[0] = i;
  }
  long long stack_bottom = (long long)malloc(0x100000);
  long long aligned_address = (stack_bottom + 0x100000) & ~0xFFFF;
  return tigermain(aligned_address, aligned_address);
}

int ord(struct LLVMString *s) {
  if (s->length == 0)
    return -1;
  else
    return s->chars[0];
}

struct LLVMString *chr(int i) {
  if (i < 0 || i >= 256) {
    printf("chr(%d) out of range\n", i);
    exit(1);
  }
  return consts + i;
}

int size(struct LLVMString *s) { return s->length; }

struct LLVMString *substring(struct LLVMString *s, int first, int n) {
  if (first < 0 || first + n > s->length) {
    printf("substring([%d],%d,%d) out of range\n", s->length, first, n);
    exit(1);
  }
  if (n == 1) return consts + s->chars[first];
  {
    struct LLVMString *t = (struct LLVMString *)malloc(sizeof(struct LLVMString));
    t->chars = (char*)malloc(n);
    int i;
    t->length = n;
    for (i = 0; i < n; i++) t->chars[i] = s->chars[first + i];
    return t;
  }
}

struct LLVMString *concat(struct LLVMString *a, struct LLVMString *b) {
  if (a->length == 0)
    return b;
  else if (b->length == 0)
    return a;
  else {
    int i, n = a->length + b->length;
    struct LLVMString *t = (struct LLVMString *)malloc(sizeof(struct LLVMString));
    t->chars = (char *)malloc(n);
    t->length = n;
    for (i = 0; i < a->length; i++) t->chars[i] = a->chars[i];
    for (i = 0; i < b->length; i++) t->chars[i + a->length] = b->chars[i];
    return t;
  }
}

int not(int i) { return !i; }

#undef getchar

struct LLVMString *__wrap_getchar() {
  int i = getc(stdin);
  if (i == EOF)
    return &empty;
  else
    return consts + i;
}