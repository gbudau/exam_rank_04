/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   microshell.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gbudau <gbudau@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2021/02/07 15:56:10 by gbudau            #+#    #+#             */
/*   Updated: 2021/02/11 20:35:29 by gbudau           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

typedef struct	s_cmd
{
	char	**argv;
	int		ispipe;
	int		currpipe[2];
	int		lastpipe[2];
}				t_cmd;

int		ft_strlen(char *str)
{
	char	*end;

	end = str;
	while (*end)
		end++;
	return (end - str);
}

void	ft_putstr_err(char *str)
{
	if (write(STDERR_FD, str, ft_strlen(str)) == -1)
		exit(EXIT_FAILURE);
}

void	print_error(char *error, char *arg)
{
	ft_putstr_err(error);
	if (arg)
		ft_putstr_err(arg);
	ft_putstr_err("\n");
}

void	error_fatal(void)
{
	print_error("error: fatal", NULL);
	exit(EXIT_FAILURE);
}

int		is_semicolon(char *str)
{
	return (!strcmp(str, ";"));
}

int		is_pipe(char *str)
{
	return (!strcmp(str, "|"));
}

int		next_pipe(char **argv, int i)
{
	while (argv[i] && !is_pipe(argv[i]))
		i++;
	return (i);
}

int		next_semicolon(char **argv, int i, int *ispipeline)
{
	while (argv[i] && !is_semicolon(argv[i]))
	{
		if (is_pipe(argv[i]))
			*ispipeline = 1;
		i++;
	}
	return (i);
}

void	cd_builtin(int argc, char **argv, int i)
{
	if (argc != 2)
		print_error("error: cd: bad arguments", NULL);
	else if (chdir(argv[i + 1]) == -1)
		print_error("error: cd: cannot change directory to ", argv[i + 1]);
}

void	execute_simple_cmd(char **argv, int i, char **envp)
{
	pid_t	pid;

	pid = fork();
	if (pid == -1)
		error_fatal();
	if (pid == 0)
	{
		execve(argv[i], argv + i, envp);
		print_error("error: cannot execute ", argv[i]); 
		exit(EXIT_FAILURE);
	}
	waitpid(pid, NULL, 0);
}

void	dup2_close(int pipefds[2], int fd)
{
	if (fd != -1)
		if (dup2(pipefds[fd], fd) == -1)
			error_fatal();
	if (close(pipefds[STDIN_FD]) == -1)
		error_fatal();
	if (close(pipefds[STDOUT_FD]) == -1)
		error_fatal();
}

void	execute_pipeline(char **argv, int i, char **envp)
{
	t_cmd	cmd;
	int		havepipe;
	int		next;
	pid_t	pid;

	havepipe = 0;
	do
	{
		next = next_pipe(argv, i);
		cmd.ispipe = argv[next] && is_pipe(argv[next]);
		argv[next] = NULL;
		cmd.argv = argv + i;
		if (cmd.ispipe)
			if (pipe(cmd.currpipe) == -1)
				error_fatal();
		pid = fork();
		if (pid == -1)
			error_fatal();
		if (pid == 0)
		{
			if (havepipe)
				dup2_close(cmd.lastpipe, STDIN_FD);
			if (cmd.ispipe)
				dup2_close(cmd.currpipe, STDOUT_FD);
			execve(cmd.argv[0], cmd.argv, envp);
			print_error("error: cannot execute ", cmd.argv[0]); 
			exit(EXIT_FAILURE);
		}
		if (havepipe)
			dup2_close(cmd.lastpipe, -1);
		havepipe = cmd.ispipe;
		if (cmd.ispipe)
		{
			cmd.lastpipe[STDIN_FD] = cmd.currpipe[STDIN_FD];
			cmd.lastpipe[STDOUT_FD] = cmd.currpipe[STDOUT_FD];
		}
		i = next + 1;
	}
	while (havepipe);
	while (waitpid(-1, NULL, 0) > 0)
		;
}

int		skip_semicolons(char **argv, int i)
{
	while (argv[i] && is_semicolon(argv[i]))
		i++;
	return (i);
}

int		main(int argc, char **argv, char **envp)
{
	int		i;
	int		next;
	int		ispipeline;

	i = 1;
	while (i < argc)
	{
		i = skip_semicolons(argv, i);
		if (!argv[i])
			return (0);
		ispipeline = 0;
		next = next_semicolon(argv, i, &ispipeline);
		argv[next] = NULL;
		if (!strcmp(argv[i], "cd"))
			cd_builtin(next - i, argv, i);
		else if (ispipeline)
			execute_pipeline(argv, i, envp);
		else
			execute_simple_cmd(argv, i, envp);
		i = next + 1;
	}
	return (0);
}
