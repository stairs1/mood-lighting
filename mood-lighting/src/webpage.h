/*
 * This is gross but it is a temporary measure until either:
 *      1) figure out how to embed files in compiled binary
 *           - there were issues with this using platformio
 *      2) figure out how to load files onto ESP fs & access them
 * 
 * That being said, this works just fine for now. Introduction of javascript
 * would be a good time to look into ESP32 files
 */
static const char *html =
    "<body>"
    "   <form method=\"post\">"
    "       <label>Red:   </label>"
    "           <input name=\"red\">"
    "       <div/>"
    "       <label>Green: </label>"
    "           <input name=\"green\">"
    "       <div/>"
    "       <label>Blue:  </label>"
    "           <input name=\"blue\">"
    "       <button>Submit</button>"
    "   </form>"
    "</body>";