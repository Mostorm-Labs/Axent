#ifndef AXDP_REQUEST_H
#define AXDP_REQUEST_H

#include <vector>
#include <string>



struct Request {
	Request(const std::string& requestType) :RequestType(requestType) {}
	~Request() = default;
	void* Device;
	std::string RequestType;
	bool HasRequestData;
	std::vector<std::string> requestParams;
};



#endif // !AXDP_REQUEST_H

