
// CS39006 COMPUTER NETWORKS LABORATORY
// Assignment 3 : NETWORK PROGRAMMING WITH FILE TRANSFER PROTOCOL
// Nakul Aggarwal 19CS10044
// Hritaban Ghosh 19CS30053

/*  THE CLIENT PROCESS */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <limits.h>
#define __default_cmd_len__ 15
#define __maxline__ 1024

void get_filename(char *path, char *name)
{
	int i = strlen(path) - 1;
	while (i >= 0 && path[i] != '/') i--;
	strcpy(name, path + i + 1);
}

int sendinchunks(int sockfd, char *command, const int size, int flag)
{
	int len = size;
	while (len)
	{
		int bs = send(sockfd, command, len, flag);
		if (bs < 0) return -1;
		len -= bs;
		command += bs;
	}
	return 0;
}

int read_command(char **buffer)
{
	int size = __default_cmd_len__;
	memset((*buffer), 0, sizeof((*buffer)));
	int i = 0;
	char c;
	int charseen = 0;
	while ((c = getchar()) != '\n' && c != EOF)
	{
		if (!charseen && c == ' ') continue;
		charseen = 1;
		if (i == size - 1)
		{
			size += 10;
			(*buffer) = (char*) realloc((*buffer), size* sizeof(char));
		}
		(*buffer)[i++] = c;
		(*buffer)[i] = '\0';
	}
	return i;
}

int validate_number(const char *str)
{
	int len = strlen(str);
	for (int i = 0; i < len; i++)
		if (str[i] > '9' || str[i]<'0') return 0;
	return 1;
}

int status_dictionary(char *status_code_string)
{
	if (validate_number(status_code_string))
	{
		uint32_t status_code = atoi(status_code_string);
		printf(" [ STATUS CODE %u : ", status_code);
		switch (status_code)
		{
			case 600:
				printf("AUTHORIZATION ERROR. Server has rejected the command. ]\n");
				break;
			case 200:
				printf("SUCCESS. Command executed successfully. ]\n");
				break;
			case 500:
				printf("FAILURE. Error executing command. ]\n");
				break;
			default:
				break;
		}
		return 1;
	}
	else
	{
		printf(" [ STATUS CODE %s : Invalid code. ]\n", status_code_string);
		return 0;
	}
}

int validate_ip(const char *ip)
{
	if (strlen(ip) > 100) return 0;
	char *ipcpy = (char*) malloc(101* sizeof(char));
	strcpy(ipcpy, ip);

	int i, num, dots = 0;
	char *ptr;
	if (!ipcpy) return 0;

	ptr = strtok(ipcpy, ".");
	if (ptr == NULL)
	{
		free(ipcpy);
		return 0;
	}

	while (ptr)
	{
		if (!validate_number(ptr))
		{
			free(ipcpy);
			return 0;
		}

		num = atoi(ptr);
		if (num >= 0 && num <= 255)
		{
			ptr = strtok(NULL, ".");
			if (ptr != NULL)
				dots++;
		}
		else
		{
			free(ipcpy);
			return 0;
		}
	}
	free(ipcpy);
	if (dots != 3) return 0;
	return 1;
}

void get_command_name(char *command_name, const char *command)
{
	int ln = 7;
	int i = 0;
	int l = strlen(command);
	while (i < ln - 1 && i < l && command[i] != ' ')
	{
		command_name[i] = command[i];
		i++;
	}
	command_name[i] = 0;
	return;
}

int get_command_array(char **command_array, const int n, const char *command, const char *delim)
{
	int init_size = strlen(command);
	char *copy_of_command = (char*) calloc(init_size + 1, sizeof(char));

	memset(copy_of_command, 0, sizeof(copy_of_command));
	strcpy(copy_of_command, command);

	int i = 0;
	char *ptr = strtok(copy_of_command, delim);

	if (ptr == NULL)
	{
		free(copy_of_command);
		return 0;
	}

	while (i < n && ptr != NULL)
	{
		int len = strlen(ptr);
		command_array[i] = (char*) realloc(command_array[i], len + 1);
		memset(command_array[i], 0, sizeof(command_array[i]));
		strcpy(command_array[i], ptr);

		if (command_array[i][len - 1] == ',')
			command_array[i][len - 1] = 0;

		ptr = strtok(NULL, delim);
		i++;
	}

	free(copy_of_command);
	return 1;
}

