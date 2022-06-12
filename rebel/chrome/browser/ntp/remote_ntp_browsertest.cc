// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "ui/native_theme/test_native_theme.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "components/onc/onc_constants.h"
#include "components/wifi/fake_wifi_service.h"
#include "components/wifi/network_properties.h"
#endif

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_impl.h"
#include "rebel/chrome/browser/ntp/remote_ntp_theme_provider.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

namespace {

constexpr base::FilePath::CharType kRemoteNtpTestRoot[] =
    FILE_PATH_LITERAL("rebel/chrome/test/data/remote_ntp");
constexpr base::FilePath::CharType kRemoteNtpTestFallbackRoot[] =
    FILE_PATH_LITERAL("rebel/chrome/test/data/remote_ntp/fallback");

constexpr size_t kIconCacheSizeForTesting = 2;

// Number of URLs in //rebel/third_party/remote_ntp/build/default_sites.json.
constexpr size_t kInitialNtpTilesSize = 3;

GURL UrlWithoutQuery(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();

  return url.ReplaceComponents(replacements);
}

// Test observer for RemoteNtpService notifications. Allows the tests to wait
// for the RemoteNtpService to trigger events with expected values.
class TestRemoteNtpServiceObserver : public rebel::RemoteNtpService::Observer {
 public:
  TestRemoteNtpServiceObserver(rebel::RemoteNtpService* remote_ntp_service)
      : remote_ntp_service_(remote_ntp_service), ntp_tiles_size_(0) {
    remote_ntp_service_->AddObserver(this);
  }

  ~TestRemoteNtpServiceObserver() override {
    remote_ntp_service_->RemoveObserver(this);
  }

