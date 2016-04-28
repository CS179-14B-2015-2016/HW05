#ifndef MESSAGE_HPP
#define MESSAGE_HPP

namespace Length {
	const std::size_t TYPE = 2, DATA_LEFT = 8, BODY_LENGTH = 4, FILENAME_LENGTH = 2;
	const std::size_t HEADER = TYPE + DATA_LEFT + BODY_LENGTH + FILENAME_LENGTH;
	const std::size_t BODY_MAX = 512;
};

namespace Index {
	const int TYPE = 0;
	const int DATA_LEFT = Length::TYPE;
	const int BODY_LENGTH = DATA_LEFT + Length::DATA_LEFT;
	const int FILENAME_LENGTH = BODY_LENGTH + Length::BODY_LENGTH;
};

class Command {
public:
	enum Type { NONE, MESSAGE, UPLOAD, DOWNLOAD, LS } type;
	
	Command() {}
	
	Command(Type type, std::size_t data_length, std::size_t filename_length = 0) : type(type), data_left(data_length), filename_length(filename_length), body_length(std::min(data_length + filename_length, Length::BODY_MAX)) {
		data_left -= (body_length - filename_length);
		encode_header();
	}
	
	int get_type() {
		return type;
	}
	
	const char* get_packet() {
		return packet;
	}
	
	std::size_t get_body_length() {
		return body_length;
	}
	
	std::size_t get_packet_length() {
		return Length::HEADER + body_length;
	}
	
	std::size_t get_data_left() {
		return data_left;
	}
	
	std::size_t get_filename_length() {
		return filename_length;
	}
	
	std::size_t get_data_length() {
		return body_length - filename_length;
	}
	
	char* get_header() {
		return packet;
	}
	
	char* get_body() {
		return packet + Length::HEADER;
	}
	
	char* get_filename() {
		return get_body();
	}
	
	char* get_data() {
		return get_body() + filename_length;
	}
	
	void update_data(std::size_t new_data = 0) {
		data_left += new_data;
		body_length = std::min(data_left + filename_length, Length::BODY_MAX);
		data_left -= (body_length - filename_length);
		encode_header();
	}
	
	void decode_header() {
		char header[Length::HEADER];
		std::memcpy(header, packet, Length::HEADER);
		type = static_cast<Type>(std::atoi(std::string(header + Index::TYPE, Length::TYPE).c_str()));
		data_left = std::atoi(std::string(header + Index::DATA_LEFT, Length::DATA_LEFT).c_str());
		body_length = std::atoi(std::string(header + Index::BODY_LENGTH, Length::BODY_LENGTH).c_str());
		filename_length = std::atoi(std::string(header + Index::FILENAME_LENGTH, Length::FILENAME_LENGTH).c_str());
		// test();
	}
	
	void test() const {
		std::cout << "Header: ";
		std::cout.write(packet, Length::HEADER);
		std::cout << std::endl;
	}
	
private:
	std::size_t body_length;
	std::size_t data_left;
	std::size_t filename_length;
	char packet[Length::HEADER + Length::BODY_MAX];
	
	void encode_header() {
		char format[24];
		std::sprintf(format, "%%%dd%%%dd%%%dd%%%dd", static_cast<int>(Length::TYPE), static_cast<int>(Length::DATA_LEFT), static_cast<int>(Length::BODY_LENGTH), static_cast<int>(Length::FILENAME_LENGTH));
		char header[Length::HEADER];
		std::sprintf(header, std::string(format, std::strlen(format)).c_str(), type, static_cast<int>(data_left), static_cast<int>(body_length), static_cast<int>(filename_length));
		std::memcpy(packet, header, Length::HEADER);
		// test();
	}
};

#endif