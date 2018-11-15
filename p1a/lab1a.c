#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>

int DEBUG = 0;
struct termios orgn_tios;

// modify the termios
void mod_termios(struct termios *tp) {
  (*tp).c_iflag = ISTRIP;
  (*tp).c_oflag = 0;
  (*tp).c_lflag = 0;
  tcsetattr(0, TCSANOW, tp);
}

// restore original termios
void restore_termios() {
  tcsetattr(0, TCSANOW, &orgn_tios);
  if (DEBUG) printf("DEBUG: Terminal modes set back to original\n");
}

// mapping code
// 1: map <cr> or <lf> to <cr><lf>, replace INT&EOT    stdin -> stdout
// 2: map <cr> to <lf>, DO NOT replace INT&EOT         stdin -> shell
// 3: mao <lf> to <cr><lf>, replace INT&EOT            shell -> stdout
char *prepare_output(char *buf, int len, int mapping_code) {
  char *new_buf = malloc(2*len*sizeof(char)+1);
  int i, j = 0;
  if (mapping_code == 1) {
    for ( i = 0; i < len; i++, j++) {
      if (buf[i] == '\r' || buf[i] == '\n') {
	new_buf[j] = '\r'; j++; new_buf[j] = '\n';
      }
      else if (buf[i] == '\x04') {
	new_buf[j] = '^'; j++; new_buf[j] = 'D';
	j++; break;
      }
      else if (buf[i] == '\x03') {
	new_buf[j] = '^'; j++; new_buf[j] = 'C';
      }
      else {
	new_buf[j] = buf[i];
      }
    }
  }
  else if (mapping_code == 2) {
    for ( i = 0; i < len; i++, j++) {
      if (buf[i] == '\r') {
        new_buf[j] = '\n';
      }
      else {
        new_buf[j] = buf[i];
	if (buf[i] == '\x04') { j++; break; }
      }
    }
  }
  else if (mapping_code == 3) {
    for ( i = 0; i < len; i++, j++) {
      if (buf[i] == '\n') {
        new_buf[j] = '\r'; j++; new_buf[j] = '\n';
      }
      else if (buf[i] == '\x04') {
        new_buf[j] = '^'; j++; new_buf[j] = 'D';
	j++; break;
      }
      else if (buf[i] == '\x03') {
        new_buf[j] = '^'; j++; new_buf[j] = 'C';
      }
      else {
        new_buf[j] = buf[i];
      }
    }    
  }
  new_buf[j] = '\0';
  return new_buf;
}

// find the index of the first EOT in buf, if no EOT return -1
int find_char(char ch, char *buf) {
  int i = 0;
  int index = -1;
  while (buf[i]) {
    if (buf[i] == ch) {
      index = i;
      break;
    }
    else i++;
  }
  return index;
}

// associate with fd1 the file originally linked with fd2
void change_fd(int fd1, int fd2) {
  if (close(fd1) == -1 ||
      dup(fd2)   == -1 ||
      close(fd2) == -1) {
    fprintf(stderr, "Fail to change fd\r\n");
    restore_termios();
    exit(1);
  }
}

// graceful exit
void shutdown() {
  int sh_stat = 0;
  waitpid(-1, &sh_stat, WNOHANG | WUNTRACED | WCONTINUED);
  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n", WTERMSIG(sh_stat), WEXITSTATUS(sh_stat));
  restore_termios();
  exit(0);
}

void sig_handler(int sig) {
  if (DEBUG) printf("DEBUG: Caught SIGPIPE%d\r\n", sig);
  if (sig == SIGPIPE) {
    shutdown();
  }
}

