#include "utils.h"

#include <boost/uuid/detail/sha1.hpp>

std::ofstream open_write(const boost::filesystem::path &p) {
  std::ofstream f(p.string());
  if (!f.is_open()) {
    throw std::system_error(errno, std::generic_category(),
                            "Unable to open " + p.string() + " for writing");
  }
  return f;
}

std::ifstream open_read(const boost::filesystem::path &p) {
  std::ifstream f(p.string());
  if (!f.is_open()) {
    throw std::system_error(errno, std::generic_category(),
                            "Unable to open " + p.string() + " for reading");
  }
  return f;
}

std::string sha1sum(const boost::filesystem::path &p) {
  auto f = open_read(p);
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  std::string buf(size, '\0');
  f.seekg(0);
  if (!f.read(&buf[0], size)) {
    throw std::system_error(errno, std::generic_category(),
                            "Unable to read content of " + p.string());
  }
  boost::uuids::detail::sha1 sha1;
  sha1.process_bytes(buf.data(), buf.size());
  unsigned hash[5] = {0};
  sha1.get_digest(hash);

  char dgst[41] = {0};
  for (int i = 0; i < 5; i++) {
    sprintf(dgst + (i << 3), "%08x", hash[i]);
  }
  return std::string(dgst);
}

constexpr const char *ostree_remote = "capp";

static void set_remote(OstreeRepo *repo, const std::string &url) {
  g_autoptr(GError) error = nullptr;

  GVariantBuilder var_builder;
  g_autoptr(GVariant) remote_options = nullptr;
  g_variant_builder_init(&var_builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&var_builder, "{s@v}", "gpg-verify",
                        g_variant_new_variant(g_variant_new_boolean(FALSE)));
  remote_options = g_variant_builder_end(&var_builder);

  if (!ostree_repo_remote_change(
          repo, nullptr, OSTREE_REPO_REMOTE_CHANGE_ADD_IF_NOT_EXISTS,
          ostree_remote, url.c_str(), remote_options, nullptr, &error)) {
    throw std::runtime_error("Failed to add a remote to " + url + ": " +
                             error->message);
  }
}

OSTreeRepo::OSTreeRepo(const std::string &url, boost::filesystem::path repo_dir)
    : path_(repo_dir) {
  g_autoptr(GFile) path = nullptr;
  g_autoptr(OstreeRepo) repo = nullptr;
  g_autoptr(GError) error = nullptr;

  path = g_file_new_for_path(path_.c_str());
  repo = ostree_repo_new(path);

  if (!boost::filesystem::is_directory(path_)) {
    if (!ostree_repo_create(repo, OSTREE_REPO_MODE_BARE, nullptr, &error)) {
      throw std::runtime_error("Failed to create ostree repo at `" +
                               path_.string() + "`: " + error->message);
    }
  } else if (!ostree_repo_open(repo, nullptr, &error)) {
    throw std::runtime_error("Failed to open ostree repo at `" +
                             path_.string() + "`: " + error->message);
  }

  repo_ = reinterpret_cast<OstreeRepo *> g_steal_pointer(&repo);
  set_remote(repo_, url);
}

OSTreeRepo::~OSTreeRepo() { g_clear_object(&repo_); }

void OSTreeRepo::checkout(const std::string &commit_hash,
                          const boost::filesystem::path &dst_dir) const {
  const char *const OSTREE_GIO_FAST_QUERYINFO =
      "standard::name,standard::type,standard::size,standard::is-symlink,"
      "standard::symlink-target,unix::device,unix::"
      "inode,unix::mode,unix::uid,unix::gid,unix::rdev";
  OstreeRepoCheckoutMode checkout_mode = OSTREE_REPO_CHECKOUT_MODE_NONE;
  OstreeRepoCheckoutOverwriteMode overwrite_mode =
      OSTREE_REPO_CHECKOUT_OVERWRITE_UNION_FILES;
  g_autoptr(GFile) root = nullptr;
  g_autoptr(GFile) dst = nullptr;
  g_autoptr(GFileInfo) file_info = nullptr;

  g_autoptr(GError) error = nullptr;
  if (!ostree_repo_read_commit(repo_, commit_hash.c_str(), &root, nullptr,
                               nullptr, &error)) {
    throw std::runtime_error("Failed to read commit " + commit_hash + ": " +
                             error->message);
  }

  file_info =
      g_file_query_info(root, OSTREE_GIO_FAST_QUERYINFO,
                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, &error);
  if (file_info == NULL) {
    throw std::runtime_error("Failed to query file info for " + commit_hash +
                             ": " + error->message);
  }

  dst = g_file_new_for_path(dst_dir.c_str());
  if (!ostree_repo_checkout_tree(repo_, checkout_mode, overwrite_mode, dst,
                                 OSTREE_REPO_FILE(root), file_info, NULL,
                                 &error)) {
    throw std::runtime_error("Failed to checkout tree from repo " +
                             commit_hash + ": " + error->message);
  }
}

void OSTreeRepo::pull(const std::string &commit_hash) const {
  g_autoptr(GError) error = nullptr;
  OstreeAsyncProgress *progress = ostree_async_progress_new_and_connect(
      ostree_repo_pull_default_console_progress_changed, nullptr);
  std::array<char *, 2> commit_id{const_cast<char *>(commit_hash.c_str()),
                                  nullptr};

  g_autoptr(GHashTable) ref_list = nullptr;
  if (!ostree_repo_list_commit_objects_starting_with(
          repo_, commit_hash.c_str(), &ref_list, nullptr, &error)) {
    guint length = g_hash_table_size(ref_list);
    if (length >= 1) {
      // already preset
      return;
    }
  }

  GVariantBuilder builder;
  g_autoptr(GVariant) pull_options = nullptr;

  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(
      &builder, "{s@v}", "refs",
      g_variant_new_variant(g_variant_new_strv(
          reinterpret_cast<const char *const *>(&commit_id), -1)));

  pull_options = g_variant_ref_sink(g_variant_builder_end(&builder));

  if (!ostree_repo_pull_with_options(repo_, ostree_remote, pull_options,
                                     progress, nullptr, &error)) {
    throw std::runtime_error("Failed to pull " + commit_hash + ": " +
                             error->message);
  }
}
