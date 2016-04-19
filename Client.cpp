#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <string>
#include <thread>
#include <sstream>
#include "Header.h"

using namespace boost::asio;
using namespace std;
typedef uint8_t byte;

struct client {
	ip::tcp::socket socket;
	string host, port, username;

	vector<byte> recBuffer;
	int recTotal, recBytes;
	MessageType recType;

	vector<byte> sendBuffer;

	client(io_service& service, string username): socket(service), username(username)  {}
	~client() {}

	void connect(ip::tcp::resolver& resolver, string host, string port) {
		auto endpoint_iterator = resolver.resolve({host, port});
		this->host = host;
		this->port = port;
		async_connect(socket, endpoint_iterator, [&](boost::system::error_code ec, ip::tcp::resolver::iterator it) {

			string msg = username;
			size_t sendSize = sizeof(MessageHeader) + username.length() + 1;
			sendBuffer.resize(sendSize);
			MessageHeader header = MessageHeader{ MessageType::CONNECT, msg.length()+1 };

			memcpy(sendBuffer.data(), &header, sizeof(MessageHeader));
			memcpy(sendBuffer.data() + sizeof(MessageHeader), msg.data(), msg.length() + 1);
			send();

			receiveHeader();
		});
	}

	void writeLoop() {
		string msg;
		cout << " > ";
		while (getline(cin, msg)) {
			execute(msg);
		}
	}

	void receiveHeader() {
		if (recBuffer.size() == 0) {
			recTotal = sizeof(MessageHeader);
			recBytes = 0;
			recBuffer.resize(recTotal);
		}

		socket.async_receive(buffer(recBuffer.data() + recBytes, (recTotal - recBytes)), [&](boost::system::error_code ec, size_t size) {
			recBytes += size;
			if (recBytes < recTotal) {
				receiveHeader();
			}
			else {
				MessageHeader* header = reinterpret_cast<MessageHeader*>(recBuffer.data());
				recBytes = 0;
				recTotal = header->size;
				recType = header->type;
				recBuffer.clear();
				receiveBody();
			}
		});
	}

	void receiveBody() {
		if (recBuffer.size() == 0)
			recBuffer.resize(recTotal);

		socket.async_receive(buffer(recBuffer.data() + recBytes, (recTotal - recBytes)), [&](boost::system::error_code ec, size_t size) {
			recBytes += size;
			if (recBytes < recTotal) {
				receiveBody();
			}
			else {
				if (recType == MessageType::CHAT)
				{
					string msg = string(reinterpret_cast<char*>(recBuffer.data()));
					cout << endl;
					cout << msg << endl;
					cout << " > ";
				}
				else if (recType == MessageType::CONFIRM)
				{
					string newUsername = string(reinterpret_cast<char*>(recBuffer.data()));
					username = newUsername;
					cout << endl;
					cout << "Your username is " + username << endl;
					cout << " > ";
				}
				recBuffer.clear();
				receiveHeader();
			}
		});
	}

	void send() {
		socket.async_send(buffer(sendBuffer.data(), sendBuffer.size()), [&](boost::system::error_code ec, size_t size) {
			sendBuffer.clear();
		});
	}

	void execute(string msg) {
		MessageType execType = MessageType::CHAT;
		if (msg[0] != '/') {
			execType = MessageType::CHAT;

			msg = username + ": " + msg;
			size_t sendSize = sizeof(MessageHeader) + msg.length() + 1;
			sendBuffer.resize(sendSize);
			MessageHeader header = MessageHeader{ MessageType::CHAT, msg.length() + 1 };

			memcpy(sendBuffer.data(), &header, sizeof(MessageHeader));
			memcpy(sendBuffer.data() + sizeof(MessageHeader), msg.data(), msg.length() + 1);
			send();
			cout << " > ";
			return;
		}

		stringstream ss;
		ss << msg;
		string command; ss >> command;
		if (command == "/exit") {
			cout << "Disconnecting..." << endl;
			msg = username;
			size_t sendSize = sizeof(MessageHeader) + msg.length() + 1;
			sendBuffer.resize(sendSize);
			MessageHeader header = MessageHeader{ MessageType::DISCONNECT, msg.length() + 1 };

			memcpy(sendBuffer.data(), &header, sizeof(MessageHeader));
			memcpy(sendBuffer.data() + sizeof(MessageHeader), msg.data(), msg.length() + 1);
			send();
			socket.close();
			exit(0);
		}
		else {
			cout << endl;
			cout << command << " is not a valid command." << endl;
			cout << " > ";
		}
	}
};

int main() {
	cout << "Host: ";
	string host; cin >> host;
	cout << "Port: ";
	string port; cin >> port;
	cout << "Username: ";
	string uname; cin >> uname;
	
	string msg;
	getline(cin, msg);

	io_service service;
	ip::tcp::socket socket(service);
	ip::tcp::resolver resolver(service);

	client client(service, uname);
	client.connect(resolver, host, port);
	
	try {
		thread receiving([&]() { service.run(); });
		client.writeLoop();
	}
	catch (std::exception ex) {
		cout << "Exception: " << ex.what() << endl;
	}

	return 0;
}