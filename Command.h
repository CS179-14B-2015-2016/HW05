#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>



enum class CommandType : uint8_t {
	MESSAGE = 0, DL = 1, UPLOAD = 2, LIST = 3, CONNECT = 4, RENAME = 5, OK = 6
};

class Command {
public:
	CommandType type;
	size_t size;
	uint8_t data[];
};