  void WaitForNtpTiles(size_t expected_ntp_tiles_size) {
    DCHECK(!quit_closure_ntp_tiles_);

    if (ntp_tiles_size_ == expected_ntp_tiles_size) {
      return;
    }

    expected_ntp_tiles_size_ = expected_ntp_tiles_size;

    base::RunLoop run_loop;
    quit_closure_ntp_tiles_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForDarkMode(bool expected_dark_mode_enabled) {
    DCHECK(!quit_closure_theme_);

    if (dark_mode_enabled_ == expected_dark_mode_enabled) {
      return;
    }

    expected_dark_mode_enabled_ = expected_dark_mode_enabled;

    base::RunLoop run_loop;
    quit_closure_theme_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForBackgroundImage(const GURL& expected_background_image_url) {
    DCHECK(!quit_closure_theme_);

    if (background_image_url_ == expected_background_image_url) {
      return;
    }

    expected_background_image_url_ = expected_background_image_url;

    base::RunLoop run_loop;
    quit_closure_theme_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForIconStored(GURL expected_host_origin) {
    DCHECK(!quit_closure_icon_stored_);

    if (icon_ && (icon_->host_origin == expected_host_origin)) {
      return;
    }

    expected_host_origin_ = expected_host_origin;

    base::RunLoop run_loop;
    quit_closure_icon_stored_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForIconEvicted(GURL expected_evicted_origin) {
    DCHECK(!quit_closure_icon_evicted_);

    if (icon_ && (evicted_origin_ == expected_evicted_origin)) {
      return;
    }

    expected_evicted_origin_ = expected_evicted_origin;

    base::RunLoop run_loop;
    quit_closure_icon_evicted_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool WaitForIconLoaded(GURL expected_loaded_origin) {
    DCHECK(!quit_closure_icon_loaded_);

    if (loaded_origin_ == expected_loaded_origin) {
      return loaded_successfully_;
    }

    expected_loaded_origin_ = expected_loaded_origin;

    base::RunLoop run_loop;
    quit_closure_icon_loaded_ = run_loop.QuitClosure();
    run_loop.Run();

    return loaded_successfully_;
  }

  const rebel::mojom::RemoteNtpIconPtr& stored_icon() const { return icon_; }
  const base::FilePath& stored_icon_file() const { return icon_file_; }

  const GURL& evicted_origin() const { return evicted_origin_; }
  const base::FilePath& evicted_icon_file() const { return evicted_icon_file_; }

 private:
  void OnNtpTilesChanged(const rebel::RemoteNtpTileList& ntp_tiles) override {
    ntp_tiles_size_ = ntp_tiles.size();

    if (quit_closure_ntp_tiles_ &&
        (ntp_tiles_size_ == expected_ntp_tiles_size_)) {
      std::move(quit_closure_ntp_tiles_).Run();
      quit_closure_ntp_tiles_.Reset();
    }
  }

  void OnThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) override {
    dark_mode_enabled_ = theme->dark_mode_enabled;
    background_image_url_ = UrlWithoutQuery(theme->image_url);

    if (quit_closure_theme_ &&
        ((dark_mode_enabled_ == expected_dark_mode_enabled_) ||
         (background_image_url_ == expected_background_image_url_))) {
      std::move(quit_closure_theme_).Run();
      quit_closure_theme_.Reset();
    }
  }

  void OnTouchIconStored(const rebel::mojom::RemoteNtpIconPtr& icon,
                         const base::FilePath& icon_file) override {
    icon_ = icon->Clone();
    icon_file_ = icon_file;

    if (quit_closure_icon_stored_ &&
        (icon_->host_origin == expected_host_origin_)) {
      std::move(quit_closure_icon_stored_).Run();
      quit_closure_icon_stored_.Reset();
    }
  }

  void OnTouchIconEvicted(const GURL& origin,
                          const base::FilePath& icon_file) override {
    evicted_origin_ = origin;
    evicted_icon_file_ = icon_file;

    if (quit_closure_icon_evicted_ &&
        (evicted_origin_ == expected_evicted_origin_)) {
      std::move(quit_closure_icon_evicted_).Run();
      quit_closure_icon_evicted_.Reset();
    }
  }

  void OnTouchIconLoadComplete(const GURL& origin, bool successful) override {
    loaded_origin_ = origin;
    loaded_successfully_ = successful;

    if (quit_closure_icon_loaded_ &&
        (loaded_origin_ == expected_loaded_origin_)) {
      std::move(quit_closure_icon_loaded_).Run();
      quit_closure_icon_loaded_.Reset();
    }
  }

  raw_ptr<rebel::RemoteNtpService> remote_ntp_service_;

  size_t ntp_tiles_size_{0};
  size_t expected_ntp_tiles_size_{0};

  bool dark_mode_enabled_{false};
  bool expected_dark_mode_enabled_{false};

  GURL background_image_url_;
  GURL expected_background_image_url_;

  rebel::mojom::RemoteNtpIconPtr icon_;
  base::FilePath icon_file_;
  GURL expected_host_origin_;

  GURL evicted_origin_;
  base::FilePath evicted_icon_file_;
  GURL expected_evicted_origin_;

  GURL loaded_origin_;
  GURL expected_loaded_origin_;
  bool loaded_successfully_{false};

  base::OnceClosure quit_closure_ntp_tiles_;
  base::OnceClosure quit_closure_theme_;
  base::OnceClosure quit_closure_icon_stored_;
  base::OnceClosure quit_closure_icon_evicted_;
  base::OnceClosure quit_closure_icon_loaded_;
};

// RemoteNtpService implementation which does not initialize its parent.
class NullRemoteNtpService : public rebel::RemoteNtpService {
 public:
  NullRemoteNtpService() : rebel::RemoteNtpService(base::FilePath(), nullptr) {}

  std::unique_ptr<AutocompleteController> CreateAutocompleteController()
      const override {
    return nullptr;
  }
};

}  // namespace

// Test fixture for testing the RemoteNTP APIs. Tests validate that the
// RemoteNtpService notices and broadcasts NTP-related events through its
// observer interface, as well as by validating via Javascript executed on a
// testing NTP website.
class RemoteNtpTest : virtual public InProcessBrowserTest {
 public:
  RemoteNtpTest() : http_server_(embedded_test_server()) {}

  static void SetUpTestSuite() {
    rebel::RemoteNtpIconStorage::SetCacheSizeLimitForTesting(
        kIconCacheSizeForTesting);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    http_server_->AddDefaultHandlers(base::FilePath(kRemoteNtpTestRoot));
    ASSERT_TRUE(http_server_->Start());

    const GURL ntp_url = http_server_->GetURL("/index.html");

    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(rebel::kRemoteNtpUrl, ntp_url.spec());
  }

 protected:
  static std::unique_ptr<net::EmbeddedTestServer> CreateServer(
      base::FilePath::StringPieceType&& root) {
    auto http_server = std::make_unique<net::EmbeddedTestServer>();

    http_server->AddDefaultHandlers(base::FilePath(root));
    CHECK(http_server->Start());

    return http_server;
  }

  content::WebContents* OpenTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* OpenNewTab() {
    return OpenTab(GURL(chrome::kChromeUINewTabURL));
  }

  content::WebContents* OpenNewTabAndWaitForTiles() {
    content::WebContents* active_tab = OpenNewTab();
    WaitForNtpTiles(active_tab);
    return active_tab;
  }

  bool HasNtpTile(const content::ToRenderFrameHost& adapter,
                  const std::string url,
                  const std::string title) {
    auto result = content::EvalJs(
        adapter, base::StringPrintf("hasNtpTile(\"%s\", \"%s\")", url.c_str(),
                                    title.c_str()));
    return result.ExtractBool();
  }

  void LoadPageAndValidateIcon(
      net::EmbeddedTestServer* http_server,
      std::string&& page_url,
      std::string&& expected_icon_url,
      rebel::mojom::RemoteNtpIconType expected_icon_type,
      int expected_icon_size) {
    rebel::RemoteNtpService* remote_ntp_service =
        rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
    TestRemoteNtpServiceObserver observer(remote_ntp_service);

    const GURL url = http_server->GetURL(page_url);
    OpenTab(url);

    if (expected_icon_type == rebel::mojom::RemoteNtpIconType::Unknown) {
      EXPECT_TRUE(observer.stored_icon().is_null());
      EXPECT_TRUE(observer.stored_icon_file().empty());
    } else {
      observer.WaitForIconStored(url.GetWithEmptyPath());

      const rebel::mojom::RemoteNtpIconPtr& icon = observer.stored_icon();
      EXPECT_EQ(icon->host_origin, url.GetWithEmptyPath());
      EXPECT_EQ(icon->icon_url, http_server->GetURL(expected_icon_url));
      EXPECT_EQ(icon->icon_type, expected_icon_type);
      EXPECT_EQ(icon->icon_size, expected_icon_size);

      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::PathExists(observer.stored_icon_file()));
    }
  }

  raw_ptr<net::EmbeddedTestServer> http_server_;
  ui::TestNativeTheme theme_;

 private:
  void WaitForNtpTiles(content::WebContents* active_tab) {
    // Script to check every 100ms for the NTP to have received the NTP tiles.
    // The |ntpTilesLoaded| flag originates from the NTP source HTML itself.
    static const char kWaitForNtpTiles[] = R"(
        (async function() {
          function waitForNtpTiles() {
            if (ntpTilesLoaded) {
              return true;
            } else {
              return new Promise((resolve) => {
                window.setTimeout(function() {
                  resolve(waitForNtpTiles());
                }, 100);
              });
            }
          }

          return await waitForNtpTiles();
        })();
      )";

    ASSERT_EQ(true, content::EvalJs(active_tab, kWaitForNtpTiles));
  }
};

// Ensure that a RemoteNtpService that was not properly initialized survives
// destruction.
IN_PROC_BROWSER_TEST_F(RemoteNtpTest, UninitializeService) {
  { [[maybe_unused]] NullRemoteNtpService remote_ntp_service; }
}

// The spare RenderProcessHost is warmed up *before* the target destination is
// known and therefore doesn't include any special command-line flags that are
// used when launching a RenderProcessHost known to be needed for NTP.  This
// test ensures that the spare RenderProcessHost doesn't accidentally end up
// being used for NTP navigations.
IN_PROC_BROWSER_TEST_F(RemoteNtpTest,
                       SpareProcessDoesntInterfereWithRemoteNtpApi) {
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a non-NTP URL, so that the next step needs to swap the process.
  const GURL non_ntp_url = http_server_->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_ntp_url));
  content::RenderProcessHost* old_process =
      active_tab->GetPrimaryMainFrame()->GetProcess();

