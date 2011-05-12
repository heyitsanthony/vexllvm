#include <unistd.h>
#include <stdio.h>

int h(void) { return getpid(); }
int g(void) { return h(); }
int f(void) { return g(); }

int main(void) { return f(); }