#if defined(_WIN32)

#include "Platform/WebView.h"
#include "Platform/Window.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Logger/Logger.h"

#include <windows.h>
#include <shlwapi.h>
#include <string>

// WebView2 headers - these will be available if WebView2 SDK is installed
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996) // 'GetVersionExA': was declared deprecated
#endif

// Try to include WebView2 headers if available
#if __has_include(<WebView2.h>)
#include <WebView2.h>
#include <wrl/client.h>
#include <wrl/event.h> // Callback<>, which builds the completion handlers below
using namespace Microsoft::WRL;
#else
// WebView2 not available - provide stub implementation
#endif

namespace GameEngine {
namespace Platform {

#if __has_include(<WebView2.h>)

// WebView2 builds its environment and its controller asynchronously, through completion
// handlers that run on this thread's message queue. The editor's window loop already pumps
// that queue (Platform::Window::PollEvents), so nothing here runs a loop of its own.
//
// The panel does not have that luxury: it drives the view from the moment the tab exists,
// sizing it, hiding it, setting its background and then navigating it. This class therefore
// answers those calls immediately and applies what they asked for when the controller lands,
// so the first page the panel asks for is the one that loads rather than one dropped on the
// floor — and so a view asked to sit off-screen stays off-screen when it appears.
//
// A page that fails to load and a page that renders both leave an empty rectangle on screen,
// because this view draws above the GPU frame. The navigation outcome is reported to the log
// for that reason: the log is the only place the two differ.

// Text arrives as UTF-8 and every part of the Win32 API takes UTF-16. Constructing a std::wstring from
// a byte range widens each byte instead of decoding it, so any character outside ASCII arrives altered
// - "cafe" with an accent becomes two unrelated characters, and a URL built from it fails to load. This
// is the same conversion the other Win32 sources in this module do, kept local like theirs.
static std::wstring WebViewTextToWide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int length =
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

class WebView_Win32 : public IWebView {
public:
    WebView_Win32(Window* parentWindow) : m_ParentWindow(parentWindow) {
        if (!parentWindow) {
            return;
        }

        GLFWwindow* glfwWindow = parentWindow->GetGLFWHandle();
        if (!glfwWindow) {
            return;
        }

        HWND hwnd = glfwGetWin32Window(glfwWindow);
        if (!hwnd) {
            return;
        }

        m_Hwnd = hwnd;
        m_Handlers = std::make_shared<PendingHandlers>();
        m_Handlers->Owner = this;
        CreateEnvironment();
    }

    ~WebView_Win32() override {
        Destroy();
    }

    bool LoadURL(const std::string& url) override {
        if (m_CreateFailed) {
            return false;
        }

        if (!IsReady()) {
            // Creation is still in flight: hold the request and load it on arrival.
            m_PendingURL = url;
            m_PendingHTML.clear();
            m_Pending = Pending::URL;
            return true;
        }

        return Navigate(url);
    }

    bool LoadHTML(const std::string& html, const std::string& /*baseURL*/) override {
        if (m_CreateFailed) {
            return false;
        }

        if (!IsReady()) {
            m_PendingHTML = html;
            m_PendingURL.clear();
            m_Pending = Pending::HTML;
            return true;
        }

        // NavigateToString takes no base URL; the panel's placeholder needs none, and the
        // pages it shows for real arrive as URLs.
        return NavigateToHTML(html);
    }

    void* GetNativeView() const override {
        // This interface has no window handle of its own — the controller owns one, and the
        // closest thing it reports is the parent window it was created against — and callers
        // read this as "the view exists". So it answers with the host window the view attaches
        // to while the view is alive, and with nothing once creation has failed or the view
        // has been destroyed.
        return (m_CreateFailed || m_Destroyed) ? nullptr : m_Hwnd;
    }

