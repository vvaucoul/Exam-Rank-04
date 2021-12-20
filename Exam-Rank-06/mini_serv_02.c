#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct s_client
{
	int			fd;
	int			id;
	struct s_client *next;
} t_client;

t_client *clients = NULL;
int sock_fd = 0, g_id = 0;
fd_set current_socket, sock_read, sock_write;
char msg[42];
char str[42 * 4096], tmp [42 * 4096], buf[42 * 4096 + 42];

void	exit_fatal()
{
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close (sock_fd);
	exit(1);
}

int get_id(int fd)
{
	t_client *tmp = clients;

	while (tmp)
	{
		if (tmp->fd == fd)
			return (tmp->id);
		tmp = tmp->next;
	}
	return (-1);
}

int get_max_fd()
{
	int max = sock_fd;
	t_client *tmp = clients;

	while (tmp)
	{
		if (tmp->fd > max)
			max = tmp->fd;
		tmp = tmp->next;
	}
	return (max);
}

void send_all(int fd, char *str_to_send)
{
	t_client *tmp = clients;

	while (tmp)
	{
		if (tmp->fd != fd && FD_ISSET(tmp->fd, &sock_write))
		{
			if ((send(tmp->fd, str_to_send, strlen(str_to_send), 0) < 0))
				exit_fatal();
		}
		tmp = tmp->next;
	}
}

int add_client_to_list(int fd)
{
	t_client *tmp = clients;
	t_client *new = NULL;

	if (!(new = calloc(1, sizeof(t_client))))
		exit_fatal();

	new->id = g_id++;
	new->fd = fd;
	new->next = NULL;

	if (!clients)
		clients = new;
	else
	{
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = new;
	}
	return (new->id);
}

void add_client()
{
	struct sockaddr clientaddr;
	socklen_t len = sizeof(clientaddr);
	int client_fd;

	if ((client_fd = accept(sock_fd, (struct sockaddr *) &clientaddr, &len)) < 0)
		exit_fatal();
	bzero(&msg, sizeof(msg));
	sprintf(msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
	send_all(client_fd, msg);
	FD_SET(client_fd, &current_socket);
}

int remove_client(int fd)
{
	t_client *tmp = clients;
	t_client *to_delete = NULL;
	int id = get_id(fd);

	if (tmp && tmp->fd == fd)
	{
		clients = tmp->next;
		free(tmp);
	}
	else
	{
		while (tmp && tmp->next && tmp->next->fd != fd)
			tmp = tmp->next;
		to_delete = tmp->next;
		tmp->next = tmp->next->next;
		free(to_delete);
	}
	return (id);
}

void ex_msg(int fd)
{
	int i = 0;
	int j = 0;

	while (str[i])
	{
		tmp[j] = str[i];
		++j;
		if (str[i] == '\n')
		{
			sprintf(buf, "client %d: %s", get_id(fd), tmp);
			send_all(fd, tmp);
			j = 0;
			bzero(&tmp, strlen(tmp));
			bzero(&buf, strlen(buf));
		}
		++i;
	}
	bzero(&str, strlen(str));
}

void loop_server()
{
	while (1)
	{
		sock_write = sock_read = current_socket;
		if ((select(get_max_fd() + 1, &sock_read, &sock_write, NULL, NULL)) < 0)
			continue;
		for (int fd = 0; fd <= get_max_fd(); fd++) {
			if (FD_ISSET(fd, &sock_read))
			{
				if (fd == sock_fd)
				{
					bzero(&msg, sizeof(msg));
					add_client();
					break;
				}
				else
				{
					int recv_ret = 1000;

					while (recv_ret == 1000 || str[strlen(str) - 1] != '\n')
					{
						recv_ret = recv(fd, str + strlen(str), 1000, 0 );
						if (recv_ret <= 0)
							break;
					}
					if (recv_ret <= 0)
					{
						bzero(&msg, sizeof(msg));
						sprintf(msg, "server: client %d just left\n", remove_client(fd));
						send_all(fd, msg);
						FD_CLR(fd, &current_socket);
						close(fd);
						break;
					}
					else
						ex_msg(fd);
				}
			}
		}
	}
}

void init_values()
{
	FD_ZERO(&current_socket);
	FD_SET(sock_fd, &current_socket);
	bzero(&tmp, sizeof(tmp));
	bzero(&buf, sizeof(buf));
	bzero(&str, sizeof(str));
}

void init_server(uint16_t port)
{
	struct sockaddr_in servaddr;

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit_fatal();
	if (bind(sock_fd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
		exit_fatal();
	if (listen(sock_fd, 0) < 0)
		exit_fatal();
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}
	init_server(atoi(argv[1]));
	init_values();
	loop_server();
	return (0);
}
