#ifndef __MOPIE_AI_H__
#define __MOPIE_AI_H__

#include <Arduino.h>

// Mopie AI — on-device LLM chatbot with device control
// Connect WiFi, store API key (OpenAI-compatible endpoint), chat, and let
// the AI run tool commands on the ESP32.

void mopie_ai_setup();   // main entry point

// Persisted settings helpers (used by AI menu + Config + webUI)
String aiGetApiKey();
void   aiSetApiKey(const String &key);
String aiGetWifiSsid();
void   aiSetWifiSsid(const String &ssid);
String aiGetEndpoint();
void   aiSetEndpoint(const String &ep);
String aiGetModel();
void   aiSetModel(const String &model);

// Web dashboard support
String aiFetchModels();                                    // GET {models:[...]}
bool   aiChatWeb(const String &q, String &reply, String &execResult);

#endif