  // Navigate to an NTP while a spare process is present.
  content::RenderProcessHost::WarmupSpareRenderProcessHost(
      browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));

  // Verify that a process swap has taken place.  This is an indirect indication
  // that the spare process could have been used (during the process swap). This
  // assertion is a sanity check of the test setup, rather than verification of
  // the core thing that the test cares about.
  content::RenderProcessHost* new_process =
      active_tab->GetPrimaryMainFrame()->GetProcess();
  ASSERT_NE(new_process, old_process);

  // Check that the RemoteNTP API is available - the spare RenderProcessHost
  // either shouldn't be used, or if used it should have been launched with the
  // appropriate, NTP-specific command-line flags.
  EXPECT_EQ(true, content::EvalJs(active_tab, "!!window.rebel"));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, RemoteNtpApiExposesStaticFunctions) {
  content::WebContents* active_tab = OpenNewTab();

  struct TestCase {
    const char* function_name;
    const char* args;
  } test_cases[] = {
      {"rebel.addCustomTile", "\"https://a.com\", \"a\""},
      {"rebel.removeCustomTile", "\"https://a.com\""},
      {"rebel.editCustomTile", "\"https://a.com\", \"https://b.com\", \"b\""},
      {"rebel.loadInternalUrl", "\"https://a.com\""},
      {"rebel.search.queryAutocomplete", "\"a\", false"},
      {"rebel.search.stopAutocomplete", ""},
      {"rebel.search.openAutocompleteMatch",
       "0, \"https://a.com\", false, false, false, false, false"},
      {"rebel.theme.showOrHideCustomizeMenu", ""},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.function_name);
    SCOPED_TRACE(active_tab->GetURL());

    // Make sure that the API function exists.
    ASSERT_EQ(true, content::EvalJs(
                        active_tab,
                        base::StringPrintf("!!%s", test_case.function_name)));

    // Check that it can be called normally.
    EXPECT_TRUE(content::ExecJs(
        active_tab,
        base::StringPrintf("%s(%s)", test_case.function_name, test_case.args)));

    // Check that it can be called even after it's assigned to a var, i.e.
    // without a "this" binding.
    EXPECT_TRUE(content::ExecJs(
        active_tab,
        base::StringPrintf("var f = %s; f(%s);", test_case.function_name,
                           test_case.args)));
  }
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, InitialNtpTiles) {
  TestRemoteNtpServiceObserver observer(
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the NTP items.
  content::WebContents* active_tab = OpenNewTabAndWaitForTiles();
  observer.WaitForNtpTiles(kInitialNtpTilesSize);

  // Make sure the same number of items is available in JS.
  ASSERT_EQ(static_cast<int>(kInitialNtpTilesSize),
            content::EvalJs(active_tab, "rebel.ntpTiles.length"));

  // Get the title of one of them.
  ASSERT_NE(std::string(),
            content::EvalJs(active_tab, "rebel.ntpTiles[0].title"));

  // Get the URL of one of them.
  ASSERT_NE(std::string(),
            content::EvalJs(active_tab, "rebel.ntpTiles[0].url"));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, AddCustomTile) {
  const std::string title("XKCD");
  const std::string url("https://xkcd.com/");

  TestRemoteNtpServiceObserver observer(
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the NTP items.
  content::WebContents* active_tab = OpenNewTabAndWaitForTiles();
  observer.WaitForNtpTiles(kInitialNtpTilesSize);

  // The tile should not exist yet.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // Add the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.addCustomTile(\"%s\", \"%s\")",
                                     url.c_str(), title.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // Now the tile should exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url, title));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, RemoveCustomTile) {
  const std::string title("XKCD");
  const std::string url("https://xkcd.com/");

  TestRemoteNtpServiceObserver observer(
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the NTP items.
  content::WebContents* active_tab = OpenNewTabAndWaitForTiles();
  observer.WaitForNtpTiles(kInitialNtpTilesSize);

  // The tile should not exist yet.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // Add the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.addCustomTile(\"%s\", \"%s\")",
                                     url.c_str(), title.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // Now the tile should exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url, title));

  // Remove the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.removeCustomTile(\"%s\")", url.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize);

  // The tile should no longer exist.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, EditCustomTileUrlAndTitle) {
  const std::string title("XKCD"), title2("XKCD2");
  const std::string url("https://xkcd.com/"), url2("https://xkcd2.com/");

  TestRemoteNtpServiceObserver observer(
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the NTP items.
  content::WebContents* active_tab = OpenNewTabAndWaitForTiles();
  observer.WaitForNtpTiles(kInitialNtpTilesSize);

  // The tile should not exist yet.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // Add the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.addCustomTile(\"%s\", \"%s\")",
                                     url.c_str(), title.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // Now the tile should exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url, title));

  // Edit the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.editCustomTile(\"%s\", \"%s\", \"%s\")",
                         url.c_str(), url2.c_str(), title2.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // The original tile should no longer exist.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // The updated tile should now exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url2, title2));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, EditCustomTileUrlOnly) {
  const std::string title("XKCD");
  const std::string url("https://xkcd.com/"), url2("https://xkcd2.com/");

  TestRemoteNtpServiceObserver observer(
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the NTP items.
  content::WebContents* active_tab = OpenNewTabAndWaitForTiles();
  observer.WaitForNtpTiles(kInitialNtpTilesSize);

  // The tile should not exist yet.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // Add the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.addCustomTile(\"%s\", \"%s\")",
                                     url.c_str(), title.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // Now the tile should exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url, title));

  // Edit the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.editCustomTile(\"%s\", \"%s\", \"\")",
                         url.c_str(), url2.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // The original tile should no longer exist.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // The updated tile should now exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url2, title));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, EditCustomTileTitleOnly) {
  const std::string title("XKCD"), title2("XKCD2");
  const std::string url("https://xkcd.com/");

  TestRemoteNtpServiceObserver observer(
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the NTP items.
  content::WebContents* active_tab = OpenNewTabAndWaitForTiles();
  observer.WaitForNtpTiles(kInitialNtpTilesSize);

  // The tile should not exist yet.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // Add the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.addCustomTile(\"%s\", \"%s\")",
                                     url.c_str(), title.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // Now the tile should exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url, title));

  // Edit the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.editCustomTile(\"%s\", \"\", \"%s\")",
                         url.c_str(), title2.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // The original tile should no longer exist.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // The updated tile should now exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url, title2));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, EditCustomTileNoFields) {
  const std::string title("XKCD");
  const std::string url("https://xkcd.com/");

  TestRemoteNtpServiceObserver observer(
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile()));

  // Navigating to an NTP should trigger an update of the NTP items.
  content::WebContents* active_tab = OpenNewTabAndWaitForTiles();
  observer.WaitForNtpTiles(kInitialNtpTilesSize);

  // The tile should not exist yet.
  ASSERT_FALSE(HasNtpTile(active_tab, url, title));

  // Add the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.addCustomTile(\"%s\", \"%s\")",
                                     url.c_str(), title.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // Now the tile should exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url, title));

  // Edit the tile.
  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.editCustomTile(\"%s\", \"\", \"\")",
                                     url.c_str())));

  observer.WaitForNtpTiles(kInitialNtpTilesSize + 1);

  // The original tile should still exist.
  ASSERT_TRUE(HasNtpTile(active_tab, url, title));

  // The empty tile should not exist.
  ASSERT_FALSE(HasNtpTile(active_tab, std::string(), std::string()));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, LoadInternalUrl) {
  content::WebContents* active_tab = OpenNewTab();

  const GURL settings_url(chrome::kChromeUISettingsURL);

  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.loadInternalUrl(\"%s\")",
                                     settings_url.spec().c_str())));

  EXPECT_TRUE(active_tab->GetVisibleURL() == settings_url);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, DoNotLoadCrashUrl) {
  content::WebContents* active_tab = OpenNewTab();

  const GURL crash_url(blink::kChromeUICrashURL);

  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.loadInternalUrl(\"%s\")",
                                     crash_url.spec().c_str())));

  EXPECT_TRUE(active_tab->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, DoNotLoadNonInternalUrl) {
  content::WebContents* active_tab = OpenNewTab();

  const GURL http_url("http://example.com");

  EXPECT_TRUE(content::ExecJs(
      active_tab, base::StringPrintf("rebel.loadInternalUrl(\"%s\")",
                                     http_url.spec().c_str())));

  EXPECT_TRUE(active_tab->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, PlatformInfoAvailable) {
  content::WebContents* active_tab = OpenNewTab();

  auto is32Or64 = [](content::EvalJsResult result) {
    const int val = result.ExtractInt();
    return (val == 32) || (val == 64);
  };

  EXPECT_EQ(version_info::GetOSType(),
            content::EvalJs(active_tab, "rebel.platformInfo.platform"));
  EXPECT_EQ(version_info::GetVersionNumber(),
            content::EvalJs(active_tab, "rebel.platformInfo.version"));
  EXPECT_PRED1(is32Or64,
               content::EvalJs(active_tab, "rebel.platformInfo.browserArch"));
  EXPECT_PRED1(is32Or64,
               content::EvalJs(active_tab, "rebel.platformInfo.systemArch"));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconParserIgnoreBadIcons) {
  // Ignore icons which:
  //   Do not have a |rel| attribute
  //   Have an empty |rel| attribute
  //   Have a |rel| attribute with an unknown icon type
  //   Do not have an |href| attribute
  //   Have an empty |href| attribute
  //   Have an invalid |href| attribute
  LoadPageAndValidateIcon(http_server_, "/invalid.html", "/",
                          rebel::mojom::RemoteNtpIconType::Unknown, -1);

  // Ignore pages that do not have any icons.
  LoadPageAndValidateIcon(http_server_, "/simple.html", "/",
                          rebel::mojom::RemoteNtpIconType::Unknown, -1);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconParserMissingSizes) {
  LoadPageAndValidateIcon(http_server_, "/favicon.html", "/pic1.png",
                          rebel::mojom::RemoteNtpIconType::Favicon, -1);
  LoadPageAndValidateIcon(http_server_, "/fluid.html", "/pic2.png",
                          rebel::mojom::RemoteNtpIconType::Fluid, -1);
  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconParserWithSizes) {
  LoadPageAndValidateIcon(http_server_, "/favicon_size.html", "/pic1.png",
                          rebel::mojom::RemoteNtpIconType::Favicon, 160);
  LoadPageAndValidateIcon(http_server_, "/fluid_size.html", "/pic2.png",
                          rebel::mojom::RemoteNtpIconType::Fluid, 170);
  LoadPageAndValidateIcon(http_server_, "/touch_size.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, 180);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconParserPreferBetterType) {
  // Prefer fluid icons over favicons.
  LoadPageAndValidateIcon(http_server_, "/favicon_fluid.html", "/pic2.png",
                          rebel::mojom::RemoteNtpIconType::Fluid, 160);

  // Prefer touch icons over favicons.
  LoadPageAndValidateIcon(http_server_, "/favicon_touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, 170);

  // Prefer touch icons over fluid icons.
  LoadPageAndValidateIcon(http_server_, "/fluid_touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, 180);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconParserPreferLargestSize) {
  LoadPageAndValidateIcon(http_server_, "/sizes.html", "/pic2.png",
                          rebel::mojom::RemoteNtpIconType::Touch, 190);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconStorageIgnoreRepeatIconOfSameType) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);
  base::FilePath icon_file_before = observer.stored_icon_file();

  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);
  base::FilePath icon_file_after = observer.stored_icon_file();

  EXPECT_EQ(icon_file_before, icon_file_after);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconStorageAcceptRepeatIconOfBetterType) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  LoadPageAndValidateIcon(http_server_, "/fluid.html", "/pic2.png",
                          rebel::mojom::RemoteNtpIconType::Fluid, -1);
  base::FilePath icon_file_before = observer.stored_icon_file();

  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);
  base::FilePath icon_file_after = observer.stored_icon_file();

  EXPECT_NE(icon_file_before, icon_file_after);
  observer.WaitForIconEvicted(http_server_->base_url().GetWithEmptyPath());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(icon_file_before));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconStorageAcceptRepeatIconOfLargerSize) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);
  base::FilePath icon_file_before = observer.stored_icon_file();

  LoadPageAndValidateIcon(http_server_, "/touch_size.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, 180);
  base::FilePath icon_file_after = observer.stored_icon_file();

  EXPECT_NE(icon_file_before, icon_file_after);
  observer.WaitForIconEvicted(http_server_->base_url().GetWithEmptyPath());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(icon_file_before));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconStorageCacheEviction) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  // Load enough pages to fill the cache + 1. Use a unique HTTP server for each
  // page load because the icon cache is keyed by origin.
  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);
  const GURL origin = http_server_->base_url().GetWithEmptyPath();

  for (size_t i = 0; i < kIconCacheSizeForTesting; ++i) {
    auto second_server = CreateServer(kRemoteNtpTestRoot);

    LoadPageAndValidateIcon(second_server.get(), "/touch.html", "/pic3.png",
                            rebel::mojom::RemoteNtpIconType::Touch, -1);
  }

  // The first icon should be evicted.
  observer.WaitForIconEvicted(origin);

  const GURL& evicted_origin = observer.evicted_origin();
  EXPECT_EQ(evicted_origin, origin);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(observer.evicted_icon_file()));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconStorageDropSmallFavicons) {
  LoadPageAndValidateIcon(http_server_, "/favicon_small.html", "/",
                          rebel::mojom::RemoteNtpIconType::Unknown, -1);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconStorageFallbackToDefaultTouchIcon) {
  auto second_server = CreateServer(kRemoteNtpTestFallbackRoot);

  LoadPageAndValidateIcon(second_server.get(), "/simple.html",
                          "/apple-touch-icon.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest,
                       IconStorageDefaultTouchIconReplacesFavicon) {
  auto second_server = CreateServer(kRemoteNtpTestFallbackRoot);

  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  LoadPageAndValidateIcon(second_server.get(), "/favicon.html", "/pic1.png",
                          rebel::mojom::RemoteNtpIconType::Favicon, -1);

  // After fetching the favicon embedded on the page, the browser should request
  // the default touch icon. Once fetched, the favicon should be evicted.
  const GURL origin = second_server->base_url().GetWithEmptyPath();
  observer.WaitForIconEvicted(origin);
  observer.WaitForIconStored(origin);

  const rebel::mojom::RemoteNtpIconPtr& icon = observer.stored_icon();
  EXPECT_EQ(icon->host_origin, origin);
  EXPECT_EQ(icon->icon_url, second_server->GetURL("/apple-touch-icon.png"));
  EXPECT_EQ(icon->icon_type, rebel::mojom::RemoteNtpIconType::Touch);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(observer.evicted_icon_file()));
  EXPECT_TRUE(base::PathExists(observer.stored_icon_file()));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconLoaderFailsWithMissingIcon) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  const GURL origin = http_server_->base_url().GetWithEmptyPath();

  EXPECT_TRUE(content::ExecJs(
      OpenNewTab(),
      base::StringPrintf("injectTouchIcon(\"%s\")", origin.spec().c_str())));

  EXPECT_FALSE(observer.WaitForIconLoaded(origin));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconLoaderFailsWithDeletedIcon) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);

  // Delete the icon from disk. When the icon is requested for rendering, the
  // service should detect the missing icon and evict its metadata.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::DeleteFile(observer.stored_icon_file());
  }

  const GURL origin = http_server_->base_url().GetWithEmptyPath();

  EXPECT_TRUE(content::ExecJs(
      OpenNewTab(),
      base::StringPrintf("injectTouchIcon(\"%s\")", origin.spec().c_str())));

  EXPECT_FALSE(observer.WaitForIconLoaded(origin));
  observer.WaitForIconEvicted(origin);

  const GURL& evicted_origin = observer.evicted_origin();
  EXPECT_EQ(evicted_origin, origin);
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconLoaderSucceedsWithCachedIcon) {
  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);

  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  const GURL origin = http_server_->base_url().GetWithEmptyPath();

  EXPECT_TRUE(content::ExecJs(
      OpenNewTab(),
      base::StringPrintf("injectTouchIcon(\"%s\")", origin.spec().c_str())));

  EXPECT_TRUE(observer.WaitForIconLoaded(origin));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest, IconLoaderSucceedsWithMultiMIMETypes) {
  struct TestCase {
    const char* test_page;
    const char* icon_file;
    const int icon_size;
  } test_cases[] = {
      {"/touch.html", "/pic3.png", -1},
      {"/touch_jpg.html", "/pic4.jpg", 180},
      {"/touch_gif.html", "/pic5.gif", 350},
  };

  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  const GURL origin = http_server_->base_url().GetWithEmptyPath();

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.test_page);

    LoadPageAndValidateIcon(
        http_server_, test_case.test_page, test_case.icon_file,
        rebel::mojom::RemoteNtpIconType::Touch, test_case.icon_size);

    TestRemoteNtpServiceObserver observer(remote_ntp_service);

    EXPECT_TRUE(content::ExecJs(
        OpenNewTab(),
        base::StringPrintf("injectTouchIcon(\"%s\")", origin.spec().c_str())));

    EXPECT_TRUE(observer.WaitForIconLoaded(origin));
  }
}

