#pragma once
enum MessageType : uint8_t {
	CONNECT = 0,
	DISCONNECT,
	CONFIRM,
	CHAT,
	FILE_REQUEST,
	FILE_SEND,
	FILE_QUERY
};

struct MessageHeader {
	MessageType type;
	size_t size;
};

struct FileInfo {
	size_t fileSize;
	size_t filenameLength;
};