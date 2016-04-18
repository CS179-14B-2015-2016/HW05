
#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <vector>
#include <unordered_map>
#include <system_error>
#include "Command.h"

using namespace std;
struct Client {
	boost::asio::ip::tcp::socket socket;
	string username;
	void send(CommandType type, const uint8_t *data, size_t length) {
		std::vector<uint8_t> send_buf(sizeof(Command) + length);
		auto p = reinterpret_cast<Command*>(send_buf.data());
		p->type = type;
		p->size = length;
		memcpy(p->data, data, length);
		socket.async_send(boost::asio::buffer(send_buf), [&](auto ec, auto length) {
			if (!ec) {

			}
			else {
				socket.close();
			}
		});
	}
};
std::unordered_map<string, Client> clients;

class Server {
private:
	boost::asio::ip::tcp::socket socket;
	boost::asio::ip::tcp::acceptor acceptor;
	std::array<uint8_t,1024> recv_buffer;
	std::function<void(boost::system::error_code, size_t)> handler = [this](boost::system::error_code ec, size_t length) {
		if (!ec) {
			auto *com = reinterpret_cast<const Command*>(recv_buffer.data());
			string user;
			switch (com->type) {
			case CommandType::CONNECT:
				char username[1024];
				cout << "Connect" << endl;
				memcpy(username, com->data, com->size);
				username[com->size] = '\0';
				user = username;
				if (clients.find(user) == clients.end()) {
					//cout << "Connected" << endl;
					//clients.emplace(make_pair(user, Client{std::move(socket),user}));
				}
				break;
			case CommandType::MESSAGE:
				char message[1024];
				cout << "Message" << endl;
				memcpy(message, com->data, com->size);
				message[com->size] = '\0';
				broadcast(*com);
				break;
			case CommandType::DL:
				break;
			case CommandType::LIST:
				break;
			case CommandType::UPLOAD:
				break;
			}
		}
		else {
			socket.close();
			//cout << "RECEIVED ERRROR\n";
		}
		receive();
	};

	void accept() {
		acceptor.async_accept(socket, [this](boost::system::error_code ec) {
			if (!ec){}
			else{}
			accept();
		});
	}

public:
	Server(boost::asio::io_service& service,
		const boost::asio::ip::tcp::endpoint& endpoint) :
		acceptor(service, endpoint),
		socket(service) {
		accept();
		receive();
	}

	void broadcast(Command command) {
		for (auto &p : clients) {
			p.second.send(command.type, command.data, command.size);
		}
	}

	void receive() {
		std::vector<uint8_t> data_gathered;
		socket.async_receive(boost::asio::buffer(recv_buffer), handler);
	}
};

int main(int argc, char* argv[]) {
	try {
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(),
			8080);
		Server serve(io_service, endpoint);
		io_service.run();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
}