    void Resize(int width, int height) override {
        // Change the size and keep the origin. WebPanel calls this and SetPosition as a pair,
        // and WebView2 moves its host window for every put_Bounds: an origin reset here would
        // put the view at the window's corner for as long as the second call takes to arrive,
        // which is a visible jump on every layout pass — the panel issues the pair each frame.
        m_DesiredBounds.right = m_DesiredBounds.left + width;
        m_DesiredBounds.bottom = m_DesiredBounds.top + height;
        m_HasBounds = true;
        ApplyBounds();
    }

    void SetPosition(int x, int y) override {
        // Move the origin, keep the size the view was last given.
        const LONG width = m_DesiredBounds.right - m_DesiredBounds.left;
        const LONG height = m_DesiredBounds.bottom - m_DesiredBounds.top;
        m_DesiredBounds = { x, y, x + width, y + height };
        m_HasBounds = true;
        ApplyBounds();
    }

    void SetVisible(bool visible) override {
        m_Visible = visible;
        m_HasVisibility = true;

        if (!m_Controller) {
            return;
        }

        BOOL isVisible = visible ? TRUE : FALSE;
        m_Controller->put_IsVisible(isVisible);
    }

    bool IsVisible() const override {
        if (!m_Controller) {
            // Answer for the state the view will be in once it exists.
            return m_HasVisibility && m_Visible;
        }

        BOOL isVisible = FALSE;
        m_Controller->get_IsVisible(&isVisible);
        return isVisible == TRUE;
    }

    void SetBackgroundColor(unsigned char r, unsigned char g, unsigned char b) override {
        // Set WebView2 background color to match dark theme
        // WebView2 uses COREWEBVIEW2_COLOR with values 0-255
        m_BackgroundColor.A = 255;
        m_BackgroundColor.R = r;
        m_BackgroundColor.G = g;
        m_BackgroundColor.B = b;
        m_HasBackgroundColor = true;
        ApplyBackgroundColor();
    }

    bool BeginFileDrag(const std::filesystem::path& /*path*/) override {
        return false;
    }

    // Mouse-down callback not wired on this platform; see WebView.h.
    void SetOnMouseDown(std::function<void()> /*callback*/) override {}

    void Destroy() override {
        if (m_Destroyed) {
            return;
        }
        m_Destroyed = true;

        // The completion handlers hold this block, not the view: one whose message is still
        // in the queue must find the view gone rather than touch it.
        if (m_Handlers) {
            m_Handlers->Owner = nullptr;
            m_Handlers.reset();
        }

        if (m_Controller) {
            if (m_WebView) {
                m_WebView->remove_NavigationCompleted(m_NavigationCompletedToken);
            }
            m_Controller->Close();
            m_Controller = nullptr;
        }
        m_WebView = nullptr;
        m_Environment = nullptr;
        m_HasAppliedBounds = false;

        if (m_ComInitialized) {
            // Balance the apartment the constructor took.
            CoUninitialize();
            m_ComInitialized = false;
        }

        m_Initialized = false;
    }

private:
    // Handed to the WebView2 completion handlers, which outlive the call that started them.
    // Destroy() clears Owner, so a handler that finds it cleared does nothing.
    struct PendingHandlers {
        WebView_Win32* Owner = nullptr;
    };

    enum class Pending {
        None,
        URL,
        HTML
    };

    bool IsReady() const {
        return m_Initialized && m_WebView;
    }

    void CreateEnvironment() {
        // WebView2 wants a single-threaded apartment on the thread that creates it — the
        // apartment the editor's shell dialogs take. RPC_E_CHANGED_MODE means something
        // already claimed this thread for multi-threaded use, in which case the creation
        // below is expected to fail and to say so rather than to misbehave.
        const HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (SUCCEEDED(hrCom)) {
            m_ComInitialized = true;
        } else if (hrCom == RPC_E_CHANGED_MODE) {
            Logger::Log::Warning("WebView: COM is already initialized in another apartment on this thread; "
                                 "WebView2 needs a single-threaded apartment");
        }

        // No user data folder is passed: the profile stays in the location WebView2 itself
        // chooses for an unpackaged application, the same way the macOS backend leaves it to
        // the default website data store. Nothing is staged beside the executable and no path
        // back into the repository is resolved.
        const std::shared_ptr<PendingHandlers> handlers = m_Handlers;
        const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [handlers](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                    if (handlers->Owner) {
                        handlers->Owner->OnEnvironmentCreated(result, environment);
                    }
                    return S_OK;
                }).Get());

