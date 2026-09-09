#include "webInterface.h"
#include "core/display.h"    // using displayRedStripe as error msg
#include "core/mykeyboard.h" // using keyboard when calling rename
#include "core/passwords.h"
#include "core/ram_profile.h"
#include "core/sd_functions.h" // using sd functions called to rename and manage sd files
#include "core/serialcmds.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h" // using common wifisetup
#include "modules/ai/mopie_ai.h"
#include "esp_task_wdt.h"
#include "webFiles.h"
#include <MD5Builder.h>
#include <cstddef>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>
#include <globals.h>

File uploadFile;
FS _webFS = LittleFS;
// WiFi as a Client
const int default_webserverporthttp = 80;

// WiFi as an Access Point
IPAddress AP_GATEWAY(172, 0, 0, 1); // Gateway

AsyncWebServer *server = nullptr; // initialise webserver
const char *host = "mopie";
String uploadFolder = "";
static bool mdnsRunning = false;

// Generate random token
String generateToken(int length = 24) {
    String token = "";
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < length; i++) { token += charset[random(0, sizeof(charset) - 1)]; }
    return token;
}

/**********************************************************************
**  Function: stopWebUi
**  Turn off the WebUI
**********************************************************************/
void stopWebUi() {
    tft.setLogging(false);
    isWebUIActive = false;
    server->end();
    server->~AsyncWebServer();
    free(server);
    server = nullptr;
    if (mdnsRunning) {
        MDNS.end();
        mdnsRunning = false;
    }
}

