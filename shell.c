#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "shell.h"
#include "isBuiltIn.h"

BackgroundProcess bgProcesses[MAX_BG_PROCESSES];
int bgProcessCount = 0;

void
showPrompt()
{
	if (!isatty(STDOUT_FILENO) || !isatty(STDIN_FILENO)) {
		return;
	}

	char *user = getenv("USER");
	char *currentPwd = getcwd(NULL, COMSIZE);

	if (strcmp(currentPwd, "/") == 0) {
		printf("%s@host:~$ ", user);
	} else {
		printf("%s@host:~%s$ ", user, currentPwd);
	}

	free(currentPwd);
	fflush(stdout);
}

char *
trim(char *str)
{
	while (isspace(*str)) {
		str++;

	}
	char *end = str + strlen(str) - 1;

	while (end > str && isspace((unsigned char)*end))
		end--;

	end[1] = '\0';

	return str;
}

void
replaceVars(char **args)
{
	for (int i = 0; args[i] != NULL; i++) {
		if (args[i][0] == '$') {
			char *var = args[i] + 1;
			char *value = getenv(var);

			if (value != NULL) {
				args[i] = value;
			} else {
				fprintf(stderr,
					"error: var %s does not exist\n", var);
				args[i] = "";
			}
		}
	}
}

void
addBackgroundProcess(pid_t pid, char *command)
{
	if (bgProcessCount < MAX_BG_PROCESSES) {
		bgProcesses[bgProcessCount].pid = pid;
		strncpy(bgProcesses[bgProcessCount].command, command,
			COMSIZE - 1);
		bgProcesses[bgProcessCount].command[COMSIZE - 1] = '\0';
		bgProcesses[bgProcessCount].active = 1;
		bgProcessCount++;
	} else {
		fprintf(stderr,
			"error: Reached maximum number of background processes\n");
	}
}

int
configExecv(char *program, char *args[])
{
	char fullPath[COMSIZE] = { 0 };
	char *paths[] = { "/bin/", "/usr/bin/", NULL };
	int i;

	if (strchr(program, '/') != NULL) {
		execv(program, args);
		perror("execv");
	} else {

		for (i = 0; paths[i] != NULL; i++) {
			memset(fullPath, 0, COMSIZE);
			strcpy(fullPath, paths[i]);
			strcat(fullPath, program);

			execv(fullPath, args);

		}
		fprintf(stderr, "Command not found: %s\n", program);
	}

	return -1;
}

pid_t
execCommand(char *program, char *args[], int background, cmdInfo *cmd)
{
	pid_t pid = fork();

	if (pid < 0) {
		perror("fork");
		return -1;
	} else if (pid == 0) {	// Child process
		// Handle input redirection
		if (cmd->input != 0) {
			dup2(cmd->input, STDIN_FILENO);
			close(cmd->input);
		}
		// Handle output redirection
		if (cmd->output != 1) {
			dup2(cmd->output, STDOUT_FILENO);
			close(cmd->output);
		}

		if (background) {
			int devnull = open("/dev/null", O_RDONLY);

			if (devnull != -1) {
				dup2(devnull, STDIN_FILENO);
				close(devnull);
			}
		}
		// Execute the command
		configExecv(program, args);
		exit(EXIT_FAILURE);
	} else {		// Parent process
		// Close file descriptors in the parent process
		if (cmd->input != 0) {
			close(cmd->input);
		}

		if (cmd->output != 1) {
			close(cmd->output);
		}

		if (!background) {
			int status;

			waitpid(pid, &status, 0);

			char status_str[16];

			sprintf(status_str, "%d", WEXITSTATUS(status));
			if (setenv("result", status_str, 1) == -1) {
				perror("setenv");
			}

		} else {
			printf("[%d] %s running in background\n", pid, program);
			// Registrar el proceso en background
			addBackgroundProcess(pid, program);
		}
	}
	return pid;
}

int
processHereDoc(char *commandLine)
{
	char *herePtr = strstr(commandLine, "HERE{");

	if (herePtr == NULL) {
		return 0;
	}

	*herePtr = '\0';

	int pipefd[2];

	if (pipe(pipefd) == -1) {
		perror("pipe");
		return -1;
	}

	char buffer[COMSIZE];

	while (fgets(buffer, COMSIZE, stdin) != NULL) {
		if (strcmp(buffer, "}\n") == 0 || strcmp(buffer, "}\r\n") == 0) {
			break;
		}
		write(pipefd[1], buffer, strlen(buffer));
	}

	close(pipefd[1]);	// Cerrar el extremo de escritura

	return pipefd[0];	// Devolver el descriptor de lectura
}

