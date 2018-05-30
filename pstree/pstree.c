#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#define PATH1 "/proc"
#define PATH2 "/task"
#define MAXLEN 1024

typedef struct procInfo {
    pid_t pid;
    char comm[MAXLEN];
    struct proInfo *parent;
    struct proInfo *next;
    struct child *children;
}PROC;

typedef struct child {
    PROC *child;
    struct child *next;
}CHILD;

struct {
    int show_pids;
    int numeric_sort;
    PROC *list;

    // used to print the tree
    int size;
    int is_last_child[MAXLEN];
    int comm_len[MAXLEN];
}global;

void init();
int process_arg(int argc, char *argv[]);
void read_dir(const char *path, pid_t parent, int is_thread);
void read_proc(const char *pathname, pid_t parent, int is_thread);
void add_proc(const char *comm, pid_t pid, pid_t ppid);
void add_child(PROC *parent, PROC *child);
int proc_cmp(PROC *left, PROC *right);
PROC *find_proc(pid_t pid);
PROC *new_proc(const char *comm, pid_t pid);
void print_tree(PROC *node);
void push_indent(int comm_len, int is_last);
void pop_indent();

void show_version();
void show_usage();
void show_tree();


int main(int argc, char *argv[]) {
  init();
  int opt = process_arg(argc, argv);
  //printf("%d\n", opt);

  switch(opt) {
    case 1:
    case 2:
    case 3:
      show_tree();
      break;
    case 4:
      show_version();
      break;
    case 5:
      printf("Unrecognized option: %s\n", argv[1]);
      show_usage();
      exit(EXIT_FAILURE);
    case 6:
      printf("Only need none or one argument\n");
      show_usage();
      exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}

void show_tree(int opt) {
  read_dir(PATH1, 0, 0);

  //printf("before print tree !!!!\n");
  print_tree(find_proc(1));
}

void print_tree(PROC *node) {
  assert(node);
  char comm[MAXLEN];
  int comm_len, cnt;
  CHILD *tmp;
  int is_last = 0;

  if (global.show_pids)
    sprintf(comm, "%s(%d)", node->comm, node->pid);
  else
    sprintf(comm, "%s", node->comm);

  comm_len = strlen(comm);
  printf("%s", comm);

  cnt = 0;
  for (tmp = node->children; tmp != NULL; tmp = tmp->next) {
    //printf("in the for\n");
    cnt++;
    if (tmp->next == NULL)
      is_last = 1;

    if (1 == cnt) {
      printf(is_last ? "---" : "-+-");
    }
    else {
      for (int i = 0; i < global.size; ++i)
        printf("%*s", global.comm_len[i] + 3, (global.is_last_child[i] ? "   " : " | "));
      printf("%*s", comm_len + 3, (is_last ? " `-" : " |-"));
    }

    // use to guarantee the indent of print
    push_indent(comm_len, is_last);
    print_tree(tmp->child);
    pop_indent();
  }

  if (0 == cnt)
    printf("\n");
}

// use to guarantee the indent of print
void push_indent(int comm_len, int is_last) {
  assert(global.size < MAXLEN);
  global.comm_len[global.size] = comm_len;
  global.is_last_child[global.size] = is_last;
  global.size++;
}

// use to guarantee the indent of print
void pop_indent() {
  assert(global.size > 0);
  global.size--;
}

void read_dir(const char *path, pid_t parent, int is_thread) {
  DIR *dir;
  struct dirent *ptr;
  char pathname[MAXLEN];

  if ((dir = opendir(path)) == NULL) {
    perror("opendir error");
    exit(EXIT_FAILURE);
  }

  while ((ptr = readdir(dir)) != NULL) {
    if (!(ptr->d_name[0] >= '0' && ptr->d_name[0] <= '9'))
      continue;
    sprintf(pathname, "%s/%s", path, ptr->d_name);
    //printf("dir pathname : %s\n", pathname);
    read_proc(pathname, parent, is_thread);
  }

  if (closedir(dir) < 0) {
    perror("closedir error");
    exit(EXIT_FAILURE);
  }
}

void read_proc(const char *pathname, pid_t parent, int is_thread) {
  FILE *stat_file;
  char stat_path[MAXLEN];
  char buf[MAXLEN];
  char comm[MAXLEN];
  char task_state;
  pid_t pid, ppid;

  sprintf(stat_path, "%s/stat", pathname);
  if (0 == (access(stat_path, F_OK))) {
    //printf("stat_path = %s\n", stat_path);
    if ((stat_file = fopen(stat_path, "r")) == NULL) {
      perror("opendir error");
      exit(EXIT_FAILURE);
    }

    assert(fgets(buf, MAXLEN, stat_file) != NULL);

    if (fclose(stat_file) != 0) {
      perror("fclose error");
      exit(EXIT_FAILURE);
    }

    sscanf(buf, "%d %s %c %d", &pid, comm, &task_state, &ppid);
    //printf("pid = %d, comm = %s, task_state = %c, ppid = %d\n", pid, comm, task_state, ppid);

    // if this process is not thread, remove the '(' ')'
    if (!is_thread) {
      comm[strlen(comm) - 1] = '\0';
      add_proc(comm + 1, pid, ppid);
    }
    else if (pid != parent) {
      comm[0] = '{';
      comm[strlen(comm) - 1] = '}';
      add_proc(comm, pid, parent);
    }
  }

  // find the thread
  sprintf(buf, "%s%s", pathname, PATH2);
  //printf("path = %s\n", buf);
  if (0 == (access(buf, F_OK)))
    read_dir(buf, pid, 1);
}

void add_proc(const char *comm, pid_t pid, pid_t ppid) {
  PROC *me;
  PROC *parent;

  me = find_proc(pid);
  if (me == NULL)
    me = new_proc(comm, pid);
  else
    strcpy(me->comm, comm);

  if (pid == ppid) {
    ppid = 0;
    //printf("pid = %d  ppid = %d\n", pid, ppid);
  }

  parent = find_proc(ppid);
  if (parent == NULL)
    parent = new_proc("", ppid);

  add_child(parent, me);
}

void add_child(PROC *parent, PROC *child) {
  CHILD *chi;
  CHILD **tmp;

  chi = malloc(sizeof(*chi));
  chi->child = child;

  //printf("parent pid = %d   child pid = %d\n", parent->pid, child->pid);

  for (tmp = &parent->children; *tmp != NULL; tmp = &(*tmp)->next) {
    //printf("in the for!!\n");
    if (proc_cmp((*tmp)->child, child) >= 0)
      break;
  }

  // insert into the children head
  chi->next = *tmp;
  *tmp = chi;

  child->parent = parent;

  //printf("%d\n", parent->children);
}

int proc_cmp(PROC *left, PROC *right) {
  // sort as pid
  if (global.numeric_sort) {
    if (left->pid < right->pid)
      return -1;
    if (left->pid > right->pid)
      return 1;
    return strcmp(left->comm, right->comm);
  }

  // sort as name
  int cmp = strcmp(left->comm, right->comm);
  if (0 == cmp) {
    if (left->pid < right->pid)
      return -1;
    if (left->pid > right->pid)
      return 1;
    return 0;
  }
  return cmp;
}

PROC *find_proc(pid_t pid) {
  PROC *tmp;

  for (tmp = global.list; tmp != NULL; tmp = tmp->next) {
    if (pid == tmp->pid)
      break;
  }

  return tmp;
}

PROC *new_proc(const char *comm, pid_t pid) {
  PROC *proc = (PROC *)malloc(sizeof(*proc));

  strcpy(proc->comm, comm);
  proc->pid = pid;
  proc->parent = NULL;
  proc->children = NULL;

  // insert the new proc into the head
  proc->next = global.list;
  global.list = proc;

  return proc;
}

int process_arg(int argc, char *argv[]) {
  int opt = 0;

  if (1 == argc) {
    opt = 1;
  }
  else if (2 == argc) {
    if (strcmp(argv[1], "-p") == 0 || strcmp(argv[1], "--show-pids") == 0) {
      opt = 2;
      global.show_pids = 1;
    }
    else if (strcmp(argv[1], "-n") == 0 || strcmp(argv[1], "--numeric-sort") == 0) {
      opt = 3;
      global.numeric_sort = 1;
    }
    else if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0) opt = 4;
    else opt = 5;
  }
  else opt = 6;

  return opt;
}

void init() {
  global.show_pids = 0;
  global.numeric_sort = 0;
  global.list = NULL;
  global.size = 0;
  memset(global.comm_len, 0, sizeof(global.comm_len));
  memset(global.is_last_child, 0, sizeof(global.is_last_child));
}

void show_usage() {
  printf("Usage: ./pstree [-p, --show-pids] [-n, --numeric-sort] [-V, --version]\n");
}

void show_version() {
  printf("Version: pstree 1.0\nCopyright (C) 2018 Violet Guo\n");
}
