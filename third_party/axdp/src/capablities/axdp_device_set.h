#ifndef AXDP_DEVICE_SET_H_
#define AXDP_DEVICE_SET_H_

#include <map>
#include <string>
#include "third_party/json/single_include/nlohmann/json.hpp"
#include "src/axdp_defines.h"


namespace axdp {

	extern std::map<std::string, nlohmann::json&> DevJson;

	typedef std::map<DeviceCode, nlohmann::json&> DevPropJsonMap;

	namespace nuroum {
		extern nlohmann::json A20;
		extern nlohmann::json V20;
		extern DevPropJsonMap devPropJsonMap;
	}

	namespace hamedal {
		extern nlohmann::json A20;
		extern nlohmann::json V20;
		extern DevPropJsonMap devPropJsonMap;
	}

	namespace nearity {
		extern nlohmann::json A20;
		extern nlohmann::json V20;
		extern DevPropJsonMap devPropJsonMap;
	}

	namespace uniview {
		extern nlohmann::json V50E;
		extern DevPropJsonMap devPropJsonMap;
	}

	namespace dahua {
		extern nlohmann::json C20;
		extern DevPropJsonMap devPropJsonMap;
	}

	typedef std::map<Customizers, DevPropJsonMap&> CustomizerDevMap;

	extern CustomizerDevMap CustomDevMap;
}

#endif // !AXDP_DEVICE_SET_H_
