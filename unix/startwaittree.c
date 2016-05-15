#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

static const char *program_name = "startwaittree";

static bool str_equal(const char *first, const char *second)
{
  if (first == NULL)
  {
    return (second == NULL);
  }
  if (second == NULL)
  {
    return false;
  }
  return (strcmp(first, second) == 0);
}

static noreturn void explode(const char *func, const char *format, ...)
{
  va_list args;

  fprintf(stderr, "%s: %s: ", program_name, func);

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  fprintf(stderr, ": [%d] %s\n", errno, strerror(errno));

  exit(1);
}

int main(int argc, char **argv)
{
  if (argc > 0 && argv[0] != NULL)
  {
    program_name = argv[0];
  }

  if (argc < 2 || argv[1] == NULL || str_equal(argv[1], "-h") ||
      str_equal(argv[1], "--help"))
  {
    fprintf(stderr, "Usage: %s PROGRAM [ARGUMENT...]\n", program_name);
    return 0;
  }

  // open pipe
  int pipe_fds[2];
  if (pipe(pipe_fds) == -1)
  {
    explode("pipe", "failed to create pipe");
  }

  // fork
  pid_t pid = fork();
  if (pid == -1)
  {
    explode("fork", "failed to fork process");
  }
  else if (pid == 0)
  {
    // child

    // close the read end of the pipe
    if (close(pipe_fds[0]) == -1)
    {
      explode("close", "failed to close read end of pipe");
    }

    // copy the arguments
    size_t arg_count = argc - 1;
    char **args = malloc((arg_count + 1) * sizeof(*args));
    if (args == NULL)
    {
      explode("malloc", "failed to allocate argument array");
    }

    for (size_t i = 0; i < arg_count; ++i)
    {
      args[i] = argv[i + 1];
    }
    args[arg_count] = NULL;

    // run
    execvp(args[0], args);

    // if we reached this point, execvp failed
    explode("execvp", "failed to execute %s", args[0]);
  }

  // parent

  // close the write end of the pipe
  if (close(pipe_fds[1]) == -1)
  {
    explode("close", "failed to close write end of pipe");
  }

  // read from the pipe
  char c;
  ssize_t read_bytes = read(pipe_fds[0], &c, sizeof(c));
  if (read_bytes == -1)
  {
    explode("read", "failed to read from pipe");
  }

  // at this point, all write ends have been closed

  // assume EOF
  if (close(pipe_fds[0]) == -1)
  {
    explode("close", "failed to close read end of pipe");
  }

  return 0;
}