        if (FAILED(hr)) {
            m_CreateFailed = true;
            Logger::Log::Error("WebView: CreateCoreWebView2EnvironmentWithOptions failed (hr=0x{:08x})",
                               static_cast<unsigned>(hr));
        }
    }

    void OnEnvironmentCreated(HRESULT result, ICoreWebView2Environment* environment) {
        if (FAILED(result) || !environment) {
            m_CreateFailed = true;
            // The usual cause on a machine where this has never worked is a missing
            // Evergreen WebView2 Runtime, which the installer of the application supplies.
            Logger::Log::Error("WebView: the WebView2 environment could not be created (hr=0x{:08x}); "
                               "the Evergreen WebView2 Runtime is required",
                               static_cast<unsigned>(result));
            return;
        }

        m_Environment = environment;

        const std::shared_ptr<PendingHandlers> handlers = m_Handlers;
        const HRESULT hr = environment->CreateCoreWebView2Controller(
            m_Hwnd,
            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [handlers](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                    if (handlers->Owner) {
                        handlers->Owner->OnControllerCreated(controllerResult, controller);
                    }
                    return S_OK;
                }).Get());

        if (FAILED(hr)) {
            m_CreateFailed = true;
            Logger::Log::Error("WebView: CreateCoreWebView2Controller failed (hr=0x{:08x})",
                               static_cast<unsigned>(hr));
        }
    }

    void OnControllerCreated(HRESULT result, ICoreWebView2Controller* controller) {
        if (FAILED(result) || !controller) {
            m_CreateFailed = true;
            Logger::Log::Error("WebView: the WebView2 controller could not be created (hr=0x{:08x})",
                               static_cast<unsigned>(result));
            return;
        }

        m_Controller = controller;
        if (FAILED(m_Controller->get_CoreWebView2(&m_WebView)) || !m_WebView) {
            m_Controller->Close();
            m_Controller = nullptr;
            m_CreateFailed = true;
            Logger::Log::Error("WebView: the WebView2 controller exposed no web view");
            return;
        }

        // Reporting the outcome is the whole point of listening: a page that renders and a
        // page that does not look identical on screen. The handler captures nothing, so it
        // cannot outlive the view it reports for.
        m_WebView->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                [](ICoreWebView2* /*sender*/, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                    BOOL succeeded = FALSE;
                    args->get_IsSuccess(&succeeded);
                    if (succeeded) {
                        Logger::Log::Info("WebView: page loaded");
                        return S_OK;
                    }

                    COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                    args->get_WebErrorStatus(&status);
                    Logger::Log::Error("WebView: navigation failed (WebErrorStatus {})",
                                       static_cast<int>(status));
                    return S_OK;
                }).Get(),
            &m_NavigationCompletedToken);

        m_Initialized = true;
        ApplyState();
    }

    void ApplyBounds() {
        if (!m_HasBounds || !m_Controller) {
            return;
        }

        // The panel re-issues its size and position on every layout pass, and WebView2 moves
        // and resizes its host window on every put_Bounds. Sending an unchanged rect buys a
        // window move and the repaint that goes with it, so it is not sent.
        if (m_HasAppliedBounds && EqualRect(&m_AppliedBounds, &m_DesiredBounds)) {
            return;
        }

        m_Controller->put_Bounds(m_DesiredBounds);
        m_AppliedBounds = m_DesiredBounds;
        m_HasAppliedBounds = true;
    }

    // The default background color is not on the base controller interface — it arrived with
    // the controller's second revision — so a WebView2 Runtime older than that revision simply
    // keeps the white default. Ask for the newer interface rather than assume it.
    void ApplyBackgroundColor() {
        if (!m_HasBackgroundColor || !m_Controller) {
            return;
        }

        ComPtr<ICoreWebView2Controller2> controller2;
        if (SUCCEEDED(m_Controller.As(&controller2)) && controller2) {
            controller2->put_DefaultBackgroundColor(m_BackgroundColor);
        }
    }

    // What the panel asked for before the controller existed, in the order that leaves the
    // view quiet once it appears: color, then geometry, then visibility, and the page last.
    void ApplyState() {
        if (!m_Controller) {
            return;
        }

        ApplyBackgroundColor();
        ApplyBounds();

        if (m_HasVisibility) {
            BOOL isVisible = m_Visible ? TRUE : FALSE;
            m_Controller->put_IsVisible(isVisible);
        }

        const Pending pending = m_Pending;
        m_Pending = Pending::None;
        if (pending == Pending::URL) {
            Navigate(m_PendingURL);
            m_PendingURL.clear();
        } else if (pending == Pending::HTML) {
            NavigateToHTML(m_PendingHTML);
            m_PendingHTML.clear();
        }
    }

    bool Navigate(const std::string& url) {
        if (!m_WebView) {
            return false;
        }

        std::wstring wurl = WebViewTextToWide(url);
        return SUCCEEDED(m_WebView->Navigate(wurl.c_str()));
    }

    bool NavigateToHTML(const std::string& html) {
        if (!m_WebView) {
            return false;
        }

        std::wstring whtml = WebViewTextToWide(html);
        return SUCCEEDED(m_WebView->NavigateToString(whtml.c_str()));
    }

    Window* m_ParentWindow = nullptr;
    HWND m_Hwnd = nullptr;
    ComPtr<ICoreWebView2Environment> m_Environment;
    ComPtr<ICoreWebView2Controller> m_Controller;
    ComPtr<ICoreWebView2> m_WebView;
    std::shared_ptr<PendingHandlers> m_Handlers;
    EventRegistrationToken m_NavigationCompletedToken{};
    RECT m_DesiredBounds{ 0, 0, 0, 0 };
    RECT m_AppliedBounds{ 0, 0, 0, 0 };
    COREWEBVIEW2_COLOR m_BackgroundColor{ 255, 39, 39, 39 };
    std::string m_PendingURL;
    std::string m_PendingHTML;
    Pending m_Pending = Pending::None;
    bool m_HasBounds = false;
    bool m_HasAppliedBounds = false;
    bool m_HasVisibility = false;
    bool m_HasBackgroundColor = false;
    bool m_Visible = false;
    bool m_CreateFailed = false;
    bool m_Destroyed = false;
    bool m_ComInitialized = false;
    bool m_Initialized = false;
};