/**********************************************************************
**  Function: cleanlyStopWebUiForWiFiFeature
**  Cleanly stop WebUI and AP mode before starting a WiFi feature
**  This prevents WiFi mode conflicts when features need exclusive control
**********************************************************************/
void cleanlyStopWebUiForWiFiFeature() {
    // Only proceed if WebUI is active
    if (!isWebUIActive && !server) { return; }

    // Brief notification (non-blocking)
    Serial.println("Stopping WebUI for WiFi feature...");

    // Stop the WebUI
    if (server) {
        stopWebUi();
        // Give the web server time to fully shut down
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Disconnect WiFi AP mode if it's the WebUI's AP
    // Check if we're in AP or APSTA mode (used by WebUI)
    wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA) {
        wifiDisconnect();
        // Give WiFi time to fully disconnect
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    Serial.println("WebUI stopped, starting WiFi feature...");
}
/**********************************************************************
**  Function: loopOptionsWebUi
**  Display options to launch the WebUI
**********************************************************************/
void loopOptionsWebUi() {
    if (isWebUIActive) {
        bool opt = WiFi.getMode() - 1;
        options = {
            {"Stop WebUI", stopWebUi},
            {"WebUi screen", lambdaHelper(startWebUi, opt)}
        };
        addOptionToMainMenu();
        loopOptions(options);
        return;
    }
    options = {
        {"my Network", lambdaHelper(startWebUi, false)},
        {"AP mode",    lambdaHelper(startWebUi, true) },
    };

    loopOptions(options);
    // On fail installing will run the following line
}

/**********************************************************************
**  Function: humanReadableSize
** Make size of files human readable
** source: https://github.com/CelliesProjects/minimalUploadAuthESP32
**********************************************************************/
String humanReadableSize(uint64_t bytes) {
    if (bytes < 1024) return String(bytes) + " B";
    else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " kB";
    else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
    else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

/**********************************************************************
**  Function: listFiles
**  list all of the files, if ishtml=true, return html rather than simple text
**********************************************************************/
String listFiles(FS &fs, const String &folder) {
    // log_i("Listfiles Start");
    String returnText = "pa:" + folder + ":0\n";
    // Serial.println("Listing files stored on SD");

    _webFS = fs;

    File root = fs.open(folder);
    uploadFolder = folder;

    while (true) {
        bool isDir;
        String fullPath = root.getNextFileName(&isDir);
        String nameOnly = fullPath.substring(fullPath.lastIndexOf("/") + 1);
        if (fullPath == "") { break; }
        // Serial.printf("Path: %s (isDir: %d)\n", fullPath.c_str(), isDir);

        if (esp_get_free_heap_size() > (String("Fo:" + nameOnly + ":0\n").length()) + 1024) {
            if (isDir) {
                // Serial.printf("Directory: %s\n", fullPath.c_str());
                returnText += "Fo:" + nameOnly + ":0\n";
            } else {
                // For files, we need to get the size, so we open the file briefly
                // Serial.printf("Opening file for size check: %s\n", fullPath.c_str());
                File file = fs.open(fullPath);
                // Serial.printf("File size: %llu bytes\n", file.size());
                if (file) {
                    returnText += "Fi:" + nameOnly + ":" + humanReadableSize(file.size()) + "\n";
                    file.close();
                }
            }
        } else break;
        delay(1);
    }
    root.close();
    // log_i("ListFiles End");
    return returnText;
}

/**********************************************************************
**  Function: checkUserWebAuth
** used by server->on functions to discern whether a user has the correct
** httpapitoken OR is authenticated by username and password
**********************************************************************/
bool checkUserWebAuth(AsyncWebServerRequest *request, bool onFailureReturnLoginPage = false) {
    if (request->hasHeader("Cookie")) {
        const AsyncWebHeader *cookie = request->getHeader("Cookie");
        String c = cookie->value();
        int idx = c.indexOf("MOPIESESSION=");
        if (idx != -1) {
            int start = idx + 13;
            int end = c.indexOf(';', start);
            if (end == -1) end = c.length();
            String token = c.substring(start, end);
            if (mopieConfig.isValidWebUISession(token)) { return true; }
        }
    }
    if (onFailureReturnLoginPage) {
        serveWebUIFile(request, "login.html", "text/html", true, login_html, login_html_size);
    } else {
        request->send(401, "text/plain", "Unauthorized");
    }
    return false;
}

/**********************************************************************
**  Function: createDirRecursive
** Create folders recursivelly
**********************************************************************/
void createDirRecursive(const String &path, FS fs) {
    String currentPath = "";
    int startIndex = 0;
    // Serial.print("Verifying folder: ");
    // Serial.println(path);

    while (startIndex < path.length()) {
        int endIndex = path.indexOf("/", startIndex);
        if (endIndex == -1) endIndex = path.length();

        currentPath += path.substring(startIndex, endIndex);
        if (currentPath.length() > 0) {
            if (!fs.exists(currentPath)) {
                fs.mkdir(currentPath);
                // Serial.print("Creating folder: ");
                // Serial.println(currentPath);
            }
        }

        if (endIndex < path.length()) { currentPath += "/"; }
        startIndex = endIndex + 1;
    }
}
/**********************************************************************
**  Function: handleUpload
** handles uploads to the filserver
**********************************************************************/
void handleUpload(
    AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final
) {
    if (checkUserWebAuth(request)) {
        if (uploadFolder == "/") uploadFolder = "";
        if (!index) {
            if (request->hasArg("password")) filename = filename + ".enc";
            // Serial.println("File: " + uploadFolder + "/" + filename);
            String relativePath = filename;
            String fullPath = uploadFolder + "/" + relativePath;
            String dirPath = fullPath.substring(0, fullPath.lastIndexOf("/"));
            if (dirPath.length() > 0) { createDirRecursive(dirPath, _webFS); }
        RETRY:
            request->_tempFile = _webFS.open(uploadFolder + "/" + filename, "w");
            if (!request->_tempFile) {
                // Serial.println("Failed to open file for writing: " + uploadFolder + "/" + filename);
                goto RETRY;
            }
        }

        if (len) {
            if (request->hasArg("password")) {
                // encryption requested
                static int chunck_no = 0;
                if (chunck_no != 0) {
                    // TODO: handle multiple chunks
                    request->send(404, "text/html", "file is too big");
                    return;
                } else chunck_no += 1;
                String enc_password = request->arg("password");
                String plaintext = String((char *)data).substring(0, len);
                String cyphertxt = encryptString(plaintext, enc_password);
                if (cyphertxt == "") { return; }
                if (request->_tempFile)
                    request->_tempFile.write((const uint8_t *)cyphertxt.c_str(), cyphertxt.length());
            } else {
                if (request->_tempFile) request->_tempFile.write(data, len);
            }
        }
        if (final) {
            // close the file handle as the upload is now done
            if (request->_tempFile) request->_tempFile.close();
        }
    }
}

void notFound(AsyncWebServerRequest *request) { request->send(404, "text/plain", "Nothing in here Sharky"); }

/**********************************************************************
**  Function: drawWebUiScreen
**  Draw information on screen of WebUI.
**********************************************************************/
void drawWebUiScreen(bool mode_ap) {
    drawMainBorderWithTitle("Mopie Agent", true);

    String txt;
    if (!mode_ap) txt = WiFi.localIP().toString();
    else txt = WiFi.softAPIP().toString();

    int padX = 14;
    int currentY = 50;

    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
    tft.setTextSize(FP);

    if (mode_ap) {
        tft.setCursor(padX, currentY);
        tft.print("Net: MopieNet/mopienet");
        currentY += LH * FP + 6;
    }

    tft.setCursor(padX, currentY);
    tft.setTextColor(DEFAULT_ACCENT, mopieConfig.bgColor);
    tft.print("Agent: http://" + txt + "/ai");
    currentY += LH * FP + 7;
    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);

    tft.setCursor(padX, currentY);
    tft.print("IP:  " + txt);
    currentY += LH * FP + 6;

    tft.setCursor(padX, currentY);
    tft.setTextColor(mopieConfig.secColor, mopieConfig.bgColor);
    tft.print("Usr: " + String(mopieConfig.webUI.user));
    currentY += LH * FP + 5;

    tft.setCursor(padX, currentY);
    tft.print("Pwd: " + String(mopieConfig.webUI.pwd));

    tft.setTextColor(TFT_RED, mopieConfig.bgColor);
    tft.setTextSize(FP);
    tft.drawCentreString("open /ai for the Agent chat", tftWidth / 2, tftHeight - 3 * LH, 1);
    tft.drawCentreString("press Esc to stop", tftWidth / 2, tftHeight - 2 * LH, 1);

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
}

/**********************************************************************
**  Function: color565ToWebHex
**  convert 565 color to web hex format for theme purposes
**********************************************************************/
String color565ToWebHex(uint16_t color565) {
    // Extract RGB components from 565
    uint8_t r = (color565 >> 11) & 0x1F;
    uint8_t g = (color565 >> 5) & 0x3F;
    uint8_t b = color565 & 0x1F;

    // Scale up to 8 bits
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);

    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    return String(hex);
}

