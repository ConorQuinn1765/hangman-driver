#ifndef UNIT_TEST_H
#define UNIT_TEST_H
#include <stdbool.h>
#include <stddef.h>

bool test_driver_exists(char*funcName, char* error, size_t len);
bool test_driver_permissions(char*funcName, char* error, size_t len);
bool test_open_close(char*funcName, char* error, size_t len);
bool test_can_read(char*funcName, char* error, size_t len);
bool test_lseek(char*funcName, char* error, size_t len);
bool test_read_first_line(char*funcName, char* error, size_t len);
bool test_read_second_line(char*funcName, char* error, size_t len);
bool test_read_third_line(char*funcName, char* error, size_t len);
bool test_ioctl_get_wordbank(char*funcName, char* error, size_t len);
bool test_ioctl_get_current_word(char*funcName, char* error, size_t len);
bool test_ioctl_set_current_word(char*funcName, char* error, size_t len);
bool test_ioctl_restart(char*funcName, char* error, size_t len);
bool test_write_correct_letter(char*funcName, char* error, size_t len);
bool test_write_correct_letter(char*funcName, char* error, size_t len);
bool test_write_wrong_letter(char*funcName, char* error, size_t len);
bool test_write_two_correct(char*funcName, char* error, size_t len);
bool test_write_two_wrong(char*funcName, char* error, size_t len);
bool test_write_same_letter_twice(char*funcName, char* error, size_t len);
bool test_write_lower_case(char*funcName, char* error, size_t len);
bool test_write_too_many_letters(char*funcName, char* error, size_t len);
bool test_write_non_alphabetic(char*funcName, char* error, size_t len);
bool test_write_empty_string(char*funcName, char* error, size_t len);
bool test_reset_the_game(char*funcName, char* error, size_t len);
bool test_read_pos_after_write(char*funcName, char* error, size_t len);
bool test_win_the_game(char*funcName, char* error, size_t len);
bool test_lose_the_game(char*funcName, char* error, size_t len);
bool test_one_more_game(char*funcName, char* error, size_t len);
bool test_ioctl_set_wordbank(char*funcName, char* error, size_t len);
bool test_llseek_cur(char*funcName, char* error, size_t len);
bool test_read_succeed_after_win(char*funcName, char* error, size_t len);
bool test_write_fail_after_win(char*funcName, char* error, size_t len);
bool test_game_reset_after_word_change(char*funcName, char* error, size_t len);

#endif