int handle_input (int ifd, int ofd_shell, pid_t cpid) {
  int bufsize = 256;
  char buf[bufsize+1]; 
  // read from ifd character-at-a-time
  int rv_r = read(ifd, buf, bufsize);
  if (rv_r == -1) {
    restore_termios();
    fprintf(stderr, "Failed to read from fd%d: %s\r\n", ifd, strerror(errno));
    exit(1);
  }
  if (rv_r == 0) return 0;

  buf[rv_r] = '\0';
  if (DEBUG) printf("DEBUG: from fd%d [%s]\r\n", ifd, buf);

  // look for EOT/interruption
  int INTindex = find_char('\x03', buf);
  int EOTindex = find_char('\x04', buf);
  
  // types of communication:
  //     from    to      code
  //     stdin   stdout  1
  //     stdin   shell   2
  //     shell   stdout  3
  
  // input from stdin(keyboard)
  if (ifd == 0) {
    // output to stdout, code 1
    char *buf1 = prepare_output(buf, rv_r, 1);
    if (DEBUG) printf("DEBUG: to fd1 [%s]\r\n", buf1);
    int rv_w1 = write(1, buf1, strlen(buf1));
    free(buf1);
    if (rv_w1 == -1) {
      restore_termios();
      fprintf(stderr, "Failed to write to fd1: %s\r\n", strerror(errno));
      exit(1);
    }
    // output to shell, code 2
    char *buf2 = prepare_output(buf, rv_r, 2);
    // if only a ^D, write nothing shell
    if (buf2[0] == '\x04') buf2[0] = '\0';
    if (DEBUG) printf("DEBUG: to sh [%s]\r\n", buf2);
    int rv_w2 = write(ofd_shell, buf2, strlen(buf2));
    free(buf2);
    if (rv_w2 == -1) {
      restore_termios();
      fprintf(stderr, "Failed to write to shell: %s\r\n", strerror(errno));
      exit(1);
    }
    if (INTindex != -1) {
      if (DEBUG) printf("DEBUG: From fd0, encountered ^C. Send SIGINT to sh\r\n");
      if (kill(cpid, SIGINT) == -1) {
	fprintf(stderr, "Failed to send SIGINT to child process: %s\r\n", strerror(errno));
      }
    }
    if (EOTindex != -1) {
      if (DEBUG) printf("DEBUG: From fd0, encountered ^D. Turn off ofd to sh\r\n");
      close(ofd_shell); if (DEBUG) printf("DEBUG: fd%d closed\r\n", ofd_shell);
    }
  }
  // input from shell
  else {
    // output to stdout, code 3
    char *buf3 = prepare_output(buf, rv_r, 3);
    if (DEBUG) printf("DEBUG: to fd1 [%s]\r\n", buf3);
    int rv_w3 = write(1, buf3, strlen(buf3));
    free(buf3);
    if (rv_w3 == -1) {
      restore_termios();
      fprintf(stderr, "Failed to write to fd1: %s\r\n", strerror(errno));
      exit(1);
    }
    if (EOTindex != -1) {
      if (DEBUG) printf("DEBUG: From sh, encountered ^D. Shut down\r\n");
      shutdown();
    }
  }
  return 1;
}

void plain_echo() {
  int bufsize = 256;
  while(1) {
    char buf[bufsize+1];
    // read from fd0 character-at-a-time
    int rv_r = read(0, buf, bufsize);
    if (rv_r == -1) {
      restore_termios();
      fprintf(stderr, "Failed to read from fd0: %s\n", strerror(errno));
      exit(1);
    }
    buf[rv_r] = '\0';
    if (DEBUG) printf("DEBUG: in [%s]\r\n",buf);
    // handle <cr> and <lf> with mapping 1
    char *new_buf = prepare_output(buf, rv_r, 1);
    // display on fd1
    if (DEBUG) printf("DEBUG: out [%s]\r\n",new_buf);
    int rv_w = write(1, new_buf, strlen(new_buf));
    free(new_buf);
    if (rv_w == -1) {
      restore_termios();
      fprintf(stderr, "Failed to write to fd1: %s\n", strerror(errno));
      exit(1);
    }
    int EOTindex = find_char('\x04', buf);
    if (EOTindex != -1) {
      if (DEBUG) printf("DEBUG: Encountered ^D. Exit\r\n");
      restore_termios();
      exit(0);
    }
  }
}

