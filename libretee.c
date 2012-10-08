#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

/* a boolean data type */
typedef enum {
	FALSE,
	TRUE
} bool_t;

/* the reading buffer size - 8K */
#define BUFFER_SIZE (8192)

/* a dummy signal handler */
void handler(int signal) {
}

/* duplicate_stdin() (bool_t)
 * purpose: reads data from standard input and writes it to an array of file
 *          descriptors
 * input  : a buffer, its size, an array of file descriptors, the array size and
 *          a pointer to a ssize_t which will be assigned the number of bytes
 *          read
 * output : TRUE upon success, FALSE on error; count gets assigned the number of
 *          bytes read in the last reading attempt */
bool_t duplicate_stdin(char *buffer,
                       size_t buffer_size,
                       int *files,
                       size_t files_len,
                       ssize_t *count) {
	/* a loop counter */
	size_t i;

	for ( ; ; ) {
		/* read from the read end of the pipe */
		*count = read(STDIN_FILENO, buffer, buffer_size);

		/* if the end of the data was reached, stop here and report success */
		if (0 == *count)
			return TRUE;

		/* otherwise, the reading succeeded - write the read data to all output
		 * file descriptors */
		for (i = 0; files_len > i; ++i) {
			if (-1 == write(files[i], buffer, *count))
				return FALSE;
		}
	}
}

int main(int argc, char *argv[]) {
	/* the return value */
	int ret = EXIT_FAILURE;

	/* the flags the standard input pipe was opened with */
	int flags = 0;

	/* the reading buffer */
	char buffer[BUFFER_SIZE] = {0};

	/* the number of bytes read at one iteration of the reading loop */
	ssize_t count = 0;

	/* a signal mask used for waiting until SIGIO arrives */
	sigset_t wait_mask = {{0}};

	/* a signal mask used for blocking SIGIO while reading */
	sigset_t block_mask = {{0}};
	sigset_t original_mask = {{0}};

	/* the signal action */
	struct sigaction action = {{0}};

	/* the file descriptors data is written to */
	int *files = NULL;

	/* the file descriptors array size */
	size_t files_size = sizeof(files[0]) * argc;

	/* a loop counter */
	int i = 1;

	/* disable buffering for both the standard input and output pipes */
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);

	/****************
	 * file opening *
	 ****************/

	/* allocate enough file descriptors */
	if (NULL == (files = malloc(files_size)))
		goto end;

	/* zero all file descriptors */
	(void) memset(files, 0, files_size);

	/* the first file is the standard output pipe */
	files[0] = STDOUT_FILENO;

	/* open all output files */
	if (1 < argc) {
		for (i = 1; argc > i; ++i) {
			if (-1 == (files[i] = open(argv[i],
			                           O_WRONLY | O_CREAT,
			                           S_IWUSR)))
				goto end;
		}
	}

	/* do one iteration of the reading loop - if data was written before the
	 * signal handler was registered, SIGIO won't be sent for it */
	if (FALSE == duplicate_stdin((char *) &buffer,
	                             sizeof(buffer),
	                             files,
	                             argc,
	                             &count))
		goto end;

	/* if all data was read even before the reading loop began, report
	 * success */
	if (0 == count) {
		ret = EXIT_SUCCESS;
		goto end;
	}

	/*******************************
	 * signal handler registration *
	 *******************************/

	/* fill the list of signals with all available ones */
	if (-1 == sigfillset(&wait_mask))
		goto end;

	/* remove SIGIO from it - that's the signal we wait for */
	if (-1 == sigdelset(&wait_mask, SIGIO))
		goto end;

	/* empty the list of signals to block */
	if (-1 == sigemptyset(&block_mask))
		goto end;

	/* add only SIGIO to the list of blocked signals */
	if (-1 == sigaddset(&block_mask, SIGIO))
		goto end;

	/* block SIGIO signals, until the reading loop begins */
	if (-1 == sigprocmask(SIG_BLOCK, &block_mask, &original_mask))
		goto end;

	/* empty the list of signals to block, during the signal handler's life */
	if (-1 == sigemptyset(&action.sa_mask))
		goto end;

	/* register the signal handler */
	action.sa_handler = handler;
	if (-1 == sigaction(SIGIO, &action, NULL))
		goto end;

	/* set the O_ASYNC flag on the standard input pipe, so the process receives
	 * SIGIO whenever data is written to it */
	if ((-1 == fcntl(STDIN_FILENO, F_SETOWN, getpid())) ||
	    (-1 == (flags = fcntl(STDIN_FILENO, F_GETFL))) ||
	    (-1 == fcntl(STDIN_FILENO, F_SETFL, flags | O_ASYNC)))
		goto end;

	/****************
	 * reading loop *
	 ****************/

	for ( ; ; ) {
		/* unblock the next SIGIO signal */
		if (-1 == sigprocmask(SIG_UNBLOCK, &block_mask, NULL))
			goto end;

		/* wait for SIGIO to arrive */
		(void) sigsuspend(&wait_mask);

		/* once the signal arrived, block the next one so system calls within
		 * this critical piece of code are not interrupted by it */
		if (-1 == sigprocmask(SIG_BLOCK, &block_mask, &wait_mask))
			goto end;

		/* read from standard input and write the read data to all file
		 * descriptors */
		if (FALSE == duplicate_stdin((char *) &buffer,
		                             sizeof(buffer),
		                             files,
		                             argc,
		                             &count))
			goto end;

		/* if the end of the data was reached, break the loop and report
		 * success */
		if (0 == count) {
			ret = EXIT_SUCCESS;
			goto end;
		}
	}

end:
	if (NULL != files) {
		/* close all output files */
		for (i = 1; argc > i; ++i) {
			if (0 != files[i])
				(void) close(files[i]);
		}

		/* free the file descriptors array */
		(void) free(files);
	}

	return ret;
}