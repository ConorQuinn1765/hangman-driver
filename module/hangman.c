#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/limits.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/mutex.h>

#define STR_SIZE 64
#define WB_SIZE 32
#define MAX_BANK_SIZE 500
#define MAX_SECRET_SIZE 50

// Ioctl command numbers
#define HANGMAN_MAGIC_NUM   0xff
#define HANGMAN_IOC_READ_BANK	 _IOR(HANGMAN_MAGIC_NUM, 1, char[MAX_BANK_SIZE])
#define HANGMAN_IOC_READ_SECRET	 _IOR(HANGMAN_MAGIC_NUM, 2, char[MAX_SECRET_SIZE])
#define HANGMAN_IOC_WRITE_BANK	 _IOW(HANGMAN_MAGIC_NUM, 3, char[MAX_BANK_SIZE])
#define HANGMAN_IOC_WRITE_SECRET _IOW(HANGMAN_MAGIC_NUM, 4, char[MAX_SECRET_SIZE])
#define HANGMAN_IOC_RESTART	 _IO(HANGMAN_MAGIC_NUM, 5)

MODULE_LICENSE("GPL");

static struct {
	char* reveal_str;
	char* bad_guess_str;
	char* secret_str;
	char* output_str;
	u8 num_guesses;
	u8 status;
	struct mutex lock;
} game = { .lock = __MUTEX_INITIALIZER(game.lock) };

// protects word_bank and word_count
DEFINE_MUTEX(word_bank_lock);
static char word_bank[WB_SIZE][STR_SIZE];
static u8 word_count = 0;

// update game.output_str to reflect the current state of the game
// should only be used when game.lock has already been acquired
static void update_output(void)
{
	strncpy(game.output_str, game.reveal_str, STR_SIZE);
	strncat(game.output_str, "\n", 1);

	strncat(game.output_str, game.bad_guess_str, STR_SIZE);
	strncat(game.output_str, "\n", 1);

	char guess_buf[STR_SIZE] = {0};
	snprintf(guess_buf, STR_SIZE, "%d guesses left\n", game.num_guesses);
	strncat(game.output_str, guess_buf, STR_SIZE);

	if(game.status == 1) {
		char lose_str[STR_SIZE] = "You Lose!\n";
		strncat(game.output_str, lose_str, STR_SIZE);
	} else if(game.status == 2) {
		char win_str[STR_SIZE] = "You Win!\n";
		strncat(game.output_str, win_str, STR_SIZE);
	}
}

// game.lock must be locked before calling init_game
static bool init_game(void)
{
	if(mutex_lock_interruptible(&word_bank_lock))
		return false;

	if(word_count == 0) {
		strncpy(word_bank[0], "EXAMPLE", STR_SIZE);
		word_count = 1;
	}

	game.num_guesses = 10;
	game.status = 0;

	game.secret_str = kzalloc(STR_SIZE, GFP_KERNEL);
	if(!game.secret_str)
		goto fail_ss;

	u8 idx = get_random_u8() % word_count;
	strncpy(game.secret_str, word_bank[idx], STR_SIZE);

	game.reveal_str = kzalloc(STR_SIZE, GFP_KERNEL);
	if(!game.reveal_str)
		goto fail_rs;

	for(int i = 0; i < strlen(game.secret_str); i++) {
		game.reveal_str[i*2] = '-';
		game.reveal_str[i*2+1] = ' ';
	}

	game.bad_guess_str = kzalloc(STR_SIZE, GFP_KERNEL);
	if(!game.bad_guess_str)
		goto fail_bg;

	game.output_str = kzalloc(STR_SIZE * 4, GFP_KERNEL);
	if(!game.output_str)
		goto fail_os;


	update_output();

	mutex_unlock(&word_bank_lock);

	return true;

fail_os:
	kfree(game.bad_guess_str);
fail_bg:
	kfree(game.reveal_str);
fail_rs:
	kfree(game.secret_str);
fail_ss:
	mutex_unlock(&word_bank_lock);
	return false;
}

// game.lock must be locked before calling free_game
static void free_game(void)
{
	kfree(game.reveal_str);
	game.reveal_str = NULL;

	kfree(game.bad_guess_str);
	game.bad_guess_str = NULL;

	kfree(game.secret_str);
	game.secret_str = NULL;

	kfree(game.output_str);
	game.output_str = NULL;
}

// should only be used when game.lock has already been acquired
bool already_guessed(char guess)
{
	bool found_guess = false;

	for(int i = 0; i < strlen(game.reveal_str); i++) {
		if(game.reveal_str[i*2] == guess) {
			found_guess = true;
			break;
		}
	}

	if(!found_guess) {
		for(int i = 0; i < strlen(game.bad_guess_str); i++) {
			if(game.bad_guess_str[i] == guess) {
				found_guess = true;
				break;
			}
		}
	}

	return found_guess;
}

