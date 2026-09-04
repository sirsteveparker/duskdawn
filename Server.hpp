AsyncWebServer server(80);
extern bool currentState;
extern bool requiredState;
extern const char* VERSION;
void setRelay(bool state);

const char* PARAM_FILTER = "filter";

int GetIdFromURL(AsyncWebServerRequest *request, String root)
{
  String string_id = request->url();
  string_id.replace(root, "");
  int id = string_id.toInt();
  return id;
}

String GetBodyContent(uint8_t *data, size_t len)
{
  String content = "";
  for (size_t i = 0; i < len; i++) {
    content .concat((char)data[i]);
  }
  return content;
}

void getAll(AsyncWebServerRequest *request)
{
  String message = "Get All";
  Serial.println(message);
  request->send(200, "text/plain", message);
}

void getFiltered(AsyncWebServerRequest *request)
{
  String message = "Get filtered by " + request->getParam(PARAM_FILTER)->value();
  Serial.println(message);
  request->send(200, "text/plain", message);
}

void getById(AsyncWebServerRequest *request)
{
  int id = GetIdFromURL(request, "/item/");

  String message = String("Get by Id ") + id;
  Serial.println(message);
  request->send(200, "text/plain", message);
}

void getStatus(AsyncWebServerRequest *request) {
//String response = "<html><head><title>Outside Lights</title></head><body class=sm>\n";
  String response = "<div class=\"text-theme-500 dark:text-theme-300 text-xs font-light service-description\">\n";
  response.concat("<form><table><tr>\n<td>" + request->host() + " Lights:</td> ");
  response.concat(currentState ? "<td>ON</td>" : "<td>OFF</td>");
  response.concat("</tr>\n<tr><td>Override: </td><td>On<input type='radio' id='on' id='post-btn' name='force' ");
  response.concat(requiredState ? "checked " : "");
  response.concat("value=\"on\">Off<input type='radio' id='off' name='force'  id='post-btn' ");
  response.concat(requiredState ? "" : "checked " );
  response.concat("value=\"off\"></td>\n</tr></table>\n");
  response.concat("<button type=\"submit\">Submit</button></form>\n");
  response.concat("<div>Version: " + String(VERSION) + "</div>\n");
  response.concat("<script>\nfunction handleSubmit(event) {\n\tevent.preventDefault();\n");
  response.concat("\ttry {\n");
  response.concat("\t\tconst data =  new FormData(event.target); \n");
  response.concat("\t\tconst value =  Object.fromEntries(data.entries()); \n");
  response.concat("\t\tvalue.force = data.getAll(\"force\");\n");
  response.concat("\t\tvar forcejson = JSON.stringify({ force : value.force[0]});\n"); 
  response.concat("\t\tfetch('/status', { \n");
  response.concat("\t\t\tmethod: 'post', \n");
  response.concat("\t\t\theaders: { 'Content-Type': 'application/json' }, \n");
  response.concat("\t\t\tbody: forcejson \n");
  response.concat("\t\t\t}); \n");
  response.concat("\t\tconsole.log('Completed!', forcejson);\n");
  response.concat("\t\t}\n\tcatch(err) { \n");
  response.concat("\t\tconsole.error(`Error: ${err}`); \n");
  response.concat("\t\t}\n\tfetch('/status',{method:'get'});\n}\n");
  response.concat("\nconst form = document.querySelector(\"form\");\n");
  response.concat("form.addEventListener(\"submit\", handleSubmit);\n</script>");
  response.concat("\n</div>");
//response.concat("\n</script></body></html>");
  request->send(200, "text/html", response);
  syslog.printf("REST status request from....%s\n",request->client()->remoteIP().toString().c_str());  
}

void postStatus(AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total)
{
  String bodyContent = GetBodyContent(data, len);
  syslog.printf("REST postStatus called from %s with body: %s\n",request->client()->remoteIP().toString().c_str(),bodyContent.c_str());
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, bodyContent);
  if (error) { request->send(400); return;}

  if ( doc["force"] ) {
    String string_data = doc["force"];
    String message = string_data;
    syslog.printf("REST postStatus called from %s with body: %s\n",request->client()->remoteIP().toString().c_str(),message.c_str());
    if ( string_data == "on") {
      requiredState = true ;
      setRelay(true);
      request->send(200, "text/plain", message);
    } else if (string_data == "off") {
      requiredState = false;
      request->send(200, "text/plain", message);
    } else {
      request->send(418, "text/plain", message);
    } 
  } else {
    String message = "<html><body><p>&#129300;</p></body></html>" ;
    syslog.printf("REST postStatus called from %s with body: %s\n",request->client()->remoteIP().toString().c_str(),message.c_str());
    request->send(406, "text/html", message);
  }
}

void getRequest(AsyncWebServerRequest *request) {
  
  if (request->hasParam(PARAM_FILTER)) {
    getFiltered(request);
  }
  else if(request->url().indexOf("/item/") != -1)
  {
    getById(request);
  }
  else {
    getAll(request);
  }
}

void postRequest(AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total)
{ 
  String bodyContent = GetBodyContent(data, len);
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, bodyContent);
  if (error) { request->send(400); return;}

  String string_data = doc["data"];
  String message = "Create " + string_data;
  syslog.printf("REST postRequest called from %s with body: %s\n",request->client()->remoteIP().toString().c_str(),bodyContent.c_str());
  request->send(200, "text/plain", message);
}

void patchRequest(AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total)
{
  int id = GetIdFromURL(request, "/item/");
  String bodyContent = GetBodyContent(data, len);
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, bodyContent);
  if (error) { request->send(400); return;}

  String string_data = doc["data"];
  String message = String("Update ") + id + " with " + string_data;
  Serial.println(message);
  request->send(200, "text/plain", message);
}

void putRequest(AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total)
{
  int id = GetIdFromURL(request, "/item/");
  String bodyContent = GetBodyContent(data, len);
   
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, bodyContent);
  if (error) { request->send(400); return;}

  String string_data = doc["data"];
  String message = String("Replace ") + id + " with " + string_data;
  Serial.println(message);
  request->send(200, "text/plain", message);
}

void deleteRequest(AsyncWebServerRequest *request) {
  int id = GetIdFromURL(request, "/item/");

  String message = String("Delete ") + id;
  Serial.println(message);
  request->send(200, "text/plain", message);
}

void homeRequest(AsyncWebServerRequest *request) {
   String response = "Outside Lights: ";
   response.concat(currentState ? "ON" : "OFF");
   request->send(200, "text/plain", response);
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void InitServer()
{
  server.on("/", HTTP_GET, homeRequest);
  server.on("/status", HTTP_GET, getStatus);
  server.on("/status", HTTP_POST, [](AsyncWebServerRequest * request){}, NULL, postStatus);
  server.on("/item", HTTP_GET, getRequest);
  server.on("/item", HTTP_POST, [](AsyncWebServerRequest * request){}, NULL, postRequest);
  server.on("/item", HTTP_PUT, [](AsyncWebServerRequest * request){}, NULL, putRequest);
  server.on("/item", HTTP_PATCH, [](AsyncWebServerRequest * request){}, NULL, patchRequest);
  server.on("/item", HTTP_DELETE, deleteRequest);
  
  server.onNotFound(notFound);

  server.begin();
  syslog.println("HTTP server started");
}
