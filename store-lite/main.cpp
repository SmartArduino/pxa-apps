// SPDX-License-Identifier: MIT
#include "../store/store_account.hpp"
#include "../store/store_binding.h"
#include "../store/store_client.h"
#include "../store/store_json.h"
#include "../store/store_media.hpp"
#include "../store/store_size.hpp"
#include "ui.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <optional>
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/ui_environment.hpp>
#include <pxa/ui_layout.hpp>
#include <pxa_store_installer.h>
using namespace pxa;
using namespace pxa::ui;
namespace lite {
using sibo::Flag;
using sibo::Painter;
enum class Screen {
  home,
  catalog,
  filters,
  detail,
  search,
  installed,
  downloads,
  account,
  picture,
  keyboard,
  loading,
  count
};
struct Row {
  State<std::string> title{""};
  State<bool> shown{false};
};
struct App;
struct View : ViewModifiers {
  static constexpr Capacity capacity{800, 600, 192, 0, 0};
  App *app;
  explicit View(App *a) : app(a) {};
  template <class Page> bool render(Page &, uint32_t);
};
inline bool image_binding(Transaction<> &tx, uint32_t node,
                          const uint64_t &handle) {
  return !handle || tx.u64(node, 781, handle);
}
struct App {
  Context *ctx = nullptr;
  Environment<DisplayMetrics, UiCapabilities> environment;
  TaskScope jobs, images;
  Permission permission;
  store::Account account;
  std::array<Flag, size_t(Screen::count)> frames;
  std::array<std::function<void()>, 192> callbacks;
  std::array<Row, 10> letter_rows;
  State<bool> groups_shown{true}, letters_shown{false};
  std::array<Row, 6> catalog_rows;
  std::array<Row, 12> installed_rows;
  std::array<Row, 8> download_rows;
  std::array<Row, 20> category_rows;
  State<bool> has_ticket{false}, has_pictures{false};
  State<bool> ready{false}, idle{true}, primary_enabled{false},
      uninstall_visible{false}, previous{false}, next_page{false},
      meta_visible{false}, image_visible{false};
  State<uint64_t> image_handle{0};
  State<std::string> notice{"欢迎来到轻量应用商城"}, catalog_title{"应用"},
      detail_title{"应用详情"}, detail_text{""}, detail_meta{""},
      primary{"安装"}, like_label{"点赞"}, account_text{"未登录"},
      login_label{"网页登录"}, query{""}, picture_title{"应用截图"};
  store_catalog_t catalog{};
  store_taxonomy_t taxonomy{};
  store_app_t selected{};
  store_gallery_t gallery{};
  std::array<StoreInstalledApp, 12> installed{};
  std::array<StoreDownloadEntry, 8> downloads{};
  unsigned installed_count = 0, download_count = 0, picture_index = 0,
           category_count = 0;
  std::array<unsigned, 20> category_indices{};
  std::array<uint64_t, 64> cursors{};
  unsigned page_index = 0;
  char profile[40]{};
  std::string kind = "app", category, sort = "updated", selected_identity;
  Screen screen = Screen::home, detail_from = Screen::catalog,
         search_from = Screen::home, loading_from = Screen::home;
  bool busy = false, stopping = false, initialized = false,
       loading_image = false;
  uint32_t installer_features = 0;
  uint64_t now = 0, last_progress = 0;
  unsigned progress = 101;
  std::optional<Asset> image;
  uint64_t now_ms() const { return now; }
  void clock() {
    (void)ctx->clock().set_period(
        (busy || loading_image) ? 100 : account.clock_period());
  }
  void log(std::string_view text) {
    (void)ctx->log().write(LogLevel::info, text);
  }
  void show(Screen s) {
    for (unsigned i = 0; i < frames.size(); ++i)
      frames[i].value.set(i == unsigned(s));
    screen = s;
    if (s != Screen::picture) {
      image_visible.set(false);
      images.cancel();
    }
    log("Lite page " + std::to_string(unsigned(s)));
  }
  void message(std::string text) { notice.set(std::move(text)); }
  template <class Task> void run(Task task) {
    if (busy)
      return;
    busy = true;
    idle.set(false);
    clock();
    if (!jobs.start(std::move(task))) {
      busy = false;
      idle.set(true);
      message("任务启动失败，请重试");
      clock();
    }
  }
  void loading(Screen from) {
    loading_from = from;
    show(Screen::loading);
  }
  void done() {
    busy = false;
    idle.set(true);
    clock();
  }
  std::string identity(const store_app_t &app) const {
    if (std::strlen(app.lineage_root_key_id) != 64 || !app.app_id[0])
      return {};
    return std::string(app.lineage_root_key_id) + ":" + app.app_id;
  }
  const StoreInstalledApp *local(const store_app_t &app) const {
    auto id = identity(app);
    for (unsigned i = 0; i < installed_count; ++i)
      if (installed[i].identity.view() == id)
        return &installed[i];
    return nullptr;
  }
  void sync_rows() {
    for (unsigned i = 0; i < catalog_rows.size(); ++i) {
      catalog_rows[i].shown.set(i < catalog.count);
      if (i < catalog.count) {
        auto &item = *store_catalog_at(&catalog, i);
        char size[40];
        store::format_package_size(size, item.size);
        const auto *current = local(item);
        catalog_rows[i].title.set(
            std::string(item.name) + "\n" + item.version + " · " + size +
            (current ? (item.release_sequence > current->sequence ? " · 可更新"
                                                                  : " · 已安装")
                     : ""));
      }
    }
    previous.set(page_index > 0);
    next_page.set(catalog.has_more && page_index + 1 < cursors.size());
    for (unsigned i = 0; i < installed_rows.size(); ++i) {
      installed_rows[i].shown.set(i < installed_count);
      if (i < installed_count)
        installed_rows[i].title.set(std::string(installed[i].app_id.view()) +
                                    "\n" +
                                    std::string(installed[i].version.view()));
    }
    for (unsigned i = 0; i < download_rows.size(); ++i) {
      download_rows[i].shown.set(i < download_count);
      if (i < download_count)
        download_rows[i].title.set(std::string(downloads[i].app_id.view()));
    }
  }
  Task<void> refresh_library() {
    auto list = co_await ctx->store_installer().installed(installed);
    if (list)
      installed_count = *list;
    auto files = co_await ctx->store_installer().downloads(downloads);
    if (files)
      download_count = *files;
    sync_rows();
    co_return Result<void>{};
  }
  Task<void> initialize() {
    auto limits = co_await ctx->store_installer().capabilities();
    if (limits)
      installer_features = limits->features;
    auto runtime = co_await ctx->device().runtime_info();
    if (runtime)
      store_client_profile_for_device(
          profile, sizeof profile, runtime->target.c_str(),
          runtime->architecture.c_str(), runtime->engine.c_str(),
          runtime->formats, installer_features);
    co_await refresh_library();
    initialized = true;
    message(profile[0] ? "应用、游戏和更新，一处轻松管理"
                       : "当前设备暂不支持在线商城");
    ready.set(true);
    show(Screen::home);
    done();
    co_return Result<void>{};
  }
  Task<std::string> fetch(std::string address) {
    if (!permission) {
      auto p =
          co_await ctx->permissions().acquire("net.client", PXA_STORE_ORIGIN);
      if (!p)
        co_return std::unexpected(p.error());
      permission = std::move(*p);
      account.network(permission.handle(), now);
    }
    std::array<std::byte, 1024> packet{};
    std::array<std::byte, 1024> chunk{};
    auto response = co_await ctx->net().request(
        {.url = address, .max_response_bytes = 65536, .timeout_ms = 20000},
        permission, packet);
    if (!response) {
      log("Lite network error " + std::to_string(int(response.error())));
      co_return std::unexpected(response.error());
    }
    if (response->status_code != 200) {
      log("Lite HTTP status " + std::to_string(response->status_code));
      co_return std::unexpected(Error::unavailable);
    }
    std::string data;
    for (;;) {
      auto n = response->body.read(chunk);
      if (!n) {
        if (n.error() == Error::would_block) {
          co_await ctx->clock().yield();
          continue;
        }
        co_return std::unexpected(n.error());
      }
      if (!*n)
        break;
      if (data.size() + *n > 65536)
        co_return std::unexpected(Error::resource_limit);
      data.append(reinterpret_cast<const char *>(chunk.data()), *n);
    }
    co_return data;
  }
  void browse(std::string filter = "app", std::string order = "updated") {
    if (busy)
      return;
    kind = std::move(filter);
    sort = std::move(order);
    category.clear();
    page_index = 0;
    cursors = {};
    show(Screen::catalog);
    run(load_catalog());
  }
  bool parse_catalog(std::string_view body) {
    // Parsing is synchronous: keep the large temporary models off the bounded
    // coroutine-frame pool, including when a request is repeated in a loop.
    store_catalog_t result{};
    store_taxonomy_t types{};
    if (!store_json_parse_catalog(
            reinterpret_cast<const uint8_t *>(body.data()), body.size(),
            &result, &types))
      return false;
    catalog = result;
    // Later pages omit the shared taxonomy; keep the first-page filters.
    if (types.kind_count || types.category_count)
      taxonomy = types;
    return true;
  }
  Task<void> load_catalog() {
    loading(catalog.count ? Screen::catalog : Screen::home);
    if (!profile[0]) {
      message("当前设备暂不支持在线商城");
      show(Screen::catalog);
      done();
      co_return Result<void>{};
    }
    message("正在加载…");
    auto cursor = cursors[page_index];
    // Compatibility filtering can leave a page empty with a valid next cursor.
    // Bound automatic continuation so a low-memory/older Host reaches usable
    // entries without an unbounded request chain or extra response buffers.
    for (unsigned skipped = 0; ; ++skipped) {
      char url[512];
      if (!store_client_build_catalog_url(url, sizeof url, profile, "", cursor,
                                          query.get().c_str(), kind.c_str(),
                                          category.c_str(), sort.c_str(), 6)) {
        message("筛选条件过长，请简化搜索");
        show(Screen::catalog);
        done();
        co_return Result<void>{};
      }
      auto body = co_await fetch(url);
      if (!body || !parse_catalog(*body)) {
        message("加载失败，检查网络后重试");
        show(Screen::catalog);
        done();
        co_return Result<void>{};
      }
      if (!catalog.count && catalog.has_more && catalog.next_cursor != cursor && skipped < 4) {
        cursor = catalog.next_cursor;
        continue;
      }
      cursors[page_index] = cursor;
      break;
    }
    sync_rows();
    catalog_title.set(query.get().empty() ? (kind == "game"  ? "游戏"
                                             : kind == "app" ? "应用"
                                                             : "热门榜")
                                          : "搜索结果");
    message(catalog.count ? "第 " + std::to_string(page_index + 1) + " 页"
                          : catalog.has_more ? "当前页无兼容项，可继续下一页"
                                             : "暂时没有匹配的应用");
    show(Screen::catalog);
    log("Lite catalog loaded n=" + std::to_string(catalog.count));
    done();
    co_return Result<void>{};
  }
  void page(int delta) {
    if (busy)
      return;
    if (delta < 0 && page_index)
      --page_index;
    else if (delta > 0 && catalog.has_more && page_index + 1 < cursors.size()) {
      cursors[++page_index] = catalog.next_cursor;
    } else
      return;
    run(load_catalog());
  }
  void filters() {
    if (busy)
      return;
    category_count = 0;
    for (unsigned i = 0; i < taxonomy.category_count && category_count < 20;
         ++i) {
      auto &c = taxonomy.categories[i];
      if (kind.empty() || kind == c.kind) {
        category_indices[category_count] = i;
        category_rows[category_count].title.set(c.label);
        category_rows[category_count++].shown.set(true);
      }
    }
    for (unsigned i = category_count; i < 20; ++i)
      category_rows[i].shown.set(false);
    show(Screen::filters);
  }
  void select_category(int index) {
    if (busy)
      return;
    category =
        index < 0 ? "" : taxonomy.categories[category_indices[index]].value;
    page_index = 0;
    show(Screen::catalog);
    run(load_catalog());
  }
  void detail(const store_app_t &item) {
    if (busy)
      return;
    detail_from = screen;
    gallery = {};
    picture_index = 0;
    meta_visible.set(false);
    selected = item;
    selected_identity = identity(selected);
    show(Screen::detail);
    describe();
    run(load_detail());
  }
  Task<void> load_detail() {
    loading(detail_from);
    char url[512];
    if (!store_client_build_identity_url(url, sizeof url, profile,
                                         selected.app_id,
                                         selected.lineage_root_key_id, "")) {
      message("应用身份不完整");
      show(Screen::detail);
      done();
      co_return Result<void>{};
    }
    auto body = co_await fetch(url);
    if (body && store_json_parse_app_detail(
                    reinterpret_cast<const uint8_t *>(body->data()),
                    body->size(), &selected, &gallery)) {
      selected_identity = identity(selected);
      message("选择安装或打开应用");
      describe();
    } else
      message("详情加载失败，可以重试");
    account.target(&selected, now);
    show(Screen::detail);
    done();
    co_return Result<void>{};
  }
  void describe() {
    detail_title.set(selected.name[0] ? selected.name : selected.app_id);
    char size[40];
    store::format_package_size(size, selected.size);
    detail_text.set(
        std::string(selected.version) + " · " + size + "\n" + selected.summary +
        (selected.changelog[0] ? "\n\n更新内容\n" : "") + selected.changelog);
    std::string meta = "适用设备：" + std::string(selected.platforms) +
                       "\n系统：" + selected.min_sdk + "\n权限：";
    for (unsigned i = 0; i < selected.permission_count; ++i)
      meta += "\n" + std::string(selected.permissions[i].name) + " · " +
              selected.permissions[i].scope;
    detail_meta.set(meta);
    const auto *current = local(selected);
    has_ticket.set(selected.size && selected.download_ticket_url[0]);
    has_pictures.set(gallery.count > 0);
    if (!selected.size && current)
      detail_text.set("已安装版本：" + std::string(selected.version));
    const bool update =
        current && selected.release_sequence > current->sequence;
    primary.set(current && !update ? "打开" : update ? "更新" : "安装");
    primary_enabled.set(
        current && !update
            ? true
            : selected.size && selected.download_ticket_url[0] &&
                  !selected_identity.empty() &&
                  (!selected.requires_resources || (installer_features & 1)));
    uninstall_visible.set(current && current->uninstallable &&
                          current->app_id.view() != "pxa-store-lite");
  }
  Task<void> install(bool only_download = false) {
    loading(Screen::detail);
    message(only_download ? "正在准备下载…" : "等待系统确认安装…");
    if (selected.publisher_binding[0]) {
      std::array<uint8_t, 396> binding{};
      auto bytes =
          store_binding_decode(selected.publisher_binding, binding.data());
      if (!bytes || !store_binding_matches(binding.data(), bytes,
                                           selected_identity.c_str())) {
        message("应用发布者信息无效，请重试");
        show(Screen::detail);
        done();
        co_return Result<void>{};
      }
      auto bound = co_await ctx->store_installer().register_namespace(
          std::as_bytes(std::span(binding.data(), bytes)));
      // Publisher display bindings are optional on Hosts without an issuer
      // policy. The installer still verifies the full signed package identity.
      if (!bound && bound.error() != Error::unavailable &&
          bound.error() != Error::unsupported) {
        log("Lite namespace error " + std::to_string(int(bound.error())));
        message("系统未能验证发布者，请检查系统版本");
        show(Screen::detail);
        done();
        co_return Result<void>{};
      }
    }
    char ticket[512];
    if (!store_client_build_download_url(ticket, sizeof ticket,
                                         selected.download_ticket_url)) {
      message("下载地址无效");
      show(Screen::detail);
      done();
      co_return Result<void>{};
    }
    StorePackage package{selected.app_id, selected.download_ticket_url,
                         selected.sha256, selected.size};
    bool ok;
    if (only_download) {
      auto result = co_await ctx->store_installer().download(package);
      ok = bool(result);
    } else {
      auto result = co_await ctx->store_installer().install_identity(
          selected_identity, package);
      ok = bool(result);
    }
    message(ok ? (only_download ? "下载完成，可在下载管理中安装" : "安装完成")
               : "操作未完成，可能已取消、存储不足或网络中断");
    co_await refresh_library();
    describe();
    show(Screen::detail);
    progress = 101;
    done();
    co_return Result<void>{};
  }
  Task<void> launch() {
    auto result =
        co_await ctx->store_installer().launch_identity(selected_identity);
    if (!result)
      message("暂时无法打开，请检查设备的防沉迷设置");
    done();
    co_return Result<void>{};
  }
  void primary_action() {
    if (busy)
      return;
    if (primary.get() == "打开")
      run(launch());
    else
      run(install());
  }
  Task<void> remove() {
    auto result =
        co_await ctx->store_installer().uninstall_identity(selected_identity);
    message(result ? "卸载完成" : "卸载已取消或未完成");
    co_await refresh_library();
    describe();
    done();
    co_return Result<void>{};
  }
  void library() {
    if (busy)
      return;
    show(Screen::installed);
    run(library_task());
  }
  Task<void> library_task() {
    loading(Screen::home);
    message("正在读取已安装应用…");
    co_await refresh_library();
    message(installed_count ? "选择应用可打开、更新或卸载"
                            : "还没有已安装的应用");
    show(Screen::installed);
    done();
    co_return Result<void>{};
  }
  void installed_detail(unsigned i) {
    if (busy || i >= installed_count)
      return;
    selected = {};
    std::snprintf(selected.app_id, sizeof selected.app_id, "%s",
                  installed[i].app_id.c_str());
    std::snprintf(selected.lineage_root_key_id,
                  sizeof selected.lineage_root_key_id, "%.64s",
                  installed[i].identity.c_str());
    std::snprintf(selected.version, sizeof selected.version, "%s",
                  installed[i].version.c_str());
    selected.release_sequence = installed[i].sequence;
    selected_identity = std::string(installed[i].identity.view());
    detail_from = Screen::installed;
    gallery = {};
    meta_visible.set(false);
    describe();
    message("可直接打开或卸载；联网刷新详情可检查新版");
    account.target(&selected, now);
    show(Screen::detail);
  }
  Task<void> update_check() {
    loading(Screen::installed);
    co_await refresh_library();
    unsigned updates = 0, failures = 0;
    std::string text;
    for (unsigned i = 0; i < installed_count; ++i) {
      message("正在检查 " + std::to_string(i + 1) + " / " +
              std::to_string(installed_count));
      char url[512], root[65];
      std::snprintf(root, sizeof root, "%.64s", installed[i].identity.c_str());
      if (!store_client_build_identity_url(url, sizeof url, profile,
                                           installed[i].app_id.c_str(), root,
                                           "")) {
        ++failures;
        continue;
      }
      auto body = co_await fetch(url);
      store_app_t app{};
      if (!body || !store_json_parse_app_detail(
                       reinterpret_cast<const uint8_t *>(body->data()),
                       body->size(), &app)) {
        ++failures;
        continue;
      }
      if (app.release_sequence > installed[i].sequence) {
        ++updates;
        text += std::string(app.name) + "\n";
      }
    }
    message("发现 " + std::to_string(updates) + " 个更新" +
            (failures ? "，部分应用检查失败" : "") +
            (text.empty() ? "" : "\n" + text));
    show(Screen::installed);
    done();
    co_return Result<void>{};
  }
  void files() {
    if (busy)
      return;
    show(Screen::downloads);
    run(files_task());
  }
  Task<void> files_task() {
    loading(Screen::home);
    co_await refresh_library();
    message(download_count ? "选择安装，或清理不再需要的下载"
                           : "暂无下载，应用详情中可选择仅下载");
    show(Screen::downloads);
    done();
    co_return Result<void>{};
  }
  Task<void> file_action(unsigned i, bool erase) {
    if (i >= download_count) {
      done();
      co_return Result<void>{};
    }
    const std::string file(downloads[i].filename.view());
    auto result = erase ? co_await ctx->store_installer().remove_download(file)
                        : co_await ctx->store_installer().install_file(file);
    message(result ? (erase ? "下载已删除" : "安装完成")
                   : "操作已取消或未完成");
    co_await refresh_library();
    done();
    co_return Result<void>{};
  }
  void account_view() {
    show(Screen::account);
    account_status();
  }
  void account_status() {
    std::string text;
    switch (account.phase) {
    case store::AccountPhase::signed_in:
      text = std::string("已登录：") + account.display_name;
      break;
    case store::AccountPhase::waiting:
      text = std::string("手机访问\napp.doit.am/device\n\n输入登录码\n") +
             account.user_code + "\n\n在手机上确认授权";
      break;
    case store::AccountPhase::starting:
      text = "正在获取登录码…";
      break;
    case store::AccountPhase::restoring:
      text = "正在恢复登录…";
      break;
    case store::AccountPhase::failed:
    case store::AccountPhase::expired:
    case store::AccountPhase::denied:
      text = "登录未完成，请重试";
      break;
    default:
      text = "登录后可同步点赞。\n手机访问 "
             "app.doit.am/device，输入设备显示的短码即可。";
    }
    log("Lite account phase=" + std::to_string(unsigned(account.phase)));
    account_text.set(text);
    login_label.set(account.phase == store::AccountPhase::signed_in ? "退出登录"
                    : account.phase == store::AccountPhase::waiting
                        ? "取消登录"
                        : "网页登录");
    like_label.set((account.liked ? "已点赞 · 取消" : "点赞") +
                   std::string(" · ") + std::to_string(account.like_count));
  }
  Task<void> login() {
    if (!permission) {
      auto grant =
          co_await ctx->permissions().acquire("net.client", PXA_STORE_ORIGIN);
      if (!grant) {
        message("联网未获允许，请重试");
        done();
        co_return Result<void>{};
      }
      permission = std::move(*grant);
      account.network(permission.handle(), now);
    }
    account.login(now);
    done();
    account_status();
    clock();
    co_return Result<void>{};
  }
  void account_action() {
    if (account.authenticated())
      account.logout(now);
    else if (account.phase == store::AccountPhase::waiting)
      account.cancel();
    else
      run(login());
    account_status();
    clock();
  }
  void like() {
    if (!account.authenticated()) {
      account_view();
      message("登录后可点赞");
      return;
    }
    account.toggle_like(now);
    clock();
  }
  void screenshot(int delta = 0) {
    if (!gallery.count) {
      message("此应用暂无截图");
      return;
    }
    if (delta < 0 && picture_index)
      --picture_index;
    else if (delta > 0 && picture_index + 1 < gallery.count)
      ++picture_index;
    else if (screen != Screen::picture)
      picture_index = 0;
    show(Screen::picture);
    images.cancel();
    image_visible.set(false);
    image.reset();
    message("正在加载图片…");
    loading_image = true;
    if (!images.start(load_image())) {
      loading_image = false;
      message("图片任务启动失败");
    }
    clock();
  }
  Task<void> load_image() {
    struct Finish {
      App &owner;
      ~Finish() {
        owner.loading_image = false;
        if (!owner.stopping)
          owner.clock();
      }
    } finish{*this};
    char url[320];
    if (!store::media_url(gallery.screenshots[picture_index].url, url,
                          sizeof url) ||
        !permission) {
      message("图片地址无效");
      co_return Result<void>{};
    }
    static constexpr std::array<NetHeaderView, 2> headers{
        {{"Accept", "image/png, image/jpeg"}, {"Accept-Encoding", "identity"}}};
    static constexpr std::array<std::string_view, 2> wanted{"content-type",
                                                            "content-encoding"};
    auto req = co_await ctx->http().open({.url = url,
                                          .headers = headers,
                                          .wanted_headers = wanted,
                                          .options = {.buffer_bytes = 4096,
                                                      .header_bytes = 512,
                                                      .max_body_bytes = 131072,
                                                      .timeout_ms = 20000}},
                                         permission);
    if (!req) {
      log("Lite image request error " + std::to_string(int(req.error())));
      message("图片加载失败，请重试");
      co_return Result<void>{};
    }
    auto response = co_await std::move(*req).response();
    if (!response || response->status_code != 200) {
      log("Lite image response error " +
          std::to_string(response ? response->status_code
                                  : int(response.error())));
      message("图片加载失败，请重试");
      co_return Result<void>{};
    }
    auto decoded = co_await ctx->assets().decode_image(
        std::move(response->body),
        {.max_width = 1024,
         .max_height = 1024,
         .max_pixels = 384000,
         .max_encoded_bytes = 131072,
         .formats = ImageFormats::jpeg | ImageFormats::png});
    if (decoded) {
      image.emplace(std::move(*decoded));
      image_handle.set(image->handle());
      image_visible.set(true);
      log("Lite image ready");
      message("图片 " + std::to_string(picture_index + 1) + " / " +
              std::to_string(gallery.count));
    } else {
      log("Lite image decode error " + std::to_string(int(decoded.error())));
      message("图片暂不可用，可重试");
    }
    co_return Result<void>{};
  }
  void cancel() {
    jobs.cancel();
    done();
    message("已取消本次操作，安装结果可在已安装列表中查看");
  }
  void search() {
    if (busy)
      return;
    search_from = screen;
    if (screen == Screen::home) {
      kind.clear();
      category.clear();
    }
    show(Screen::search);
  }
  void keyboard() {
    groups_shown.set(true);
    letters_shown.set(false);
    show(Screen::keyboard);
  }
  void letter_group(unsigned i) {
    static constexpr const char *groups[] = {
        "abc",  "def", "ghi",  "jkl",        "mno",
        "pqrs", "tuv", "wxyz", "0123456789", "-_. "};
    auto text = std::string_view(groups[i]);
    for (unsigned j = 0; j < 10; ++j) {
      letter_rows[j].shown.set(j < text.size());
      if (j < text.size())
        letter_rows[j].title.set(std::string(1, text[j]));
    }
    groups_shown.set(false);
    letters_shown.set(true);
  }
  void choose_letter(unsigned i) {
    if (query.get().size() < 64)
      query.set(query.get() + letter_rows[i].title.get());
    groups_shown.set(true);
    letters_shown.set(false);
  }
  void erase_letter() {
    auto text = query.get();
    if (!text.empty()) {
      size_t i = text.size() - 1;
      while (i && (uint8_t(text[i]) & 0xc0) == 0x80)
        --i;
      text.resize(i);
      query.set(text);
    }
  }
  void do_search() {
    if (busy)
      return;
    page_index = 0;
    category.clear();
    show(Screen::catalog);
    run(load_catalog());
  }
  void back() {
    if (screen == Screen::loading) {
      cancel();
      show(loading_from);
      return;
    }
    if (busy)
      cancel();
    if (screen == Screen::keyboard) {
      if (letters_shown.get()) {
        groups_shown.set(true);
        letters_shown.set(false);
      } else
        show(Screen::search);
    } else if (screen == Screen::picture) {
      image_visible.set(false);
      images.cancel();
      image.reset();
      show(Screen::detail);
    } else if (screen == Screen::detail)
      show(detail_from);
    else if (screen == Screen::search)
      show(search_from);
    else if (screen == Screen::filters)
      show(Screen::catalog);
    else
      show(Screen::home);
  }
  Result<void> on_start(Context &c, std::span<const std::byte> config) {
    ctx = &c;
    auto result = environment.initialize(config);
    if (!result)
      return result;
    (void)c.window().fullscreen(WindowBarMode::hidden, WindowBarMode::hidden);
    account.start();
    show(Screen::home);
    busy = true;
    clock();
    return c.tasks().start(initialize());
  }
  Result<bool> on_event(Context &, const Event &event) {
    jobs.reap();
    images.reap();
    pxa_event_t raw{event.service, event.opcode, event.token,
                    reinterpret_cast<const uint8_t *>(event.payload.data()),
                    uint32_t(event.payload.size())};
    const bool handled = account.event(raw, now);
    if (auto tick = decode_clock_tick(event)) {
      now = tick->timestamp_us / 1000;
      account.tick(now);
      if (account.changed) {
        account.changed = false;
        account_status();
      }
      clock();
      return true;
    }
    if (event.is<StoreDownloadProgress>() && busy) {
      auto p = decode_store_download_progress(event);
      if (p && p->total && now - last_progress >= 1000) {
        last_progress = now;
        unsigned percent = unsigned(p->received * 100 / p->total);
        if (percent != progress) {
          progress = percent;
          message("正在下载 · " + std::to_string(percent) + "%");
        }
      }
      return true;
    }
    if (account.changed) {
      account.changed = false;
      account_status();
      clock();
    }
    if (handled)
      return true;
    return environment.update(event);
  }
  void on_foreground(Context &c) {
    (void)c.window().fullscreen(WindowBarMode::hidden, WindowBarMode::hidden);
    account.pause(false);
    clock();
  }
  void on_background(Context &) {
    account.pause(true);
    images.cancel();
    clock();
  }
  void on_stop(Context &) {
    stopping = true;
    account.stop();
    jobs.cancel();
    images.cancel();
    image.reset();
  }
  BackAction on_back() {
    if (screen == Screen::home) {
      if (busy) {
        cancel();
        return BackAction::stay;
      }
      return BackAction::close;
    }
    back();
    return BackAction::stay;
  }
  auto view() {
    return SafeArea(environment.display(), View(this))
        .background(sibo::paper)
        .enabled(ready);
  }
};

template <class Page> bool View::render(Page &page, uint32_t parent) {
  auto &a = *app;
  Painter p(page, a.callbacks);
  auto metrics = a.environment.display().get();
  const int width = int(uint64_t(metrics.width) * 65536 /
                        std::max(1u, metrics.density_q16)),
            height = int(uint64_t(metrics.height) * 65536 /
                         std::max(1u, metrics.density_q16));
  p.roomy = width >= 380;
  p.header_size = height < 200 ? 28 : 34;
  auto root = p.box(parent);
  p.fill(root, true);
  p.padding(root, 8, 5, 8, 5);
  auto frame = [&](Screen s) {
    return p.frame(root, a.frames[unsigned(s)].value);
  };
  auto header = [&](uint32_t f, auto &title) {
    p.header(f, title, [&a] { a.back(); });
  };
  auto button = [&](uint32_t b, auto &&label, std::function<void()> fn,
                    bool primary = false) {
    auto n = p.button(b, label, std::move(fn), primary);
    p.enabled(n, a.idle);
    return n;
  };
  {
    auto f = frame(Screen::home), b = p.scroll(p.body(f));
    p.text(b, "小巧的商城，大大的发现", Font::title);
    p.text(b, a.notice, Font::caption);
    button(
        b, "应用",
        [&a] {
          a.query.set("");
          a.browse("app");
        },
        true);
    button(b, "游戏", [&a] {
      a.query.set("");
      a.browse("game");
    });
    button(b, "热门榜", [&a] {
      a.query.set("");
      a.browse("", "downloads");
    });
    button(b, "搜索", [&a] { a.search(); });
    button(b, "已安装与更新", [&a] { a.library(); });
    button(b, "下载管理", [&a] { a.files(); });
    button(b, "我的账号", [&a] { a.account_view(); });
    auto title = std::string_view("轻量应用商城");
    p.header(f, title, [&a] { (void)a.ctx->window().request_back(); }, true);
  }
  {
    auto f = frame(Screen::catalog), b = p.scroll(p.body(f));
    p.text(b, a.notice, Font::caption);
    for (unsigned i = 0; i < a.catalog_rows.size(); ++i) {
      auto n = button(b, a.catalog_rows[i].title, [&a, i] {
        if (i < a.catalog.count)
          a.detail(*store_catalog_at(&a.catalog, i));
      });
      p.visible(n, a.catalog_rows[i].shown);
    }
    auto n = button(b, "下一页", [&a] { a.page(1); }, true);
    p.visible(n, a.next_page);
    n = button(b, "上一页", [&a] { a.page(-1); });
    p.visible(n, a.previous);
    button(b, "分类筛选", [&a] { a.filters(); });
    button(b, "搜索", [&a] { a.search(); });
    button(b, "刷新 / 重试", [&a] { a.run(a.load_catalog()); });
    header(f, a.catalog_title);
  }
  {
    auto f = frame(Screen::filters), b = p.scroll(p.body(f));
    button(b, "全部分类", [&a] { a.select_category(-1); }, true);
    for (unsigned i = 0; i < 20; ++i) {
      auto n = button(b, a.category_rows[i].title,
                      [&a, i] { a.select_category(int(i)); });
      p.visible(n, a.category_rows[i].shown);
    }
    auto title = std::string_view("选择分类");
    header(f, title);
  }
  {
    auto f = frame(Screen::detail), b = p.scroll(p.body(f));
    p.text(b, a.detail_title, Font::headline);
    auto n = button(b, a.primary, [&a] { a.primary_action(); }, true);
    p.enabled(n, a.primary_enabled);
    p.text(b, a.detail_text);
    p.text(b, a.notice, Font::caption);
    n = button(b, "仅下载", [&a] {
      if (!a.busy)
        a.run(a.install(true));
    });
    p.visible(n, a.has_ticket);
    n = button(b, "查看截图", [&a] { a.screenshot(); });
    p.visible(n, a.has_pictures);
    button(b, a.like_label, [&a] { a.like(); });
    button(b, "权限与兼容信息",
           [&a] { a.meta_visible.set(!a.meta_visible.get()); });
    n = p.text(b, a.detail_meta, Font::caption);
    p.visible(n, a.meta_visible);
    n = button(b, "卸载应用", [&a] { a.run(a.remove()); });
    p.visible(n, a.uninstall_visible);
    button(b, "刷新详情", [&a] { a.run(a.load_detail()); });
    auto title = std::string_view("应用详情");
    header(f, title);
  }
  {
    auto f = frame(Screen::search), b = p.scroll(p.body(f));
    p.text(b, a.query, Font::title);
    button(b, "按键输入", [&a] { a.keyboard(); }, true);
    if (a.environment.capabilities().get().supports(
            UiFeature::text_input_control)) {
      p.text(b, "中文也可用系统输入法", Font::caption);
      auto input = TextInput(a.query).max_bytes(64).single_line().on_submit(
          [&a] { a.do_search(); });
      p.ok &= input.render(page, b);
    }
    button(b, "开始搜索", [&a] { a.do_search(); }, true);
    button(b, "清空", [&a] { a.query.set(""); });
    static constexpr const char *words[] = {"学习", "数学", "英语", "诗词",
                                            "工具", "天气", "音乐", "迷宫",
                                            "拼音", "骰子"};
    for (auto word : words) {
      auto label = std::string_view(word);
      button(b, label, [&a, word] {
        a.query.set(word);
        a.do_search();
      });
    }
    auto title = std::string_view("搜索");
    header(f, title);
  }
  {
    auto f = frame(Screen::keyboard), b = p.scroll(p.body(f));
    p.text(b, a.query, Font::title);
    auto groups = p.box(b);
    p.visible(groups, a.groups_shown);
    static constexpr const char *names[] = {"abc",  "def",       "ghi", "jkl",
                                            "mno",  "pqrs",      "tuv", "wxyz",
                                            "数字", "符号与空格"};
    for (unsigned i = 0; i < 10; ++i) {
      auto label = std::string_view(names[i]);
      button(groups, label, [&a, i] { a.letter_group(i); });
    }
    auto letters = p.box(b);
    p.visible(letters, a.letters_shown);
    for (unsigned i = 0; i < 10; ++i) {
      auto n = button(letters, a.letter_rows[i].title,
                      [&a, i] { a.choose_letter(i); });
      p.visible(n, a.letter_rows[i].shown);
    }
    button(b, "完成并搜索", [&a] { a.do_search(); }, true);
    button(b, "退一格", [&a] { a.erase_letter(); });
    button(b, "清空", [&a] { a.query.set(""); });
    auto title = std::string_view("按键输入");
    header(f, title);
  }

  {
    auto f = frame(Screen::installed), b = p.scroll(p.body(f));
    p.text(b, a.notice, Font::caption);
    button(b, "检查所有更新", [&a] { a.run(a.update_check()); }, true);
    for (unsigned i = 0; i < 12; ++i) {
      auto n = button(b, a.installed_rows[i].title,
                      [&a, i] { a.installed_detail(i); });
      p.visible(n, a.installed_rows[i].shown);
    }
    button(b, "刷新", [&a] { a.run(a.library_task()); });
    auto title = std::string_view("已安装与更新");
    header(f, title);
  }
  {
    auto f = frame(Screen::downloads), b = p.scroll(p.body(f));
    p.text(b, a.notice, Font::caption);
    for (unsigned i = 0; i < 8; ++i) {
      auto box = p.box(b);
      p.visible(box, a.download_rows[i].shown);
      p.text(box, a.download_rows[i].title, Font::title);
      button(
          box, "安装下载", [&a, i] { a.run(a.file_action(i, false)); }, true);
      button(box, "删除下载", [&a, i] { a.run(a.file_action(i, true)); });
    }
    button(b, "刷新", [&a] { a.run(a.files_task()); });
    auto title = std::string_view("下载管理");
    header(f, title);
  }
  {
    auto f = frame(Screen::account), b = p.scroll(p.body(f));
    p.text(b, a.account_text);
    p.text(b, a.notice, Font::caption);
    button(b, a.login_label, [&a] { a.account_action(); }, true);
    auto title = std::string_view("我的账号");
    header(f, title);
  }
  {
    auto f = frame(Screen::picture), b = p.scroll(p.body(f));
    auto img = page.create(b, protocol::image);
    p.fill(img);
    p.ok &=
        p.tx.logical_px(img, protocol::height, std::max(80, height - 160)) &&
        image_binding(p.tx, img, a.image_handle.get()) &&
        page.template bind<uint64_t, image_binding>(a.image_handle, img);
    p.visible(img, a.image_visible);
    p.text(b, a.notice, Font::caption);
    button(b, "下一张", [&a] { a.screenshot(1); });
    button(b, "上一张", [&a] { a.screenshot(-1); });
    button(b, "重试图片", [&a] { a.screenshot(); });
    header(f, a.picture_title);
  }
  {
    auto f = frame(Screen::loading), b = p.scroll(p.body(f));
    p.text(b, a.notice, Font::title);
    p.button(b, "取消本次操作", [&a] { a.back(); });
    auto title = std::string_view("请稍候");
    header(f, title);
  }
  return p.ok;
}
} // namespace lite
PXA_APPLICATION(lite::App)
