#include <stdio.h>

int f() __attribute__((user("alwayszero")));
f() {
  printf("In f\n");
  return 5;
}

int main() {
  int x = f();
  printf("%d\n", x);
  return 0;
}
