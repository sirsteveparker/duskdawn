AsyncWebServer server(80);

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
  Serial.println("HTTP server started");
  syslog.println("HTTP server started");
}
