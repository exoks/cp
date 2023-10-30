#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//====< defines >================================
#define PIPE	0
#define SEQ		1
#define CMD		2

//====< Structs >================================
typedef struct s_node
{
	int			type;
}				t_node;

typedef struct s_oper
{
	int			type;
	t_node		*left;
	t_node		*right;
}				t_oper;

typedef struct s_cmd
{
	int			type;
	char		*path;
	char		**args;
}				t_cmd;

typedef struct s_pipeline
{
	int			fd[2];
	int			offset;
}				t_pipeline;

//====< builtins >===============================
int		cd(char **args);

//====< parser >=================================
t_node	*parse_tree(char **av, int index);
t_node	*parse_cmd(char **av, int *index);

//====< interpreter >============================
void	exec_tree(t_node *tree, char **env);
int		exec_cmd(t_node	*cmd, t_pipeline *pline, char **env, int type);
void	update_pipeline(t_pipeline *pline);

//====< cleaner >================================
void	clean_tree(t_node *tree);

//====< main >===================================
int	main(int ac, char **av, char **env)
{
	t_node	*tree;

	if (ac == 1)
		return (EXIT_FAILURE);
	tree = parse_tree(av, 1);
	exec_tree(tree, env);
	clean_tree(tree);
	return (EXIT_SUCCESS);
}

//=========
t_node	*parse_tree(char **av, int index)
{
	t_node	*cmd;
	t_oper	*oper;

	cmd = parse_cmd(av, &index);
	if (cmd && av[index] && (*av[index] == '|' || *av[index] == ';'))
	{
		oper = malloc(sizeof(t_oper));
		if (oper == NULL)
			return (NULL);
		oper->type = (*av[index] == ';');
		oper->left = cmd;
		av[index++] = NULL;
		oper->right = parse_tree(av, index);
		return ((t_node *) oper);
	}
	return (cmd);
}

//=========
t_node	*parse_cmd(char **av, int *index)
{
	t_cmd	*cmd;

	cmd = malloc(sizeof(t_cmd));
	if (cmd == NULL)
		return (NULL);
	cmd->type = CMD;
	cmd->path = av[*index];
	cmd->args = &av[*index];
	while (av[*index] && *av[*index] != '|' && *av[*index] != ';')
		(*index)++;
	return ((t_node *) cmd);
}

//========
void	exec_tree(t_node *tree, char **env)
{
	t_pipeline	pline;

	pline.offset = -1;
	while (tree)
	{
		if (tree->type == CMD)
		{
			exec_cmd(tree, &pline, env, CMD);
			return ;
		}
		exec_cmd(((t_oper *) tree)->left, &pline, env, tree->type);
		update_pipeline(&pline);
		tree = ((t_oper *) tree)->right;
	}
}

//=======
int	exec_cmd(t_node *cmd, t_pipeline *pline, char **env, int type)
{
	t_cmd	*ecmd;
	pid_t	pid;

	ecmd = (t_cmd *) cmd;
	if (strcmp(ecmd->path, "cd") == 0)
		return (cd(ecmd->args));
	if (type == PIPE)
		pipe(pline->fd);
	pid = fork();
	if (pid < 0)
		return (perror("error"), 0);
	if (pid == 0)
	{
		dup2(pline->offset, 0);
		if (type == PIPE)
			dup2(pline->fd[1], 1);
		close(pline->offset);
		close(pline->fd[0]);
		close(pline->fd[1]);
		if (execve(ecmd->path, ecmd->args, env) == -1)
			return (perror("exec error"), 0);
	}
	if (type != PIPE)
		waitpid(pid, NULL, 0);
	return (0);
}

//======
void	update_pipeline(t_pipeline *pline)
{
	close(pline->offset);
	pline->offset = dup(pline->fd[0]);
	close(pline->fd[0]);
	close(pline->fd[1]);
}

//======
void	clean_tree(t_node *tree)
{
	t_node	*tmp;

	while (tree)
	{
		if (tree->type == CMD)
		{
			free(tree);
			return;
		}
		free(((t_oper *) tree)->left);
		tmp = tree;
		tree = ((t_oper *) tree)->right;
		free(tmp);
	}
}

//======
int	cd(char **args)
{
	int	index;

	index = 0;
	while (args && args[index])
		index++;
	if (index != 2)
		return (0);
	chdir(*(args + 1));
	return (1);
}