// should only be used when game.lock has already been acquired
bool reveal_chars(char guess)
{
	bool found_guess = false;

	for(int i = 0; i < strnlen(game.secret_str, STR_SIZE); i++) {
		if(game.secret_str[i] == guess) {
			game.reveal_str[i*2] = guess;
			found_guess = true;
		}
	}

	return found_guess;
}

// should only be used when game.lock has already been acquired
void check_win(void)
{
	for(int i = 0; i < strnlen(game.reveal_str, STR_SIZE); i++) {
		if(game.reveal_str[i] == '-')
			return;
	}

	game.status = 2;
}

// Used to parse a new word bank from ioctl_write_word_bank
// returns each individual word in a comma seperated list of words
static char* next_word(const char* buf, u8* pos)
{
	char* word = kzalloc(STR_SIZE, GFP_KERNEL);
	if(!word)
		return NULL;

	u8 i = 0;
	while(buf[*pos] != ',' && i < STR_SIZE - 1) {
		if(buf[*pos] == '\0') {
			word[i] = '\0';
			*pos = U8_MAX;
			goto end_of_buffer;
		}

		word[i++] = buf[(*pos)++];
	}

	(*pos)++; // move past comma

end_of_buffer:
	word[i] = '\0';
	return word;
}

static long ioctl_read_word_bank(char* __user buf)
{
	if(mutex_lock_interruptible(&word_bank_lock))
		return -EINTR;

	if(word_count == 0) {
		mutex_unlock(&word_bank_lock);
		return -ENODATA;
	}

	static char local_buf[WB_SIZE * (STR_SIZE + 1)];

	strncat(local_buf, word_bank[0], STR_SIZE);
	for(int i = 1; i < word_count; i++) {
		strncat(local_buf, ",", 1);
		strncat(local_buf, word_bank[i], STR_SIZE);
	}

	mutex_unlock(&word_bank_lock);

	// Specification says buffer passed to ioctl will be 500 bytes
	size_t count = min_t(size_t, strlen(local_buf), MAX_BANK_SIZE);
	if(copy_to_user(buf, local_buf, count))
		return -EFAULT;

	memset(local_buf, 0, WB_SIZE * (STR_SIZE + 1));
	return 0;
}

static long ioctl_read_secret_word(char* __user buf)
{
	if(mutex_lock_interruptible(&game.lock))
		return -EINTR;

	if(!game.secret_str)
		goto err_unlock;

	// Specification says buffer passed to ioctl will be 50 bytes
	if(copy_to_user(buf, game.secret_str, 50))
		goto err_unlock;

	mutex_unlock(&game.lock);
	return 0;

err_unlock:
	mutex_unlock(&game.lock);
	return -EFAULT;
}

static long ioctl_write_word_bank(const char* __user buf)
{
	char local_buf[MAX_BANK_SIZE] = {0};
	char* word;
	u8 i = 0;
	u8 pos = 0;

	// Specification says buffer passed to ioctl will be 500 bytes
	if(copy_from_user(local_buf, buf, MAX_BANK_SIZE))
		return -EFAULT;

	if(mutex_lock_interruptible(&word_bank_lock))
		return -EINTR;

	word_count = 0;
	while((word = next_word(local_buf, &pos)) && i < WB_SIZE) {
		strncpy(word_bank[i], word, STR_SIZE);
		kfree(word);
		i++;
		word_count++;

		if(pos == U8_MAX)
			break;
	}

	mutex_unlock(&word_bank_lock);
	return 0;
}

static long ioctl_write_secret_word(const char* __user buf)
{
	char local_buf[MAX_SECRET_SIZE] = {0};
	if(copy_from_user(local_buf, buf, MAX_SECRET_SIZE))
		return -EFAULT;

	for(int i = 0; i < MAX_SECRET_SIZE; i++) {
		if(local_buf[i] == '\0')
			break;

		local_buf[i] = toupper(local_buf[i]);
	}

	if(mutex_lock_interruptible(&game.lock))
		return -EINTR;

	memset(game.secret_str, 0, STR_SIZE);
	strncpy(game.secret_str, local_buf, MAX_SECRET_SIZE);

	memset(game.reveal_str, 0, STR_SIZE);
	for(int i = 0; i < strnlen(game.secret_str, MAX_SECRET_SIZE); i++) {
		game.reveal_str[i*2] = '-';
		game.reveal_str[i*2+1] = ' ';
	}

	memset(game.bad_guess_str, 0, STR_SIZE);
	game.num_guesses = 10;
	game.status = 0;

	update_output();

	mutex_unlock(&game.lock);
	return 0;
}

