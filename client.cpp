#include <fstream>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "command.hpp"

using boost::asio::ip::tcp;
using namespace boost::filesystem;

class Client {
public:
	Client(boost::asio::io_service& io_service, tcp::resolver::iterator endpoint_iterator)
	: socket(io_service) {
		boost::asio::async_connect(
			socket,
			endpoint_iterator,
			[this](boost::system::error_code ec, tcp::resolver::iterator) {
				if (!ec) {
					std::cout << "Connected" << std::endl;
					read_header();
				} else {
					socket.close();
				}
			}
		);
	}
	
	void write(Command& cmd) {
		boost::asio::async_write(
			socket,
			boost::asio::buffer(
				cmd.get_packet(),
				cmd.get_packet_length()
			),
			[this](boost::system::error_code ec, std::size_t len) {
				if (!ec) {
					
				} else {
					socket.close();
				}
			}
		);
	}
	
	void close() {
		socket.close();
	}
	
private:
	tcp::socket socket;
	Command rcvd_cmd;
	ofstream rcvd_file;
	
	void read_header() {
		boost::asio::async_read(
			socket,
			boost::asio::buffer(
				rcvd_cmd.get_header(),
				Length::HEADER
			),
			[this](boost::system::error_code ec, std::size_t len) {
				if (!ec) {
					rcvd_cmd.decode_header();
					read_body();
				} else {
					socket.close();
				}
			}
		);
	}
	
	void read_body() {
		boost::asio::async_read(
			socket,
			boost::asio::buffer(
				rcvd_cmd.get_body(),
				rcvd_cmd.get_body_length()
			),
			[this](boost::system::error_code ec, std::size_t len) {
				if (!ec) {
					handle(rcvd_cmd);
					read_header();
				} else {
					socket.close();
				}
			}
		);
	}
	
	void handle(Command& cmd) {
		switch (cmd.get_type()) {
			case Command::Type::MESSAGE: {
				std::cout.write(cmd.get_data(), cmd.get_data_length());
				std::cout << std::endl;
				break;
			}
			case Command::Type::DOWNLOAD: {
				if (!rcvd_file.is_open()) {
					std::string old_filename(cmd.get_filename(), cmd.get_filename_length());
					std::string new_filename = old_filename;
					char filename_suff[8];
					for (int i = 1; std::ifstream(new_filename); ++i) {
						new_filename = old_filename;
						std::sprintf(filename_suff, "(%d)", i);
						new_filename.insert(new_filename.find('.'), std::string(filename_suff, std::strlen(filename_suff)));
					}
					std::cout << "INFO:\tDownloading " << new_filename << "..." << std::endl;
					rcvd_file.open(new_filename, std::ios::binary|std::ios::app);
				}
				if (rcvd_file.is_open()) {
					rcvd_file.write(cmd.get_data(), cmd.get_data_length());
					if (cmd.get_data_left() == 0) {
						rcvd_file.close();
						std::cout << "INFO:\tDownload completed"<< std::endl;
					}
				}
				break;
			}
		}
	}
};

int main(int argc, char* argv[]) {
	try {
		if (argc != 3) {
			std::cerr << "Usage: client <host> <port>" << std::endl;
			return 1;
		}
		
		boost::asio::io_service io_service;
		
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(argv[1], argv[2]);
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		
		Client client(io_service, endpoint_iterator);
		
		boost::thread thread([&io_service]() { io_service.run(); });
		
		char line[Length::BODY_MAX];
		while (std::cin.getline(line, Length::BODY_MAX)) {
			std::string line_str(line, std::strlen(line));
			if (line_str.substr(0, line_str.find(' ')) == "/upload") {
				std::string filename = line_str.substr(line_str.find(' ') + 1);
				std::ifstream rqst_file(filename, std::ios::binary);
				if (rqst_file.is_open()) {
					Command cmd(Command::Type::UPLOAD, file_size(filename), filename.length());
					std::memcpy(cmd.get_filename(), filename.c_str(), cmd.get_filename_length());
					rqst_file.seekg(0);
					while (cmd.get_data_length() > 0) {
						rqst_file.read(cmd.get_data(), cmd.get_data_length());
						client.write(cmd);
						cmd.update_data();
					}
					rqst_file.close();
				} else {
					std::cout << "INFO: Failed to open " << filename << std::endl;
				}
			} else if (line_str.substr(0, line_str.find(' ')) == "/download") {
				std::string filename = line_str.substr(line_str.find(' ') + 1);
				Command cmd(Command::Type::DOWNLOAD, 0, filename.length());
				std::memcpy(cmd.get_filename(), filename.c_str(), cmd.get_filename_length());
				client.write(cmd);
			} else if (line_str == "/ls") {
				Command cmd(Command::Type::LS, 0);
				client.write(cmd);
			} else {
				Command cmd(Command::Type::MESSAGE, line_str.length());
				std::memcpy(cmd.get_body(), line_str.c_str(), cmd.get_body_length());
				client.write(cmd);
			}
		}
		
		client.close();
		thread.join();
		
		/* for (;;) {
			boost::array<char, 128> buf;
			boost::system::error_code error;
			
			size_t len = socket.read_some(boost::asio::buffer(buf), error);
			
			if (error == boost::asio::error::eof) {
				break; // Connection closed cleanly by peer.
			} else if (error) {
				throw boost::system::system_error(error); // Some other error
			}
			
			std::cout.write(buf.data(), len);
		} */
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}