/**********************************************************************
**  Function: serveWebUIFile
**  serves files for WebUI and checks for custom WebUI files
**********************************************************************/
void serveWebUIFile(AsyncWebServerRequest *request, const String &filename, const char *contentType) {
    serveWebUIFile(request, filename, contentType, false, nullptr, 0);
}
void serveWebUIFile(
    AsyncWebServerRequest *request, const String &filename, const char *contentType, bool gzip,
    const uint8_t *originaFile, uint32_t originalFileSize
) {
    AsyncWebServerResponse *response = nullptr;
    FS *fs = NULL;
    if (setupSdCard()) {
        if (SD.exists("MopieWebUI/" + filename)) fs = &SD;
    } else if (LittleFS.exists("MopieWebUI/" + filename)) {
        fs = &LittleFS;
    }
    if (fs) {
        response = request->beginResponse(*fs, "MopieWebUI/" + filename, contentType);
    } else {
        if (filename == "theme.css") {
            String css = ":root{--color:" + color565ToWebHex(mopieConfig.priColor) +
                         ";--sec-color:" + color565ToWebHex(mopieConfig.secColor) +
                         ";--background:" + color565ToWebHex(mopieConfig.bgColor) + ";}";
            AsyncWebServerResponse *themeResponse = request->beginResponse(200, "text/css", css);
            request->send(themeResponse);
            return;
        }
        response = request->beginResponse(200, String(contentType), originaFile, originalFileSize);
        if (gzip) {
            if (!response->addHeader("Content-Encoding", "gzip")) log_e("Failed to add gzip header");
        }
    }
    request->send(response);
}

/**********************************************************************
**  Function: startMdnsResponder
**  Try to start mDNS only if there is enough internal heap available
**********************************************************************/
static bool startMdnsResponder() {
    RAM_LOG("before MDNS");

    if (!MDNS.begin(host)) {
        RAM_LOG("MDNS failed");
        Serial.printf("Error setting up MDNS responder!\n");
        return false;
    }

    RAM_LOG("after MDNS");
    return true;
}

