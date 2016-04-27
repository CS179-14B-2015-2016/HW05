#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <vector>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "command.hpp"

using boost::asio::ip::tcp;
using namespace boost::filesystem;

class Participant {
public:
	virtual ~Participant() {}
	virtual void receive(Command& cmd) = 0;
};

class Room {
public:
	void join(std::shared_ptr<Participant> participant) {
		participants.insert(participant);
	}
	
	void leave(std::shared_ptr<Participant> participant) {
		participants.erase(participant);
	}
	
	void broadcast(Command& cmd) {
		// std::cout << "Broadcasting to " << participants.size() << " participants" << std::endl;
		for (auto participant : participants) {
			participant->receive(cmd);
		}
	}

private:
	std::set<std::shared_ptr<Participant>> participants;
};

class Client : public Participant, public std::enable_shared_from_this<Client> {
public:
	Client(tcp::socket socket, Room& room, path root_dir) : socket(std::move(socket)), room(room), root_dir(root_dir) {}
	void start() {
		room.join(shared_from_this());
		read_header();
	}
	
	void receive(Command& cmd) {
		write(cmd);
	}

private:
	tcp::socket socket;
	Room& room;
	path root_dir;
	Command rcvd_cmd;
	ofstream rcvd_file;

	void read_header() {
		auto self(shared_from_this());
		boost::asio::async_read(
			socket,
			boost::asio::buffer(
				rcvd_cmd.get_header(),
				Length::HEADER
			),
			[this, self](boost::system::error_code ec, std::size_t len) {
				if (!ec) {
					rcvd_cmd.decode_header();
					read_body();
				} else {
					room.leave(shared_from_this());
				}
			}
		);
	}
	
	void read_body() {
		auto self(shared_from_this());
		boost::asio::async_read(
			socket,
			boost::asio::buffer(
				rcvd_cmd.get_body(),
				rcvd_cmd.get_body_length()
			),
			[this, self](boost::system::error_code ec, std::size_t len) {
				if (!ec) {
					handle(rcvd_cmd);
					read_header();
				} else {
					room.leave(shared_from_this());
				}
			}
		);
	}
	
	void write(Command& cmd) {
		auto self(shared_from_this());
		boost::asio::async_write(
			socket,
			boost::asio::buffer(
				cmd.get_packet(),
				cmd.get_packet_length()
			),
			[this, self](boost::system::error_code ec, std::size_t len) {
				if (!ec) {
					
				} else {
					room.leave(shared_from_this());
				}
			}
		);
	}
	
	void handle(Command& cmd) {
		switch (cmd.get_type()) {
			case Command::Type::MESSAGE: {
				std::cout << "INFO:\tMessage received: ";
				std::cout.write(cmd.get_data(), cmd.get_data_length());
				std::cout << std::endl;
				std::string new_msg = "INFO:\tMessage received: " + std::string(cmd.get_data(), cmd.get_data_length());
				cmd.update_data(new_msg.length());
				std::memcpy(cmd.get_data(), new_msg.c_str(), cmd.get_data_length());
				room.broadcast(cmd);
				break;
			}
			case Command::Type::UPLOAD: {
				if (!rcvd_file.is_open()) {
					std::cout << "INFO:\tUpload requested" << std::endl;
					std::string old_filename(cmd.get_filename(), cmd.get_filename_length());
					std::string new_filename = old_filename;
					char filename_suff[8];
					for (int i = 1; ifstream(root_dir / new_filename); ++i) {
						new_filename = old_filename;
						std::sprintf(filename_suff, "(%d)", i);
						new_filename.insert(new_filename.find('.'), std::string(filename_suff, std::strlen(filename_suff)));
					}
					std::string msg = "INFO:\tUploading " + new_filename + " to " + root_dir.string() + "...";
					Command status(Command::Type::MESSAGE, msg.length());
					std::memcpy(status.get_body(), msg.c_str(), status.get_body_length());
					write(status);
					rcvd_file.open(root_dir / new_filename, std::ios::binary|std::ios::app);
				}
				if (rcvd_file.is_open()) {
					rcvd_file.write(cmd.get_data(), cmd.get_data_length());
					if (cmd.get_data_left() == 0) {
						std::string msg = "INFO:\tUpload completed";
						Command status(Command::Type::MESSAGE, msg.length());
						std::memcpy(status.get_body(), msg.c_str(), status.get_body_length());
						write(status);
						rcvd_file.close();
						std::cout << msg << std::endl;
					}
				}
				break;
			}
			case Command::Type::DOWNLOAD: {
				std::cout << "INFO:\tDownload requested" << std::endl;
				std::string filename(cmd.get_filename(), cmd.get_filename_length());
				ifstream rqst_file(root_dir / filename, std::ios::binary);
				if (rqst_file.is_open()) {
					cmd.update_data(file_size(filename));
					while (cmd.get_data_length() > 0) {
						rqst_file.read(cmd.get_data(), cmd.get_data_length());
						write(cmd);
						cmd.update_data();
					}
					rqst_file.close();
					std::cout << "INFO:\tDownload completed" << std::endl;
				} else {
					std::string msg = "ERROR:\tFailed to download " + filename;
					Command status(Command::Type::MESSAGE, msg.length());
					std::memcpy(status.get_body(), msg.c_str(), status.get_body_length());
					write(status);
				}
				break;
			}
			case Command::Type::LS: {
				std::cout << path(root_dir).filename() << std::endl;
				std::vector<path> files;
				copy(directory_iterator(root_dir), directory_iterator(), std::back_inserter(files));
				sort(files.begin(), files.end());
				for (std::vector<path>::const_iterator i(files.begin()); i != files.end(); ++i) {
					std::cout << "   " << i->filename() << std::endl;
				} 
				break;
			}
		}
	}
};

class Server {
public:
	Server(boost::asio::io_service& io_service, const tcp::endpoint& endpoint, path root_dir)
	: acceptor(io_service, endpoint), socket(io_service), root_dir(root_dir) {
		accept();
	}

private:
	tcp::acceptor acceptor;
	tcp::socket socket;
	Room room;
	path root_dir;
	
	void accept() {
		acceptor.async_accept(
			socket,
			[this](boost::system::error_code ec) {
				if (!ec) {
					std::cout << "Accepted" << std::endl;
					std::make_shared<Client>(std::move(socket), room, root_dir)->start();
				}
				accept();
			}
		);
	}
};

int main(int argc, char* argv[]) {
	try {
		if (argc != 3) {
			std::cerr << "Usage: server <port> <root>" << std::endl;
			return 1;
		}
		
		path root_dir(argv[2]);
		if (!exists(root_dir) || !is_directory(root_dir)) {
			std::cout << "Error: Invalid root directory" << std::endl;
			return 1;
		}
		
		boost::asio::io_service io_service;
		
		tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[1]));
		Server server(io_service, endpoint, root_dir);
		
		io_service.run();
		
		/* for (;;) {
			tcp::socket socket(io_service);
			acceptor.accept(socket);
			
			std::string message = make_daytime_string();
			
			boost::system::error_code ignored_error;
			boost::asio::write(socket, boost::asio::buffer(message), ignored_error);
		} */
		
	} catch (std::exception e) {
		std::cerr << e.what() << std::endl;
	}
	
	return 0;
}