#include "third_party/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

namespace axdp {
    namespace uniview {
        json V50E = R"(
            {
              "name": "V50E",
              "vendor": "Uniview",
              "hid_params": {
                  "vid": "0x0525",
                  "pid": "0xa4ac",
                  "usage": "0x82",
                  "usage_page": "0x81",
                  "report_id": "0x00",
                  "interface_num": "0x11",
                  "output_report_len": "0x100"
              },
              "update": {
                "strategy": "0x00400000",
                "dst" : "0xff"
              },
              "protocol": {
                  "type": "0xB0000",
                  "magic": "0xFFA5",
                  "version": "0x1"
              },
              "capabilities": []
            }
        )"_json;
    }
}