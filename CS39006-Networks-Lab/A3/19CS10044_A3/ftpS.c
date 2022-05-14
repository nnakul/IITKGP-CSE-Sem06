
// CS39006 COMPUTER NETWORKS LABORATORY
// Assignment 3 : NETWORK PROGRAMMING WITH FILE TRANSFER PROTOCOL
// Nakul Aggarwal 19CS10044
// Hritaban Ghosh 19CS30053

/*
			NETWORK PROGRAMMING WITH SOCKETS

In this program we illustrate the use of Berkeley sockets for interprocess
communication across the network. We show the communication between a server
process and a client process.

*/
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
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <limits.h>
#define __default_len__ 15
#define MAXLINE 1024

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

int receive_command(const int sockfd, char **command)
{
	int size = __default_len__;
	int i = 0;
	char c;

	while (1)
	{
		if (recv(sockfd, &c, 1, 0) == 1)
		{
			if (i == size - 1)
			{
				size += 15;
				(*command) = (char*) realloc((*command), size);
			}
			(*command)[i++] = c;

			if (c == '\0') break;
		}
	}
	return i;
}

int read_upto_delimiter(const int fd, char **str, const char delim)
{
	char character_read_from_file[2];
	memset(character_read_from_file, 0, 2);
	int charread = 0;
	int i = 0;
	int size = __default_len__;

	while (1)
	{
		int ret = read(fd, character_read_from_file, 1);
		if (ret == -1)
		{
			perror("ERROR: Could not read the file.\n");
			return 0;
		}

		if (ret == 0) break;

		character_read_from_file[1] = 0;
		if (!charread && character_read_from_file[0] == ' ') continue;

		charread = 1;
		if (character_read_from_file[0] == delim)
			break;
		else if(delim == '\n')
		{
			// For windows files \r is followed by \n
			if(character_read_from_file[0] == '\r')
			{
				do
				{
					int ret = read(fd, character_read_from_file, 1);
					if (ret == -1)
					{
						perror("ERROR: Could not read the file.\n");
						return 0;
					}
				}while(character_read_from_file[0] != '\n');
				break;
			}
			if(character_read_from_file[0] == EOF)
			{
				break;
			}
		}
		if (i == size - 1)
		{
			size += 10;
			(*str) = (char*) realloc((*str), size* sizeof(char));
		}
		(*str)[i++] = character_read_from_file[0];
		(*str)[i] = '\0';
	}
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

int get_command_array(char command_array[][200], const int n, const char *command, const char *delim)
{
	int init_size = strlen(command);

	char copy_of_command[init_size + 1];
	memset(copy_of_command, 0, sizeof(copy_of_command));
	strcpy(copy_of_command, command);

	int i = 0;
	char *ptr = strtok(copy_of_command, delim);

	if (ptr == NULL)
	{
		return 0;
	}

	while (i < n && ptr != NULL)
	{
		memset(command_array[i], 0, sizeof(command_array[i]));
		strcpy(command_array[i], ptr);
		ptr = strtok(NULL, delim);
		i++;
	}

	return 1;
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
			perror("ERROR: Error while reading from file.\n");
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
			perror("ERROR: Error while writing to file.\n");
			return 0;
		}

		if (block_check_header == 'M') continue;
		else if (block_check_header == 'L') break;
		else
		{
			printf("ERROR: Some error ocurred while file transfer.\n");
			return 0;
		}
	}
	return 1;
}

void get_filename(char *path, char *name)
{
	int i = strlen(path) - 1;
	while (i >= 0 && path[i] != '/') i--;
	strcpy(name, path + i + 1);
}