int trimwhitespace(char *str, int len)
{
	if (len == 0) return 0;
	int i = len - 1;
	while (i >= 0 && str[i] == ' ') i--;
	str[i + 1] = 0;
	return i + 1;
}

int receive_ndata(const int sockfd, char *buffer, int n)
{
	int number_of_bytes_received = 0;
	while (n > 0)
	{
		int br = recv(sockfd, buffer, n, 0);
		if (br < 0) return -1;
		number_of_bytes_received += br;
		buffer += number_of_bytes_received;
		n -= number_of_bytes_received;
	}
	return 0;
}

int send_file(const int sockfd, int fd)
{
	char block_check_header = 'M';
	int READ_MAX = 100;
	uint16_t number_of_data_bytes_to_send = 0;
	char buffer[1 + 2 + 65536];

	do {
		memset(buffer, 0, sizeof(buffer));

		number_of_data_bytes_to_send = read(fd, buffer + 3, READ_MAX);
		if (number_of_data_bytes_to_send < 0)
		{
			perror(" [ ERROR : Error while reading from file. ]\n");
			return 0;
		}

		if (number_of_data_bytes_to_send < READ_MAX)
			block_check_header = 'L';
		else block_check_header = 'M';

		memcpy(buffer + 0, &block_check_header, 1);
		memcpy(buffer + 1, &number_of_data_bytes_to_send, 2);

		if (sendinchunks(sockfd, buffer, number_of_data_bytes_to_send + 3, 0) < 0)
		{
			return 0;
		}
	} while (block_check_header != 'L');

	return 1;
}

int receive_file(const int sockfd, int fd)
{
	while (1)
	{
		char block_check_header;
		uint16_t number_of_data_bytes_to_receive = 0;
		uint16_t number_of_data_bytes_written = 0;
		char buffer[1 + 2 + 65536];

		memset(buffer, 0, sizeof(buffer));
		if (receive_ndata(sockfd, buffer + 0, 1) < 0) return 0;

		memcpy(&block_check_header, buffer + 0, 1);
		if (receive_ndata(sockfd, buffer + 1, 2) < 0) return 0;

		memcpy(&number_of_data_bytes_to_receive, buffer + 1, 2);
		if (receive_ndata(sockfd, buffer + 3, number_of_data_bytes_to_receive) < 0) return 0;

		number_of_data_bytes_written = write(fd, buffer + 3, number_of_data_bytes_to_receive);
		if (number_of_data_bytes_written != number_of_data_bytes_to_receive)
		{
			perror(" [ ERROR : Error while writing to file. ]\n");
			return 0;
		}

		if (block_check_header == 'M') continue;
		else if (block_check_header == 'L') break;
		else
		{
			printf(" [ ERROR : Some error ocurred while file transfer. ]\n");
			return 0;
		}
	}
	return 1;
}

int receive_status_code(const int sockfd, char *buf)
{
	char *temp = buf;
	memset(buf, 0, sizeof(buf));
	int number_of_bytes_received = 0, n = 4;
	while (n > 0)
	{
		int br = recv(sockfd, buf, n, 0);
		if (br < 0) return -1;

		number_of_bytes_received += br;
		buf += number_of_bytes_received;
		n -= number_of_bytes_received;
	}
	return status_dictionary(temp);
}

int receive_upto_delimiter(const int sockfd, char **buf, const char delim)
{
	memset(*buf, 0, sizeof(*buf));
	int size = __default_cmd_len__;
	int i = 0;
	char c;
	while (1)
	{
		if (recv(sockfd, &c, 1, 0) == 1)
		{
			if (i == size - 1)
			{
				size += 10;
				(*buf) = (char*) realloc((*buf), size* sizeof(char));
			}
			(*buf)[i++] = c;
			(*buf)[i] = '\0';
			if (c == delim) break;
		}
	}
	return i;
}

void printcmdprompt(int conn)
{
	if (!conn)
	{
		printf("\033[1m\033[31m");
		printf("\n[myftp]$[CONNECT]> ");
		printf("\033[0m");
		return;
	}
	printf("\033[1m\033[36m");
	printf("\n[myftp]> ");
	printf("\033[0m");
}

