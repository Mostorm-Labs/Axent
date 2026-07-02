#include "axdp_device_set.h"

using json = nlohmann::json;
namespace axdp {

	/*
	* device str name for test
	*/
	std::map<std::string, nlohmann::json&> DevJson = {
		{"V20", hamedal::V20},
		{"A20", hamedal::A20}
	};

	/*
	* nuroum device json config map
	*/
	DevPropJsonMap nuroum::devPropJsonMap = {
	};

	/*
	* hamedal device json config map
	*/
	DevPropJsonMap hamedal::devPropJsonMap = {
		{DeviceCode::HAMEDAL_V20, hamedal::V20},
		{DeviceCode::HAMEDAL_A20, hamedal::A20},
	};

	/*
	* nearity device json config map
	*/
	DevPropJsonMap nearity::devPropJsonMap = {
	};

	/*
	* uniview device json config map
	*/
	DevPropJsonMap uniview::devPropJsonMap = {
		{DeviceCode::UNIVIEW_V50E, uniview::V50E},
	};

	/*
	* dahua device json config map
	*/
	DevPropJsonMap dahua::devPropJsonMap = {
	};

	CustomizerDevMap CustomDevMap = {
		{Customizers::Nuroum,	nuroum::devPropJsonMap},
		{Customizers::Hamedal,	hamedal::devPropJsonMap},
		{Customizers::Nearity,	nearity::devPropJsonMap},
		{Customizers::Uniview,	uniview::devPropJsonMap},
		{Customizers::DaHua,	dahua::devPropJsonMap},
	};
}
