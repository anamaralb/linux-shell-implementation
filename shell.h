enum {
	COMSIZE = 1024,
	MAXARGS = 20,
	MAX_BG_PROCESSES = 100
};

struct BackgroundProcess {
	pid_t pid;
	char command[COMSIZE];
	int active;		// 1 si está activo, 0 si ha terminado
};
typedef struct BackgroundProcess BackgroundProcess;

// Command info structure
struct cmdInfo {
	int pipes;
	int input;
	int output;
	int background;
	char *commands[MAXARGS];
};
typedef struct cmdInfo cmdInfo;

// Array para almacenar los procesos en background
extern BackgroundProcess bgProcesses[MAX_BG_PROCESSES];
extern int bgProcessCount;

void parseCommand(char *commandLine, char *args[], cmdInfo *cmd);
void replaceVars(char *args[]);
void showPrompt(void);
void addBackgroundProcess(pid_t pid, char *command);
pid_t execCommand(char *program, char *args[], int background, cmdInfo *cmd);
void processCommand(char *commandLine);
char *trim(char *str);
int processHereDoc(char *commandLine);
char *redir(char *commandLine, cmdInfo *cmd);
int configExecv(char *program, char *args[]);
