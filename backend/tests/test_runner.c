#include <stdio.h>

int test_db_run(void);
int test_ai_parser_run(void);

int main(void) {
    if (test_db_run() != 0) {
        fprintf(stderr, "test_db_run failed\n");
        return 1;
    }

    if (test_ai_parser_run() != 0) {
        fprintf(stderr, "test_ai_parser_run failed\n");
        return 1;
    }

    printf("All tests passed.\n");
    return 0;
}
