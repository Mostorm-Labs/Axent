#include "third_party/md5/include/md5.h"
#include <map>

static std::map<char, char> ascii {
	{'0', 0},
	{'1', 1},
	{'2', 2},
	{'3', 3},
	{'4', 4},
	{'5', 5},
	{'6', 6},
	{'7', 7},
	{'8', 8},
	{'9', 9},
	{'a', 0xa},
	{'b', 0xb},
	{'c', 0xc},
	{'d', 0xd},
	{'e', 0xe},
	{'f', 0xf},
	{'A', 0xa},
	{'B', 0xb},
	{'C', 0xc},
	{'D', 0xd},
	{'E', 0xe},
	{'F', 0xf}
};


void str2md5(const char* message, int len, char output[16]) {
	if (len >= 32 && len < 64)
	{
		for (size_t i = 0; i < 16; i++)
		{
			char high = ((ascii[message[2 * i]] & 0x0f) << 4);
			char low = (ascii[message[2 * i + 1]] & 0x0f);
			output[i] = high | low;
		}
	}
	if (len >= 64)
	{

	}
}