IN_PROC_BROWSER_TEST_F(RemoteNtpTest,
                       IconLoaderRequestedIconNotEvictedFromCache) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  // Load enough pages to fill the cache + 1. Use a unique HTTP server for each
  // page load because the icon cache is keyed by origin.
  LoadPageAndValidateIcon(http_server_, "/touch.html", "/pic3.png",
                          rebel::mojom::RemoteNtpIconType::Touch, -1);
  const GURL origin = http_server_->base_url().GetWithEmptyPath();
  GURL expected_evicted_origin;

  // Request the icon for the first origin. This should prevent this icon from
  // consideration during cache eviction.
  EXPECT_TRUE(content::ExecJs(
      OpenNewTab(),
      base::StringPrintf("injectTouchIcon(\"%s\")", origin.spec().c_str())));
  EXPECT_TRUE(observer.WaitForIconLoaded(origin));

  for (size_t i = 0; i < kIconCacheSizeForTesting; ++i) {
    auto second_server = CreateServer(kRemoteNtpTestRoot);

    LoadPageAndValidateIcon(second_server.get(), "/touch.html", "/pic3.png",
                            rebel::mojom::RemoteNtpIconType::Touch, -1);

    // The first icon loaded in this loop should be the evicted icon.
    if (i == 0) {
      expected_evicted_origin = second_server->base_url().GetWithEmptyPath();
    }
  }

  observer.WaitForIconEvicted(expected_evicted_origin);

  const GURL& evicted_origin = observer.evicted_origin();
  EXPECT_EQ(evicted_origin, expected_evicted_origin);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(observer.evicted_icon_file()));
}