int main(int argc, char *argv[]) {
  // parse --shell option
  char *prog_name = NULL;
  char *usage = "lab1a [--shell=program] [--debug]";
  static struct option opts[] = {
    {"shell", 1, 0, 1},
    {"debug", 0, 0, 2},
    {0, 0, 0, 0}
  };
  while (1) {
    int opt_index = -1;
    int rv = getopt_long(argc, argv, "", opts, &opt_index);
    if (rv == -1) break;
    else if (rv == '?') {
      fprintf(stderr, "Usage: %s\n", usage);
      exit(1);
    }
    else if (rv == 1) {
      prog_name = optarg;
      if (DEBUG) printf("DEBUG: Program name: %s\n", prog_name);
    }
    else if (rv == 2) {
      DEBUG = 1;
    }
    else {
      if (DEBUG) printf("DEBUG: Unexpected error with parsing\n");
    }
  }

  // store original terminal modes & change terminal to new modes
  tcgetattr(0, &orgn_tios);
  struct termios mod_tios;
  mod_tios = orgn_tios;
  mod_termios(&mod_tios);
  if (DEBUG) printf("DEBUG: Changed terminal modes\r\n");
    //close(0); // if close(0) follows tcsetattr, restoration will fail
    //restore_termios();
    //exit(0);

  // no shell option specified
  if (!prog_name) {
    plain_echo();
    exit(0);
  }

  // setup pipes & fork
  int t2s_pipefd[2];
  int s2t_pipefd[2];
  if (pipe(t2s_pipefd) == -1 || pipe(s2t_pipefd) == -1) {
    fprintf(stderr, "Failed to pipe: %s\r\n", strerror(errno));
    restore_termios();
    exit(1);
  }
  if (DEBUG) printf("DEBUG: Pipes set up\r\n");
  pid_t cpid = fork();
  if (cpid == -1) {
    fprintf(stderr, "Failed to fork: %s\r\n", strerror(errno));
    restore_termios();
    exit(1);
  }
  else if (cpid == 0) { // child process, shell
    if (DEBUG) printf("DEBUG: Child process %d started\r\n", getpid());
    // configure pipes
    // terminal to shell, shell does not write
    if (close(t2s_pipefd[1]) == -1) {
      fprintf(stderr, "Failed to close fd%d: %s\r\n", t2s_pipefd[1], strerror(errno));
      restore_termios();
      _exit(1);
    }
    // shell to terminal, shell does not rea
    if (close(s2t_pipefd[0]) == -1) {
      fprintf(stderr, "Failed to close fd%d: %s\r\n", s2t_pipefd[0], strerror(errno));
      restore_termios();
      _exit(1);
    }
    //close(t2s_pipefd[1]); 
    //close(s2t_pipefd[0]); 
    change_fd(0, t2s_pipefd[0]); // associate the fd0 of shell with t2s_pipe
    change_fd(1, s2t_pipefd[1]); // associate the fd1 of shell with s2t_pipe
    close(2); dup(1); // fd2 and fd1 share the same s2t_pipe
    if (close(2) == -1 || dup(1) == -1) {
      fprintf(stderr, "Failed to close fd2 and dup fd1: %s\r\n", strerror(errno));
      restore_termios();
      _exit(1);
    }
    char *path = "/bin/bash";
    if (execl(path, prog_name, (char *) NULL) < 0) {
      fprintf(stderr, "Cannot execute %s\r\n", prog_name);
      restore_termios();
      _exit(1);
    }
  }
  else { // parent process, terminal
    // configure pipes
    // terminal to shell, terminal does not read
    if (close(t2s_pipefd[0]) == -1) {
      fprintf(stderr, "Failed to close fd%d: %s\r\n", t2s_pipefd[0], strerror(errno));
      restore_termios();
      exit(1);
    }
    // shell to terminal, terminal does not write
    if (close(s2t_pipefd[1]) == -1) {
      fprintf(stderr, "Failed to close fd%d: %s\r\n", s2t_pipefd[1], strerror(errno));
      restore_termios();
      exit(1);
    }
    //close(t2s_pipefd[0]); 
    //close(s2t_pipefd[1]);
 
    // setup poll
    struct pollfd fds[2];
    fds[0].fd = 0; // fd for keyboard (stdin)
    fds[0].events = POLLIN | POLLHUP | POLLERR;
    fds[1].fd = s2t_pipefd[0]; // fd for shell to terminal pipe
    fds[1].events = POLLIN | POLLHUP | POLLERR;

    // register signal handler for the exit of shell
    signal(SIGPIPE, sig_handler);

    // wait for events from poll
    while (1) {
      int rv = poll(fds, 2, 0);
      if (rv == -1) {
	fprintf(stderr, "Failed to poll: %s\r\n", strerror(errno));
	restore_termios();
	exit(1);
      }
      if (rv > 0) {
	int i;
	for (i = 0; i < 2; i++) {
	  if (fds[i].revents & POLLIN) {
	    if (DEBUG) printf("DEBUG: Ready to read from fd%d\r\n", fds[i].fd);
	    int rv = handle_input(fds[i].fd, t2s_pipefd[1], cpid);
	    if (!rv) {
	      if (close(fds[i].fd) == -1) {
		fprintf(stderr, "Failed to close fd%d: %s\r\n", fds[i].fd, strerror(errno));
		restore_termios();
		exit(1);
	      }
	      if (DEBUG) printf("DEBUG: fd%d closed\r\n", fds[i].fd);
	    }
	  }
	  if (fds[i].revents & POLLHUP) {
	    if (DEBUG) printf("DEBUG: Hang-up from fd%d\r\n", fds[i].fd);
	    if (close(fds[i].fd) == -1) {
	      fprintf(stderr, "Failed to close fd%d: %s\r\n", fds[i].fd, strerror(errno));
	      restore_termios();
	      exit(1);
	    }
            if (DEBUG) printf("DEBUG: fd%d closed\r\n", fds[i].fd);
	    shutdown();
	  }
	  if (fds[i].revents & POLLERR) {
	    if (DEBUG) printf("DEBUG: Error with fd%d\r\n", fds[i].fd);
	    shutdown();
	  }
	}
      }
    }
  }
  return 0;
}