#else // WebView2 not available

class WebView_Win32 : public IWebView {
public:
    WebView_Win32(Window* /*parentWindow*/) {}
    ~WebView_Win32() override = default;
    bool LoadURL(const std::string& /*url*/) override { return false; }
    bool LoadHTML(const std::string& /*html*/, const std::string& /*baseURL*/) override { return false; }
    void* GetNativeView() const override { return nullptr; }
    void Resize(int /*width*/, int /*height*/) override {}
    void SetPosition(int /*x*/, int /*y*/) override {}
    void SetVisible(bool /*visible*/) override {}
    bool IsVisible() const override { return false; }
    void SetBackgroundColor(unsigned char /*r*/, unsigned char /*g*/, unsigned char /*b*/) override {}
    bool BeginFileDrag(const std::filesystem::path& /*path*/) override { return false; }
    void SetOnMouseDown(std::function<void()> /*callback*/) override {}
    void Destroy() override {}
};

#endif // __has_include(<WebView2.h>)

std::unique_ptr<IWebView> CreateWebView(Window* parentWindow) {
    if (!parentWindow) {
        return nullptr;
    }

    auto webView = std::make_unique<WebView_Win32>(parentWindow);
    if (!webView->GetNativeView()) {
        return nullptr;
    }

    return webView;
}

} // namespace Platform
} // namespace GameEngine

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // _WIN32
