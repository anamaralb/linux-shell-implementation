#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"
#include "isBuiltIn.h"

int
is_builtin(char **args)
{
	if (args[0] == NULL) {
		return 0;
	}
	// Check for built-in commands
	if (strcmp(args[0], "cd") == 0 || strcmp(args[0], "exit") == 0 ||
	    strcmp(args[0], "pwd") == 0 || strcmp(args[0], "ifok") == 0 ||
	    strcmp(args[0], "ifnot") == 0 || strcmp(args[0], "jobs") == 0) {
		return 1;
	}

	return 0;
}

void
runBuiltin(char **args)
{
	if (args[0] == NULL) {
		return;
	}

	if (strcmp(args[0], "cd") == 0) {
		builtin_cd(args);

	} else if (strcmp(args[0], "exit") == 0) {
		builtin_exit(args);

	} else if (strcmp(args[0], "pwd") == 0) {
		builtin_pwd(args);

	} else if (strcmp(args[0], "ifok") == 0) {
		builtin_ifok(args);
	} else if (strcmp(args[0], "ifnot") == 0) {
		builtin_ifnot(args);
	} else if (strcmp(args[0], "jobs") == 0) {
		for (int i = 0; i < bgProcessCount; i++) {
			if (bgProcesses[i].active) {
				printf("[%d] %s\n", bgProcesses[i].pid,
				       bgProcesses[i].command);
			}
		}
	} else {
		fprintf(stderr, "%s: comando no reconocido\n", args[0]);
	}
}

void
runCommand(char **args)
{
	if (args == NULL || args[0] == NULL)
		return;

	cmdInfo cmd = { 0 };
	cmd.background = 0;
	cmd.input = 0;
	cmd.output = 1;

	if (is_builtin(args)) {
		runBuiltin(args);
	} else {
		execCommand(args[0], args, 0, &cmd);
	}
}

void
builtin_cd(char **args)
{
	if (args[1] == NULL) {
		char *home = getenv("HOME");

		if (home == NULL) {
			fprintf(stderr,
				"cd: variable HOME no está definida\n");
			return;
		}
		if (chdir(home) == -1) {
			perror("cd");
			return;
		}
	} else if (args[2] != NULL) {
		fprintf(stderr, "cd: demasiados argumentos\n");
		return;
	} else {
		if (chdir(args[1]) == -1) {
			perror("cd");
			return;
		}
	}
}

void
builtin_exit(char **args)
{
	int status = 0;

	if (args[1] != NULL) {
		status = atoi(args[1]);
	}

	exit(status);
}

void
builtin_pwd(char **args)
{
	char cwd[1024];

	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s\n", cwd);
	} else {
		perror("pwd");
	}
}

void
builtin_ifok(char **args)
{
	char *result = getenv("result");
	int status = result ? atoi(result) : -1;

	if (status == 0 && args[1] != NULL) {
		char *newArgs[MAXARGS];
		int i;

		for (i = 0; args[i + 1] != NULL && i < MAXARGS - 1; i++) {
			newArgs[i] = args[i + 1];
		}
		newArgs[i] = NULL;

		runCommand(newArgs);
	} else {
		fprintf(stderr, "ifok: condición no cumplida o sin comando\n");
	}
}

void
builtin_ifnot(char **args)
{
	char *result = getenv("result");
	int status = result ? atoi(result) : -1;

	if (status != 0 && args[1] != NULL) {
		char *newArgs[MAXARGS];
		int i;

		for (i = 0; args[i + 1] != NULL && i < MAXARGS - 1; i++) {
			newArgs[i] = args[i + 1];
		}
		newArgs[i] = NULL;

		runCommand(newArgs);
	} else {
		fprintf(stderr,
			"ifnot: condición no cumplida o sin comando\n");
	}
}
