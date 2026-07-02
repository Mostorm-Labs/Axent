#pragma once
#include "include/axdp_api.h"
#include <unordered_map>
#include <string>
class Request;
class RequestHandler;
typedef int (RequestHandler::*MethodHandler)(axdp::DeviceAccessor* dev, const std::vector<std::string>& params);
class RequestHandler
{
public:
	RequestHandler();
	~RequestHandler();

	int processRequest(const Request& request);

private:
	static const std::unordered_map<std::string, MethodHandler> _handlerMap;
	int getDeviceInfo(axdp::DeviceAccessor* dev, const std::vector<std::string>& params);
	int startUpgrade(axdp::DeviceAccessor* dev, const std::vector<std::string>& params);
	int setWiFiSSID(axdp::DeviceAccessor* dev, const std::vector<std::string>& params);
};

