
#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include "Command.h"
#include <string>

using namespace std;
using boost::asio::ip::tcp;

class Client {


private:
	boost::asio::io_service& io_service;
	bool connected;
	tcp::socket socket;
	std::array<uint8_t,1024> buffer;
	std::deque<Command> commandsGiven;

public:
	Client(boost::asio::io_service& io_service,
		tcp::resolver::iterator endpoint_iterator, string username)
		: io_service(io_service), socket(io_service) {
		connected = false;
		connect(endpoint_iterator, username);
	}

	bool isConnected() {
		return connected;
	}

	void close() {
		io_service.post([this]() { socket.close(); });
	}

	void write(CommandType type, const uint8_t *data, size_t length) {
		std::vector<uint8_t> send_buf(sizeof(Command) + length);
		auto p = reinterpret_cast<Command*>(send_buf.data());
		p->type = type;
		p->size = length;
		memcpy(p->data, data, length);
		socket.async_send(boost::asio::buffer(send_buf), [&](auto ec, size_t length) {
			if (ec) {
				socket.close();
			}
			else {
			}
		});
	}



private:
	void connect(tcp::resolver::iterator endpoint,string username) {
		boost::asio::async_connect(socket, endpoint,
			[&](boost::system::error_code ec, tcp::resolver::iterator) {
			if (!ec) {
				connected = true;
				cout << "Connected!";
				//std::vector<uint8_t>attemptuser(username.begin(), username.end());
				//write(CommandType::CONNECT, &attemptuser[0], attemptuser.size());
			}
			else { socket.close(); }

		});
	}
	/*
	void receive() {
		socket.async_receive(boost::asio::buffer(buffer), [&](auto ec, auto length) {
			if (!ec) {
				auto *com = reinterpret_cast<const Command*>(buffer.data());
				switch (com->type) {
				case CommandType::MESSAGE:
					char message[1024];
					memcpy(message, com->data, com->size);
					message[com->size] = '\0';
					cout << ">> " << message << endl;
					break;
				}
			}
			else {
			}
		});
		receive();
	}
	
	*/
	

};

int main(int argc, char*argv[]) {
	try {
		string ip;
		string port;
		string username;

		cout << "Enter IP: ";
		cin >> ip;
		cout << "Enter Port: ";
		cin >> port;
		cout << "Enter Username: ";
		cin >> username;


		boost::asio::io_service service;
		tcp::resolver resolver(service);
		auto endpoint_iterator = resolver.resolve({ ip, port });
		Client c(service, endpoint_iterator, username);

		std::thread t([&]() {service.run(); });


		string message = "";

		while (cin >> message) {
			if (c.isConnected()) {
				std::vector<uint8_t> data(message.begin(), message.end());
				c.write(CommandType::MESSAGE, &data[0], data.size());
				cout << "Sending \n";
			}
			else {
				cout << "Not Connected, please wait \n";
			}
		}
		c.close();
		t.join();
	}

	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}

	return 0;
}