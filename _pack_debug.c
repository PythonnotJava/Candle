#include <stdio.h>
int interp_run_string(const char *source, const char *filename);

int main(void) {
    printf("START\n");
    fflush(stdout);
    int rc = interp_run_string("int main() { print(\"HELLO\"); return 0; }", "test.candle");
    printf("RC=%d\n", rc);
    return rc;
}