int main()
{
	int sockfd, newsockfd;
	int clilen;
	struct sockaddr_in cli_addr, serv_addr;
	char buf[MAXLINE];

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Cannot create socket\n");
		exit(0);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(20000);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5);
	printf("Server Running...\n");

	while (1)
	{
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

		if (newsockfd < 0)
		{
			printf("Accept error\n");
			exit(0);
		}

		if (fork() == 0)
		{
			close(sockfd);
			printf("Connection established successfully...\n");

			while (1)
			{
				struct dirent * de;
				DIR *dr = opendir(".");

				if (dr == NULL)
				{
					printf("ERROR: Could not open current directory.\n");
					memset(buf, 0, sizeof(buf));
					strcpy(buf, "500");
					sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
					break;
				}

				int user_txt_flag = 0;
				while ((de = readdir(dr)) != NULL)
				{
					if (!strcmp(de->d_name, "user.txt"))
					{
						user_txt_flag = 1;
					}
				}

				if (user_txt_flag == 0)
				{
					printf("ERROR: user.txt FILE NOT FOUND.\n");
					memset(buf, 0, sizeof(buf));
					strcpy(buf, "500");
					sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
					break;
				}

				int username_flag = 0, password_flag = 0;

				char *command = (char*) malloc(__default_len__* sizeof(char));
				receive_command(newsockfd, &command);

				char command_name[7];
				get_command_name(command_name, command);

				char *password_to_be_matched;
				if (!strcmp(command_name, "user"))
				{
					int fd = open("./user.txt", O_RDONLY);

					if (fd == -1)
					{
						perror("ERROR: Could not open the file.\n");
						memset(buf, 0, sizeof(buf));
						strcpy(buf, "500");
						sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
						break;
					}
					else
					{
						char command_array[2][200];
						get_command_array(command_array, 2, command, " ");

						char *username[2], *password[2];
						for (int i = 0; i < 2; i++)
						{
							username[i] = (char*) malloc(__default_len__* sizeof(char));
							password[i] = (char*) malloc(__default_len__* sizeof(char));
						}

						int read_flag = 0;

						read_flag += read_upto_delimiter(fd, &username[0], ' ');
						read_flag += read_upto_delimiter(fd, &password[0], '\n');
						read_flag += read_upto_delimiter(fd, &username[1], ' ');
						read_flag += read_upto_delimiter(fd, &password[1], '\n');

						if (read_flag < 4)
						{
							printf("ERROR: Could not read the file.\n");
							memset(buf, 0, sizeof(buf));
							strcpy(buf, "500");
							sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
							close(fd);
							break;
						}

						if (!strcmp(command_array[1], username[0]))
						{
							memset(buf, 0, sizeof(buf));
							strcpy(buf, "200");
							password_to_be_matched = (char*) malloc(strlen(password[0]) + 1);
							memset(password_to_be_matched, 0, sizeof(password_to_be_matched));
							strcpy(password_to_be_matched, password[0]);
							sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
						}
						else if (!strcmp(command_array[1], username[1]))
						{
							memset(buf, 0, sizeof(buf));
							strcpy(buf, "200");
							password_to_be_matched = (char*) malloc(strlen(password[1]) + 1);
							memset(password_to_be_matched, 0, sizeof(password_to_be_matched));
							strcpy(password_to_be_matched, password[1]);
							sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
						}
						else
						{
							memset(buf, 0, sizeof(buf));
							strcpy(buf, "500");
							password_to_be_matched = NULL;
							sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
						}

						for (int i = 0; i < 2; i++)
						{
							free(username[i]);
							free(password[i]);
						}
					}

					if (atoi(buf) == 200)
					{
						username_flag = 1;
					}
					else
					{
						close(fd);
						continue;
					}

					close(fd);
				}
				else
				{
					memset(buf, 0, sizeof(buf));
					strcpy(buf, "600");
					sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
					continue;
				}

				if (username_flag)
				{
					while (1)
					{
						char *command = (char*) malloc(__default_len__* sizeof(char));
						receive_command(newsockfd, &command);

						char command_name[7];
						get_command_name(command_name, command);

						if (!strcmp(command_name, "pass"))
						{
							char command_array[2][200];
							get_command_array(command_array, 2, command, " ");

							if (!strcmp(command_array[1], password_to_be_matched))
							{
								memset(buf, 0, sizeof(buf));
								strcpy(buf, "200");
							}
							else
							{
								memset(buf, 0, sizeof(buf));
								strcpy(buf, "500");
							}

							sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);

							if (atoi(buf) == 200)
							{
								password_flag = 1;
								break;
							}
							else
							{
								break;
							}
						}
						else
						{
							memset(buf, 0, sizeof(buf));
							strcpy(buf, "600");
							sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
							break;
						}
						free(command);
					}
				}

				if (username_flag && password_flag)
				{
					while (1)
					{
						char *command = (char*) malloc(__default_len__* sizeof(char));
						receive_command(newsockfd, &command);

						char command_name[7];
						get_command_name(command_name, command);

						if (!strcmp(command_name, "cd"))
						{
							char command_array[2][200];
							get_command_array(command_array, 2, command, " ");

							if (chdir(command_array[1]) < 0)
							{
								perror("ERROR: Server Side Directory could not be changed.\n");
								memset(buf, 0, sizeof(buf));
								strcpy(buf, "500");
								sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
							}
							else
							{
								memset(buf, 0, sizeof(buf));
								strcpy(buf, "200");
								sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);

								char cwd[PATH_MAX];
								if (getcwd(cwd, sizeof(cwd)) != NULL)
								{
									printf("Current working dir: %s\n", cwd);
								}
								else
								{
									perror("ERROR: getcwd() error\n");
								}
							}
						}
						else if (!strcmp(command_name, "dir"))
						{
							struct dirent * de;
							DIR *dr = opendir(".");

							if (dr == NULL)
							{
								printf("ERROR: Could not open current directory.\n");
							}

							char *buffer = malloc(__default_len__* sizeof(char));
							memset(buffer, 0, sizeof(buffer));
							int size = __default_len__;

							while ((de = readdir(dr)) != NULL)
							{
								int i = 0;
								int de_len = strlen(de->d_name);
								while (i < de_len)
								{
									if (i == size - 1)
									{
										size += 10;
										buffer = realloc(buffer, size);
									}

									buffer[i] = (de->d_name)[i];
									i++;
									buffer[i] = '\0';
								}

								if (sendinchunks(newsockfd, buffer, strlen(buffer) + 1, 0) < 0)
								{
									char errcode[4] = "500";
									sendinchunks(newsockfd, errcode, 4, 0);
								}

								char nullchar = 0;
								send(newsockfd, &nullchar, 1, 0);
							}
							char nullchar = 0;
							send(newsockfd, &nullchar, 1, 0);

							free(buffer);
							closedir(dr);
						}
						else if (!strcmp(command_name, "get"))
						{
							char command_array[3][200];
							get_command_array(command_array, 3, command, " ");

							int fd = open(command_array[1], O_RDONLY);
							if (fd == -1)
							{
								perror("ERROR: Could not open the file.\n");
								memset(buf, 0, sizeof(buf));
								strcpy(buf, "500");
								sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
							}
							else
							{
								memset(buf, 0, sizeof(buf));
								strcpy(buf, "200");
								sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);

								send_file(newsockfd, fd);

								close(fd);
							}
						}
						else if (!strcmp(command_name, "put"))
						{
							char command_array[3][200];
							get_command_array(command_array, 3, command, " ");

							int fd = open(command_array[2], O_WRONLY | O_CREAT, 0777);
							if (fd == -1)
							{
								perror("ERROR: Could not open the file.\n");
								memset(buf, 0, sizeof(buf));
								strcpy(buf, "500");
								sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);
							}
							else
							{
								memset(buf, 0, sizeof(buf));
								strcpy(buf, "200");
								sendinchunks(newsockfd, buf, strlen(buf) + 1, 0);

								receive_file(newsockfd, fd);

								close(fd);
							}
						}
						free(command);
					}
				}
				free(command);
				free(password_to_be_matched);
				closedir(dr);
			}
			close(newsockfd);
			exit(0);
		}
		close(newsockfd);
	}
}