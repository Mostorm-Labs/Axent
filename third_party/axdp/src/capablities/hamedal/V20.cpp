#include "third_party/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

namespace axdp {
    namespace hamedal {
        json V20 = R"(
            {
              "name": "V20",
              "vendor": "hamedal",
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
                  "strategy": "0x00100000",
                  "default_dst" : "0xff"
              },
              "protocol": {
                  "type": "0xA0000",
                  "magic": "0xFFA5",
                  "version": "0x1"
              },
              "capabilities": [
                   {
                       "name":"CommonSetVideoMode",
                       "cmd":"0xC0021",
                       "resp":"0xC00A1" 
                   },
                   {
                       "name":"CommonGetVideoMode",
                       "cmd":"0xC0022",
                       "resp":"0xC00A2" 
                   }
              ]
            }
        )"_json;
    }
}