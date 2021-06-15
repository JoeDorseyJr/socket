#ifndef WEBVIEW_H
#define WEBVIEW_H

typedef void *webview_t;

#define WEBVIEW_HINT_NONE 0  // Width and height are default size
#define WEBVIEW_HINT_MIN 1   // Width and height are minimum bounds
#define WEBVIEW_HINT_MAX 2   // Width and height are maximum bounds
#define WEBVIEW_HINT_FIXED 3 // Window size can not be changed by a user

#ifndef WEBVIEW_HEADER

#include <functional>
#include <map>
#include <string>

namespace Opkit {
  using dispatch_fn_t = std::function<void()>;
  std::map<std::string, std::string> appData;
} // namespace Opkit

#ifdef __linux__
#include "linux.h"
#elif __APPLE__
#include "darwin.h"
#elif _WIN32
#include "win.h"
#endif

namespace Opkit {

class webview : public browser_engine {
  public:
    webview(bool debug = false, void *wnd = nullptr)
      : browser_engine(debug, wnd) {}

    void navigate(const std::string url) {
      browser_engine::navigate(url);
    }

    using binding_t = std::function<void(std::string, std::string, void *)>;
    using binding_ctx_t = std::pair<binding_t *, void *>;

    using sync_binding_t = std::function<void(std::string, std::string)>;
    using sync_binding_ctx_t = std::pair<webview *, sync_binding_t>;

    void ipc(const std::string name, sync_binding_t fn) {
      ipc(
        name,
        [](std::string seq, std::string req, void *arg) {
          auto pair = static_cast<sync_binding_ctx_t *>(arg);
          pair->second(seq, req);
        },
        new sync_binding_ctx_t(this, fn)
      );
    }

    void ipc(const std::string name, binding_t f, void *arg) {
      init(
        "(() => {"
        "  const name = '" + name + "';"
        "  window.system[name] = value => window._ipc.send(name, value);"
        "})()"
      );

      bindings[name] = new binding_ctx_t(new binding_t(f), arg);
    }

    void dialog(std::string seq) {
      dispatch([=]() {
        auto result = createNativeDialog(
          NOC_FILE_DIALOG_OPEN | NOC_FILE_DIALOG_DIR,
          NULL,
          NULL,
          NULL
        );

        eval(
          "(() => {"
          "  window._ipc[" + seq + "].resolve(`" + result + "`);"
          "  delete window._ipc[" + seq + "];"
          "})();"
        );
      });
    }

    void resolve(const std::string msg) {
      dispatch([=]() {
        eval("(() => { window._ipc.resolve(`" + msg + "`); })()");
      });
    }

    void emit(const std::string event, const std::string data) {
      dispatch([=]() {
        eval(
          "(() => {"
          "  let detail;"
          "  try {"
          "    detail = JSON.parse(decodeURIComponent(`" + data + "`));"
          "  } catch (err) {"
          "    console.error(`Unable to parse (${detail})`);"
          "    return;"
          "  }"
          "  const event = new window.CustomEvent('" + event + "', { detail });"
          "  window.dispatchEvent(event);"
          "})()"
        );
      });
    }

    void binding(const std::string msg) {
      auto parts = split(msg, ';');

      auto name = parts[0];
      auto args = replace(msg, "^\\w+;", "");

      if (bindings.find(name) == bindings.end()) {
        return;
      }

      auto fn = bindings[name];

      dispatch([=]() {
        (*fn->first)("0", args, fn->second);
      });
    }

  private:
    void on_message(const std::string msg) {
      auto parts = split(msg, ';');

      auto seq = parts[1];
      auto name = parts[2];
      auto args = parts[3];

      if (bindings.find(name) == bindings.end()) {
        return;
      }

      auto fn = bindings[name];
      (*fn->first)(seq, args, fn->second);
    }
    std::map<std::string, binding_ctx_t *> bindings;
  };
} // namespace Opkit

#endif /* WEBVIEW_HEADER */
#endif /* WEBVIEW_H */
