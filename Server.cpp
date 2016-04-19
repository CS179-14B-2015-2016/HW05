#include <boost/asio.hpp>
#include <iostream>
#include <map>
#include <utility>
#include "Header.h"

using namespace boost::asio;
using namespace std;
typedef uint8_t byte;

struct session;
vector<session*> sessions;
map<string, session*> session_id;
map<string, pair<FileInfo, vector<byte>>> files;
void broadcast(vector<byte>& buffer);

struct session {
	vector<byte> recBuffer;
	int recTotal, recBytes;
	MessageType recType;
	string username;

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

				if (recType == MessageType::CHAT) 
				{
					string msg = string(reinterpret_cast<char*>(recBuffer.data()));;
					cout << "Received " << msg << endl;
					sendBuffer.clear();
					size_t sendSize = sizeof(MessageHeader) + msg.length() + 1;
					sendBuffer.resize(sendSize);
					MessageHeader header = MessageHeader{ MessageType::CHAT, msg.length() + 1 };

					memcpy(sendBuffer.data(), &header, sizeof(MessageHeader));
					memcpy(sendBuffer.data() + sizeof(MessageHeader), msg.data(), msg.length() + 1);
					broadcast(sendBuffer);
				}
				else if (recType == MessageType::CONNECT)
				{
					string desiredUname = string(reinterpret_cast<char*>(recBuffer.data()));
					while (session_id.find(desiredUname) != session_id.end()) {
						char digit = desiredUname[desiredUname.length()-1];
						if (digit >= '0' && digit <= '8')
						{
							desiredUname[desiredUname.length() - 1] = (char)(digit + 1);
						}
						else if (digit == '9')
						{
							desiredUname[desiredUname.length() - 1] = '0';
							desiredUname += "0";
						}
						else
						{
							desiredUname += "0";
						}
					}
					username = desiredUname;
					session_id[username] = this;
					cout << username << " has connected." << endl;
					cout << "Active users: " << sessions.size() << endl;
					string msg = username;

					sendBuffer.clear();
					size_t sendSize = sizeof(MessageHeader) + msg.length() + 1;
					sendBuffer.resize(sendSize);
					MessageHeader header = MessageHeader{ MessageType::CONFIRM, msg.length() + 1 };

					memcpy(sendBuffer.data(), &header, sizeof(MessageHeader));
					memcpy(sendBuffer.data() + sizeof(MessageHeader), msg.data(), msg.length() + 1);
					send();
				}
				else if (recType == MessageType::DISCONNECT) {
					string msg = string(reinterpret_cast<char*>(recBuffer.data()));
					session* toRemove = session_id[msg];

					session_id.erase( msg );
					sessions.erase(std::remove(sessions.begin(), sessions.end(), toRemove));

					msg += " has disconnected.";

					cout << msg << endl;
					cout << "Active users: " << sessions.size() << endl;

					sendBuffer.clear();
					size_t sendSize = sizeof(MessageHeader) + msg.length() + 1;
					sendBuffer.resize(sendSize);
					MessageHeader header = MessageHeader{ MessageType::CONFIRM, msg.length() + 1 };

					memcpy(sendBuffer.data(), &header, sizeof(MessageHeader));
					memcpy(sendBuffer.data() + sizeof(MessageHeader), msg.data(), msg.length() + 1);
					broadcast(sendBuffer);
				}
				else if (recType == MessageType::FILE_QUERY) {
					string msg = "";
					if (files.size() == 0)
						msg = "There are no files uploaded in the server.";
					else {
						msg = "Files in the server are:";
						for (auto iterator = files.begin(); iterator != files.end(); iterator++)
							msg += " " + iterator->first;
					}

					sendBuffer.clear();
					size_t sendSize = sizeof(MessageHeader) + msg.length() + 1;
					sendBuffer.resize(sendSize);
					MessageHeader header = MessageHeader{ MessageType::CHAT, msg.length() + 1 };

					memcpy(sendBuffer.data(), &header, sizeof(MessageHeader));
					memcpy(sendBuffer.data() + sizeof(MessageHeader), msg.data(), msg.length() + 1);
					send();
				}

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
		});
	}
};

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
			sessions.push_back(new session(std::move(socket)));
			sessions.back()->receiveHeader();
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