/*
 * telnet.cpp - Interactive debug shell over TCP/IP
 * Author: Daniel Meszaros <easimer@gmail.com>
 * EasimerNet-Confidental
 */

#include <aqir.h>
#include <wow.h>
#include <x11.h>
#include <telnet.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <string>
#include <sstream>
#include <vector>
#include <iterator>

void telnet_process_client(int fd);

void * aqir_net_thread_func(void *param)
{
	int sockfd;
	struct sockaddr_in self;

	sleep(2);

	// open socket

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		std::cout << "network fail: socket()" << std::endl;
		pthread_exit(NULL);
		return NULL;
	}

	// make socket reusable
	int en = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(int)) < 0)
	{
		std::cout << "network fail: SO_REUSEADDR" << std::endl;
		pthread_exit(NULL);
		return NULL;
	}

	// init structs, bind to 0.0.0.0:1666

	memset(&self, 0, sizeof(self));
	self.sin_family = AF_INET;
	self.sin_port = htons(1666);
	self.sin_addr.s_addr = INADDR_ANY;

	if(bind(sockfd, reinterpret_cast<struct sockaddr*>(&self), sizeof(self)) != 0)
	{
		std::cout << "network fail: bind()" << std::endl;
		perror("cause");
		pthread_exit(NULL);
		return NULL;
	}

	// listen on socket

	if(listen(sockfd, 20) != 0)
	{
		std::cout << "network fail: listen()" << std::endl;
		pthread_exit(NULL);
		return NULL;
	}

	std::cout << "network: accepting connections" << std::endl;

	while(true)
	{
		int clientfd;

		struct sockaddr_in client_addr;
		socklen_t addrlen = sizeof(client_addr);

		// accept connection
		clientfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&client_addr), &addrlen);
		std::cout << "network: client " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << " connected"  << std::endl;
		telnet_process_client(clientfd);
		close(clientfd);
	}

}

std::vector<std::string> split(std::string& s)
{
	std::string buf;
	std::stringstream ss(s);

	std::vector<std::string> vec;

	while(ss >> buf)
	{
		if(!buf.empty())
			vec.push_back(buf);
	}
	return vec;
}

void telnet_err(int fd, int errcode)
{
	const char* err_nocmd = "no such command\n";
	const char* err_insuff = "not enough arguments\n";
	const char* err_generic = "error\n";
	//std::string err_nocmd = "no such command\n";
	//std::string err_insuff = "not enough arguments\n";
	//std::string err_generic = "error\n";

	switch(errcode)
	{
		case TELNET_ERR_INSUF:
			send(fd, err_insuff, 21, 0);
			break;
		case TELNET_ERR_NOSUCH:
			send(fd, err_nocmd, 16, 0);
			break;
		default:
			send(fd, err_generic, 6, 0);
			break;
	}
}

void telnet_process_command(int fd, std::string& cmd)
{
		auto tok = split(cmd);
		for(auto& t : tok)
		{
			std::cout << t << std::endl;
		}
		if(tok[0] == "enable")
		{
			CWow::EnableBot();
			std::cout << "network: bot enabled remotely" << std::endl;
		}
		else if(tok[0] == "disable")
		{
			CWow::DisableBot();
			std::cout << "network: bot disabled remotely" << std::endl;
		}
		else if(tok[0] == "testkey")
		{
			sleep(2);
			x11_open();
			x11_kbkey(KEY_4);
			x11_close();
			std::cout << "network: key test issued" << std::endl;
		}
		else if(tok[0] == "dump")
		{
			if(tok.size() < 3)
			{
				//send(fd, err_insuff.c_str(), err_insuff.size(), 0);
				telnet_err(fd, TELNET_ERR_INSUF);
				return;
			}

			uintptr_t addr;
			size_t size;

			addr = std::stoull(tok[1], nullptr, 16);
			size = std::stoull(tok[2], nullptr, 10);

			if(addr == 0)
			{
				telnet_err(fd, -1);
			}

			std::stringstream buf;

			buf << std::hex;

			for(size_t i = 0; i < size; i++)
			{
				uint8_t byte = reinterpret_cast<uint8_t*>(addr)[i];
				buf << std::to_string(byte) << ' ';
				if((i & 15) == 15)
				{
					buf << std::endl;
					std::string s = buf.str();
					send(fd, s.c_str(), s.size(), 0);
					buf.clear();
				}
			}

		}
		else if(tok[0] == "mclick")
		{
			if(tok.size() < 3)
			{
				telnet_err(fd, TELNET_ERR_INSUF);
				//send(fd, err_insuff.c_str(), err_insuff.size(), 0);
				return;
			}
			
		}
		else if(tok[0] == "pos")
		{
			std::cout << "Player position: " << std::dec << CWowPlayer::GetX() << ' '
				<< std::dec << CWowPlayer::GetY() << ' '
				<< std::dec << CWowPlayer::GetZ() << std::endl;
		}
		else if(tok[0] == "jump")
		{
			CWowPlayer::SetZ(CWowPlayer::GetZ() + 1572418);
		}
		else
		{
			telnet_err(fd, TELNET_ERR_NOSUCH);
			//send(fd, err_nocmd.c_str(), err_nocmd.size(), 0);
		}
}

void telnet_process_client(int fd)
{
	int rc;
	std::stringstream ss;
	const char* prompt = "# ";
	std::string motd = "aqsh\navailable commands: enable, disable, testkey, dump\n\n";
	send(fd, motd.c_str(), motd.size(), 0);

	send(fd, prompt, 2, 0);

	while(true) {
		char buf[32];
		memset(buf, 0, 32);
		rc = recv(fd, buf, 32, 0);
		if(rc == 0)
			return;
		ss << buf;
		std::string str = ss.str();
		if(str.find_first_of('\n') != std::string::npos)
		{
			if(str.size() > 1)
				telnet_process_command(fd, str);
			ss.str("");
			ss.clear();
			send(fd, prompt, 2, 0);
		}
	}
}