// Test fixture to set up backend pieces needed for querying autocomplete search
// results from the browser.
class RemoteNtpSearchTest : public RemoteNtpTest {
 protected:
  void SetUpOnMainThread() override {
    RemoteNtpTest::SetUpOnMainThread();

    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

  base::Value::Dict WaitForAutocompleteResult(
      content::WebContents* active_tab) {
    // Script to check every 100ms for the NTP to have received a search query's
    // autocomplete result. Returns that result.
    static const char kWaitForAutocompleteResult[] = R"js(
        (async function() {
          function waitForAutocompleteResult() {
            if (ntpAutocompleteResult !== null) {
              return ntpAutocompleteResult;
            } else {
              return new Promise((resolve) => {
                window.setTimeout(function() {
                  resolve(waitForAutocompleteResult());
                }, 100);
              });
            }
          }

          return await waitForAutocompleteResult();
        })();
      )js";

    auto result = content::EvalJs(active_tab, kWaitForAutocompleteResult);
    return result.value.GetDict().Clone();
  }
};

IN_PROC_BROWSER_TEST_F(RemoteNtpSearchTest, SimpleSearchAllFields) {
  content::WebContents* active_tab = OpenNewTab();

  const std::string query("what is");

  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.search.queryAutocomplete(\"%s\", false)",
                         query.c_str())));

  auto result = WaitForAutocompleteResult(active_tab);

  const std::string* input = result.FindString("input");
  ASSERT_NE(input, nullptr);
  EXPECT_EQ(*input, query);

  const base::Value::List* matches = result.FindList("matches");
  ASSERT_NE(matches, nullptr);

  for (const base::Value& match_value : *matches) {
    SCOPED_TRACE(match_value);
    const auto& match = match_value.GetDict();

    ASSERT_EQ(match.size(), 10U)
        << "Was a field added or removed from rebel.search.autocompleteResult? "
           "Be sure to update RemoteNtpSearchTest.SimpleSearchAllFields in: "
        << __FILE__;

    const std::string* contents = match.FindString("contents");
    ASSERT_NE(contents, nullptr);
    EXPECT_FALSE(contents->empty());

    const base::Value::List* contents_class = match.FindList("contentsClass");
    ASSERT_NE(contents_class, nullptr);

    for (const base::Value& classification_value : *contents_class) {
      const auto& classification = classification_value.GetDict();

      absl::optional<int> offset = classification.FindInt("offset");
      EXPECT_TRUE(offset.has_value());

      absl::optional<int> style = classification.FindInt("style");
      EXPECT_TRUE(style.has_value());
    }

    const std::string* description = match.FindString("description");
    ASSERT_NE(description, nullptr);
    // |description| is optional: don't verify it is non-empty

    const base::Value::List* description_class =
        match.FindList("descriptionClass");
    ASSERT_NE(description_class, nullptr);

    const std::string* destination_url = match.FindString("destinationUrl");
    ASSERT_NE(destination_url, nullptr);
    EXPECT_FALSE(destination_url->empty());
    EXPECT_TRUE(GURL(*destination_url).is_valid());

    const std::string* type = match.FindString("type");
    ASSERT_NE(type, nullptr);
    EXPECT_FALSE(type->empty());

    absl::optional<bool> is_search_type = match.FindBool("isSearchType");
    EXPECT_TRUE(is_search_type.has_value());

    const std::string* fill_into_edit = match.FindString("fillIntoEdit");
    ASSERT_NE(fill_into_edit, nullptr);
    EXPECT_FALSE(fill_into_edit->empty());

    const std::string* inline_autocompletion =
        match.FindString("inlineAutocompletion");
    ASSERT_NE(inline_autocompletion, nullptr);
    // |inlineAutocompletion| is optional: don't verify it is non-empty

    absl::optional<bool> allowed_to_be_default_match =
        match.FindBool("allowedToBeDefaultMatch");
    EXPECT_TRUE(allowed_to_be_default_match.has_value());
  }
}

