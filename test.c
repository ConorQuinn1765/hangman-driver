#include <stdio.h>
#include <stdbool.h>
#include "unitTest.h"

#define BUF_SIZE 128

int main(void)
{
    bool (*tests[])(char* funcName, char* buffer, size_t len) = {
        test_driver_exists,
        test_open_close,
        test_can_read,
        test_lseek,
        test_read_first_line,
        test_read_second_line,
        test_read_third_line,
        test_ioctl_get_wordbank,
        test_ioctl_get_current_word,
        test_ioctl_set_current_word,
        test_ioctl_restart,
        test_write_correct_letter,
        test_write_wrong_letter,
        test_write_two_correct,
        test_write_two_wrong,
        test_write_same_letter_twice,
        test_write_lower_case,
        test_write_too_many_letters,
        test_write_non_alphabetic,
        test_write_empty_string,
        test_reset_the_game,
        test_read_pos_after_write,
        test_win_the_game,
        test_lose_the_game,
        test_one_more_game,
        test_ioctl_set_wordbank,
        test_llseek_cur,
        test_read_succeed_after_win,
        test_write_fail_after_win,
        test_game_reset_after_word_change,
    };

    int numTests = sizeof(tests) / sizeof(tests[0]);
    int passCount = 0;

    for(int i = 0; i < numTests; i++) {
        char funcName[BUF_SIZE] = {0};
        char err[BUF_SIZE] = {0};

        bool result = tests[i](funcName, err, BUF_SIZE);
        printf("%s: ", funcName);
        if(result) {
            printf("PASSED\n");
            passCount++;
        } else {
            printf("FAILED\n");
            printf("\t%s\n", err);
        }
    }

    printf("Total: %d / %d tests passed\n", passCount, numTests);

    return 0;
}
