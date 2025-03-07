#define ACH_USE_TERMINALUI

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include "ach/util/Commons.hh"
#include "ach/sys/Init.hh"
#include "ach/uic/Protocol.hh"
#include <wv.hh>
#include <cJSON.h>
#include <log.hh>
#include "ach/uic/Brain.hh"
#include "ach/sys/Schedule.hh"
#include "ach/uic/UserData.hh"
#include "ach/core/platform/OSType.hh"

typedef struct
{
  bool lock;
  double scale;
  webview_t window;
} TellSizeData;

int
main(int argc, char **argv)
{
  Alicorn::Commons::argv0 = argv[0];
  std::vector<std::string> args;
  for(int i = 0; i < argc; i++)
    {
      std::string xi = argv[i];
      args.push_back(xi);
    }
  if(argc <= 1)
    {
      args.push_back("");
    }

  webview_t w = webview_create(true, nullptr);
  webview_set_size(w, 960, 540, WEBVIEW_HINT_NONE);

  auto tsd = new TellSizeData;
  tsd->window = w;
  tsd->scale = 0.7;
  tsd->lock = true;
  webview_bind(
      w, "tellSize",
      [](const char *seq, const char *req, void *arg) -> void {
        auto ts = (TellSizeData *)arg;
        if(ts->lock
           && Alicorn::Platform::OS_TYPE != Alicorn::Platform::OS_DARWIN)
          {
            ts->lock = false;
            webview_t loginWindow = ts->window;
            int scrnW = 1920, scrnH = 1080, vw = 960, vh = 540;
            cJSON *a = cJSON_Parse(req);
            if(cJSON_IsArray(a))
              {
                if(cJSON_GetArraySize(a) != 4)
                  {
                    cJSON_Delete(a);
                    return;
                  }
                scrnW = cJSON_GetNumberValue(cJSON_GetArrayItem(a, 0));
                scrnH = cJSON_GetNumberValue(cJSON_GetArrayItem(a, 1));
                vw = cJSON_GetNumberValue(cJSON_GetArrayItem(a, 2));
                vh = cJSON_GetNumberValue(cJSON_GetArrayItem(a, 3));
              }
            int aw = (960.0 / vw) * scrnW * ts->scale;
            int ah = (540.0 / vh) * scrnH * ts->scale;
            webview_set_size(loginWindow, aw, ah, WEBVIEW_HINT_NONE);
            cJSON_Delete(a);
            delete ts;
          }
      },
      tsd);

  if(args[1] == std::string("mslogin"))
    {
      tsd->scale = 0.8; // Make larger
      // Start login
      webview_init(
          w,
          "window.onload=function(){window.tellSize(screen.availWidth,screen."
          "availHeight,window.outerWidth,window.outerHeight);var "
          "u=window.location.href;if(u.includes(\"code=\")){u=u.split(\"code="
          "\")[1];if(u.includes(\"&lc=\"))u=u.split(\"&lc=\")[0];window."
          "tellCode(u);}}");
      webview_navigate(
          w, "https://login.live.com/"
             "oauth20_authorize.srf?client_id=00000000402b5328&response_type="
             "code&scope=service%3A%3Auser.auth.xboxlive.com%3A%3AMBI_SSL&"
             "redirect_uri=https%3A%2F%2Flogin.live."
             "com%2Foauth20_desktop.srf");
      webview_set_title(w, "Login with Microsoft");
      webview_bind(
          w, "tellCode",
          [](const char *seq, const char *req, void *arg) -> void {
            webview_t loginWindow = (webview_t)arg;
            cJSON *params = cJSON_Parse(req);
            if(cJSON_IsArray(params))
              {
                cJSON *code = cJSON_GetArrayItem(params, 0);
                if(cJSON_IsString(code))
                  {
                    std::cout << cJSON_GetStringValue(code) << std::endl;
                  }
              }
            cJSON_Delete(params);
            Alicorn::Sys::runOnMainThread(
                [loginWindow]() -> void {
                  webview_destroy(loginWindow);
                  webview_terminate(loginWindow);
                },
                loginWindow);
          },
          w);
      webview_run(w);
    }
  else if(args[1] == std::string("modrinth"))
    {
      tsd->scale = 0.8; // Webpages
      // Load modrinth
      std::ifstream jsf("ModrinthTweak.js");
      std::stringstream jss;
      jss << jsf.rdbuf();
      std::string js = jss.str();
      webview_set_title(w, "Modrinth");
      webview_init(
          w,
          ("window.onload=function(){window.tellSize(screen.availWidth,screen."
           "availHeight,window.outerWidth,window.outerHeight);"
           + js + "}")
              .c_str());
      webview_navigate(w, "https://modrinth.com/mods");
      webview_run(w);
      return 0;
    }
  else
    {
      // Run init
      tsd->scale = 0.6; // Application
      using namespace Alicorn;
      Sys::initSys();
      UIC::initMainWindow(w);
      webview_set_title(w, "Ari 2023");
      UIC::bindListener("Ping",
                        [](const std::string &s, UIC::Callback cb) -> void {
                          cb("\"...It's courage.\"");
                        });
      UIC::bindListener(
          "Ready",
          [w](const std::string &dat, UIC::Callback cb) -> void {
            LOG("Start executing main entry script.");
            Sys::runOnWorkerThread([w]() -> void {
              UIC::runProgram(
                  "Main", []() -> void {}, UIC::getUserData());
            });
            cb("");
          },
          true);

      // Load Script
      std::ifstream jsf("Main.js");
      std::stringstream jss;
      jss << jsf.rdbuf();
      std::string js = jss.str();
      auto boot = "window.addEventListener('load', ()=>{" + js + "});";
      webview_init(w, boot.c_str());
      if(Platform::OS_TYPE == Platform::OS_DARWIN)
        {
          webview_navigate(w, "about:blank");
        }
      else
        {
          webview_navigate(w,
                           "data:text/html,<!DOCTYPE "
                           "html><html><body><style>body{text-align:center;} "
                           "h1{width:100%;} p{width:100%;}</"
                           "style><h1>Please Wait...</h1><p>Loading "
                           "components</p></body></html>");
        }
      webview_run(w);
      Sys::saveData();
      Sys::downSys();
    }

  return 0;
}