void
parseCommand(char *commandLine, char *args[], cmdInfo *cmd)
{
	char *token, *saveptr;
	int i = 0;

	token = strtok_r(commandLine, " \t", &saveptr);
	while (token != NULL && i < MAXARGS - 1) {
		if (strcmp(token, "&") == 0) {
			cmd->background = 1;

		} else if (strchr(token, '=') != NULL && token[0] != '=') {
			char *equal = strchr(token, '=');

			*equal = '\0';
			char *var = token;
			char *value = equal + 1;

			if (setenv(var, value, 1) == -1) {
				perror("setenv");
			}

		} else {
			args[i++] = token;
		}

		token = strtok_r(NULL, " \t", &saveptr);
	}

	args[i] = NULL;

	replaceVars(args);

	if (is_builtin(args)) {
		runBuiltin(args);
	} else {
		execCommand(args[0], args, cmd->background, cmd);
	}
}

char *
redir(char *commandLine, cmdInfo *cmd)
{
	char *firstredir = NULL;
	char *ptr = commandLine + strlen(commandLine);

	while (ptr != commandLine) {
		if (*ptr == '>') {
			firstredir = ptr;
			char *outfile = ptr + 1;

			outfile = trim(outfile);
			if ((cmd->output =
			     open(outfile, O_WRONLY | O_CREAT | O_TRUNC,
				  0666)) == -1) {
				fprintf(stderr, "Error opening file: %s\n",
					strerror(errno));
			}
			*firstredir = '\0';
		}
		if (*ptr == '<') {
			firstredir = ptr;
			char *infile = ptr + 1;

			infile = trim(infile);
			if ((cmd->input = open(infile, O_RDONLY)) == -1) {
				fprintf(stderr, "Error opening file: %s\n",
					strerror(errno));
			}
			*firstredir = '\0';
		}
		ptr--;
	}
	return commandLine;
}

cmdInfo *
buildCmd(int pipes, int input, int output, int background)
{
	cmdInfo *cmd = (cmdInfo *)malloc(sizeof(cmdInfo));

	if (cmd == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	cmd->pipes = pipes;
	cmd->input = input;
	cmd->output = output;
	cmd->background = background;

	for (int i = 0; i < MAXARGS; i++) {
		cmd->commands[i] = NULL;
	}

	return cmd;
}

void
processCommand(char *commandLine)
{
	char *args[MAXARGS] = { NULL };
	memset(args, 0, MAXARGS * sizeof(char *));	// Initialize to NULL

	commandLine[strlen(commandLine) - 1] = '\0';

	cmdInfo *cmd = buildCmd(0, 0, 1, 0);

	char *processed_cmd = malloc(strlen(commandLine) + 1);

	if (!processed_cmd) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	strcpy(processed_cmd, commandLine);

	int herefd = processHereDoc(processed_cmd);

	if (herefd > 0) {
		cmd->input = herefd;
	}

	processed_cmd = redir(processed_cmd, cmd);

	parseCommand(processed_cmd, args, cmd);

	free(processed_cmd);
	free(cmd);
}

void
updateBackgroundProcesses()
{
	for (int i = 0; i < bgProcessCount;) {
		if (bgProcesses[i].active) {
			int status;
			pid_t result =
			    waitpid(bgProcesses[i].pid, &status, WNOHANG);
			if (result == bgProcesses[i].pid) {
				for (int j = i; j < bgProcessCount - 1; j++) {
					bgProcesses[j] = bgProcesses[j + 1];
				}
				bgProcessCount--;
				continue;
			}
		}
		i++;
	}

	// Mostrar contenido del array bgProcesses
	// for (int i = 0; i < bgProcessCount; i++) {
	//     printf("[%d] PID: %d, CMD: %s, ACTIVO: %d\n",
	//         i, bgProcesses[i].pid,
	//         bgProcesses[i].command,
	//         bgProcesses[i].active);
	// }
}

int
main(int argc, char *argv[])
{
	char *commandLine = (char *)malloc(sizeof(char) * COMSIZE);

	if (commandLine == NULL) {
		fprintf(stderr, "Error allocating memory for command line\n");
		exit(EXIT_FAILURE);
	}

	setenv("result", "0", 1);	// Inicializar la variable de entorno result

	showPrompt();
	while (fgets(commandLine, COMSIZE, stdin) != NULL) {
		processCommand(commandLine);
		updateBackgroundProcesses();
		showPrompt();
	}

	printf("\n");
	free(commandLine);
	return 0;
}