int main()
{
	int sockfd;
	struct sockaddr_in serv_addr;
	char buf[__maxline__];
	char *command;

	while (1)
	{
		printcmdprompt(0);

		command = (char*) malloc(__default_cmd_len__* sizeof(char));
		int bytesread = read_command(&command);
		if (bytesread == 0)
		{
			printf(" [ ERROR : First command should be of the form \"open IP_address port\". ]\n");
			free(command);
			continue;
		}

		trimwhitespace(command, strlen(command));

		int init_size = strlen(command), i = 0, err_flag = 0;
		char delim[] = " ";
		char *command_array[3];
		char *ptr = strtok(command, delim);

		while (ptr != NULL)
		{
			if (i >= 3)
			{
				printf(" [ ERROR : First command should be of the form \"open IP_address port\". \n           You have entered too many arguments. ]\n");
				err_flag = 1;
				break;
			}
			command_array[i] = ptr;
			i++;
			ptr = strtok(NULL, delim);
		}

		if (i < 3)
		{
			printf(" [ ERROR : First command should be of the form \"open IP_address port\". \n           You have entered too few arguments. ]\n");
			err_flag = 1;
		}
		else if (strcmp(command_array[0], "open"))
		{
			printf(" [ ERROR : First command should be of the form \"open IP_address port\". ]\n");
			err_flag = 1;
		}
		else if (!validate_ip(command_array[1]))
		{
			printf(" [ ERROR : First command should be of the form \"open IP_address port\".\n           IP_Address specified is not valid. ]\n");
			err_flag = 1;
		}
		else if (!validate_number(command_array[2]))
		{
			printf(" [ ERROR : First command should be of the form \"open IP_address port\".\n           Port specified is not valid. ]\n");
			err_flag = 1;
		}
		else if (atoi(command_array[2]) < 20000 || atoi(command_array[2]) > 65535)
		{
			printf(" [ ERROR : First command should be of the form \"open IP_address port\".\n           Port should be in between 20000 and 65535. ]\n");
			err_flag = 1;
		}

		if (err_flag == 1)
		{
			free(command);
			continue;
		}
		else
		{
			char *IP_Address = command_array[1];
			uint16_t port = atoi(command_array[2]);

			if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			{
				perror(" [ ERROR : Unable to create socket. ]\n\n");
				free(command);
				exit(0);
			}

			serv_addr.sin_family = AF_INET;
			inet_aton(IP_Address, &serv_addr.sin_addr);
			serv_addr.sin_port = htons(port);

			if ((connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0)
			{
				perror(" [ ERROR : Unable to connect to server. ]\n\n");
				free(command);
				exit(0);
			}

			free(command);
			printf(" [ SUCCESS : Connection opened successfully. ]\n") ;
			break;
		}
	}

	int username_flag = 0, password_flag = 0;
	while (1)
	{
		printcmdprompt(1);

		command = (char*) malloc(__default_cmd_len__* sizeof(char));
		int bytesread = read_command(&command);
		if (bytesread == 0)
		{
			free(command);
			continue;
		}
		trimwhitespace(command, strlen(command));

		char command_name[7];
		get_command_name(command_name, command);
		
		if (!strcmp(command_name, "open")) {
			printf(" [ INFO : Server-client connection already established. ]\n");
		}
		else if (!strcmp(command_name, "quit"))
		{
			close(sockfd);
			free(command);
			printf(" [ CLOSING CONNECTION ... ]\n\n");
			exit(0);
		}
		else if (!strcmp(command_name, "user"))
		{
			if (username_flag && password_flag) {
				printf(" [ INFO : Client has already logged in. ]\n");
			}
			else {
				if (sendinchunks(sockfd, command, strlen(command) + 1, 0) < 0)
				{
					free(command);
					continue;
				}
				if (receive_status_code(sockfd, buf) >= 0 && atoi(buf) == 200)
				{
					printf(" [ USERNAME VERIFIED ]\n");
					username_flag = 1;
				}
			}
		}
		else if (!strcmp(command_name, "pass"))
		{
			if (username_flag && password_flag) {
				printf(" [ INFO : Client has already authenticated. ]\n");
			}
			else {
				if (sendinchunks(sockfd, command, strlen(command) + 1, 0) < 0)
				{
					free(command);
					continue;
				}
				if (receive_status_code(sockfd, buf) >= 0 && atoi(buf) == 200)
				{
					printf(" [ PASSWORD VERIFIED ]\n");
					password_flag = 1;
				}
			}
		}
		else if (username_flag && password_flag)
		{
			if (!strcmp(command_name, "lcd"))
			{
				char *command_array[2];
				for (int i = 0; i < 2; i++)
					command_array[i] = (char*) malloc(1* sizeof(char));
				get_command_array(command_array, 2, command, " ");

				if (chdir(command_array[1]) < 0)
				{
					perror(" [ ERROR : Client-side directory could not be changed. ]\n");
				}
				else
				{
					char cwd[PATH_MAX];
					if (getcwd(cwd, sizeof(cwd)) != NULL)
					{
						printf(" [ SUCCESS. Command executed successfully. ]\n");
						printf(" [ Current working directory : %s ]\n", cwd);
					}
					else
					{
						perror(" [ ERROR : getcwd() error ]\n");
					}
				}
				free(command_array[0]);
				free(command_array[1]);
			}
			else if (!strcmp(command_name, "dir"))
			{
				if (sendinchunks(sockfd, command, strlen(command) + 1, 0) < 0)
				{
					free(command);
					continue;
				}

				char *buffer = (char*) malloc(__default_cmd_len__* sizeof(char));
				memset(buffer, 0, sizeof(buffer));
				int count_null = 0;

				do { 	if (receive_upto_delimiter(sockfd, &buffer, '\0') == 1)
						count_null++;
					else
					{
						count_null = 0;
						printf(" > %s\n", buffer);
					}
					free(buffer);
					buffer = (char*) malloc(__default_cmd_len__* sizeof(char));
					memset(buffer, 0, sizeof(buffer));
				} while (count_null != 2);

				free(buffer);
			}
			else if (!strcmp(command_name, "get"))
			{
				char *command_array[3];
				for (int i = 0; i < 3; i++)
					command_array[i] = (char*) malloc(1* sizeof(char));
				get_command_array(command_array, 3, command, " ");

				int fd = open(command_array[2], O_WRONLY | O_CREAT, 0777);

				if (fd == -1)
					perror(" [ ERROR : Could not open the file. ]\n");
				else
				{
					if (sendinchunks(sockfd, command, strlen(command) + 1, 0) < 0)
					{
						free(command);
						continue;
					}
					if (receive_status_code(sockfd, buf) < 0)
					{
						free(command_array[0]);
						free(command_array[1]);
						free(command_array[2]);
						close(fd);
						free(command);
						continue;
					}

					if (atoi(buf) == 200)
						receive_file(sockfd, fd);
					else
					{
						close(fd);
						free(command_array[0]);
						free(command_array[1]);
						free(command_array[2]);
						free(command);
						continue;
					}
					close(fd);
				}
				free(command_array[0]);
				free(command_array[1]);
				free(command_array[2]);
			}
			else if (!strcmp(command_name, "put"))
			{
				char *command_array[3];
				for (int i = 0; i < 3; i++)
					command_array[i] = (char*) malloc(1* sizeof(char));
				get_command_array(command_array, 3, command, " ");

				int fd = open(command_array[1], O_RDONLY);

				if (fd == -1)
					perror(" [ ERROR : Could not open the file. ]\n");
				else
				{
					if (sendinchunks(sockfd, command, strlen(command) + 1, 0) < 0)
					{
						free(command);
						continue;
					}
					if (receive_status_code(sockfd, buf) < 0)
					{
						free(command_array[0]);
						free(command_array[1]);
						free(command_array[2]);
						close(fd);
						free(command);
						continue;
					}

					if (atoi(buf) == 200)
						send_file(sockfd, fd);
					else
					{
						close(fd);
						free(command_array[0]);
						free(command_array[1]);
						free(command_array[2]);
						free(command);
						continue;
					}
					close(fd);
				}
				free(command_array[0]);
				free(command_array[1]);
				free(command_array[2]);
			}
			else if (!strcmp(command_name, "mget"))
			{
				int count_args = 1;
				int len = strlen(command);
				for (int i = 0; i < len; i++)
				{
					if (command[i] == ' ') count_args++;
				}

				char **command_array = (char **) calloc(count_args, sizeof(char*));
				for (int i = 0; i < count_args; i++)
					command_array[i] = (char*) malloc(1* sizeof(char));

				get_command_array(command_array, count_args, command, " ");

				for (int i = 1; i <= count_args - 1; i++)
				{
					char* basename = (char*)malloc(sizeof(char)*strlen(command_array[i]));
					get_filename(command_array[i], basename);

					char *sub_command = (char*) calloc(strlen(command_array[i]) *2 + 6, sizeof(char));
					memset(sub_command, 0, sizeof(sub_command));
					strcpy(sub_command, "get ");
					strcat(sub_command, command_array[i]);
					strcat(sub_command, " ");
					strcat(sub_command, basename);

					int fd = open(basename, O_WRONLY | O_CREAT, 0777);
					if (fd == -1)
					{
						printf(" [ ERROR : Could not open the file - ");
						printf("%s ]\n", command_array[i]);
						free(basename);
						free(sub_command);
						break;
					}
					else
					{
						if (sendinchunks(sockfd, sub_command, strlen(sub_command) + 1, 0) < 0)
						{
							close(fd);
							free(basename);
							free(sub_command);
							break;
						}

						if (receive_status_code(sockfd, buf) < 0)
						{
							free(sub_command);
							free(basename);
							close(fd);
							continue;
						}

						if (atoi(buf) == 200)
							receive_file(sockfd, fd);
						else
						{
							close(fd);
							free(basename);
							free(sub_command);
							break;
						}
						close(fd);
					}
					free(basename);
					free(sub_command);
				}

				for (int i = 0; i < count_args; i++)
					free(command_array[i]);
				free(command_array);
			}
			else if (!strcmp(command_name, "mput"))
			{
				int count_args = 1;
				int len = strlen(command);
				for (int i = 0; i < len; i++)
				{
					if (command[i] == ' ') count_args++;
				}

				char **command_array = (char **) calloc(count_args, sizeof(char*));
				for (int i = 0; i < count_args; i++)
					command_array[i] = (char*) malloc(1* sizeof(char));

				get_command_array(command_array, count_args, command, " ");

				for (int i = 1; i <= count_args - 1; i++)
				{
					char* basename = (char*)malloc(sizeof(char)*strlen(command_array[i]));
					get_filename(command_array[i], basename);

					char *sub_command = (char*) calloc(strlen(command_array[i]) *2 + 6, sizeof(char));
					memset(sub_command, 0, sizeof(sub_command));
					strcpy(sub_command, "put ");
					strcat(sub_command, command_array[i]);
					strcat(sub_command, " ");
					strcat(sub_command, basename);

					int fd = open(command_array[i], O_RDONLY);
					if (fd == -1)
					{
						printf(" [ ERROR : Could not open the file - ");
						printf("%s ]\n", command_array[i]);
						free(basename);
						free(sub_command);
						break;
					}
					else
					{
						if (sendinchunks(sockfd, sub_command, strlen(sub_command) + 1, 0) < 0)
						{
							close(fd);
							free(basename);
							free(sub_command);
							break;
						}

						if (receive_status_code(sockfd, buf) < 0)
						{
							free(sub_command);
							free(basename);
							close(fd);
							continue;
						}

						if (atoi(buf) == 200)
							send_file(sockfd, fd);
						else
						{
							close(fd);
							free(basename);
							free(sub_command);
							break;
						}
						close(fd);
					}
					free(basename);
					free(sub_command);
				}

				for (int i = 0; i < count_args; i++)
					free(command_array[i]);
				free(command_array);
			}
			else if (!strcmp(command_name, "cd"))
			{
				if (sendinchunks(sockfd, command, strlen(command) + 1, 0) < 0)
				{
					free(command);
					continue;
				}
				receive_status_code(sockfd, buf);
			}
			else
			{
				printf(" [ CMD ERROR : No such command. ]\n");
			}
		}
		else
		{
			printf(" [ AUTH ERROR : Please authenticate yourself. ]\n");
		}

		free(command);
	}

	printf("\n\n");
	close(sockfd);
	return 0;
}