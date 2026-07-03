#include "request_handler.h"
#include "request.h"
const std::unordered_map<std::string, MethodHandler> RequestHandler::_handlerMap{
	{"getDeviceInfo", &RequestHandler::getDeviceInfo},
	{"startUpgrade", &RequestHandler::startUpgrade},
	{"setWiFiSSID", &RequestHandler::setWiFiSSID}
};

RequestHandler::RequestHandler()
{
}

RequestHandler::~RequestHandler()
{
}

int RequestHandler::processRequest(const Request& request)
{
	//if (!request.RequestData.is_object() && !request.RequestData.is_null())
	//	return RequestResult::Error(RequestStatus::InvalidRequestFieldType, "Your requestHandler data is not an object.");

	//if (request.RequestType.empty())
	//	return RequestResult::Error(RequestStatus::MissingRequestType, "Your requestHandler's `requestType` may not be empty.");

	//RequestMethodHandler handler;
	//try {
	//	handler = _handlerMap.at(request.RequestType);
	//}
	//catch (const std::out_of_range& oor) {
	//	UNUSED_PARAMETER(oor);
	//	return RequestResult::Error(RequestStatus::UnknownRequestType, "Your requestHandler type is not valid.");
	//}

	//return std::bind(handler, this, std::placeholders::_1)(request);
	return 0;
}

int RequestHandler::getDeviceInfo(axdp::DeviceAccessor* dev, const std::vector<std::string>& params)
{

	return 0;
}

int RequestHandler::startUpgrade(axdp::DeviceAccessor* dev, const std::vector<std::string>& params)
{
	return 0;
}

int RequestHandler::setWiFiSSID(axdp::DeviceAccessor* dev, const std::vector<std::string>& params)
{
	return 0;
}