/**********************************************************************
**  Function: configureWebServer
**  configure web server
**********************************************************************/
void configureWebServer() {
    mdnsRunning = startMdnsResponder();
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server->onNotFound(notFound);

    // Index — land on the Mopie Agent chat by default (the primary face of the device)
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Redirect straight to the Mopie Agent dashboard, the primary landing page
        AsyncWebServerResponse *res = request->beginResponse(302);
        res->addHeader("Location", "/ai");
        request->send(res);
    });

    // Login
    server->on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("username", true) && request->hasParam("password", true)) {
            String username = request->getParam("username", true)->value();
            String password = request->getParam("password", true)->value();

            if (username == mopieConfig.webUI.user && password == mopieConfig.webUI.pwd) {
                String token = generateToken();
                AsyncWebServerResponse *response = request->beginResponse(302);
                response->addHeader("Location", "/");
                response->addHeader("Set-Cookie", "MOPIESESSION=" + token + "; Path=/; HttpOnly");
                request->send(response);
                mopieConfig.addWebUISession(token);
                return;
            }
        }
        AsyncWebServerResponse *response = request->beginResponse(302);
        response->addHeader("Location", "/?failed");
        request->send(response);
    });

    // Logout
    server->on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasHeader("Cookie")) {
            const AsyncWebHeader *cookie = request->getHeader("Cookie");
            String c = cookie->value();
            int idx = c.indexOf("MOPIESESSION=");
            if (idx != -1) {
                int start = idx + 13;
                int end = c.indexOf(';', start);
                if (end == -1) end = c.length();
                String token = c.substring(start, end);
                mopieConfig.removeWebUISession(token);
            }
        }
        AsyncWebServerResponse *response = request->beginResponse(302);
        response->addHeader("Location", "/?loggedout");
        response->addHeader("Set-Cookie", "MOPIESESSION=0; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        request->send(response);
    });

    // Static files
    server->on("/theme.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        serveWebUIFile(request, "theme.css", "text/css");
    });
    server->on("/index.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        serveWebUIFile(request, "index.css", "text/css", true, index_css, index_css_size);
    });
    server->on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        serveWebUIFile(request, "index.js", "text/javascript", true, index_js, index_js_size);
    });

    // System Info
    server->on("/systeminfo", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            char response_body[300];
            uint64_t LittleFSTotalBytes = LittleFS.totalBytes();
            uint64_t LittleFSUsedBytes = LittleFS.usedBytes();
            uint64_t SDTotalBytes = SD.totalBytes();
            uint64_t SDUsedBytes = SD.usedBytes();
            snprintf(
                response_body,
                sizeof(response_body),
                "{\"%s\":\"%s\",\"SD\":{\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"},"
                "\"LittleFS\":{\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"}}",
                "MOPIE_VERSION",
                MOPIE_VERSION,
                "free",
                humanReadableSize(SDTotalBytes - SDUsedBytes).c_str(),
                "used",
                humanReadableSize(SDUsedBytes).c_str(),
                "total",
                humanReadableSize(SDTotalBytes).c_str(),
                "free",
                humanReadableSize(LittleFSTotalBytes - LittleFSUsedBytes).c_str(),
                "used",
                humanReadableSize(LittleFSUsedBytes).c_str(),
                "total",
                humanReadableSize(LittleFSTotalBytes).c_str()
            );
            request->send(200, "application/json", response_body);
        }
    });

    // Get Screen
    server->on("/getscreen", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            static uint8_t *screenBinBuffer = nullptr;
            static size_t screenBinBufferSize = 0;

            if (!screenBinBuffer) {
                size_t desiredSize = MAX_LOG_ENTRIES * MAX_LOG_SIZE;
                if (psramFound()) screenBinBuffer = static_cast<uint8_t *>(ps_malloc(desiredSize));
                if (!screenBinBuffer) screenBinBuffer = static_cast<uint8_t *>(malloc(desiredSize));
                if (!screenBinBuffer) {
                    request->send(503, "text/plain", "Insufficient memory for screen buffer");
                    return;
                }
                screenBinBufferSize = desiredSize;
            }

            size_t binSize = 0;
            tft.getBinLog(screenBinBuffer, binSize);
            if (binSize > screenBinBufferSize) {
                request->send(500, "text/plain", "Screen buffer overflow");
                return;
            }
            request->send(200, "application/octet-stream", (const uint8_t *)screenBinBuffer, binSize);
        }
    });

    // Rename file or folder
    server->on("/rename", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            if (request->hasArg("fileName") && request->hasArg("filePath")) {
                String fs = request->arg("fs").c_str();
                String fileName = request->arg("fileName").c_str();
                String filePath = request->arg("filePath").c_str();
                String filePath2 = filePath.substring(0, filePath.lastIndexOf('/') + 1) + fileName;
                // Rename the file of folder
                if (fs == "SD") {
                    if (SD.rename(filePath, filePath2))
                        request->send(200, "text/plain", filePath + " renamed to " + filePath2);
                    else request->send(200, "text/plain", "Fail renaming file.");
                } else {
                    if (LittleFS.rename(filePath, filePath2))
                        request->send(200, "text/plain", filePath + " renamed to " + filePath2);
                    else request->send(200, "text/plain", "Fail renaming file.");
                }
            }
        }
    });

    // Route to send a generic command (Tasmota compatible API)
    // https://tasmota.github.io/docs/Commands/#with-web-requests
    server->on("/cm", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!checkUserWebAuth(request)) { return; }
        if (request->hasArg("cmnd")) {
            String cmnd = request->arg("cmnd");
            if (cmnd.startsWith("nav")) {
                volatile bool *var = &SelPress;
                if (cmnd.startsWith("nav sel")) var = &SelPress;
                if (cmnd.startsWith("nav esc")) var = &EscPress;
                if (cmnd.startsWith("nav up")) var = &UpPress;
                if (cmnd.startsWith("nav down")) var = &DownPress;
                if (cmnd.startsWith("nav next")) var = &NextPress;
                if (cmnd.startsWith("nav prev")) var = &PrevPress;
                request->send(200, "text/plain", "command " + cmnd + " success");
                int time;
                if (cmnd.endsWith("0")) time = cmnd.substring(cmnd.lastIndexOf(' ')).toInt();
                else time = 10;
                auto tmp = millis() + time;
                while (tmp > millis()) {
                    AnyKeyPress = true;
                    SerialCmdPress = true;
                    *var = true;
                    if (!LongPress) vTaskDelay(pdMS_TO_TICKS(190));
                    else vTaskDelay(pdMS_TO_TICKS(50));
                }
            } else {
                if (parseSerialCommand(cmnd, false)) {
                    request->send(200, "text/plain", "command " + cmnd + " queued");
                } else {
                    request->send(400, "text/plain", "command failed, check the serial log for details");
                }
            }
        } else {
            request->send(400, "text/plain", "http request missing required arg: cmnd");
        }
    });

    // Reboot device
    server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) { ESP.restart(); }
    });

    // List files
    server->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            String folder = "/";
            if (request->hasArg("folder")) { folder = request->arg("folder"); }
            if (strcmp(request->arg("fs").c_str(), "SD") == 0) {
                request->send(200, "text/plain", listFiles(SD, folder));
            } else {
                request->send(200, "text/plain", listFiles(LittleFS, folder));
            }
        }
    });

    // Download, create folder and delete
    server->on("/file", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            if (request->hasArg("name") && request->hasArg("action")) {
                String fileName = request->arg("name").c_str();
                String fileAction = request->arg("action").c_str();
                String fileSys = request->arg("fs").c_str();
                bool useSD = false;
                if (fileSys == "SD") useSD = true;

                FS *fs;
                if (useSD) {
                    fs = &SD;
                } else fs = &LittleFS;

                log_i("filename: %s\n", fileName.c_str());
                log_i("fileAction: %s\n", fileAction.c_str());

                if (!fs->exists(fileName)) {
                    if (strcmp(fileAction.c_str(), "create") == 0) {
                        if (fs->mkdir(fileName)) {
                            request->send(200, "text/plain", "Created new folder: " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL creating folder: " + String(fileName));
                        }
                    } else if (strcmp(fileAction.c_str(), "createfile") == 0) {
                        File newFile = fs->open(fileName, FILE_WRITE, true);
                        if (newFile) {
                            newFile.close();
                            request->send(200, "text/plain", "Created new file: " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL creating file: " + String(fileName));
                        }
                    } else request->send(400, "text/plain", "ERROR: file does not exist");

                } else {
                    if (strcmp(fileAction.c_str(), "download") == 0) {
                        request->send(*fs, fileName, "application/octet-stream", true);
                    } else if (strcmp(fileAction.c_str(), "image") == 0) {
                        String extension = fileName.substring(fileName.lastIndexOf('.') + 1);
                        // https://www.iana.org/assignments/media-types/media-types.xhtml#image
                        if (extension == "jpg") extension = "jpeg"; // www.rfc-editor.org/rfc/rfc2046.html
                        request->send(*fs, fileName, "image/" + extension);
                    } else if (strcmp(fileAction.c_str(), "delete") == 0) {
                        if (deleteFromSd(*fs, fileName)) {
                            request->send(200, "text/plain", "Deleted : " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL deleting: " + String(fileName));
                        }
                    } else if (strcmp(fileAction.c_str(), "create") == 0) {
                        if (fs->mkdir(fileName)) {
                            request->send(200, "text/plain", "Created new folder: " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL creating folder: " + String(fileName));
                        }
                    } else if (strcmp(fileAction.c_str(), "createfile") == 0) {
                        File newFile = fs->open(fileName, FILE_WRITE, true);
                        if (newFile) {
                            newFile.close();
                            request->send(200, "text/plain", "Created new file: " + String(fileName));
                        } else {
                            request->send(200, "text/plain", "FAIL creating file: " + String(fileName));
                        }

                    } else if (strcmp(fileAction.c_str(), "edit") == 0) {
                        File editFile = fs->open(fileName, FILE_READ);
                        if (editFile) {
                            String fileContent = editFile.readString();
                            request->send(200, "text/plain", fileContent);
                            editFile.close();
                        } else {
                            request->send(500, "text/plain", "Failed to open file for reading");
                        }

                    } else {
                        request->send(400, "text/plain", "ERROR: invalid action param supplied");
                    }
                }
            } else {
                request->send(400, "text/plain", "ERROR: name and action params required");
            }
        }
    });

    // Edit file
    server->on("/edit", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            if (request->hasArg("name") && request->hasArg("content") && request->hasArg("fs")) {
                String fileName = request->arg("name");
                String fileContent = request->arg("content");
                bool useSD = false;

                if (strcmp(request->arg("fs").c_str(), "SD") == 0) { useSD = true; }

                fs::FS *fs = useSD ? (fs::FS *)&SD : (fs::FS *)&LittleFS;
                String fsType = useSD ? "SD" : "LittleFS";

                if (useSD) {              // LittleFS is already mounted
                    if (!setupSdCard()) { // only tries to mount SD if editting on SD
                        request->send(500, "text/plain", "Failed to initialize file system: " + fsType);
                        return;
                    }
                }

                File editFile = fs->open(fileName, FILE_WRITE);
                if (editFile) {
                    if (editFile.write((const uint8_t *)fileContent.c_str(), fileContent.length())) {
                        request->send(200, "text/plain", "File edited: " + fileName);
                    } else {
                        request->send(500, "text/plain", "Failed to write to file: " + fileName);
                    }
                    editFile.close();
                } else {
                    request->send(500, "text/plain", "Failed to open file for writing: " + fileName);
                }

            } else {
                request->send(400, "text/plain", "ERROR: name, content, and fs parameters required");
            }
        }
    });

    // File upload
    server->on(
        "/upload",
        HTTP_POST,
        [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "File upload completed"); },
        handleUpload
    );

    // Wi-Fi configuration
    server->on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (checkUserWebAuth(request)) {
            if (request->hasArg("usr") && request->hasArg("pwd")) {
                const char *usr = request->arg("usr").c_str();
                const char *pwd = request->arg("pwd").c_str();
                mopieConfig.setWebUICreds(usr, pwd);
                request->send(
                    200, "text/plain", "User: " + String(usr) + " configured with password: " + String(pwd)
                );
            }
        }
    });
    // ------------------------------------------------------------------
    // Mopie AI dashboard: chat from browser, config stored on device
    // ------------------------------------------------------------------
    server->on("/ai", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Mopie Agent dashboard — publicly reachable on LAN (the API key is the
        // real secret; the dashboard only edits it). No auth wall for easy chatbot use.
        AsyncWebServerResponse *res = request->beginResponse_P(
            200, "text/html", (const char*)R"HTML(<!DOCTYPE html>
<html><head><meta name=viewport content='width=device-width,initial-scale=1'>
<title>Mopie Agent</title><style>
body{background:#0d0d12;color:#e8e8f0;font-family:-apple-system,monospace;margin:0;padding:14px;max-width:760px;margin:auto}
h2{color:#4d9fff;border-bottom:1px solid #2a4a7a;padding-bottom:6px;letter-spacing:1px}
#log{height:48vh;overflow-y:auto;border:1px solid #2a2a3a;padding:10px;background:#0a0a0f;border-radius:6px}
.msg{margin:6px 0;padding:7px 10px;border-radius:8px;clear:both;max-width:82%}
.u{background:#1a3a6a;float:right;text-align:left}
.a{background:#1e1e2e;float:left;text-align:left}
.e{background:#4a1420;float:left;text-align:left;color:#ff9c9c}
input,button,select{width:100%;padding:9px;margin:4px 0;background:#1a1a26;color:#e8e8f0;border:1px solid #33334a;border-radius:6px;box-sizing:border-box}
button{background:#1a3a6a;cursor:pointer;font-weight:bold}
button:hover{background:#2a5a9a}
.row{display:flex;gap:8px}.row input{flex:1}
.tag{display:inline-block;background:#13233f;color:#6aa8ff;border-radius:10px;padding:1px 8px;font-size:11px}
</style></head><body>
<h2>MOPIE AGENT</h2>
<span class=tag>on-device</span> <span class=tag>ESP32</span> <span class=tag>M5StickC+</span>
<div id=status style='margin:8px 0;color:#6aa8ff;font-size:12px'>connecting...&nbsp;</div>
<div id=log></div>
<div class=row><input id=q placeholder='Ask the AI...' autocomplete=off><button id=go onclick=chat()>Go</button></div>
<div class=row><select id=model></select><button onclick=loadModels()>Refresh models</button></div>
<div class=row><input id=key placeholder='API key (stored on device)'><input id=ep placeholder='endpoint URL'></div>
<button onclick=saveCfg()>Save Config</button>
<script>
let dev='';
function setStatus(t){document.getElementById('status').textContent=t;}
async function cfg(){try{let r=await fetch('/ai/config');let j=await r.json();
 document.getElementById('key').value=j.key||'';document.getElementById('ep').value=j.endpoint||'';
 if(j.model)document.getElementById('model').value=j.model;
 setStatus('key '+(j.key?'SET':'MISSING')+' · '+(j.model||'no model'));}catch(e){setStatus('dashboard online');}}
async function loadModels(){let btn=document.querySelector('button[onclick=loadModels()]');btn.textContent='Loading...';
 let r=await fetch('/ai/models');let j=await r.json();
 if(j.error){setStatus('models: '+j.error);btn.textContent='Refresh models';return;}
 let s=document.getElementById('model');s.innerHTML='';
 (j.models||[]).forEach(m=>{let o=document.createElement('option');o.value=o.textContent=m;s.appendChild(o)});
 btn.textContent='Refresh models';setStatus((j.models||[]).length+' models detected');}
async function saveCfg(){setStatus('saving...');
 let r=await fetch('/ai/config',{method:'POST',
 body:'key='+encodeURIComponent(document.getElementById('key').value)
 +'&endpoint='+encodeURIComponent(document.getElementById('ep').value)
 +'&model='+encodeURIComponent(document.getElementById('model').value)});
 let j=await r.json();setStatus(j.ok?'saved':('err '+JSON.stringify(j)));addLog('cfg saved','e');}
async function chat(){let q=document.getElementById('q').value;if(!q)return;
 log('You: '+q,'u');document.getElementById('q').value='';
 document.getElementById('go').disabled=true;setStatus('thinking...');
 try{let r=await fetch('/ai/chat?q='+encodeURIComponent(q));let j=await r.json();
  if(j.reply){log('AI: '+j.reply,'a');if(j.exec)log('[device] '+j.exec,'e');
   setStatus('ready');}else{setStatus('error');log('ERR: '+(j.error||'?'),'e');}}
 catch(e){setStatus('network error');log('ERR: request failed','e');}
 document.getElementById('go').disabled=false;}
function log(t,c){let d=document.createElement('div');d.className='msg '+c;d.textContent=t;
 d.style.cssText='clear:both;padding:6px;margin:4px;border-radius:4px;'+(c==='u'?'background:#331;text-align:right':c==='a'?'background:#222':'background:#400');
 document.getElementById('log').appendChild(d);document.getElementById('log').scrollTop=1e9;}
document.getElementById('q').addEventListener('keydown',e=>{if(e.key==='Enter')chat()});
cfg();  // load saved config; models require Refresh to avoid auto TLS OOM on small-RAM chip
</script></body></html>)HTML");
        res->addHeader("Cache-Control", "no-store");
        request->send(res);
    });

    // config GET/POST: key, endpoint, model
    server->on("/ai/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"key\":\"" + aiGetApiKey() + "\",\"endpoint\":\"" + aiGetEndpoint() +
                      "\",\"model\":\"" + aiGetModel() + "\"}";
        request->send(200, "application/json", json);
    });
    server->on("/ai/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasArg("key")) aiSetApiKey(request->arg("key"));
        if (request->hasArg("endpoint")) aiSetEndpoint(request->arg("endpoint"));
        if (request->hasArg("model")) aiSetModel(request->arg("model"));
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // list models from upstream provider
    server->on("/ai/models", HTTP_GET, [](AsyncWebServerRequest *request) {
        String out = aiFetchModels();
        request->send(200, "application/json", out);
    });

    // chat proxy: browser asks, device forwards w/ key, executes device cmds in reply
    server->on("/ai/chat", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasArg("q")) { request->send(400, "application/json", "{\"error\":\"q required\"}"); return; }
        String reply, execResult;
        bool ok = aiChatWeb(request->arg("q"), reply, execResult);
        if (!ok) { request->send(502, "application/json", "{\"error\":\"" + reply + "\"}"); return; }
        // escape quotes for JSON
        reply.replace("\\", "\\\\");
        reply.replace("\"", "\\\"");
        reply.replace("\n", "\\n");
        String json = "{\"reply\":\"" + reply + "\"";
        if (execResult.length()) {
            execResult.replace("\"", "'");
            json += ",\"exec\":\"" + execResult + "\"";
        }
        json += "}";
        request->send(200, "application/json", json);
    });
    server->begin();
    Serial.println("Webserver started");
}

/**********************************************************************
**  Function: startWebUi
**  Start the WebUI
**********************************************************************/
void startWebUi(bool mode_ap) {
    bool keepWifiConnected = false;
    if (!WiFi.isConnected()) {
        if (mode_ap) wifiConnectMenu(WIFI_AP);
        else wifiConnectMenu(WIFI_STA);
    } else {
        keepWifiConnected = true;
    }

    // configure web server

    if (!server) {
        // Clear this vector to free stack memory
        options.clear();

        Serial.println("Configuring Webserver ...");
        if (psramFound()) server = (AsyncWebServer *)ps_malloc(sizeof(AsyncWebServer));
        else server = (AsyncWebServer *)malloc(sizeof(AsyncWebServer));

        new (server) AsyncWebServer(default_webserverporthttp);

        configureWebServer();

        isWebUIActive = true;
    }
    tft.setLogging();
    drawWebUiScreen(mode_ap);
#ifdef HAS_SCREEN // Headless always run in the background!
    while (!check(EscPress)) {
        // nothing here, just to hold the screen until the server is on.
        vTaskDelay(pdMS_TO_TICKS(70));
    }

    bool closeServer = false;

    options.clear();
    options.emplace_back("Run in background", []() {});
    options.emplace_back("Exit", [&closeServer]() { closeServer = true; });

    loopOptions(options);

    if (closeServer) {
        stopWebUi();
        vTaskDelay(pdMS_TO_TICKS(100));
        if (!keepWifiConnected) { wifiDisconnect(); }
    }
#endif
}
