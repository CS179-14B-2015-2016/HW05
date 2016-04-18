#include <boost/asio.hpp>
#include <iostream>
#include "Header.h"

using namespace boost::asio;
using namespace std;
typedef uint8_t byte;

void broadcast(vector<byte>& buffer);

struct session {
	vector<byte> recBuffer;
	int recTotal, recBytes;
	MessageType recType;

	vector<byte> sendBuffer;

	ip::tcp::socket socket;
	
	session(ip::tcp::socket socket) : socket(std::move(socket)), recTotal(0), recBytes(0) {}

	ip::tcp::socket* getSocketPtr() {
		return &socket;
	}

	void receiveHeader() {
		if (recBuffer.size() == 0) {
			recTotal = sizeof(MessageHeader);
			recBytes = 0;
			recBuffer.resize(recTotal);
		}

		socket.async_receive(buffer(recBuffer.data()+recBytes, (recTotal-recBytes)), [&](boost::system::error_code ec, size_t size) {
			recBytes += size;
			if (recBytes < recTotal) {
				receiveHeader();
			} else {
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
				string msg = string(reinterpret_cast<char*>(recBuffer.data()));
				cout << "received: " << msg << endl;
				
				sendBuffer.clear();
				size_t sendSize = sizeof(MessageHeader) + msg.length() + 1;
				sendBuffer.resize(sendSize);
				MessageHeader header = MessageHeader{ MessageType::CHAT, msg.length()+1 };

				memcpy(sendBuffer.data(), &header, sizeof(MessageHeader));
				memcpy(sendBuffer.data() + sizeof(MessageHeader), msg.data(), msg.length()+1);
				broadcast(sendBuffer);
				

				recBuffer.clear();
				receiveHeader();
			}
		});
	}

	void setSendBuffer(const vector<byte>& copy) {
		sendBuffer = vector<byte>(copy);
	}

	void send() {
		socket.async_send(buffer(sendBuffer.data(), sendBuffer.size()), [&](boost::system::error_code ec, size_t size) {
			cout << "Sent somthing" << endl;
		});
	}
};

vector<shared_ptr<session>> sessions;

void broadcast(vector<byte>& buffer) {
	for (auto session : sessions) {
		session->setSendBuffer(buffer);
		session->send();
	}
}

int main() {
	cout << "Port: ";
	uint16_t port; cin >> port;

	io_service service;
	ip::tcp::endpoint ep(ip::tcp::v4(), port);
	ip::tcp::acceptor ac(service, ep);
	ip::tcp::socket socket(service);

	function<void (void)> accept = [&]() {
		cout << "waiting for connections..." << endl;
		ac.async_accept(socket, [&](boost::system::error_code ec) {
			sessions.push_back(std::make_shared<session>(std::move(socket)));
			sessions.back()->receiveHeader();
			cout << "someone connected!" << endl;
			accept();
		});
	};

	accept();
	
	try {
		service.run();
	}
	catch (std::exception ex) {
		cout << ex.what() << endl;
	}
	
	system("pause");
	return 0;
}