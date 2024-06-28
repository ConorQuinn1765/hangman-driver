#include "unitTest.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DRIVER_PATH "/dev/hangman"
#define MAX_BANK_SIZE 500
#define MAX_SECRET_SIZE 50

// Ioctl command numbers
#define HANGMAN_MAGIC_NUM   0xff
#define HANGMAN_IOC_READ_BANK	 _IOR(HANGMAN_MAGIC_NUM, 1, char[MAX_BANK_SIZE])
#define HANGMAN_IOC_READ_SECRET	 _IOR(HANGMAN_MAGIC_NUM, 2, char[MAX_SECRET_SIZE])
#define HANGMAN_IOC_WRITE_BANK	 _IOW(HANGMAN_MAGIC_NUM, 3, char[MAX_BANK_SIZE])
#define HANGMAN_IOC_WRITE_SECRET _IOW(HANGMAN_MAGIC_NUM, 4, char[MAX_SECRET_SIZE])
#define HANGMAN_IOC_RESTART	 _IO(HANGMAN_MAGIC_NUM, 5)

#define RETURN_ERROR(error, len, msg) { snprintf(error, len, "%s", msg == NULL ? strerror(errno) : msg); return false; }
#define INIT_TEST(funcName, error, len) ({ snprintf(funcName, len, "%s", __FUNCTION__);\
                                int fdes = open(DRIVER_PATH, O_RDWR);\
                                if(!fdes) RETURN_ERROR(error, len, NULL) fdes; })

#define RETURN_CLEANUP(fd, status, error, len, msg) { ioctl(fd, HANGMAN_IOC_RESTART); \
                                                 close(fd); \
                                                 if(status) return true; \
                                                 else RETURN_ERROR(error, len, msg) }

bool test_driver_exists(char* funcName, char* error, size_t len)
{
    snprintf(funcName, len, "%s", __FUNCTION__);
    if(access(DRIVER_PATH, F_OK) == 0)
        return true;

    RETURN_ERROR(error, len, NULL)
}

bool test_driver_permissions(char*funcName, char* error, size_t len)
{
    snprintf(funcName, len, "%s", __FUNCTION__);
    if(access(DRIVER_PATH, R_OK | W_OK) == 0)
        return true;

    RETURN_ERROR(error, len, NULL)
}

bool test_open_close(char* funcName, char* error, size_t len)
{
    snprintf(funcName, len, "%s", __FUNCTION__);
    int fd = open(DRIVER_PATH, O_RDWR);
    if(fd > 0) {
        if(close(fd) < 0)
            RETURN_ERROR(error, len, NULL)

        return true;
    }

    RETURN_ERROR(error, len, NULL)
}

bool test_can_read(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);

    bool status = true;
    char temp[128] = {0};
    if(read(fd, temp, 128) < 0)
        status = false;

    RETURN_CLEANUP(fd, status, error, len, NULL)
}

bool test_lseek(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char temp0[128] = {0};
    if(read(fd, temp0, 128) < 0)
        RETURN_CLEANUP(fd, false, error, len, NULL)

    lseek(fd, 0, SEEK_SET);

    char temp1[128] = {0};
    if(read(fd, temp1, 128) < 0) {
        status = false;
    } else if(strncmp(temp0, temp1, 128) != 0) {
        status = false;
    }

    RETURN_CLEANUP(fd, status, error, len, NULL)
}

bool test_read_first_line(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    char temp[128] = {0};
    int nread = read(fd, temp, 128);
    if(nread < 0)
        RETURN_CLEANUP(fd, false, error, len, NULL)

    bool status = true;
    int i = 0;
    for(; i < nread; i++) {
        if(temp[i] == '\n')
            break;

        if(i % 2 == 0 && temp[i] != '-') {
            status = false;
            break;
        } else if(i % 2 == 1 && temp[i] != ' ') {
            status = false;
            break;
        }
    }

    RETURN_CLEANUP(fd, status, error, len, "Hidden word is improperly formatted")
}

bool test_read_second_line(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    char temp[128] = {0};
    if(read(fd, temp, 128) < 0)
        RETURN_CLEANUP(fd, false, error, len, NULL)

    bool status = true;
    int i = 0;
    while(temp[i++] != '\n') {}

    if(temp[i] != '\n')
        status = false;

    RETURN_CLEANUP(fd, status, error, len, "Second line should be blank")
}

