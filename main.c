#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#define MAX_LINE 1024
#define MAX_ARGS 100
#define MAX_BG 100
#define HISTORY_SIZE 20

typedef struct {
  pid_t pid;
  char name[256];
  char start_time[64];
  int active;
} BackgroundProcess;
BackgroundProcess bg_processes[MAX_BG];
char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;
char history_file[PATH_MAX];
void save_history();
void print_history(int sig) {
  write(STDOUT_FILENO, "\nCommand history:\n", 18);

  for (int i = 0; i < history_count; i++) {
    char buffer[MAX_LINE + 30];
    int len = snprintf(buffer, sizeof(buffer), "%d: %s\n", i + 1, history[i]);
    write(STDOUT_FILENO, buffer, len);
  }
}

void load_history() {
  FILE *file = fopen(history_file, "r");
  if (file == NULL)
    return;
  char line[MAX_LINE];
  while (fgets(line, sizeof(line), file) != NULL &&
         history_count < HISTORY_SIZE) {
    line[strcspn(line, "\n")] = '\0';
    strncpy(history[history_count], line, MAX_LINE - 1);
    history_count++;
  }
  fclose(file);
}

void save_history() {
  FILE *file = fopen(history_file, "w");
  if (file == NULL)
    return;
  for (int i = 0; i < history_count; i++) {
    fprintf(file, "%s\n", history[i]);
  }
  fclose(file);
}

void add_history(const char *cmd) {
  if (history_count < HISTORY_SIZE) {
    strncpy(history[history_count], cmd, MAX_LINE - 1);
    history[history_count][MAX_LINE - 1] = '\0';
    history_count++;
  } else {
    for (int i = 1; i < HISTORY_SIZE; i++) {
      strcpy(history[i - 1], history[i]);
    }
    strncpy(history[HISTORY_SIZE - 1], cmd, MAX_LINE - 1);
    history[HISTORY_SIZE - 1][MAX_LINE - 1] = '\0';
  }
  save_history();
}

void parse_command(char *line, char **args, int *background) {
  int i = 0;
  *background = 0;
  char *token = strtok(line, " ");
  while (token != NULL && i < MAX_ARGS - 1) {
    args[i] = token;
    i++;
    token = strtok(NULL, " ");
  }
  args[i] = NULL;
  if (i > 0 && strcmp(args[i - 1], "&") == 0) {
    *background = 1;
    args[i - 1] = NULL;
    i--;
  }
  printf("\n--- Debug: Parsed Arguments ---\n");
  for (int j = 0; j < i; j++) {
    printf("args[%d] = \"%s\"\n", j, args[j]);
  }
  printf("args[%d] = NULL\n", i);
  printf("-------------------------------\n\n");
}

void add_background_process(pid_t pid, const char *name) {
  for (int i = 0; i < MAX_BG; i++) {
    if (!bg_processes[i].active) {
      bg_processes[i].pid = pid;
      strncpy(bg_processes[i].name, name, sizeof(bg_processes[i].name) - 1);
      bg_processes[i].name[sizeof(bg_processes[i].name) - 1] = '\0';
      time_t now = time(NULL);
      struct tm *tm_info = localtime(&now);
      strftime(bg_processes[i].start_time, sizeof(bg_processes[i].start_time),
               "%Y-%m-%d %H:%M:%S", tm_info);
      bg_processes[i].active = 1;
      return;
    }
  }
  printf("Background process list is full.\n");
}

void check_background_processes() {
  for (int i = 0; i < MAX_BG; i++) {
    if (bg_processes[i].active) {
      int status;
      pid_t result = waitpid(bg_processes[i].pid, &status, WNOHANG);
      if (result > 0) {
        printf("[background finished] PID: %d Name: %s\n", bg_processes[i].pid,
               bg_processes[i].name);

        bg_processes[i].active = 0;
      }
    }
  }
}

void list_background_processes() {
  int found = 0;
  printf("Background processes:\n");
  for (int i = 0; i < MAX_BG; i++) {
    if (bg_processes[i].active) {
      printf("PID: %d | Name: %s | Start: %s\n", bg_processes[i].pid,
             bg_processes[i].name, bg_processes[i].start_time);
      found = 1;
    }
  }
  if (!found) {
    printf("No background processes.\n");
  }
}

int main() {
  char line[MAX_LINE];
  char line_copy[MAX_LINE];
  char *args[MAX_ARGS];
  int background;
  openlog("my_shell", LOG_PID | LOG_CONS, LOG_USER);
  syslog(LOG_INFO, "Shell started");
  signal(SIGQUIT, print_history);
  char *home = getenv("HOME");
  if (home == NULL) {
    fprintf(stderr, "HOME environment variable not found\n");
    return 1;
  }

  snprintf(history_file, sizeof(history_file), "%s/.my_shell_history", home);
  load_history();

  while (1) {
    check_background_processes();
    printf("my_shell> ");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL) {
      break;
    }
    line[strcspn(line, "\n")] = '\0';
    if (strlen(line) == 0) {
      continue;
    }

    add_history(line);
    if (strcmp(line, "bglist") == 0 || strcmp(line, "jobs") == 0) {
      list_background_processes();
      continue;
    }

    if (strcmp(line, "history") == 0) {
      for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i]);
      }

      continue;
    }

    strcpy(line_copy, line);
    parse_command(line_copy, args, &background);
    if (args[0] == NULL) {
      continue;
    }

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork failed");
      continue;
    }

    if (pid == 0) {
      execvp(args[0], args);
      perror("execvp failed");
      exit(1);
    } else {
      if (background) {
        add_background_process(pid, args[0]);
        printf("[background] PID: %d started\n", pid);
      } else {
        waitpid(pid, NULL, 0);
      }
    }
  }

  save_history();
  syslog(LOG_INFO, "Shell stopped");
  closelog();
  printf("\nShell closed.\n");
  return 0;
}