static long ioctl_restart(struct file* file)
{
	if(mutex_lock_interruptible(&game.lock))
		return -EINTR;

	free_game();

	if(mutex_lock_interruptible(&word_bank_lock)) {
		mutex_unlock(&game.lock);
		return -EINTR;
	}

	for(int i = 0; i < WB_SIZE; i++)
		memset(word_bank[i], 0, STR_SIZE);

	word_count = 0;
	mutex_unlock(&word_bank_lock);

	bool ret = init_game();
    file->f_pos = 0;

	mutex_unlock(&game.lock);

	return ret ? 0 : -EFAULT;
}

ssize_t hangman_read(struct file* file, char* __user buf, size_t size, loff_t* off)
{
	if(mutex_lock_interruptible(&game.lock))
		return -EINTR;

	char* msg = game.output_str;
	if(!msg || *off < 0 || *off > strlen(msg)) {
		mutex_unlock(&game.lock);
		return -EINVAL;
	}

	int count = min_t(size_t, strlen(msg) - *off, size);
	int ret = copy_to_user(buf, msg + *off, count);
	mutex_unlock(&game.lock);

	*off += count - ret;
	return *off - file->f_pos;
}

ssize_t hangman_write(struct file* file, const char* __user buf, size_t size, loff_t* off)
{
	if(mutex_lock_interruptible(&game.lock))
		return -EINTR;

	if(size != 2 || game.status != 0)
		goto err;

	char guess[2] = {0};
	if(copy_from_user(guess, buf, 2))
		goto err;

	if(!isalpha(guess[0]))
		goto err;

	guess[0] = toupper(guess[0]);
	if(already_guessed(guess[0]))
		goto out;

	bool found_char = reveal_chars(guess[0]);
	if(!found_char) {
		if(--game.num_guesses == 0) {
			game.status = 1; // lost game
		}

		if(strlen(game.bad_guess_str) != 0)
			strncat(game.bad_guess_str, " ", 1);

		strncat(game.bad_guess_str, guess, 1);
	} else {
		check_win();
	}

	update_output();

out:
	*off = 0;
	mutex_unlock(&game.lock);
	return size;

err:
	mutex_unlock(&game.lock);
	return -EFAULT;
}

static long hangman_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
	switch(cmd)
	{
	case HANGMAN_IOC_READ_BANK:
		return ioctl_read_word_bank((void* __user)arg);
	case HANGMAN_IOC_READ_SECRET:
		return ioctl_read_secret_word((void* __user)arg);
	case HANGMAN_IOC_WRITE_BANK:
		return ioctl_write_word_bank((void* __user)arg);
	case HANGMAN_IOC_WRITE_SECRET:
		return ioctl_write_secret_word((void* __user)arg);
	case HANGMAN_IOC_RESTART:
		return ioctl_restart(file);
	default:
		return -EINVAL;
	}

	return 0;
}

static loff_t hangman_llseek(struct file* file, loff_t off, int whence)
{
	if(mutex_lock_interruptible(&game.lock))
		return -EINTR;

	switch(whence)
	{
	case SEEK_SET:
		file->f_pos = off;
		break;
	case SEEK_CUR:
		file->f_pos += off;
		break;
	case SEEK_END:
		file->f_pos = strlen(game.output_str) + off;
		break;
	default:
		return -EINVAL;
	}


	if(file->f_pos < 0)
		file->f_pos = 0;
	else if(file->f_pos >= strlen(game.output_str))
		file->f_pos = strlen(game.output_str) - 1;

	mutex_unlock(&game.lock);

	return file->f_pos;
}

static struct file_operations hangman_fops = {
	.read = hangman_read,
	.write = hangman_write,
	.unlocked_ioctl = hangman_ioctl,
	.llseek = hangman_llseek,
};

static struct miscdevice hangman_md = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hangman",
	.fops = &hangman_fops,
	.mode = 0666,
};

static int __init hangman_init(void)
{
	if(mutex_lock_interruptible(&game.lock))
		return -EINTR;

	if(!init_game())
		return -EFAULT;

	mutex_unlock(&game.lock);

	misc_register(&hangman_md);
	return 0;
}

static void __exit hangman_exit(void)
{
	misc_deregister(&hangman_md);

	if(mutex_lock_interruptible(&game.lock))
		return;

	free_game();
	mutex_unlock(&game.lock);
	mutex_destroy(&game.lock);
}

module_init(hangman_init);
module_exit(hangman_exit);
