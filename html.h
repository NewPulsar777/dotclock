
const PROGMEM char html[] = 
        "<HTML>"
        "<BODY>"
        "<h1>DOT CLOCK</h1>"
        "<form method=\"post\" action=\"/wifiset\">"
        "<p>SSID <input type=\"text\" name=\"SSID\" size=\"30\" maxlength=\"32\" value=\"\"></p><BR>"
        "<BR>"
        "<p>KEY <input type=\"text\" name=\"KEY\" size=\"30\" maxlength=\"64\" value=\"\"></p><BR>"
        "<BR>"
        "<p><input type=\"submit\" value=\"SEND\"></p>"
        "</form>"
        "</BODY>"
        "</HTML>";

const PROGMEM char postResp[] = 
                    "<HTML>"
                    "<BODY>"
                    "<h1>Please Reset</h1>"
                    "</BODY>"
                    "</HTML>";