IN_PROC_BROWSER_TEST_F(RemoteNtpSearchTest, DoNotLoadWithoutQuery) {
  content::WebContents* active_tab = OpenNewTab();

  const GURL bad_url("https://xkcd.com");

  // Trying to open a URL when no query has been issued should result in the
  // request being dropped.
  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.search.openAutocompleteMatch(0, "
                         "\"%s\", false, false, false, false, false)",
                         bad_url.spec().c_str())));

  EXPECT_EQ(active_tab->GetVisibleURL(), GURL(chrome::kChromeUINewTabURL));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpSearchTest, DoNotLoadBadMatch) {
  content::WebContents* active_tab = OpenNewTab();

  const GURL bad_url("https://xkcd.com");
  const std::string query("what is");

  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.search.queryAutocomplete(\"%s\", false)",
                         query.c_str())));

  auto result = WaitForAutocompleteResult(active_tab);

  const base::Value::List* matches = result.FindList("matches");
  ASSERT_NE(matches, nullptr);

  const auto& match = (*matches)[0].GetDict();

  // Sanity check that the match's URL is not the URL we are attempting to load.
  const std::string* destinationUrl = match.FindString("destinationUrl");
  ASSERT_NE(destinationUrl, nullptr);
  EXPECT_NE(*destinationUrl, bad_url.spec())
      << "Somehow the test query matched with our bad URL";

  // Trying to open a URL that does not match the autocomplete match should
  // result in the request being dropped.
  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.search.openAutocompleteMatch(0, "
                         "\"%s\", false, false, false, false, false)",
                         bad_url.spec().c_str())));

  EXPECT_EQ(active_tab->GetVisibleURL(), GURL(chrome::kChromeUINewTabURL));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpSearchTest, LoadGoodMatch) {
  content::WebContents* active_tab = OpenNewTab();

  const std::string query("what is");

  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.search.queryAutocomplete(\"%s\", false)",
                         query.c_str())));

  auto result = WaitForAutocompleteResult(active_tab);

  const base::Value::List* matches = result.FindList("matches");
  ASSERT_NE(matches, nullptr);

  const auto& match = (*matches)[0].GetDict();

  const std::string* destinationUrl = match.FindString("destinationUrl");
  ASSERT_NE(destinationUrl, nullptr);

  // Loading a match's URL should result in the tab navigating to that URL.
  EXPECT_TRUE(content::ExecJs(
      active_tab,
      base::StringPrintf("rebel.search.openAutocompleteMatch(0, "
                         "\"%s\", false, false, false, false, false)",
                         destinationUrl->c_str())));

  // The URL we actually navigate to may be slightly different than the URL
  // provided in the match, because the browser may update the URL with values
  // that were not available when the match was generated. So only verify that
  // we navigated away from the NTP and that the new host matches.
  EXPECT_NE(active_tab->GetVisibleURL(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(active_tab->GetVisibleURL().host(), GURL(*destinationUrl).host());
}

// Test fixture to allow installing third-party themes.
class RemoteNtpThemeTest : public extensions::ExtensionBrowserTest,
                           public RemoteNtpTest {
 protected:
  void ValidateBackground(const content::ToRenderFrameHost& adapter,
                          const GURL& image_url,
                          const std::string& image_alignment,
                          const std::string& image_tiling,
                          const std::string& attribution_line_1,
                          const std::string& attribution_line_2,
                          const GURL& attribution_url,
                          const GURL& attribution_image_url) {
    // We may not know the exact image URL because it might contain an extension
    // ID as a query param, which isn't stored in the extension itself.
    GURL actual_image_url(
        content::EvalJs(adapter, "ntpBackground.imageUrl").ExtractString());
    actual_image_url = UrlWithoutQuery(actual_image_url);

    GURL actual_attribution_image_url(
        content::EvalJs(adapter, "ntpBackground.attributionImageUrl")
            .ExtractString());
    actual_attribution_image_url =
        UrlWithoutQuery(actual_attribution_image_url);

    EXPECT_EQ(image_url, actual_image_url);
    EXPECT_EQ(image_alignment,
              content::EvalJs(adapter, "ntpBackground.imageAlignment"));
    EXPECT_EQ(image_tiling,
              content::EvalJs(adapter, "ntpBackground.imageTiling"));
    EXPECT_EQ(attribution_line_1,
              content::EvalJs(adapter, "ntpBackground.attributionLine1"));
    EXPECT_EQ(attribution_line_2,
              content::EvalJs(adapter, "ntpBackground.attributionLine2"));
    EXPECT_EQ(attribution_url.spec(),
              content::EvalJs(adapter, "ntpBackground.attributionUrl"));
    EXPECT_EQ(attribution_image_url, actual_attribution_image_url);
  }

  void SetNativeTheme() {
    rebel::RemoteNtpService* remote_ntp_service =
        rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());

    // N.B. can't use dynamic_cast because Chromium disables RTTI
    rebel::RemoteNtpServiceImpl* remote_ntp_service_impl =
        reinterpret_cast<rebel::RemoteNtpServiceImpl*>(remote_ntp_service);

    rebel::RemoteNtpThemeProvider* remote_ntp_theme_provider =
        remote_ntp_service_impl->GetThemeProviderForTesting();
    remote_ntp_theme_provider->SetNativeThemeForTesting(&theme_);
  }

  void InstallTheme(const std::string& theme_directory,
                    const std::string& theme_name) {
    const base::FilePath theme_path =
        test_data_dir_.AppendASCII(theme_directory);

    test::ThemeServiceChangedWaiter theme_change_observer(
        ThemeServiceFactory::GetForProfile(profile()));

    const extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile());
    bool had_previous_theme =
        !!ThemeServiceFactory::GetThemeForProfile(profile());

    // Themes install asynchronously so we must check the number of enabled
    // extensions after theme install completes.
    size_t num_before = extension_registry->enabled_extensions().size();
    ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(
        theme_path, 1, extensions::ExtensionBrowserTest::browser()));
    theme_change_observer.WaitForThemeChanged();
    size_t num_after = extension_registry->enabled_extensions().size();

    // If a theme was already installed, we're just swapping one for another, so
    // no change in extension count.
    int expected_change = had_previous_theme ? 0 : 1;
    EXPECT_EQ(num_before + expected_change, num_after);

    const extensions::Extension* new_theme =
        ThemeServiceFactory::GetThemeForProfile(profile());
    ASSERT_NE(nullptr, new_theme);
    ASSERT_EQ(new_theme->name(), theme_name);
  }
};