bool test_read_third_line(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    char temp[128] = {0};
    int nread = read(fd, temp, 128);
    if(nread < 0)
        RETURN_CLEANUP(fd, false, error, len, NULL)

    bool status = true;
    int i = 0;
    while(temp[i++] != '\n') {}
    while(temp[i++] != '\n') {}

    if(strncmp(temp + i, "10 guesses left\n", nread - i) != 0)
        status = false;

    RETURN_CLEANUP(fd, status, error, len, "Third line displays incorrect message")
}

bool test_ioctl_get_wordbank(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char wbBuf[MAX_BANK_SIZE] = {0};
    char* errMsg = NULL;
    char* emsgRd = "Failed to read word bank";
    char* emsgCmp = "Returned word bank does not match expected word bank";

    if(ioctl(fd, HANGMAN_IOC_READ_BANK, wbBuf) != 0) {
        status = false;
        errMsg = emsgRd;
    } else if(strncmp(wbBuf, "EXAMPLE", sizeof("EXAMPLE")) != 0) {
        status = false;
        errMsg = emsgCmp;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg)
}

bool test_ioctl_get_current_word(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char secretBuf[MAX_SECRET_SIZE] = {0};
    char* errMsg = NULL;
    char* emsgRd = "Failed to read secret word";
    char* emsgCmp = "Returned secret word does not match expected secret word";

    if(ioctl(fd, HANGMAN_IOC_READ_SECRET, secretBuf) != 0) {
        status = false;
        errMsg = emsgRd;
    } else if(strncmp(secretBuf, "EXAMPLE", sizeof("EXAMPLE")) != 0) {
        status = false;
        errMsg = emsgCmp;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg)
}

bool test_ioctl_set_current_word(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char secretBuf[MAX_SECRET_SIZE] = {0};
    char* newSecret = "TEST";
    char* errMsg = NULL;
    char* emsgWr = "Failed to write secret word";
    char* emsgRd = "Failed to read secret word";
    char* emsgCmp = "Failed to properly update the secret word";

    if(ioctl(fd, HANGMAN_IOC_WRITE_SECRET, newSecret) != 0) {
        status = false;
        errMsg = emsgWr;
    }

    if(ioctl(fd, HANGMAN_IOC_READ_SECRET, secretBuf) != 0) {
        status = false;
        errMsg = emsgRd;
    } else if(strncmp(secretBuf, "TEST", sizeof("TEST")) != 0) {
        status = false;
        errMsg = emsgCmp;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg)
}

