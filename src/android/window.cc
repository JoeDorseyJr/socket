#include "../core/core.hh"
#include "../ipc/ipc.hh"
#include "internal.hh"

using namespace SSC::android;

namespace SSC::android {
  Window::Window (
    JNIEnv* env,
    jobject self,
    Bridge* bridge,
    WindowOptions options
  ) : options(options) {
    this->env = env;
    this->self = env->NewGlobalRef(self);
    this->bridge = bridge;
    this->config = SSC::getUserConfig();
    this->pointer = reinterpret_cast<jlong>(this);

    for (const auto& key : this->config.list("build.env")) {
      if (!Env::has(key)) {
        continue;
      }

      auto value = Env::get(key.c_str());

      if (value.size() > 0) {
        envvars[key] = value;
      }
    }

    StringWrap rootDirectory(env, (jstring) CallObjectClassMethodFromEnvironment(
      env,
      self,
      "getRootDirectory",
      "()Ljava/lang/String;"
    ));

    const auto argv = this->config["ssc.argv"];

    options.headless = this->config["build.headless"] == "true";
    options.debug = isDebugEnabled() ? true : false;
    options.env = encodeURIComponent(envvars);
    options.cwd = rootDirectory.str();
    options.userConfig = this->config;
    options.argv = argv;
    options.isTest = argv.find("--test") != -1;

    preloadSource = createPreload(options);
  }

  Window::~Window () {
    this->env->DeleteGlobalRef(this->self);
  }

  void Window::evaluateJavaScript (String source, JVMEnvironment& jvm) {
    auto attachment = JNIEnvironmentAttachment { jvm.get(), jvm.version() };
    auto env = attachment.env;
    if (!attachment.hasException()) {
      auto sourceString = env->NewStringUTF(source.c_str());
      CallVoidClassMethodFromEnvironment(
        env,
        self,
        "evaluateJavaScript",
        "(Ljava/lang/String;)V",
        sourceString
      );

      env->DeleteLocalRef(sourceString);
    }
  }
}

extern "C" {
  jlong external(Window, alloc)(
    JNIEnv *env,
    jobject self,
    jlong bridgePointer
  ) {
    auto bridge = Bridge::from(bridgePointer);

    if (bridge == nullptr) {
      Throw(env, BridgeNotInitializedException);
      return 0;
    }

    auto options = SSC::WindowOptions {};
    auto window = new Window(env, self, bridge, options);
    auto jvm = JVMEnvironment(env);

    if (window == nullptr) {
      Throw(env, WindowNotInitializedException);
      return 0;
    }

    bridge->router.evaluateJavaScriptFunction = [window, jvm](auto source) mutable {
      window->evaluateJavaScript(source, jvm);
    };

    return window->pointer;
  }

  jboolean external(Window, dealloc)(
    JNIEnv *env,
    jobject self
  ) {
    auto window = Window::from(env, self);

    if (window == nullptr) {
      Throw(env, WindowNotInitializedException);
      return false;
    }

    delete window;
    return true;
  }

  jstring external(Window, getPathToFileToLoad)(
    JNIEnv *env,
    jobject self
  ) {
    auto window = Window::from(env, self);

    if (window == nullptr) {
      Throw(env, WindowNotInitializedException);
      return nullptr;
    }

    auto filename = window->config["webview.root"];

    if (filename.size() > 0) {
      return env->NewStringUTF(filename.c_str());
    }

    return env->NewStringUTF("/index.html");
  }

  jstring external(Window, getJavaScriptPreloadSource)(
    JNIEnv *env,
    jobject self
  ) {
    auto window = Window::from(env, self);

    if (window == nullptr) {
      Throw(env, WindowNotInitializedException);
      return nullptr;
    }

    auto source = window->preloadSource.c_str();

    return env->NewStringUTF(source);
  }

  jstring external(Window, getResolveToRenderProcessJavaScript)(
    JNIEnv *env,
    jobject self,
    jstring seq,
    jstring state,
    jstring value
  ) {
    auto window = Window::from(env, self);

    if (window == nullptr) {
      Throw(env, WindowNotInitializedException);
      return nullptr;
    }

    auto resolved = SSC::encodeURIComponent(StringWrap(env, value).str());
    auto source = SSC::getResolveToRenderProcessJavaScript(
      StringWrap(env, seq).str(),
      StringWrap(env, state).str(),
      resolved
    );

    return env->NewStringUTF(source.c_str());
  }
}