IN_PROC_BROWSER_TEST_F(RemoteNtpThemeTest, ToggleDarkMode) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  SetNativeTheme();

  // Initially disable dark mode.
  theme_.SetDarkMode(false);
  theme_.NotifyOnNativeThemeUpdated();

  content::WebContents* active_tab = OpenNewTab();

  // Dark mode should not be applied.
  observer.WaitForDarkMode(false);
  EXPECT_EQ(false, content::EvalJs(active_tab, "ntpDarkMode"));

  // Enable dark mode and wait until the NTP has updated.
  theme_.SetDarkMode(true);
  theme_.NotifyOnNativeThemeUpdated();

  // Check that dark mode has been properly applied.
  observer.WaitForDarkMode(true);
  EXPECT_EQ(true, content::EvalJs(active_tab, "ntpDarkMode"));

  // Disable dark mode and wait until the NTP has updated.
  theme_.SetDarkMode(false);
  theme_.NotifyOnNativeThemeUpdated();

  // Check that dark mode has been removed.
  observer.WaitForDarkMode(false);
  EXPECT_EQ(false, content::EvalJs(active_tab, "ntpDarkMode"));
}

IN_PROC_BROWSER_TEST_F(RemoteNtpThemeTest, SetBackgroundImage) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  content::WebContents* active_tab = OpenNewTab();

  const std::string collection_id("Viasat");
  const GURL image_url("https://viasat.com/logo.png");
  const std::string attribution_line_1("Attribution 1");
  const std::string attribution_line_2("Attribution 2");
  const GURL attribution_url("https://viasat.com");

  auto* background_service =
      NtpCustomBackgroundServiceFactory::GetForProfile(browser()->profile());
  background_service->AddValidBackdropUrlForTesting(image_url);
  background_service->SetCustomBackgroundInfo(
      image_url, image_url, attribution_line_1, attribution_line_2,
      attribution_url, collection_id);

  observer.WaitForBackgroundImage(image_url);

  ValidateBackground(active_tab, image_url, "center center", "no-repeat",
                     attribution_line_1, attribution_line_2, attribution_url,
                     GURL());
}

IN_PROC_BROWSER_TEST_F(RemoteNtpThemeTest, ResetBackgroundImageToDefault) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  content::WebContents* active_tab = OpenNewTab();

  // Set the background image to a valid background
  const std::string collection_id("Viasat");
  const GURL image_url("https://viasat.com/logo.png");
  const std::string attribution_line_1("Attribution 1");
  const std::string attribution_line_2("Attribution 2");
  const GURL attribution_url("https://viasat.com");

  auto* background_service =
      NtpCustomBackgroundServiceFactory::GetForProfile(browser()->profile());
  background_service->AddValidBackdropUrlForTesting(image_url);
  background_service->SetCustomBackgroundInfo(
      image_url, image_url, attribution_line_1, attribution_line_2,
      attribution_url, collection_id);

  observer.WaitForBackgroundImage(image_url);

  ValidateBackground(active_tab, image_url, "center center", "no-repeat",
                     attribution_line_1, attribution_line_2, attribution_url,
                     GURL());

  background_service->ResetCustomBackgroundInfo();
  observer.WaitForBackgroundImage(GURL());

  ValidateBackground(active_tab, GURL(), "", "", "", "", GURL(), GURL());
}

IN_PROC_BROWSER_TEST_F(RemoteNtpThemeTest, InvalidBackgroundImage) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  content::WebContents* active_tab = OpenNewTab();

  const std::string collection_id("");
  const GURL image_url("");
  const std::string attribution_line_1("Attribution 1");
  const std::string attribution_line_2("Attribution 2");
  const GURL attribution_url("https://viasat.com");

  auto* background_service =
      NtpCustomBackgroundServiceFactory::GetForProfile(browser()->profile());
  background_service->SetCustomBackgroundInfo(
      image_url, image_url, attribution_line_1, attribution_line_2,
      attribution_url, collection_id);

  observer.WaitForBackgroundImage(image_url);

  ValidateBackground(active_tab, GURL(), "", "", "", "", GURL(), GURL());
}

