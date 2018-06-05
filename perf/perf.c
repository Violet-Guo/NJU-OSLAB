#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#define MAXLEN 2048

int fildes[2];

struct {
    char func_name[MAXLEN][50];
    double func_time[MAXLEN];
    double total_time;
    int cnt;
}global;

void init();
void calcu(char data[]);
void print_result();

int main(int argc, char *argv[], char *envp[]) {
  if (argc < 2) {
    printf("Usage: perf COMMAND [ARG]...\n");
    return 0;
  }

  for (int i = 0; i < argc; i++)
    printf("para %d: %s\n", i + 1, argv[i]);

  if (pipe(fildes) != 0) {
    printf("ERROR: Fail to create the pipe !\n");
    return 0;
  }

  pid_t pid;
  pid = fork();
  if (pid < 0) {
    printf("ERROR: Fail to fork !\n");
    return 0;
  }

  if (0 == pid) {
    // child
    close(fildes[0]);
    char *new_argv[] = {"strace", "-T", argv[1], NULL};
    int fd_null;

    if ((fd_null = open("/dev/null", O_WRONLY)) < 0) {
      printf("ERROR: Fail to wire the /dev/null file !\n");
      return 0;
    }
    dup2(fildes[1], STDERR_FILENO);
    dup2(fd_null, STDOUT_FILENO);
    execve("/usr/bin/strace", new_argv, envp);
  }
  else {
    // father
    close(fildes[1]);
    dup2(fildes[0], STDIN_FILENO);

    init();
    char buffer[2048];
    while (fgets(buffer, 2048, stdin) != NULL) {
      //printf("%s", buffer);
      calcu(buffer);
    }

    printf("\n");
    print_result();
  }

  return 0;
}

void init() {
  memset(global.func_name, '\0', sizeof(global.func_name));
  memset(global.func_time, 0.0, sizeof(global.func_time));
  global.total_time = 0.0;
  global.cnt = 0;
}

void calcu(char data[]) {
  char name[100];
  double ti = 0.0;
  int len = strlen(data);
  int i = len - 1;
  int flag;

  sscanf(data, "%[0-9|a-z|A-Z]", name);
  //printf("func name = %s\t", name);
  if (name[0] == '\0')
    return ;

  while (data[i] != '<') { i--; }
  sscanf(&data[i], "<%lf>", &ti);
  //printf("func time = %lf\n", ti);
  if (ti == 0.0)
    return ;

  flag = 0;
  global.total_time += ti;
  for (i = 0; i < global.cnt; i++) {
    if (strcmp(global.func_name[i], name) == 0) {
      global.func_time[i] += ti;
      flag = 1;
    }
  }

  if (!flag) {
    strcpy(global.func_name[i], name);
    global.func_time[i] = ti;
    global.cnt++;
  }
}

void print_result() {
  for (int i = 0; i < global.cnt; i++) {
    //printf("func name = %s\t func time = %lf\t percent = %lf%%\n",
    //       global.func_name[i], global.func_time[i], (global.func_time[i] / global.total_time * 100));
    printf("%s: %lf%%\n", global.func_name[i], (global.func_time[i] / global.total_time * 100));
  }
  printf("\nTotal time: %lf\nTotal func: %d\n", global.total_time, global.cnt);
}