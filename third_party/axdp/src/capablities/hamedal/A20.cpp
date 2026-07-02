#include "third_party/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
namespace axdp {
    namespace hamedal {
        json A20 = R"(
            {
              "name": "A20",
              "vendor": "hamedal",
              "hid_params": {
                  "vid": "0x1fc9",
                  "pid": "0x826b",
                  "usage": "0x82",
                  "usage_page": "0x81",
                  "report_id": "0x05",
                  "interface_num": "0x05",
                  "output_report_len": "0x400"
              },
              "update": {
                  "strategy": "0x00200000",
                  "default_dst" : "0xff"
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