IN_PROC_BROWSER_TEST_F(RemoteNtpThemeTest, ThirdPartyThemeWithBackground) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  content::WebContents* active_tab = OpenNewTab();
  const GURL image_url("chrome-search://theme/IDR_THEME_NTP_BACKGROUND");

  ASSERT_NO_FATAL_FAILURE(InstallTheme("theme", "camo theme"));
  observer.WaitForBackgroundImage(image_url);

  ValidateBackground(active_tab, image_url, "center center", "no-repeat", "",
                     "", GURL(), GURL());
}

IN_PROC_BROWSER_TEST_F(RemoteNtpThemeTest, ThirdPartyThemeWithAttribution) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());
  TestRemoteNtpServiceObserver observer(remote_ntp_service);

  content::WebContents* active_tab = OpenNewTab();

  const GURL image_url("chrome-search://theme/IDR_THEME_NTP_BACKGROUND");
  const GURL attribution_image_url(
      "chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION");

  ASSERT_NO_FATAL_FAILURE(
      InstallTheme("theme_with_attribution", "attribution theme"));
  observer.WaitForBackgroundImage(image_url);

  ValidateBackground(active_tab, image_url, "center center", "no-repeat", "",
                     "", GURL(), attribution_image_url);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

class TestWifiService : public wifi::FakeWiFiService {
 public:
  void GetNetworkProperties(const std::string& network_guid,
                            wifi::NetworkProperties* properties,
                            std::string* error) override {
    if (network_guid == "stub_wifi1_guid") {
      properties->ssid = "wifi1";
      properties->bssid = "01:00:00:00:00:00";
      properties->connection_state = onc::connection_state::kConnected;
      properties->signal_strength = -40;
      properties->frequency = 2400;
      properties->link_speed = 512;
      properties->rx_mbps = 1024;
      properties->tx_mbps = 2048;
      properties->noise_measurement = -100;
    } else if (network_guid == "stub_wifi2_guid") {
      properties->ssid = "wifi2";
      properties->bssid = "02:00:00:00:00:00";
      properties->connection_state = onc::connection_state::kNotConnected;
      properties->signal_strength = -80;
      properties->frequency = 5000;
      properties->link_speed = 4096;
      properties->rx_mbps = 8192;
      properties->tx_mbps = 16384;
      properties->noise_measurement = -50;
    } else {
      NOTREACHED() << "Check wifi::FakeWiFiService::FakeWiFiService() for "
                      "updated mock WiFi network GUIDs";
    }
  }
};

// Test fixture to allow setup of fake WiFi networks.
class RemoteNtpWiFiTest : public RemoteNtpTest {
 protected:
  void SetWiFiService() {
    rebel::RemoteNtpService* remote_ntp_service =
        rebel::RemoteNtpServiceFactory::GetForProfile(browser()->profile());

    // N.B. can't use dynamic_cast because Chromium disables RTTI
    rebel::RemoteNtpServiceImpl* remote_ntp_service_impl =
        reinterpret_cast<rebel::RemoteNtpServiceImpl*>(remote_ntp_service);

    remote_ntp_service_impl->SetWiFiService(
        std::make_unique<TestWifiService>());
  }

  base::Value WaitForWiFiStatus(content::WebContents* active_tab) {
    // Script to check every 100ms for the NTP to have received an updated WiFi
    // status. Returns that status.
    static const char kWaitForWiFiStatus[] = R"js(
        (async function() {
          function WaitForWiFiStatus() {
            if (ntpWiFiStatus !== null) {
              return ntpWiFiStatus;
            } else {
              return new Promise((resolve) => {
                window.setTimeout(function() {
                  resolve(WaitForWiFiStatus());
                }, 100);
              });
            }
          }

          return await WaitForWiFiStatus();
        })();
      )js";

    auto result = content::EvalJs(active_tab, kWaitForWiFiStatus);
    return result.ExtractList();
  }

  void ValidateWiFiStatus(const base::Value::Dict& wifi_status,
                          base::StringPiece expected_ssid,
                          base::StringPiece expected_bssid,
                          base::StringPiece expected_connection_state,
                          int expected_rssi,
                          int expected_frequency,
                          double expected_link_speed,
                          int expected_rx_mbps,
                          int expected_tx_mbps,
                          int expected_noise_measurement) {
    auto validate_string = [&wifi_status](auto key, auto expectation) {
      const std::string* result = wifi_status.FindString(key);
      ASSERT_NE(result, nullptr);
      EXPECT_EQ(*result, expectation);
    };

    auto validate_int = [&wifi_status](auto key, auto expectation) {
      const absl::optional<int> result = wifi_status.FindInt(key);
      ASSERT_TRUE(result.has_value());
      EXPECT_EQ(*result, expectation);
    };

    validate_string("ssid", expected_ssid);
    validate_string("bssid", expected_bssid);
    validate_string("connectionState", expected_connection_state);
    validate_int("rssi", expected_rssi);
    validate_int("frequency", expected_frequency);
    validate_int("linkSpeed", expected_link_speed);
    validate_int("rxMbps", expected_rx_mbps);
    validate_int("txMbps", expected_tx_mbps);
    validate_int("noiseMeasurement", expected_noise_measurement);
  }
};

IN_PROC_BROWSER_TEST_F(RemoteNtpWiFiTest, WiFiNetworks) {
  content::WebContents* active_tab = OpenNewTab();
  SetWiFiService();

  EXPECT_TRUE(content::ExecJs(active_tab, "rebel.network.updateWiFiStatus()"));
  auto wifi_status = WaitForWiFiStatus(active_tab);

  const auto& wifi_list = wifi_status.GetList();
  ASSERT_EQ(wifi_list.size(), 2u);

  ValidateWiFiStatus(wifi_list[0].GetDict(), "wifi1", "01:00:00:00:00:00",
                     onc::connection_state::kConnected, -40, 2400, 512, 1024,
                     2048, -100);
  ValidateWiFiStatus(wifi_list[1].GetDict(), "wifi2", "02:00:00:00:00:00",
                     onc::connection_state::kNotConnected, -80, 5000, 4096,
                     8192, 16384, -50);
}

#endif