bool test_ioctl_restart(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char buf0[128] = {0}, buf1[128] = {0};
    char* errMsg = NULL;
    char* emsgIoc = "Failed when resetting game";
    char* emsgCmp = "Failed to properly reset the game";

    if(read(fd, buf0, 128) < 0) {
        status = false;
    } else if(ioctl(fd, HANGMAN_IOC_RESTART) != 0) {
        status = false;
        errMsg = emsgIoc;
    } else if(read(fd, buf1, 128) < 0) {
        status = false;
    } else if(strncmp(buf0, buf1, 128) != 0) {
        status = false;
        errMsg = emsgCmp;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_correct_letter(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* expected = "E - - - - - E \n\n10 guesses left\n";
    char* errMsg = NULL;
    char buf[128] = {0};
    char* emsg = "Failed to properly update game board on correct guess";

    if(write(fd, "E", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expected, strlen(expected)) != 0) {
        status = false;
        errMsg = emsg;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_wrong_letter(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* expected = "- - - - - - - \nZ\n9 guesses left\n";
    char* errMsg = NULL;
    char buf[128] = {0};
    char* emsg = "Failed to properly update game board on incorrect guess";

    if(write(fd, "Z", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expected, strlen(expected)) != 0) {
        status = false;
        errMsg = emsg;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_two_correct(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* expected = "E X - - - - E \n\n10 guesses left\n";
    char* errMsg = NULL;
    char buf[128] = {0};
    char* emsg = "Failed to properly update game board on correct guess";

    if(write(fd, "E", 2) != 2) {
        status = false;
    } else if(write(fd, "X", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expected, strlen(expected)) != 0) {
        status = false;
        errMsg = emsg;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_two_wrong(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* expected = "- - - - - - - \nZ Y\n8 guesses left\n";
    char* errMsg = NULL;
    char buf[128] = {0};
    char* emsg = "Failed to properly update game board on incorrect guess";

    if(write(fd, "Z", 2) != 2) {
        status = false;
    } else if(write(fd, "Y", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expected, strlen(expected)) != 0) {
        status = false;
        errMsg = emsg;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_same_letter_twice(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* expectedCorrect = "- X - - - - - \n\n10 guesses left\n";
    char* expectedIncorrect = "- - - - - - - \nZ\n9 guesses left\n";
    char* errMsg = NULL;
    char buf[128] = {0};
    char* emsgC = "Failed to properly update game board on repeated correct guess";
    char* emsgI = "Failed to properly update game board on repeated incorrect guess";
    char* emsgRst = "Failed to reset the game";

    if(write(fd, "X", 2) != 2) {
        status = false;
    } else if(write(fd, "X", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expectedCorrect, strlen(expectedCorrect)) != 0) {
        status = false;
        errMsg = emsgC;
    } else if(ioctl(fd, HANGMAN_IOC_RESTART) != 0) {
        status = false;
        errMsg = emsgRst;
    } else if(write(fd, "Z", 2) != 2) {
        status = false;
    } else if(write(fd, "Z", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expectedIncorrect, strlen(expectedIncorrect)) != 0) {
        status = false;
        errMsg = emsgI;
    }


    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_lower_case(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* expected = "- - A - - - - \nB\n9 guesses left\n";
    char* errMsg = NULL;
    char buf[128] = {0};
    char* msg = "Failed to properly update game board on lowercase guess";

    if(write(fd, "a", 2) != 2) {
        status = false;
    } else if(write(fd, "b", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expected, strlen(expected)) != 0) {
        status = false;
        errMsg = msg;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_too_many_letters(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* errMsg = NULL;
    char* emsgWr = "Failed to block a write of more the one character";

    if(write(fd, "HELLO", 5) >= 0) {
        status = false;
        errMsg = emsgWr;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_non_alphabetic(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;

    if(write(fd, "1", 2) > 0)
        status = false;

    RETURN_CLEANUP(fd, status, error, len, NULL);
}

bool test_write_empty_string(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    if(write(fd, "", 1) > 0)
        status = false;

    RETURN_CLEANUP(fd, status, error, len, NULL);
}

bool test_reset_the_game(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* expected = "- - - - - - - \n\n10 guesses left\n";
    char* errMsg = NULL;
    char* emsgRst = "Failed while resetting game board";
    char* emsgCmp = "Failed to properly reset game board";
    char buf[128] = {0};

    if(write(fd, "A", 2) != 2) {
        status = false;
    } else if(ioctl(fd, HANGMAN_IOC_RESTART) != 0) {
        status = false;
        errMsg = emsgRst;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expected, strlen(expected)) != 0) {
        status = false;
        errMsg = emsgCmp;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_read_pos_after_write(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);

    bool status = true;
    char* expected = "E - - - - - E \n\n10 guesses left\n";
    char buf[128] = {0};
    char* errMsg = NULL;
    char* emsgCmp = "Failed to reset file position after write";

    if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(write(fd, "E", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expected, strlen(expected)) != 0) {
        status = false;
        errMsg = emsgCmp;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_win_the_game(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* errMsg = NULL;
    char* emsg = "Failed to correctly update the board after game win";
    char* expected = "E X A M P L E \n\n10 guesses left\nYou Win!\n";
    char buf[128] = {0};

    if(write(fd, "E", 2) != 2) {
        status = false;
    } else if(write(fd, "E", 2) != 2) {
        status = false;
    } else if(write(fd, "X", 2) != 2) {
        status = false;
    } else if(write(fd, "A", 2) != 2) {
        status = false;
    } else if(write(fd, "M", 2) != 2) {
        status = false;
    } else if(write(fd, "P", 2) != 2) {
        status = false;
    } else if(write(fd, "L", 2) != 2) {
        status = false;
    } else if(read(fd, buf, 128) < 0) {
        status = false;
    } else if(strncmp(buf, expected, strlen(expected)) != 0) {
        status = false;
        errMsg = emsg;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_lose_the_game(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* errMsg = NULL;
    char* emsg = "Failed to correctly update the board after game lost";
    char* expected = "- - - - - - - \nB C D F G H I J K N\n0 guesses left\nYou Lose!\n";
    char buf[128] = {0};

    char* guesses[] = {"B", "C", "D", "F", "G", "H", "I", "J", "K", "N"};
    for(int i = 0; i < 10; i++) {
        if(write(fd, guesses[i], 2) != 2) {
            status = false;
            break;
        }
    }

    if(status) {
        if(read(fd, buf, 128) < 0) {
            status = false;
        } else if(strncmp(buf, expected, strlen(expected)) != 0) {
            status = false;
            errMsg = emsg;
        }
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_one_more_game(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char word[MAX_SECRET_SIZE] = "GOODBYE";
    char* errMsg = NULL;
    char* emsg = "Failed to play game with updated secret word";
    char* expected = "G O O D B Y E \n\n10 guesses left\nYou Win!\n";
    char buf[128] = {0};

    if(ioctl(fd, HANGMAN_IOC_WRITE_SECRET, word) != 0)
        status = false;

    char* guesses[] = {"B", "D", "E", "G", "O", "Y"};
    for(int i = 0; i < 6; i++) {
        if(write(fd, guesses[i], 2) != 2) {
            status = false;
            break;
        }
    }

    if(status) {
        if(read(fd, buf, 128) < 0) {
            status = false;
        } else if(strncmp(buf, expected, strlen(expected)) != 0) {
            status = false;
            errMsg = emsg;
        }
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_ioctl_set_wordbank(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* errMsg = NULL;
    char* emsgCmp = "Failed to properly update word bank";
    char newBank[MAX_BANK_SIZE] = "HELLO,GOODBYE,TEST_A,TEST_B";
    char buf[MAX_BANK_SIZE] = {0};

    if(ioctl(fd, HANGMAN_IOC_WRITE_BANK, newBank) != 0) {
        status = false;
    } else if(ioctl(fd, HANGMAN_IOC_READ_BANK, buf) != 0) {
        status = false;
    } else if(strncmp(buf, newBank, MAX_BANK_SIZE) != 0) {
        status = false;
        errMsg = emsgCmp;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_llseek_cur(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char buf0[8] = {0};
    char buf1[8] = {0};
    char* errMsg = NULL;
    char* emsgCmp = "";

    if(read(fd, buf0, 8) < 0) {
        status = false;
    } else if(lseek(fd, -8, SEEK_CUR) < 0) {
        status = false;
    } else if(read(fd, buf1, 8) < 0) {
        status = false;
    } else if(strncmp(buf0, buf1, 8) != 0) {
        status = false;
        errMsg = emsgCmp;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_read_succeed_after_win(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* errMsg = NULL;
    char* emsg = "Failed to read game board after winning the game";
    char buf[128] = {0};

    char* guesses[] = {"E", "X", "A", "M", "P", "L"};
    for(int i = 0; i < 6; i++) {
        if(write(fd, guesses[i], 2) < 0) {
            status = false;
            break;
        }
    }

    if(read(fd, buf, 128) < 0) {
        status = false;
        errMsg = emsg;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_write_fail_after_win(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* errMsg = NULL;
    char* emsgWr = "Failed to block write after the game has ended";

    char* guesses[] = {"E", "X", "A", "M", "P", "L"};
    for(int i = 0; i < 6; i++) {
        if(write(fd, guesses[i], 2) < 0) {
            status = false;
            break;
        }
    }

    if(status && write(fd, "B", 2) >= 0) {
        status = false;
        errMsg = emsgWr;
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}

bool test_game_reset_after_word_change(char* funcName, char* error, size_t len)
{
    int fd = INIT_TEST(funcName, error, len);
    bool status = true;
    char* expected = "- - - - - \n\n10 guesses left\n";
    char* errMsg = NULL;
    char* emsgCmp = "Failed to reset game after updating the secret word";
    char newSecret[MAX_SECRET_SIZE] = "HELLO";
    char buf[128] = {0};

    char* guesses[] = {"E", "X", "A", "M"};
    for(int i = 0; i < 4; i++) {
        if(write(fd, guesses[i], 2) != 2) {
            status = false;
            break;
        }
    }

    if(status) {
        if(ioctl(fd, HANGMAN_IOC_WRITE_SECRET, newSecret) != 0) {
            status = false;
        } else if(read(fd, buf, 128) < 0) {
            status = false;
        } else if(strncmp(buf, expected, strlen(expected)) != 0) {
            status = false;
            errMsg = emsgCmp;
        }
    }

    RETURN_CLEANUP(fd, status, error, len, errMsg);
}
