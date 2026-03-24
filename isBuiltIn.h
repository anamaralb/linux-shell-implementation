int is_builtin(char **args);
void runBuiltin(char **args);
void runCommand(char **args);
void builtin_cd(char **args);
void builtin_exit(char **args);
void builtin_pwd(char **args);
void builtin_ifok(char **args);
void builtin_ifnot(char **args);