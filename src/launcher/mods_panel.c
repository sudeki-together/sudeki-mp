#define _WIN32_WINNT 0x0600
#define COBJMACROS
#include "mods_panel.h"
#include "engine/texture_mod_index.h"
#include "modding/mod_archive.h"
#include "modding/mod_groups.h"
#include "modding/mod_image.h"
#include "modding/mod_manifest.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <wchar.h>
#include <wincodec.h>
#include <windows.h>

#define PANEL_CLASS L"SudekiMPModsPanel"
#define PREVIEW_CLASS L"SudekiMPModPreview"
#define NAME_CLASS L"SudekiMPModName"
#define PATH_CAP 1024
#define THUMB_SIDE 80
#define MAX_ARCHIVES 512
#define MAX_MODS 256
#define IMAGE_LIMIT (128u * 1024u * 1024u)
/* Thumbnail decoding: 8 threads share the one mapped archive. Results are
 * collected in batches so at most DECODE_BATCH bitmaps exist at a time. */
#define DECODE_WORKERS 8
#define DECODE_BATCH 512
/* The launcher palette: the panel sits on its page surface. */
#define BG RGB(23, 34, 49)
#define INPUT_BG RGB(17, 27, 40)
#define FG RGB(232, 240, 248)
#define MUTED RGB(159, 181, 202)
#define CYAN RGB(54, 193, 218)
#define BUTTON_BG RGB(38, 55, 75)
#define OUTLINE RGB(77, 105, 133)
#define SELECTED_BG RGB(32, 58, 84)

enum {
  ID_MOD = 2100,
  ID_NEW,
  ID_ENABLED,
  ID_FOLDER,
  ID_RESCAN,
  ID_CHARACTER,
  ID_CATEGORY,
  ID_SEARCH,
  ID_GRID,
  ID_BROWSE,
  ID_APPLY,
  ID_REVERT,
  ID_EXPORT,
  ID_NAME,
  ID_NAME_OK,
  ID_NAME_CANCEL,
  ID_HEADING,
  ID_NOTE
};
enum { CAT_WEAPONS, CAT_ARMOUR, CAT_BODY, CAT_OTHER, CAT_MODELS };
static const WCHAR *characters[] = {L"Tal", L"Ailish", L"Buki", L"Elco",
                                    L"World / Other"};
static const WCHAR *categories[] = {L"Weapons", L"Armour", L"Body / Face",
                                    L"Other textures",
                                    L"Models (beta)"};
typedef struct Preview {
  HBITMAP bitmap;
  HFONT font;
  WCHAR empty[128];
} Preview;
typedef struct ArchiveLocation {
  WCHAR path[PATH_CAP];
  WIN32_FILE_ATTRIBUTE_DATA attributes;
} ArchiveLocation;
typedef struct Resource {
  SudekiMpModCatalogEntry entry;
  SudekiMpModArchiveRecord record;
  int character, category, icon;
  HBITMAP thumbnail;
} Resource;
typedef struct ScanResult {
  HIMAGELIST images; /* Owned until transferred to the panel at completion. */
  ArchiveLocation *archives;
  size_t archive_count;
  Resource *resources;
  size_t count;
  WCHAR error[256];
} ScanResult;
typedef struct ThumbResult {
  size_t index;
  HBITMAP bitmap;
} ThumbResult;
typedef struct ScanJob {
  HANDLE cancel;
  WCHAR directory[PATH_CAP], cache[PATH_CAP];
  volatile LONG phase, progress, total, thumbs_done, thumbs_total;
  ScanResult *result;
  /* Progressive scan: the catalogue is handed to the panel (published) as
   * soon as it is built and classified; thumbnails follow through the queue,
   * the selected character/category first (priority_*, set by the panel). */
  const SudekiMpModGroups *grouping;
  volatile LONG published, priority_character, priority_category;
  CRITICAL_SECTION queue_lock;
  ThumbResult *queue;
  size_t queue_count, queue_capacity;
} ScanJob;
typedef struct DetailJob {
  HANDLE cancel;
  WCHAR archive[PATH_CAP], replacement[PATH_CAP];
  SudekiMpModArchiveRecord record;
  int model;
  int saved_replacement;
  SudekiMpModImage original, imported;
  int import_valid, import_preview_unavailable;
  WIN32_FILE_ATTRIBUTE_DATA replacement_identity;
  WCHAR error[256];
} DetailJob;
typedef struct ModFolder {
  WCHAR folder[MAX_PATH], name[256];
  int enabled;
} ModFolder;
typedef struct Panel {
  HWND window, selector, enabled, folder, rescan, new_mod, status_sink,
      character, category, search, grid, information, check, drop, browse,
      apply, revert, export_original, status, original_label, replacement_label,
      order_label, character_label, category_label;
  HWND original_preview, replacement_preview;
  Preview original, replacement;
  HINSTANCE instance;
  HBRUSH background, input_background;
  HFONT font, body_font, heading_font; /* body/heading: borrowed from the launcher */
  HIMAGELIST images;
  WCHAR game[PATH_CAP], ini[PATH_CAP], mods[PATH_CAP], cache[PATH_CAP],
      groups[PATH_CAP];
  ModFolder mod_list[MAX_MODS];
  size_t mod_count;
  int selected_mod, selected_resource, character_filter, category_filter;
  SudekiMpModManifest manifest;
  SudekiMpModGroups grouping;
  ScanResult *catalog;
  HANDLE scan_thread, detail_thread;
  ScanJob *scan;
  DetailJob *detail;
  SudekiMpModImage original_image;
  WCHAR candidate[PATH_CAP];
  WIN32_FILE_ATTRIBUTE_DATA candidate_identity;
  int candidate_pending, candidate_valid, rescanning, detail_pending,
      destroying, refreshing, loading_off;
} Panel;
typedef struct MappedFile {
  HANDLE file, mapping;
  const uint8_t *bytes;
  size_t size;
} MappedFile;

static int join(WCHAR *out, size_t cap, const WCHAR *dir, const WCHAR *leaf) {
  return SUCCEEDED(StringCchPrintfW(out, cap, L"%ls\\%ls", dir, leaf));
}
static void utf8_to_wide(const char *s, WCHAR *out, size_t cap) {
  if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, out, (int)cap))
    out[0] = 0;
}
static int wide_to_utf8(const WCHAR *s, char *out, size_t cap) {
  return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, -1, out,
                             (int)cap, NULL, NULL) != 0;
}
static void status(Panel *p, const WCHAR *s) {
  SetWindowTextW(p->status_sink ? p->status_sink : p->status, s);
}
static HFONT body(Panel *p) { return p->body_font ? p->body_font : p->font; }
static void error_box(Panel *p, const WCHAR *s) {
  MessageBoxW(p->window, s, L"SudekiMP Mods", MB_OK | MB_ICONERROR);
}
static int is_directory(const WCHAR *path) {
  DWORD a = GetFileAttributesW(path);
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
static int make_directory(const WCHAR *path) {
  int result;
  if (is_directory(path))
    return 1;
  result = SHCreateDirectoryExW(NULL, path, NULL);
  return result == ERROR_SUCCESS ||
         ((result == ERROR_ALREADY_EXISTS || result == ERROR_FILE_EXISTS) &&
          is_directory(path));
}
static int same_identity(const WIN32_FILE_ATTRIBUTE_DATA *a,
                         const WIN32_FILE_ATTRIBUTE_DATA *b) {
  return a->nFileSizeHigh == b->nFileSizeHigh &&
         a->nFileSizeLow == b->nFileSizeLow &&
         a->ftLastWriteTime.dwHighDateTime ==
             b->ftLastWriteTime.dwHighDateTime &&
         a->ftLastWriteTime.dwLowDateTime == b->ftLastWriteTime.dwLowDateTime;
}
static int unchanged_file(const WCHAR *path,
                          const WIN32_FILE_ATTRIBUTE_DATA *identity) {
  WIN32_FILE_ATTRIBUTE_DATA current;
  return GetFileAttributesExW(path, GetFileExInfoStandard, &current) &&
         same_identity(identity, &current);
}
static int read_range(const WCHAR *path, uint32_t offset, size_t length,
                      uint8_t **out, size_t *size) {
  HANDLE h;
  LARGE_INTEGER total, position;
  DWORD got = 0;
  uint8_t *bytes;
  *out = NULL;
  *size = 0;
  h = CreateFileW(path, GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  if (!GetFileSizeEx(h, &total) || total.QuadPart < 0) {
    CloseHandle(h);
    return 0;
  }
  if (length == (size_t)-1) {
    if ((uint64_t)total.QuadPart > IMAGE_LIMIT) {
      CloseHandle(h);
      return 0;
    }
    length = (size_t)total.QuadPart;
  }
  if (!length || length > IMAGE_LIMIT ||
      (uint64_t)offset + length > (uint64_t)total.QuadPart) {
    CloseHandle(h);
    return 0;
  }
  bytes = (uint8_t *)malloc(length);
  position.QuadPart = offset;
  if (!bytes || !SetFilePointerEx(h, position, NULL, FILE_BEGIN) ||
      !ReadFile(h, bytes, (DWORD)length, &got, NULL) || got != length) {
    free(bytes);
    CloseHandle(h);
    return 0;
  }
  CloseHandle(h);
  *out = bytes;
  *size = length;
  return 1;
}
static int atomic_write(const WCHAR *path, const uint8_t *bytes, size_t size) {
  WCHAR temporary[PATH_CAP];
  HANDLE h;
  DWORD wrote;
  int ok;
  if (size > MAXDWORD ||
      FAILED(StringCchPrintfW(temporary, PATH_CAP, L"%ls.launcher-%lu.tmp",
                              path, (unsigned long)GetCurrentProcessId())))
    return 0;
  h = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                  FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  ok = WriteFile(h, bytes, (DWORD)size, &wrote, NULL) && wrote == size &&
       FlushFileBuffers(h);
  CloseHandle(h);
  if (ok)
    ok = MoveFileExW(temporary, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  if (!ok)
    DeleteFileW(temporary);
  return ok;
}
static int map_file(const WCHAR *path, MappedFile *m) {
  LARGE_INTEGER size;
  memset(m, 0, sizeof(*m));
  m->file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (m->file == INVALID_HANDLE_VALUE) {
    m->file = NULL;
    return 0;
  }
  if (!GetFileSizeEx(m->file, &size) || size.QuadPart <= 0 ||
      (uint64_t)size.QuadPart > SIZE_MAX)
    goto fail;
  m->size = (size_t)size.QuadPart;
  m->mapping = CreateFileMappingW(m->file, NULL, PAGE_READONLY, 0, 0, NULL);
  if (!m->mapping)
    goto fail;
  m->bytes = (const uint8_t *)MapViewOfFile(m->mapping, FILE_MAP_READ, 0, 0, 0);
  if (m->bytes)
    return 1;
fail:
  if (m->mapping)
    CloseHandle(m->mapping);
  CloseHandle(m->file);
  memset(m, 0, sizeof(*m));
  return 0;
}
static void unmap_file(MappedFile *m) {
  if (m->bytes)
    UnmapViewOfFile(m->bytes);
  if (m->mapping)
    CloseHandle(m->mapping);
  if (m->file)
    CloseHandle(m->file);
  memset(m, 0, sizeof(*m));
}
static HBITMAP image_bitmap(const SudekiMpModImage *image, int side) {
  BITMAPINFO info;
  HBITMAP bitmap;
  uint8_t *dst;
  uint32_t x, y, width, height;
  size_t pixels;
  if (!image->rgba || !image->info.width || !image->info.height)
    return NULL;
  width = side ? (uint32_t)side : image->info.width;
  height = side ? (uint32_t)side : image->info.height;
  pixels = (size_t)width * height;
  if (pixels > IMAGE_LIMIT / 4)
    return NULL;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = (LONG)width;
  info.bmiHeader.biHeight = -(LONG)height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  bitmap =
      CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&dst, NULL, 0);
  if (!bitmap)
    return NULL;
  for (y = 0; y < height; ++y)
    for (x = 0; x < width; ++x) {
      uint32_t sx = x, sy = y;
      unsigned alpha, background = (((x / 8) + (y / 8)) & 1) ? 42 : 60;
      const uint8_t *src = NULL;
      uint8_t *d = dst + ((size_t)y * width + x) * 4;
      if (side) {
        uint32_t largest = image->info.width > image->info.height
                               ? image->info.width
                               : image->info.height;
        uint32_t w = image->info.width * width / largest,
                 h = image->info.height * height / largest;
        uint32_t left = (width - w) / 2, top = (height - h) / 2;
        if (w && h && x >= left && x < left + w && y >= top && y < top + h) {
          sx = (x - left) * image->info.width / w;
          sy = (y - top) * image->info.height / h;
          src = image->rgba + ((size_t)sy * image->info.width + sx) * 4;
        }
      } else
        src = image->rgba + ((size_t)sy * image->info.width + sx) * 4;
      if (src) {
        alpha = src[3];
        d[0] = (uint8_t)((src[2] * alpha + background * (255 - alpha)) / 255);
        d[1] = (uint8_t)((src[1] * alpha + background * (255 - alpha)) / 255);
        d[2] = (uint8_t)((src[0] * alpha + background * (255 - alpha)) / 255);
      } else
        d[0] = d[1] = d[2] = (uint8_t)background;
      d[3] = 255;
    }
  return bitmap;
}
static void preview_set(HWND window, Preview *preview, HBITMAP bitmap,
                        const WCHAR *empty) {
  if (preview->bitmap)
    DeleteObject(preview->bitmap);
  preview->bitmap = bitmap;
  StringCchCopyW(preview->empty, 128, empty ? empty : L"No preview");
  InvalidateRect(window, NULL, TRUE);
}
static LRESULT CALLBACK preview_proc(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam) {
  Preview *p = (Preview *)GetWindowLongPtrW(window, GWLP_USERDATA);
  if (message == WM_NCCREATE) {
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      (LONG_PTR)((CREATESTRUCTW *)lparam)->lpCreateParams);
    return TRUE;
  }
  if (message == WM_DROPFILES)
    return SendMessageW(GetParent(window), message, wparam, lparam);
  if (message == WM_PAINT) {
    PAINTSTRUCT paint;
    RECT r;
    HDC dc = BeginPaint(window, &paint);
    HBRUSH brush = CreateSolidBrush(INPUT_BG);
    GetClientRect(window, &r);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
    if (p && p->bitmap) {
      BITMAP b;
      HDC memory = CreateCompatibleDC(dc);
      HGDIOBJ previous = SelectObject(memory, p->bitmap);
      int width = r.right, height = r.bottom, draw_width, draw_height;
      GetObjectW(p->bitmap, sizeof(b), &b);
      draw_width = width;
      draw_height = (int)((int64_t)b.bmHeight * width / b.bmWidth);
      if (draw_height > height) {
        draw_height = height;
        draw_width = (int)((int64_t)b.bmWidth * height / b.bmHeight);
      }
      SetStretchBltMode(dc, HALFTONE);
      StretchBlt(dc, (width - draw_width) / 2, (height - draw_height) / 2,
                 draw_width, draw_height, memory, 0, 0, b.bmWidth, b.bmHeight,
                 SRCCOPY);
      SelectObject(memory, previous);
      DeleteDC(memory);
    } else if (p) {
      SetTextColor(dc, MUTED);
      SetBkMode(dc, TRANSPARENT);
      SelectObject(dc, p->font ? p->font : GetStockObject(DEFAULT_GUI_FONT));
      InflateRect(&r, -8, -8);
      DrawTextW(dc, p->empty, -1, &r, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }
    EndPaint(window, &paint);
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}
static LRESULT CALLBACK drop_zone_proc(HWND window, UINT message, WPARAM wparam,
                                       LPARAM lparam, UINT_PTR subclass_id,
                                       DWORD_PTR reference) {
  (void)reference;
  if (message == WM_DROPFILES)
    return SendMessageW(GetParent(window), message, wparam, lparam);
  if (message == WM_NCDESTROY)
    RemoveWindowSubclass(window, drop_zone_proc, subclass_id);
  return DefSubclassProc(window, message, wparam, lparam);
}

static uint64_t cache_hash(const ArchiveLocation *archive, uint32_t key) {
  uint64_t hash = UINT64_C(1469598103934665603);
  size_t i;
  const uint8_t *bytes;
  WCHAR lowered[PATH_CAP];
  StringCchCopyW(lowered, PATH_CAP, archive->path);
  CharLowerBuffW(lowered, (DWORD)wcslen(lowered));
  bytes = (const uint8_t *)lowered;
  for (i = 0; i < wcslen(lowered) * sizeof(WCHAR); ++i) {
    hash ^= bytes[i];
    hash *= UINT64_C(1099511628211);
  }
  /* Serialize the immutable size/mtime identity, independent of path spelling.
   */
  {
    uint32_t identity[] = {
        archive->attributes.nFileSizeHigh, archive->attributes.nFileSizeLow,
        archive->attributes.ftLastWriteTime.dwHighDateTime,
        archive->attributes.ftLastWriteTime.dwLowDateTime, key};
    bytes = (const uint8_t *)identity;
    for (i = 0; i < sizeof(identity); ++i) {
      hash ^= bytes[i];
      hash *= UINT64_C(1099511628211);
    }
  }
  return hash;
}
static HBITMAP thumbnail_cache(ScanJob *job, const ArchiveLocation *archive,
                               uint32_t key, const uint8_t *data, size_t size) {
  WCHAR path[PATH_CAP] = {0};
  uint8_t *cached = NULL;
  size_t cached_size = 0;
  HBITMAP bitmap = NULL;
  uint64_t hash = cache_hash(archive, key);
  BITMAPINFO info;
  void *pixels = NULL;
  SudekiMpModImage image = {0};
  char error[128];
  if (job->cache[0] &&
      SUCCEEDED(StringCchPrintfW(path, PATH_CAP, L"%ls\\%016llx.thumb",
                                 job->cache, (unsigned long long)hash)) &&
      read_range(path, 0, (size_t)-1, &cached, &cached_size) &&
      cached_size == 16 + THUMB_SIDE * THUMB_SIDE * 4 &&
      !memcmp(cached, "SDMPTH01", 8) && !memcmp(cached + 8, &hash, 8)) {
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = THUMB_SIDE;
    info.bmiHeader.biHeight = -THUMB_SIDE;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (bitmap)
      memcpy(pixels, cached + 16, THUMB_SIDE * THUMB_SIDE * 4);
  }
  free(cached);
  if (bitmap)
    return bitmap;
  if (!SudekiMpModImageDecode(data, size, &image, error, sizeof(error)))
    return NULL;
  bitmap = image_bitmap(&image, THUMB_SIDE);
  SudekiMpModImageFree(&image);
  if (bitmap && path[0]) {
    BITMAP description;
    uint8_t output[16 + THUMB_SIDE * THUMB_SIDE * 4];
    GetObjectW(bitmap, sizeof(description), &description);
    memcpy(output, "SDMPTH01", 8);
    memcpy(output + 8, &hash, 8);
    memcpy(output + 16, description.bmBits, THUMB_SIDE * THUMB_SIDE * 4);
    atomic_write(path, output, sizeof(output));
  }
  return bitmap;
}
static int cancelled(void *context) {
  return WaitForSingleObject(((ScanJob *)context)->cancel, 0) == WAIT_OBJECT_0;
}
static void free_catalog(ScanResult *r) {
  size_t i;
  if (!r)
    return;
  for (i = 0; i < r->count; ++i)
    if (r->resources[i].thumbnail)
      DeleteObject(r->resources[i].thumbnail);
  if (r->images)
    ImageList_Destroy(r->images);
  free(r->resources);
  free(r->archives);
  free(r);
}
static int archive_compare(const void *a, const void *b) {
  return _wcsicmp(((const ArchiveLocation *)a)->path,
                  ((const ArchiveLocation *)b)->path);
}
typedef struct DecodeBatch {
  ScanJob *job;
  const ArchiveLocation *archive;
  const SudekiMpModArchive *source;
  const Resource *resources; /* the whole catalogue (read-only here) */
  const size_t *indices;     /* the textures of this batch */
  HBITMAP *bitmaps;          /* one result per index */
  size_t count;
  volatile LONG next;
} DecodeBatch;
/* Pure decode + cache I/O: no window, image list or panel state is touched. */
static DWORD WINAPI decode_worker(void *context) {
  DecodeBatch *b = (DecodeBatch *)context;
  for (;;) {
    LONG index = InterlockedIncrement(&b->next) - 1;
    const Resource *resource;
    const uint8_t *bytes;
    size_t size;
    if (index < 0 || (size_t)index >= b->count || cancelled(b->job))
      return 0;
    resource = &b->resources[b->indices[index]];
    bytes = SudekiMpModArchiveResource(b->source, resource->entry.resource_index, &size);
    if (bytes)
      b->bitmaps[index] = thumbnail_cache(b->job, b->archive,
                                          resource->entry.archive_key, bytes, size);
    InterlockedIncrement(&b->job->thumbs_done);
  }
}
/* Runs one batch on up to DECODE_WORKERS threads and waits for all of them;
 * falls back to this thread when no worker can be created. */
static void decode_batch(DecodeBatch *b) {
  HANDLE threads[DECODE_WORKERS];
  unsigned started = 0, i, want = b->count < DECODE_WORKERS ? (unsigned)b->count : DECODE_WORKERS;
  for (i = 0; i < want; ++i) {
    threads[started] = CreateThread(NULL, 256 * 1024, decode_worker, b, 0, NULL);
    if (threads[started])
      ++started;
  }
  if (!started)
    decode_worker(b);
  if (started)
    WaitForMultipleObjects(started, threads, TRUE, INFINITE);
  for (i = 0; i < started; ++i)
    CloseHandle(threads[i]);
}
static DWORD WINAPI scan_worker(void *context) {
  ScanJob *job = (ScanJob *)context;
  ScanResult *r = (ScanResult *)calloc(1, sizeof(*r));
  MappedFile mapped = {0};
  SudekiMpModArchive *archives = NULL;
  SudekiMpModCatalog catalog = {0};
  SudekiMpModNames names = {0};
  WIN32_FIND_DATAW found;
  HANDLE find = INVALID_HANDLE_VALUE;
  WCHAR pattern[PATH_CAP], path[PATH_CAP];
  size_t i;
  char error[256] = {0};
  if (!r)
    goto done;
  r->archives = (ArchiveLocation *)calloc(MAX_ARCHIVES, sizeof(*r->archives));
  if (!r->archives)
    goto fail;
  if (!join(pattern, PATH_CAP, job->directory, L"*.baf"))
    goto fail;
  find = FindFirstFileW(pattern, &found);
  if (find != INVALID_HANDLE_VALUE)
    do {
      ArchiveLocation *a;
      if (cancelled(job))
        goto done;
      if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        continue;
      if (r->archive_count == MAX_ARCHIVES) {
        StringCchCopyW(r->error, 256, L"Too many archives to scan.");
        goto done;
      }
      a = &r->archives[r->archive_count++];
      join(a->path, PATH_CAP, job->directory, found.cFileName);
      if (!GetFileAttributesExW(a->path, GetFileExInfoStandard, &a->attributes))
        goto fail;
    } while (FindNextFileW(find, &found));
  if (find != INVALID_HANDLE_VALUE) {
    FindClose(find);
    find = INVALID_HANDLE_VALUE;
  }
  if (!r->archive_count) {
    StringCchCopyW(
        r->error, 256,
        L"No .baf archives found. Select your Sudeki game folder on Play.");
    goto done;
  }
  qsort(r->archives, r->archive_count, sizeof(*r->archives), archive_compare);
  archives = (SudekiMpModArchive *)calloc(r->archive_count, sizeof(*archives));
  if (!archives)
    goto fail;
  InterlockedExchange(&job->total, (LONG)r->archive_count);
  for (i = 0; i < r->archive_count; ++i) {
    if (cancelled(job))
      goto done;
    InterlockedExchange(&job->progress, (LONG)i + 1);
    if (!map_file(r->archives[i].path, &mapped)) {
      StringCchCopyW(r->error, 256,
                     L"Cannot map a game archive. Close other large "
                     L"applications and rescan.");
      goto done;
    }
    if (!SudekiMpModArchiveParse(mapped.bytes, mapped.size, &archives[i], error,
                                 sizeof(error))) {
      utf8_to_wide(error, r->error, 256);
      goto done;
    }
    archives[i].data = NULL;
    unmap_file(&mapped);
  }
  InterlockedExchange(&job->phase, 1);
  if (!SudekiMpModNamesBegin(archives, r->archive_count, &names, error,
                             sizeof(error)))
    goto core_fail;
  /* Only one game file occupies the 32-bit address space at a time. */
  join(path, PATH_CAP, job->directory, L"SUDEKI.exe");
  if (map_file(path, &mapped)) {
    if (!SudekiMpModNamesHarvestBlob(&names, mapped.bytes, mapped.size,
                                     cancelled, job, error, sizeof(error)))
      goto core_fail;
    unmap_file(&mapped);
  }
  for (i = 0; i < r->archive_count; ++i) {
    if (cancelled(job))
      goto done;
    InterlockedExchange(&job->progress, (LONG)i + 1);
    if (!map_file(r->archives[i].path, &mapped) ||
        !unchanged_file(r->archives[i].path, &r->archives[i].attributes))
      goto changed;
    if (!SudekiMpModNamesHarvestBlob(&names, mapped.bytes, mapped.size,
                                     cancelled, job, error, sizeof(error)))
      goto core_fail;
    unmap_file(&mapped);
  }
  SudekiMpModNamesFinish(&names);
  InterlockedExchange(&job->phase, 2);
  /* Catalogue first (no decoding): every archive's entries, classified. */
  for (i = 0; i < r->archive_count; ++i) {
    size_t j, old_count = r->count;
    Resource *grown;
    if (cancelled(job))
      goto done;
    InterlockedExchange(&job->progress, (LONG)i + 1);
    if (!map_file(r->archives[i].path, &mapped) ||
        !unchanged_file(r->archives[i].path, &r->archives[i].attributes))
      goto changed;
    archives[i].data = mapped.bytes;
    if (!SudekiMpModCatalogBuildNamed(&archives[i], &names, &catalog, cancelled,
                                      job, error, sizeof(error)))
      goto core_fail;
    if (catalog.count > SIZE_MAX / sizeof(*r->resources) - r->count)
      goto fail;
    grown = (Resource *)realloc(r->resources, (r->count + catalog.count) *
                                                  sizeof(*r->resources));
    if (catalog.count && !grown)
      goto fail;
    if (grown)
      r->resources = grown;
    if (catalog.count)
      memset(r->resources + r->count, 0, catalog.count * sizeof(*r->resources));
    r->count += catalog.count;
    for (j = 0; j < catalog.count; ++j) {
      Resource *resource = &r->resources[old_count + j];
      unsigned hero = 0, category = 0;
      resource->entry = catalog.entries[j];
      resource->entry.archive_index = i;
      resource->record = archives[i].records[resource->entry.resource_index];
      resource->icon = -1;
      SudekiMpModGroupsClassify(job->grouping, resource->entry.name,
                                resource->entry.kind == SUDEKIMP_MOD_RESOURCE_MODEL,
                                &hero, &category);
      resource->character = (int)hero;
      resource->category = (int)category;
      if (resource->entry.kind == SUDEKIMP_MOD_RESOURCE_TEXTURE)
        InterlockedIncrement(&job->thumbs_total);
    }
    SudekiMpModCatalogFree(&catalog);
    archives[i].data = NULL;
    unmap_file(&mapped);
  }
  /* Hand the catalogue to the panel now; from here on this thread only reads
   * entries/records and reports thumbnails through the queue. */
  InterlockedExchange(&job->phase, 3);
  InterlockedExchange(&job->published, 1);
  {
    size_t *pending = (size_t *)malloc(r->count * sizeof(size_t) + 1);
    size_t *indices = (size_t *)malloc(DECODE_BATCH * sizeof(size_t));
    HBITMAP *bitmaps = (HBITMAP *)calloc(DECODE_BATCH, sizeof(HBITMAP));
    if (!pending || !indices || !bitmaps) {
      free(pending); free(indices); free(bitmaps);
      goto fail;
    }
    for (i = 0; i < r->archive_count; ++i) {
      size_t j, pending_count = 0;
      for (j = 0; j < r->count; ++j)
        if (r->resources[j].entry.archive_index == i &&
            r->resources[j].entry.kind == SUDEKIMP_MOD_RESOURCE_TEXTURE)
          pending[pending_count++] = j;
      if (!pending_count)
        continue;
      if (!map_file(r->archives[i].path, &mapped) ||
          !unchanged_file(r->archives[i].path, &r->archives[i].attributes)) {
        free(pending); free(indices); free(bitmaps);
        goto changed;
      }
      archives[i].data = mapped.bytes;
      while (pending_count && !cancelled(job)) {
        /* Next batch: the panel's current selection first, then the rest. */
        LONG want_character = InterlockedCompareExchange(&job->priority_character, 0, 0);
        LONG want_category = InterlockedCompareExchange(&job->priority_category, 0, 0);
        size_t taken = 0, k, kept = 0;
        DecodeBatch batch;
        for (int pass = 0; pass < 2 && taken < DECODE_BATCH; ++pass)
          for (k = 0; k < pending_count && taken < DECODE_BATCH; ++k) {
            const Resource *c = &r->resources[pending[k]];
            int match = c->character == want_character && c->category == want_category;
            if (pending[k] != (size_t)-1 && (pass ? !match : match)) {
              indices[taken++] = pending[k];
              pending[k] = (size_t)-1;
            }
          }
        for (k = 0; k < pending_count; ++k)
          if (pending[k] != (size_t)-1)
            pending[kept++] = pending[k];
        pending_count = kept;
        memset(&batch, 0, sizeof(batch));
        memset(bitmaps, 0, DECODE_BATCH * sizeof(HBITMAP));
        batch.job = job;
        batch.archive = &r->archives[i];
        batch.source = &archives[i];
        batch.resources = r->resources;
        batch.indices = indices;
        batch.bitmaps = bitmaps;
        batch.count = taken;
        decode_batch(&batch);
        EnterCriticalSection(&job->queue_lock);
        for (k = 0; k < taken; ++k) {
          if (!bitmaps[k])
            continue;
          if (job->queue_count == job->queue_capacity) {
            size_t capacity = job->queue_capacity ? job->queue_capacity * 2 : 256;
            ThumbResult *grown = (ThumbResult *)realloc(job->queue, capacity * sizeof(*grown));
            if (!grown) {
              DeleteObject(bitmaps[k]);
              continue;
            }
            job->queue = grown;
            job->queue_capacity = capacity;
          }
          job->queue[job->queue_count].index = indices[k];
          job->queue[job->queue_count].bitmap = bitmaps[k];
          ++job->queue_count;
        }
        LeaveCriticalSection(&job->queue_lock);
      }
      archives[i].data = NULL;
      unmap_file(&mapped);
      if (cancelled(job))
        break;
    }
    free(pending); free(indices); free(bitmaps);
  }
  goto done;
core_fail:
  if (!cancelled(job))
    utf8_to_wide(error, r->error, 256);
  goto done;
changed:
  StringCchCopyW(r->error, 256,
                 L"A game archive changed during the scan. Rescan game files.");
  goto done;
fail:
  if (r)
    StringCchCopyW(r->error, 256,
                   L"Unable to allocate or read the game catalog.");
done:
  if (find != INVALID_HANDLE_VALUE)
    FindClose(find);
  SudekiMpModCatalogFree(&catalog);
  for (i = 0; r && i < r->archive_count; ++i)
    if (archives)
      SudekiMpModArchiveFree(&archives[i]);
  SudekiMpModNamesFree(&names);
  unmap_file(&mapped);
  free(archives);
  job->result = r;
  return 0;
}

/* WIC is optional under Wine. Header validation still rejects plainly corrupt
 * input when no codec is installed; supported core formats always fully decode.
 */
static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}
static uint32_t le32(const uint8_t *p) {
  return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static int header_dimensions(const uint8_t *data, size_t size,
                             const WCHAR *extension, uint32_t *width,
                             uint32_t *height) {
  if (!_wcsicmp(extension, L".png")) {
    size_t pos = 8;
    int idat = 0, iend = 0;
    if (size < 33 || memcmp(data, "\x89PNG\r\n\x1a\n", 8) ||
        be32(data + 8) != 13 || memcmp(data + 12, "IHDR", 4))
      return 0;
    *width = be32(data + 16);
    *height = be32(data + 20);
    while (pos + 12 <= size) {
      uint32_t length = be32(data + pos);
      if (length > size - pos - 12)
        return 0;
      if ((SudekiMpTexModCrc32(data + pos + 4, (size_t)length + 4) ^
           UINT32_MAX) != be32(data + pos + 8 + length))
        return 0;
      if (!memcmp(data + pos + 4, "IDAT", 4))
        idat = 1;
      if (!memcmp(data + pos + 4, "IEND", 4)) {
        iend = length == 0 && pos + 12 == size;
        break;
      }
      pos += (size_t)length + 12;
    }
    return idat && iend && *width && *height;
  }
  if (!_wcsicmp(extension, L".bmp")) {
    uint32_t header, offset, bits, row;
    int32_t w, h;
    if (size < 54 || data[0] != 'B' || data[1] != 'M' || le32(data + 2) > size)
      return 0;
    header = le32(data + 14);
    offset = le32(data + 10);
    w = (int32_t)le32(data + 18);
    h = (int32_t)le32(data + 22);
    bits = data[28] | ((uint32_t)data[29] << 8);
    if (header < 40 || header > size - 14 || offset >= size || w <= 0 || !h ||
        h == INT32_MIN || !bits)
      return 0;
    *width = (uint32_t)w;
    *height = (uint32_t)(h < 0 ? -h : h);
    if (le32(data + 30) == 0) {
      uint64_t pitch = (((uint64_t)*width * bits + 31) / 32) * 4;
      row = (uint32_t)pitch;
      if (pitch != row || (uint64_t)row * *height > size - offset)
        return 0;
    }
    return 1;
  }
  if (!_wcsicmp(extension, L".jpg") || !_wcsicmp(extension, L".jpeg")) {
    size_t pos = 2;
    if (size < 4 || data[0] != 255 || data[1] != 216)
      return 0;
    while (pos + 4 <= size) {
      unsigned marker, length;
      if (data[pos++] != 255)
        return 0;
      while (pos < size && data[pos] == 255)
        ++pos;
      if (pos == size)
        return 0;
      marker = data[pos++];
      if (marker == 217)
        return *width && *height;
      if (marker == 218) {
        size_t end;
        for (end = pos; end + 1 < size; ++end)
          if (data[end] == 255 && data[end + 1] == 217)
            return *width && *height;
        return 0;
      }
      if (marker == 1 || (marker >= 208 && marker <= 215))
        continue;
      if (size - pos < 2)
        return 0;
      length = ((unsigned)data[pos] << 8) | data[pos + 1];
      if (length < 2 || length > size - pos)
        return 0;
      if ((marker >= 192 && marker <= 195) ||
          (marker >= 197 && marker <= 199) ||
          (marker >= 201 && marker <= 203) ||
          (marker >= 205 && marker <= 207)) {
        if (length < 8)
          return 0;
        *height = ((uint32_t)data[pos + 3] << 8) | data[pos + 4];
        *width = ((uint32_t)data[pos + 5] << 8) | data[pos + 6];
      }
      pos += length;
    }
    return 0;
  }
  return 0;
}
static HRESULT wic_decode(const uint8_t *data, size_t size,
                          SudekiMpModImage *image) {
  IWICImagingFactory *factory = NULL;
  IWICStream *stream = NULL;
  IWICBitmapDecoder *decoder = NULL;
  IWICBitmapFrameDecode *frame = NULL;
  IWICFormatConverter *converter = NULL;
  HRESULT hr;
  UINT width = 0, height = 0;
  size_t bytes;
  hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                        &IID_IWICImagingFactory, (void **)&factory);
  if (FAILED(hr))
    return WINCODEC_ERR_COMPONENTNOTFOUND;
  hr = IWICImagingFactory_CreateStream(factory, &stream);
  if (FAILED(hr))
    goto done;
  hr = IWICStream_InitializeFromMemory(stream, (BYTE *)data, (DWORD)size);
  if (FAILED(hr))
    goto done;
  hr = IWICImagingFactory_CreateDecoderFromStream(
      factory, (IStream *)stream, NULL, WICDecodeMetadataCacheOnDemand,
      &decoder);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapFrameDecode_GetSize(frame, &width, &height);
  if (FAILED(hr))
    goto done;
  bytes = (size_t)width * height * 4;
  if (!width || !height || width > IMAGE_LIMIT / 4 / height ||
      bytes > IMAGE_LIMIT) {
    hr = E_OUTOFMEMORY;
    goto done;
  }
  hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
  if (FAILED(hr))
    goto done;
  hr = IWICFormatConverter_Initialize(
      converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppRGBA,
      WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom);
  if (FAILED(hr))
    goto done;
  image->rgba = (uint8_t *)malloc(bytes);
  if (!image->rgba) {
    hr = E_OUTOFMEMORY;
    goto done;
  }
  hr = IWICFormatConverter_CopyPixels(converter, NULL, width * 4, (UINT)bytes,
                                      image->rgba);
  if (FAILED(hr)) {
    free(image->rgba);
    image->rgba = NULL;
    goto done;
  }
  image->info.width = width;
  image->info.height = height;
  image->info.d3d_format = 21;
  image->info.mip_levels = 1;
done:
  if (converter)
    IWICFormatConverter_Release(converter);
  if (frame)
    IWICBitmapFrameDecode_Release(frame);
  if (decoder)
    IWICBitmapDecoder_Release(decoder);
  if (stream)
    IWICStream_Release(stream);
  IWICImagingFactory_Release(factory);
  return hr;
}
static DWORD WINAPI detail_worker(void *context) {
  DetailJob *job = (DetailJob *)context;
  uint8_t *bytes = NULL;
  size_t size = 0;
  char error[256];
  const WCHAR *extension;
  HRESULT initialized, hr;
  initialized = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (!job->model && WaitForSingleObject(job->cancel, 0) != WAIT_OBJECT_0 &&
      read_range(job->archive, job->record.offset, job->record.size, &bytes,
                 &size)) {
    SudekiMpModImageDecode(bytes, size, &job->original, error, sizeof(error));
    free(bytes);
    bytes = NULL;
  }
  if (!job->replacement[0] ||
      WaitForSingleObject(job->cancel, 0) == WAIT_OBJECT_0)
    goto done;
  if (!GetFileAttributesExW(job->replacement, GetFileExInfoStandard,
                            &job->replacement_identity) ||
      !read_range(job->replacement, 0, (size_t)-1, &bytes, &size)) {
    StringCchCopyW(job->error, 256,
                   L"File is unreadable, empty, or larger than 128 MiB.");
    goto done;
  }
  extension = wcsrchr(job->replacement, L'.');
  if (!extension)
    extension = L"";
  if (job->model) {
    StringCchCopyW(
        job->error, 256,
        L"Models are experimental. Apply is awaiting the live model test.");
    goto done;
  }
  if (!_wcsicmp(extension, L".dds") || !_wcsicmp(extension, L".tga")) {
    if (!SudekiMpModImageDecode(bytes, size, &job->imported, error,
                                sizeof(error))) {
      utf8_to_wide(error, job->error, 256);
      goto done;
    }
  } else if (job->saved_replacement && !_wcsicmp(extension, L".sqx")) {
    if (!SudekiMpModImageDecode(bytes, size, &job->imported, error,
                                sizeof(error))) {
      utf8_to_wide(error, job->error, 256);
      goto done;
    }
  } else {
    if (!header_dimensions(bytes, size, extension, &job->imported.info.width,
                           &job->imported.info.height)) {
      StringCchCopyW(
          job->error, 256,
          L"This is not a readable PNG, DDS, TGA, BMP or JPG image.");
      goto done;
    }
    hr = wic_decode(bytes, size, &job->imported);
    if (hr == WINCODEC_ERR_COMPONENTNOTFOUND || hr == REGDB_E_CLASSNOTREG ||
        hr == E_NOINTERFACE)
      job->import_preview_unavailable = 1;
    else if (FAILED(hr)) {
      StringCchCopyW(
          job->error, 256,
          L"The image codec could not decode this file. Choose a valid image.");
      goto done;
    }
  }
  if (!unchanged_file(job->replacement, &job->replacement_identity)) {
    StringCchCopyW(job->error, 256,
                   L"The replacement changed while reading. Browse or drop it "
                   L"again to review it.");
    goto done;
  }
  job->import_valid = 1;
done:
  free(bytes);
  if (SUCCEEDED(initialized))
    CoUninitialize();
  return 0;
}

static int selected_mod_path(Panel *p, WCHAR *root, WCHAR *manifest) {
  if (p->selected_mod < 0 || (size_t)p->selected_mod >= p->mod_count)
    return 0;
  if (!join(root, PATH_CAP, p->mods, p->mod_list[p->selected_mod].folder))
    return 0;
  return !manifest || join(manifest, PATH_CAP, root, L"mod.ini");
}
static int save_manifest_at(const WCHAR *path,
                            const SudekiMpModManifest *manifest) {
  uint8_t *bytes = NULL;
  size_t size = 0;
  int ok = SudekiMpModManifestEncode(manifest, &bytes, &size);
  if (ok)
    ok = atomic_write(path, bytes, size);
  free(bytes);
  if (ok)
    WritePrivateProfileStringW(NULL, NULL, NULL, path);
  return ok;
}
static int load_manifest_at(const WCHAR *path, SudekiMpModManifest *manifest) {
  uint8_t *bytes = NULL;
  size_t size = 0;
  int ok = read_range(path, 0, (size_t)-1, &bytes, &size);
  if (ok)
    ok = SudekiMpModManifestLoad(bytes, size, manifest);
  free(bytes);
  if (ok)
    ok = SudekiMpModManifestValidate(manifest, NULL, NULL, NULL, NULL);
  if (!ok)
    SudekiMpModManifestFree(manifest);
  return ok;
}
static int create_mod(Panel *p, const WCHAR *name) {
  WCHAR root[PATH_CAP], manifest_path[PATH_CAP];
  char utf8[1024];
  SudekiMpModManifest manifest = {0};
  int ok;
  if (!join(root, PATH_CAP, p->mods, name) ||
      !join(manifest_path, PATH_CAP, root, L"mod.ini") ||
      !wide_to_utf8(name, utf8, sizeof(utf8)))
    return 0;
  if (GetFileAttributesW(root) != INVALID_FILE_ATTRIBUTES)
    return 0;
  if (!make_directory(p->mods) || !CreateDirectoryW(root, NULL))
    return 0;
  ok = SudekiMpModManifestCreate(utf8, &manifest) &&
       save_manifest_at(manifest_path, &manifest);
  SudekiMpModManifestFree(&manifest);
  if (!ok)
    RemoveDirectoryW(root);
  return ok;
}
static int mod_compare(const void *a, const void *b) {
  return _wcsicmp(((const ModFolder *)a)->folder,
                  ((const ModFolder *)b)->folder);
}
static int resource_replaced(Panel *p, const Resource *r, char *relative,
                             size_t cap) {
  char key[16];
  if (!p->manifest.text)
    return 0;
  if (r->entry.kind == SUDEKIMP_MOD_RESOURCE_TEXTURE &&
      SudekiMpModManifestGetTexture(&p->manifest, r->entry.texture_key,
                                    relative, cap))
    return 1;
  if (r->entry.name[0])
    return SudekiMpModManifestGetFile(&p->manifest, r->entry.name, relative,
                                      cap);
  snprintf(key, sizeof(key), "0x%08lX", (unsigned long)r->entry.archive_key);
  return SudekiMpModManifestGetFile(&p->manifest, key, relative, cap);
}
static void set_action_state(Panel *p) {
  int selected = p->catalog && p->selected_resource >= 0 &&
                 (size_t)p->selected_resource < p->catalog->count;
  int model =
      selected && p->catalog->resources[p->selected_resource].entry.kind ==
                      SUDEKIMP_MOD_RESOURCE_MODEL;
  char path[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
  EnableWindow(p->browse, selected && !model);
  EnableWindow(p->apply, selected && !model && p->manifest.text &&
                             p->candidate_pending && p->candidate_valid &&
                             !p->detail_thread);
  EnableWindow(
      p->revert,
      selected && p->manifest.text &&
          resource_replaced(p, &p->catalog->resources[p->selected_resource],
                            path, sizeof(path)));
  EnableWindow(p->export_original, selected && !model &&
                                       p->original_image.rgba &&
                                       !p->detail_thread);
  EnableWindow(p->enabled, p->manifest.text != NULL);
  EnableWindow(p->folder, p->selected_mod >= 0);
}
static void cancel_detail(Panel *p) {
  if (p->detail)
    SetEvent(p->detail->cancel);
}
static void detail_start(Panel *p) {
  Resource *r;
  if (p->detail_thread || !p->detail_pending)
    return;
  p->detail_pending = 0;
  if (!p->catalog || p->selected_resource < 0 ||
      (size_t)p->selected_resource >= p->catalog->count)
    return;
  r = &p->catalog->resources[p->selected_resource];
  p->detail = (DetailJob *)calloc(1, sizeof(*p->detail));
  if (!p->detail)
    return;
  p->detail->cancel = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (!p->detail->cancel) {
    free(p->detail);
    p->detail = NULL;
    SetWindowTextW(p->check, L"Unable to create the image preview worker.");
    set_action_state(p);
    return;
  }
  StringCchCopyW(p->detail->archive, PATH_CAP,
                 p->catalog->archives[r->entry.archive_index].path);
  StringCchCopyW(p->detail->replacement, PATH_CAP, p->candidate);
  p->detail->record = r->record;
  p->detail->model = r->entry.kind == SUDEKIMP_MOD_RESOURCE_MODEL;
  p->detail->saved_replacement = !p->candidate_pending;
  p->detail_thread = CreateThread(NULL, 0, detail_worker, p->detail, 0, NULL);
  if (!p->detail_thread) {
    CloseHandle(p->detail->cancel);
    free(p->detail);
    p->detail = NULL;
    SetWindowTextW(p->check, L"Unable to start image preview.");
  }
  set_action_state(p);
}
static void queue_candidate(Panel *p, const WCHAR *path) {
  WCHAR copy[PATH_CAP];
  if (!p->catalog || p->selected_resource < 0)
    return;
  if (p->catalog->resources[p->selected_resource].entry.kind ==
      SUDEKIMP_MOD_RESOURCE_MODEL) {
    status(p, L"Models are experimental; Apply awaits the live model test.");
    return;
  }
  if (FAILED(StringCchCopyW(copy, PATH_CAP, path))) {
    error_box(p, L"The selected path is too long.");
    return;
  }
  StringCchCopyW(p->candidate, PATH_CAP, copy);
  p->candidate_pending = 1;
  p->candidate_valid = 0;
  p->detail_pending = 1;
  cancel_detail(p);
  preview_set(p->replacement_preview, &p->replacement, NULL,
              L"Reading replacement…");
  SetWindowTextW(p->check, L"Checking replacement image…");
  detail_start(p);
  set_action_state(p);
}
static void select_resource(Panel *p, int index) {
  WCHAR text[768], name[128], archive[MAX_PATH], format[64];
  char relative[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
  WCHAR relative_wide[SUDEKIMP_TEXTURE_MOD_PATH_MAX], root[PATH_CAP];
  Resource *r;
  const WCHAR *leaf;
  cancel_detail(p);
  p->selected_resource = index;
  p->candidate[0] = 0;
  p->candidate_pending = 0;
  p->candidate_valid = 0;
  SudekiMpModImageFree(&p->original_image);
  preview_set(p->original_preview, &p->original, NULL, L"Select a resource");
  preview_set(p->replacement_preview, &p->replacement, NULL, L"No replacement");
  SetWindowTextW(p->information, L"");
  SetWindowTextW(p->check, L"");
  if (!p->catalog || index < 0 || (size_t)index >= p->catalog->count) {
    p->detail_pending = 0;
    set_action_state(p);
    return;
  }
  r = &p->catalog->resources[index];
  utf8_to_wide(r->entry.name, name, 128);
  if (!name[0])
    StringCchPrintfW(name, 128, L"Unnamed 0x%08lX",
                     (unsigned long)r->entry.archive_key);
  leaf = wcsrchr(p->catalog->archives[r->entry.archive_index].path, L'\\');
  StringCchCopyW(archive, MAX_PATH,
                 leaf ? leaf + 1
                      : p->catalog->archives[r->entry.archive_index].path);
  if (r->entry.kind == SUDEKIMP_MOD_RESOURCE_TEXTURE) {
    utf8_to_wide(SudekiMpModImageFormatName(r->entry.d3d_format), format, 64);
    StringCchPrintfW(
        text, 768, L"%ls\r\n%ls\r\nTexture key 0x%08lX\r\n%lu × %lu  ·  %ls",
        name, archive, (unsigned long)r->entry.texture_key,
        (unsigned long)r->entry.width, (unsigned long)r->entry.height, format);
    SetWindowTextW(p->drop,
                   L"Drag a PNG / DDS / TGA / BMP / JPG here, or Browse…");
    preview_set(p->original_preview, &p->original, NULL, L"Reading original…");
  } else {
    StringCchPrintfW(
        text, 768,
        L"%ls\r\n%ls\r\nArchive key 0x%08lX\r\nExperimental game model (.HOM)",
        name, archive, (unsigned long)r->entry.archive_key);
    SetWindowTextW(p->drop, L"Game .HOM models only; no OBJ / FBX converter. "
                            L"Apply awaits the live model test.");
    preview_set(p->original_preview, &p->original, NULL,
                L"Model preview unavailable");
  }
  SetWindowTextW(p->information, text);
  if (resource_replaced(p, r, relative, sizeof(relative)) &&
      selected_mod_path(p, root, NULL)) {
    utf8_to_wide(relative, relative_wide, SUDEKIMP_TEXTURE_MOD_PATH_MAX);
    join(p->candidate, PATH_CAP, root, relative_wide);
    SetWindowTextW(p->check, L"Reading this mod's saved replacement…");
  }
  p->detail_pending = 1;
  detail_start(p);
  set_action_state(p);
}
static int contains_case_insensitive(const WCHAR *text, const WCHAR *query) {
  size_t n = wcslen(query);
  if (!n)
    return 1;
  while (*text) {
    if (!_wcsnicmp(text, query, n))
      return 1;
    ++text;
  }
  return 0;
}
static void tile_caption(HDC dc, const WCHAR *name, WCHAR *caption,
                         size_t capacity) {
  SIZE measured;
  size_t length;
  StringCchCopyW(caption, capacity, name);
  length = wcslen(caption);
  if (!GetTextExtentPoint32W(dc, caption, (int)length, &measured) ||
      measured.cx <= 112)
    return;
  /* One native caption line leaves the lower two lines for size and badge. */
  while (length > 1) {
    --length;
    caption[length - 1] = L'…';
    caption[length] = 0;
    if (GetTextExtentPoint32W(dc, caption, (int)length, &measured) &&
        measured.cx <= 112)
      return;
  }
}
static LRESULT grid_custom_draw(Panel *p, NMLVCUSTOMDRAW *draw) {
  if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
    return CDRF_NOTIFYITEMDRAW;
  if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
    return CDRF_NOTIFYPOSTPAINT;
  if (draw->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT && p->catalog) {
    int index = (int)draw->nmcd.dwItemSpec;
    LVITEMW item;
    RECT bounds, label, metadata, line;
    TEXTMETRICW metrics;
    WCHAR size[96];
    char replacement[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
    Resource *r;
    HGDIOBJ previous;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (!ListView_GetItem(p->grid, &item) || item.lParam < 0 ||
        (size_t)item.lParam >= p->catalog->count ||
        !ListView_GetItemRect(p->grid, index, &bounds, LVIR_BOUNDS) ||
        !ListView_GetItemRect(p->grid, index, &label, LVIR_LABEL))
      return CDRF_DODEFAULT;
    r = &p->catalog->resources[item.lParam];
    previous = SelectObject(draw->nmcd.hdc, body(p));
    GetTextMetricsW(draw->nmcd.hdc, &metrics);
    metadata = bounds;
    metadata.left += 2;
    metadata.right -= 2;
    metadata.top = label.top + metrics.tmHeight + 1;
    metadata.bottom = metadata.top + metrics.tmHeight * 2;
    FillRect(draw->nmcd.hdc, &metadata, p->input_background);
    SetBkMode(draw->nmcd.hdc, TRANSPARENT);
    SetTextColor(draw->nmcd.hdc, MUTED);
    if (r->entry.kind == SUDEKIMP_MOD_RESOURCE_TEXTURE)
      StringCchPrintfW(size, 96, L"%lu × %lu", (unsigned long)r->entry.width,
                       (unsigned long)r->entry.height);
    else
      StringCchCopyW(size, 96, L"HOM · Experimental");
    line = metadata;
    line.bottom = line.top + metrics.tmHeight;
    DrawTextW(draw->nmcd.hdc, size, -1, &line,
              DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (resource_replaced(p, r, replacement, sizeof(replacement))) {
      line.top = line.bottom;
      line.bottom += metrics.tmHeight;
      SetTextColor(draw->nmcd.hdc, RGB(54, 193, 218));
      DrawTextW(draw->nmcd.hdc, L"Replaced", -1, &line,
                DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    SelectObject(draw->nmcd.hdc, previous);
  }
  return CDRF_DODEFAULT;
}
static void refresh_grid(Panel *p) {
  size_t i;
  WCHAR search[256];
  int selected = p->selected_resource, new_selection = -1;
  HDC dc = GetDC(p->grid);
  HGDIOBJ previous = SelectObject(dc, body(p));
  GetWindowTextW(p->search, search, 256);
  p->refreshing = 1;
  SendMessageW(p->grid, WM_SETREDRAW, FALSE, 0);
  ListView_DeleteAllItems(p->grid);
  if (p->catalog)
    for (i = 0; i < p->catalog->count; ++i) {
      Resource *r = &p->catalog->resources[i];
      WCHAR name[128], label[256];
      LVITEMW item;
      if (r->character != p->character_filter ||
          r->category != p->category_filter)
        continue;
      utf8_to_wide(r->entry.name, name, 128);
      if (!name[0])
        StringCchPrintfW(name, 128, L"Unnamed 0x%08lX",
                         (unsigned long)r->entry.archive_key);
      if (search[0]) {
        /* Names, or the TexMod texture key / archive key in hex ("4FADDA1E"),
         * so converted .tpf entries that only carry a key can be found. */
        WCHAR keys[64];
        StringCchPrintfW(keys, 64, L"0x%08lX 0x%08lX",
                         (unsigned long)r->entry.texture_key,
                         (unsigned long)r->entry.archive_key);
        if (!contains_case_insensitive(name, search) &&
            !contains_case_insensitive(keys, search))
          continue;
      }
      tile_caption(dc, name, label, 256);
      memset(&item, 0, sizeof(item));
      item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
      item.pszText = label;
      item.iImage = r->icon;
      item.lParam = (LPARAM)i;
      item.iItem = ListView_GetItemCount(p->grid);
      if ((int)i == selected)
        new_selection = item.iItem;
      ListView_InsertItem(p->grid, &item);
    }
  SelectObject(dc, previous);
  ReleaseDC(p->grid, dc);
  if (new_selection >= 0)
    ListView_SetItemState(p->grid, new_selection, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
  p->refreshing = 0;
  SendMessageW(p->grid, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(p->grid, NULL, TRUE);
  if (new_selection < 0)
    select_resource(p, -1);
  else
    set_action_state(p);
}
static void load_selected_mod(Panel *p) {
  WCHAR root[PATH_CAP], manifest[PATH_CAP], order[256];
  char enabled[32];
  SudekiMpModManifestFree(&p->manifest);
  if (selected_mod_path(p, root, manifest) &&
      load_manifest_at(manifest, &p->manifest)) {
    int value = !SudekiMpModManifestGetValue(&p->manifest, "Mod", "Enabled",
                                             enabled, sizeof(enabled)) ||
                (_stricmp(enabled, "false") && strcmp(enabled, "0") &&
                 _stricmp(enabled, "no") && _stricmp(enabled, "off"));
    p->mod_list[p->selected_mod].enabled = value;
    SendMessageW(p->enabled, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED,
                 0);
    StringCchPrintfW(
        order, 256,
        L"Load order %d of %u: mods load by folder name, and a later mod wins "
        L"when two change the same thing.",
        p->selected_mod + 1, (unsigned)p->mod_count);
  } else {
    StringCchCopyW(order, 256, L"Choose a mod to save replacements.");
    if (p->selected_mod >= 0)
      status(p, L"This mod.ini is unreadable or has an unsupported format.");
  }
  if (p->loading_off)
    StringCchCopyW(order, 256,
                   L"Mod loading is switched off in SudekiMP.ini ([Mods] Enable=false): "
                   L"edits are saved, but the game ignores every mod.");
  SetWindowTextW(p->order_label, order);
  refresh_grid(p);
  if (p->selected_resource >= 0)
    select_resource(p, p->selected_resource);
  set_action_state(p);
}
static void refresh_mods(Panel *p, const WCHAR *preferred) {
  WCHAR pattern[PATH_CAP], root[PATH_CAP], manifest_path[PATH_CAP], name[256],
      previous[MAX_PATH] = {0};
  WIN32_FIND_DATAW found;
  HANDLE find;
  size_t i;
  int selected = -1;
  if (!preferred && p->selected_mod >= 0 &&
      (size_t)p->selected_mod < p->mod_count)
    StringCchCopyW(previous, MAX_PATH, p->mod_list[p->selected_mod].folder);
  p->mod_count = 0;
  p->selected_mod = -1;
  SendMessageW(p->selector, CB_RESETCONTENT, 0, 0);
  if (!join(pattern, PATH_CAP, p->mods, L"*"))
    return;
  find = FindFirstFileW(pattern, &found);
  if (find != INVALID_HANDLE_VALUE) {
    do {
      SudekiMpModManifest m = {0};
      char value[1024];
      ModFolder *f;
      if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
          found.cFileName[0] == L'.' || found.cFileName[0] == L'_' ||
          p->mod_count == MAX_MODS)
        continue;
      if (!join(root, PATH_CAP, p->mods, found.cFileName) ||
          !join(manifest_path, PATH_CAP, root, L"mod.ini") ||
          GetFileAttributesW(manifest_path) == INVALID_FILE_ATTRIBUTES)
        continue;
      f = &p->mod_list[p->mod_count++];
      StringCchCopyW(f->folder, MAX_PATH, found.cFileName);
      StringCchCopyW(f->name, 256, found.cFileName);
      f->enabled = 1;
      if (load_manifest_at(manifest_path, &m)) {
        if (SudekiMpModManifestGetValue(&m, "Mod", "Name", value,
                                        sizeof(value)))
          utf8_to_wide(value, f->name, 256);
        if (SudekiMpModManifestGetValue(&m, "Mod", "Enabled", value,
                                        sizeof(value)))
          f->enabled = _stricmp(value, "false") && strcmp(value, "0") &&
                       _stricmp(value, "no") && _stricmp(value, "off");
      }
      SudekiMpModManifestFree(&m);
    } while (FindNextFileW(find, &found));
    FindClose(find);
  }
  if (!p->mod_count && p->game[0] && is_directory(p->game) &&
      create_mod(p, L"My Mods")) {
    refresh_mods(p, L"My Mods");
    return;
  }
  qsort(p->mod_list, p->mod_count, sizeof(p->mod_list[0]), mod_compare);
  /* A mod copied into another mod's folder is never loaded: say where it is. */
  for (i = 0; i < p->mod_count; ++i) {
    WCHAR inner[PATH_CAP], nested[PATH_CAP], text[512];
    WIN32_FIND_DATAW sub;
    HANDLE look;
    if (!join(root, PATH_CAP, p->mods, p->mod_list[i].folder) ||
        !join(inner, PATH_CAP, root, L"*"))
      continue;
    look = FindFirstFileW(inner, &sub);
    if (look == INVALID_HANDLE_VALUE)
      continue;
    do {
      if (!(sub.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || sub.cFileName[0] == L'.' ||
          !join(nested, PATH_CAP, root, sub.cFileName) ||
          !join(manifest_path, PATH_CAP, nested, L"mod.ini") ||
          GetFileAttributesW(manifest_path) == INVALID_FILE_ATTRIBUTES)
        continue;
      StringCchPrintfW(text, 512,
                       L"\"%ls\" is inside \"%ls\" and will not load: move it directly into the mods folder.",
                       sub.cFileName, p->mod_list[i].folder);
      status(p, text);
    } while (FindNextFileW(look, &sub));
    FindClose(look);
  }
  for (i = 0; i < p->mod_count; ++i) {
    StringCchPrintfW(name, 256, L"%u. %ls%ls", (unsigned)i + 1,
                     p->mod_list[i].name,
                     p->mod_list[i].enabled ? L"" : L" (disabled)");
    SendMessageW(p->selector, CB_ADDSTRING, 0, (LPARAM)name);
    if (!_wcsicmp(p->mod_list[i].folder, preferred ? preferred : previous))
      selected = (int)i;
  }
  SendMessageW(p->selector, CB_ADDSTRING, 0, (LPARAM)L"New mod…");
  if (selected < 0 && p->mod_count)
    selected = 0;
  p->selected_mod = selected;
  SendMessageW(p->selector, CB_SETCURSEL,
               (WPARAM)(selected >= 0 ? selected : (int)p->mod_count), 0);
  load_selected_mod(p);
}

static void scan_start(Panel *p) {
  if (p->scan_thread) {
    p->rescanning = 1;
    SetEvent(p->scan->cancel);
    status(p, L"Cancelling the previous scan…");
    return;
  }
  if (!p->game[0] || !is_directory(p->game)) {
    status(p, L"Select your Sudeki game folder on Play, then open Mods.");
    return;
  }
  p->rescanning = 0;
  p->scan = (ScanJob *)calloc(1, sizeof(*p->scan));
  if (!p->scan) {
    status(p, L"Unable to allocate the scan worker.");
    return;
  }
  p->scan->cancel = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (!p->scan->cancel) {
    free(p->scan);
    p->scan = NULL;
    status(p, L"Unable to create the scan cancellation event.");
    return;
  }
  StringCchCopyW(p->scan->directory, PATH_CAP, p->game);
  StringCchCopyW(p->scan->cache, PATH_CAP, p->cache);
  p->scan->grouping = &p->grouping;
  p->scan->priority_character = p->character_filter;
  p->scan->priority_category = p->category_filter;
  InitializeCriticalSection(&p->scan->queue_lock);
  p->scan_thread = CreateThread(NULL, 0, scan_worker, p->scan, 0, NULL);
  if (!p->scan_thread) {
    DeleteCriticalSection(&p->scan->queue_lock);
    CloseHandle(p->scan->cancel);
    free(p->scan);
    p->scan = NULL;
    status(p, L"Unable to start the scan worker.");
    return;
  }
  SetWindowTextW(p->rescan, L"Cancel scan");
  status(p, L"Reading game archives…");
}
/* Moves finished thumbnails into the panel's image list and onto any grid
 * tile already showing that resource. Panel (UI) thread only. */
static void drain_thumbnails(Panel *p, ScanJob *job) {
  ThumbResult *items;
  size_t count, i;
  int taken = p->catalog != NULL && p->catalog == job->result;
  EnterCriticalSection(&job->queue_lock);
  items = job->queue;
  count = job->queue_count;
  job->queue = NULL;
  job->queue_count = job->queue_capacity = 0;
  LeaveCriticalSection(&job->queue_lock);
  for (i = 0; i < count; ++i) {
    if (taken && items[i].index < p->catalog->count) {
      Resource *r = &p->catalog->resources[items[i].index];
      LVFINDINFOW find;
      int item;
      r->icon = ImageList_Add(p->images, items[i].bitmap, NULL);
      memset(&find, 0, sizeof(find));
      find.flags = LVFI_PARAM;
      find.lParam = (LPARAM)items[i].index;
      item = ListView_FindItem(p->grid, -1, &find);
      if (item >= 0 && r->icon >= 0) {
        LVITEMW update;
        memset(&update, 0, sizeof(update));
        update.mask = LVIF_IMAGE;
        update.iItem = item;
        update.iImage = r->icon;
        ListView_SetItem(p->grid, &update);
      }
    }
    DeleteObject(items[i].bitmap);
  }
  free(items);
}
/* Releases a finished (joined) scan job; a catalogue the panel took stays. */
static void release_scan_job(Panel *p, ScanJob *job) {
  drain_thumbnails(p, job);
  DeleteCriticalSection(&job->queue_lock);
  CloseHandle(job->cancel);
  if (job->result != p->catalog)
    free_catalog(job->result);
  free(job);
}
static void join_worker(HANDLE worker);
/* Stops a running scan and waits for it (used before freeing the catalogue
 * it may still be reading). */
static void stop_scan(Panel *p) {
  if (!p->scan_thread)
    return;
  SetEvent(p->scan->cancel);
  join_worker(p->scan_thread);
  p->scan_thread = NULL;
  release_scan_job(p, p->scan);
  p->scan = NULL;
}

static void poll_jobs(Panel *p) {
  WCHAR text[256];
  /* The catalogue is shown as soon as the worker publishes it. */
  if (p->scan_thread && p->scan->result && p->catalog != p->scan->result &&
      InterlockedCompareExchange(&p->scan->published, 0, 0)) {
    cancel_detail(p);
    select_resource(p, -1);
    free_catalog(p->catalog); /* the previous scan has finished */
    p->catalog = p->scan->result;
    ImageList_RemoveAll(p->images);
    refresh_grid(p);
  }
  if (p->scan_thread)
    drain_thumbnails(p, p->scan);
  if (p->scan_thread) {
    if (WaitForSingleObject(p->scan_thread, 0) == WAIT_OBJECT_0) {
      ScanJob *job = p->scan;
      int was_cancelled = WaitForSingleObject(job->cancel, 0) == WAIT_OBJECT_0;
      int taken = job->result != NULL && job->result == p->catalog;
      CloseHandle(p->scan_thread);
      p->scan_thread = NULL;
      p->scan = NULL;
      if (taken && !job->result->error[0]) {
        StringCchPrintfW(text, 256,
                         L"%u resources scanned. Grouping follows name "
                         L"patterns; changes appear next game launch.",
                         (unsigned)p->catalog->count);
        status(p, text);
      } else if (!was_cancelled)
        status(p, job->result && job->result->error[0] ? job->result->error
                              : L"Unable to allocate the game catalog.");
      else if (!p->rescanning)
        status(p, L"Scan cancelled.");
      release_scan_job(p, job);
      SetWindowTextW(p->rescan, L"Rescan game");
      if (p->rescanning)
        scan_start(p);
    } else {
      const WCHAR *phase = p->scan->phase == 0   ? L"Reading archives"
                           : p->scan->phase == 1 ? L"Finding resource names"
                           : p->scan->phase == 2 ? L"Building the catalogue"
                                                 : L"Thumbnails";
      if (p->scan->phase == 1)
        StringCchPrintfW(text, 256, L"%ls…", phase);
      else if (p->scan->phase == 3)
        StringCchPrintfW(text, 256,
                         L"%ls: %ld / %ld (selected category first; browse "
                         L"and apply now)",
                         phase, p->scan->thumbs_done, p->scan->thumbs_total);
      else
        StringCchPrintfW(text, 256, L"%ls: %ld / %ld", phase,
                         (long)p->scan->progress, (long)p->scan->total);
      status(p, text);
    }
  }
  if (p->detail_thread &&
      WaitForSingleObject(p->detail_thread, 0) == WAIT_OBJECT_0) {
    DetailJob *job = p->detail;
    int was_cancelled = WaitForSingleObject(job->cancel, 0) == WAIT_OBJECT_0;
    CloseHandle(p->detail_thread);
    p->detail_thread = NULL;
    p->detail = NULL;
    CloseHandle(job->cancel);
    if (!was_cancelled && !p->detail_pending) {
      Resource *r = p->catalog && p->selected_resource >= 0
                        ? &p->catalog->resources[p->selected_resource]
                        : NULL;
      SudekiMpModImageFree(&p->original_image);
      p->original_image = job->original;
      memset(&job->original, 0, sizeof(job->original));
      preview_set(p->original_preview, &p->original,
                  image_bitmap(&p->original_image, 0),
                  r && r->entry.kind == SUDEKIMP_MOD_RESOURCE_MODEL
                      ? L"Model preview unavailable"
                      : L"Original preview unavailable");
      preview_set(
          p->replacement_preview, &p->replacement,
          image_bitmap(&job->imported, 0),
          job->import_preview_unavailable
              ? L"No preview: WIC codec unavailable. Apply is available."
              : L"No replacement");
      p->candidate_valid = job->import_valid;
      p->candidate_identity = job->replacement_identity;
      if (job->error[0])
        SetWindowTextW(p->check, job->error);
      else if (job->import_valid && r) {
        uint32_t w = job->imported.info.width, h = job->imported.info.height;
        int differs = w != r->entry.width || h != r->entry.height;
        int aspect =
            (uint64_t)w * r->entry.height != (uint64_t)h * r->entry.width;
        StringCchPrintfW(text, 256, L"%lu × %lu%ls%ls%ls", (unsigned long)w,
                         (unsigned long)h, aspect ? L" · Aspect differs" : L"",
                         differs ? L" · Size differs (allowed)" : L"",
                         job->import_preview_unavailable ? L" · No WIC preview"
                                                         : L"");
        SetWindowTextW(p->check, text);
      } else
        SetWindowTextW(p->check, L"Choose a replacement image.");
    }
    SudekiMpModImageFree(&job->original);
    SudekiMpModImageFree(&job->imported);
    free(job);
    detail_start(p);
    set_action_state(p);
  }
}

static void apply_resource(Panel *p) {
  Resource *r;
  WCHAR root[PATH_CAP], manifest_path[PATH_CAP], textures[PATH_CAP],
      destination[PATH_CAP], relative_wide[256], text[512], resource_name[128];
  char relative[256];
  const WCHAR *extension;
  SudekiMpModManifest next = {0};
  uint8_t *encoded = NULL;
  size_t encoded_size = 0;
  unsigned attempt;
  int copied = 0;
  if (!p->catalog || p->selected_resource < 0 || !p->candidate_pending ||
      !p->candidate_valid || p->detail_thread ||
      !selected_mod_path(p, root, manifest_path))
    return;
  r = &p->catalog->resources[p->selected_resource];
  if (r->entry.kind != SUDEKIMP_MOD_RESOURCE_TEXTURE)
    return;
  if (!unchanged_file(p->candidate, &p->candidate_identity)) {
    queue_candidate(p, p->candidate);
    status(p, L"Replacement changed after preview. Review the updated preview, "
              L"then Apply again.");
    return;
  }
  extension = wcsrchr(p->candidate, L'.');
  if (!extension)
    return;
  utf8_to_wide(r->entry.name, resource_name, 128);
  if (!resource_name[0])
    StringCchPrintfW(resource_name, 128, L"0x%08lX",
                     (unsigned long)r->entry.texture_key);
  if (!join(textures, PATH_CAP, root, L"textures") || !make_directory(textures))
    goto fail;
  /* Fresh payload names preserve the old manifest's target until the new
   * manifest has reached disk. A failed commit removes only this new copy. */
  for (attempt = 0; attempt < 100; ++attempt) {
    StringCchPrintfW(relative_wide, 256, L"textures/%ls-%08lX-%u%ls",
                     resource_name, (unsigned long)GetTickCount(), attempt,
                     extension);
    if (!join(destination, PATH_CAP, root, relative_wide))
      goto fail;
    if (CopyFileW(p->candidate, destination, TRUE)) {
      copied = 1;
      break;
    }
    if (GetLastError() != ERROR_FILE_EXISTS &&
        GetLastError() != ERROR_ALREADY_EXISTS)
      goto fail;
  }
  if (!copied || !wide_to_utf8(relative_wide, relative, sizeof(relative)))
    goto fail;
  if (!unchanged_file(p->candidate, &p->candidate_identity)) {
    DeleteFileW(destination);
    queue_candidate(p, p->candidate);
    status(p, L"Replacement changed during copying. Review the updated "
              L"preview, then Apply again.");
    return;
  }
  if (!SudekiMpModManifestEncode(&p->manifest, &encoded, &encoded_size) ||
      !SudekiMpModManifestLoad(encoded, encoded_size, &next) ||
      !SudekiMpModManifestSetTexture(&next, r->entry.texture_key, relative) ||
      !save_manifest_at(manifest_path, &next))
    goto fail;
  free(encoded);
  SudekiMpModManifestFree(&p->manifest);
  p->manifest = next;
  p->candidate_pending = 0;
  StringCchPrintfW(text, 512, L"Applied to %ls. Start the game to see it.",
                   p->mod_list[p->selected_mod].name);
  status(p, text);
  refresh_grid(p);
  select_resource(p, p->selected_resource);
  return;
fail:
  free(encoded);
  SudekiMpModManifestFree(&next);
  if (copied)
    DeleteFileW(destination);
  error_box(p, L"Unable to save the replacement. The previous manifest entry "
               L"was preserved.");
}
static void revert_resource(Panel *p) {
  WCHAR root[PATH_CAP], path[PATH_CAP], text[512];
  char key[16];
  Resource *r;
  SudekiMpModManifest next = {0};
  uint8_t *bytes = NULL;
  size_t size = 0;
  int ok;
  if (!p->catalog || p->selected_resource < 0 ||
      !selected_mod_path(p, root, path))
    return;
  r = &p->catalog->resources[p->selected_resource];
  ok = SudekiMpModManifestEncode(&p->manifest, &bytes, &size) &&
       SudekiMpModManifestLoad(bytes, size, &next);
  free(bytes);
  if (ok && r->entry.kind == SUDEKIMP_MOD_RESOURCE_TEXTURE)
    ok = SudekiMpModManifestSetTexture(&next, r->entry.texture_key, NULL);
  snprintf(key, sizeof(key), "0x%08lX", (unsigned long)r->entry.archive_key);
  if (ok)
    ok = SudekiMpModManifestSetFile(
        &next, r->entry.name[0] ? r->entry.name : key, NULL);
  if (ok)
    ok = save_manifest_at(path, &next);
  if (!ok) {
    SudekiMpModManifestFree(&next);
    error_box(p, L"Unable to save the reverted manifest.");
    return;
  }
  SudekiMpModManifestFree(&p->manifest);
  p->manifest = next;
  StringCchPrintfW(text, 512, L"Reverted in %ls. Start the game to see it.",
                   p->mod_list[p->selected_mod].name);
  status(p, text);
  refresh_grid(p);
  select_resource(p, p->selected_resource);
}
static int export_to_path(Panel *p, const WCHAR *path) {
  IWICImagingFactory *factory = NULL;
  IWICStream *stream = NULL;
  IWICBitmapEncoder *encoder = NULL;
  IWICBitmapFrameEncode *frame = NULL;
  IPropertyBag2 *properties = NULL;
  IWICBitmap *bitmap = NULL;
  IWICFormatConverter *converter = NULL;
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
  WCHAR temporary[PATH_CAP];
  HRESULT hr, initialized;
  int result = 0;
  if (!p->original_image.rgba ||
      FAILED(StringCchPrintfW(temporary, PATH_CAP, L"%ls.launcher-%lu.tmp",
                              path, (unsigned long)GetCurrentProcessId())))
    return 0;
  /* WIC's stream writes only a fresh staging file; replacement follows a
   * successful frame/encoder commit and the release of its file handle. */
  if (GetFileAttributesW(temporary) != INVALID_FILE_ATTRIBUTES)
    return 0;
  initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                        &IID_IWICImagingFactory, (void **)&factory);
  if (FAILED(hr))
    goto done;
  hr = IWICImagingFactory_CreateStream(factory, &stream);
  if (FAILED(hr))
    goto done;
  hr = IWICStream_InitializeFromFilename(stream, temporary, GENERIC_WRITE);
  if (FAILED(hr))
    goto done;
  hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL,
                                        &encoder);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapEncoder_Initialize(encoder, (IStream *)stream,
                                    WICBitmapEncoderNoCache);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &properties);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapFrameEncode_Initialize(frame, properties);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapFrameEncode_SetSize(frame, p->original_image.info.width,
                                     p->original_image.info.height);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
  if (FAILED(hr))
    goto done;
  hr = IWICImagingFactory_CreateBitmapFromMemory(
      factory, p->original_image.info.width, p->original_image.info.height,
      &GUID_WICPixelFormat32bppRGBA, p->original_image.info.width * 4,
      p->original_image.info.width * p->original_image.info.height * 4,
      p->original_image.rgba, &bitmap);
  if (FAILED(hr))
    goto done;
  hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
  if (FAILED(hr))
    goto done;
  hr = IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)bitmap,
                                      &format, WICBitmapDitherTypeNone, NULL, 0,
                                      WICBitmapPaletteTypeCustom);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource *)converter,
                                         NULL);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapFrameEncode_Commit(frame);
  if (FAILED(hr))
    goto done;
  hr = IWICBitmapEncoder_Commit(encoder);
  if (FAILED(hr))
    goto done;
  result = 1;
done:
  if (converter)
    IWICFormatConverter_Release(converter);
  if (bitmap)
    IWICBitmap_Release(bitmap);
  if (properties)
    IPropertyBag2_Release(properties);
  if (frame)
    IWICBitmapFrameEncode_Release(frame);
  if (encoder)
    IWICBitmapEncoder_Release(encoder);
  if (stream)
    IWICStream_Release(stream);
  if (factory)
    IWICImagingFactory_Release(factory);
  if (SUCCEEDED(initialized))
    CoUninitialize();
  if (result)
    result = MoveFileExW(temporary, path,
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  if (!result)
    DeleteFileW(temporary);
  return result;
}

typedef struct NamePrompt {
  HWND edit, window;
  WCHAR value[MAX_PATH];
  int done, accepted;
  HINSTANCE instance;
} NamePrompt;
static LRESULT CALLBACK name_proc(HWND window, UINT message, WPARAM wparam,
                                  LPARAM lparam) {
  NamePrompt *prompt = (NamePrompt *)GetWindowLongPtrW(window, GWLP_USERDATA);
  if (message == WM_NCCREATE) {
    prompt = (NamePrompt *)((CREATESTRUCTW *)lparam)->lpCreateParams;
    SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)prompt);
    return TRUE;
  }
  if (message == WM_CREATE) {
    HWND label = CreateWindowW(L"STATIC", L"Mod folder and display name:",
                               WS_CHILD | WS_VISIBLE, 16, 14, 328, 24, window,
                               NULL, prompt->instance, NULL);
    HWND ok, cancel;
    SendMessageW(label, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),
                 TRUE);
    prompt->edit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"My Mods",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 16, 40, 328, 28,
        window, (HMENU)(INT_PTR)ID_NAME, prompt->instance, NULL);
    SendMessageW(prompt->edit, EM_SETLIMITTEXT, 80, 0);
    SendMessageW(prompt->edit, WM_SETFONT,
                 (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    ok = CreateWindowW(L"BUTTON", L"Create",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                       160, 84, 88, 30, window, (HMENU)(INT_PTR)ID_NAME_OK,
                       prompt->instance, NULL);
    cancel = CreateWindowW(
        L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 256, 84, 88,
        30, window, (HMENU)(INT_PTR)ID_NAME_CANCEL, prompt->instance, NULL);
    SendMessageW(ok, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),
                 TRUE);
    SendMessageW(cancel, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),
                 TRUE);
    return 0;
  }
  if (message == WM_COMMAND && prompt) {
    if (LOWORD(wparam) == ID_NAME_OK) {
      GetWindowTextW(prompt->edit, prompt->value, MAX_PATH);
      prompt->accepted = 1;
      prompt->done = 1;
      return 0;
    }
    if (LOWORD(wparam) == ID_NAME_CANCEL) {
      prompt->done = 1;
      return 0;
    }
  }
  if (message == WM_CLOSE && prompt) {
    prompt->done = 1;
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}
static int valid_mod_name(const WCHAR *name) {
  size_t i, length = wcslen(name);
  WCHAR stem[16];
  const WCHAR *dot;
  if (!length || length > 80 || name[0] == L'.' || name[0] == L'_' ||
      name[length - 1] == L'.' || name[length - 1] == L' ')
    return 0;
  for (i = 0; i < length; ++i)
    if (name[i] < 32 || wcschr(L"\\/:*?\"<>|", name[i]))
      return 0;
  dot = wcschr(name, L'.');
  i = dot ? (size_t)(dot - name) : length;
  if (i < 16) {
    memcpy(stem, name, i * sizeof(WCHAR));
    stem[i] = 0;
    if (!_wcsicmp(stem, L"CON") || !_wcsicmp(stem, L"PRN") ||
        !_wcsicmp(stem, L"AUX") || !_wcsicmp(stem, L"NUL"))
      return 0;
    if (i == 4 &&
        (!_wcsnicmp(stem, L"COM", 3) || !_wcsnicmp(stem, L"LPT", 3)) &&
        stem[3] >= L'1' && stem[3] <= L'9')
      return 0;
  }
  return 1;
}
static void new_mod_prompt(Panel *p) {
  NamePrompt prompt;
  MSG message;
  RECT owner;
  int quit = 0;
  memset(&prompt, 0, sizeof(prompt));
  prompt.instance = p->instance;
  GetWindowRect(GetAncestor(p->window, GA_ROOT), &owner);
  prompt.window = CreateWindowExW(
      WS_EX_DLGMODALFRAME, NAME_CLASS, L"New mod",
      WS_POPUP | WS_CAPTION | WS_SYSMENU, owner.left + 80, owner.top + 80, 376,
      160, GetAncestor(p->window, GA_ROOT), NULL, p->instance, &prompt);
  if (!prompt.window)
    return;
  EnableWindow(GetAncestor(p->window, GA_ROOT), FALSE);
  ShowWindow(prompt.window, SW_SHOW);
  SetFocus(prompt.edit);
  SendMessageW(prompt.edit, EM_SETSEL, 0, -1);
  while (!prompt.done) {
    int got = GetMessageW(&message, NULL, 0, 0);
    if (got <= 0) {
      quit = got == 0;
      break;
    }
    if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN &&
        IsChild(prompt.window, message.hwnd)) {
      SendMessageW(prompt.window, WM_COMMAND, ID_NAME_OK, 0);
      continue;
    }
    if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
      prompt.done = 1;
      continue;
    }
    if (!IsDialogMessageW(prompt.window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  DestroyWindow(prompt.window);
  EnableWindow(GetAncestor(p->window, GA_ROOT), TRUE);
  SetActiveWindow(GetAncestor(p->window, GA_ROOT));
  if (quit) {
    PostQuitMessage((int)message.wParam);
    return;
  }
  if (prompt.accepted) {
    if (!valid_mod_name(prompt.value))
      error_box(
          p,
          L"Use a folder name up to 80 characters, without path punctuation, a "
          L"leading dot/underscore, or a trailing dot/space.");
    else if (!create_mod(p, prompt.value))
      error_box(p, L"Unable to create this mod. The folder may already exist "
                   L"or be read-only.");
    else {
      refresh_mods(p, prompt.value);
      status(p, L"Mod created. Select a texture and choose a replacement.");
    }
  }
  SendMessageW(
      p->selector, CB_SETCURSEL,
      (WPARAM)(p->selected_mod >= 0 ? p->selected_mod : (int)p->mod_count), 0);
}
static void browse_replacement(Panel *p) {
  OPENFILENAMEW dialog;
  WCHAR path[PATH_CAP] = {0};
  memset(&dialog, 0, sizeof(dialog));
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = p->window;
  dialog.lpstrFilter =
      L"Texture images\0*.png;*.dds;*.tga;*.bmp;*.jpg;*.jpeg\0All files\0*.*\0";
  dialog.lpstrFile = path;
  dialog.nMaxFile = PATH_CAP;
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  dialog.lpstrTitle = L"Choose a replacement texture";
  if (GetOpenFileNameW(&dialog))
    queue_candidate(p, path);
}
static void export_original(Panel *p) {
  OPENFILENAMEW dialog;
  WCHAR path[PATH_CAP] = {0};
  WCHAR name[128];
  Resource *r;
  if (!p->original_image.rgba || p->selected_resource < 0)
    return;
  r = &p->catalog->resources[p->selected_resource];
  utf8_to_wide(r->entry.name, name, 128);
  if (name[0])
    StringCchPrintfW(path, PATH_CAP, L"%ls.png", name);
  else
    StringCchPrintfW(path, PATH_CAP, L"0x%08lX.png",
                     (unsigned long)r->entry.texture_key);
  memset(&dialog, 0, sizeof(dialog));
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = p->window;
  dialog.lpstrFilter = L"PNG image\0*.png\0";
  dialog.lpstrFile = path;
  dialog.nMaxFile = PATH_CAP;
  dialog.lpstrDefExt = L"png";
  dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetSaveFileNameW(&dialog)) {
    if (export_to_path(p, path))
      status(p, L"Original exported as PNG.");
    else
      error_box(p, L"PNG export failed. WIC/windowscodecs must be available, "
                   L"and the destination must be writable.");
  }
}
static void save_mod_enabled(Panel *p) {
  WCHAR root[PATH_CAP], path[PATH_CAP], preferred[MAX_PATH];
  SudekiMpModManifest next = {0};
  uint8_t *bytes = NULL;
  size_t size = 0;
  int value, ok;
  if (!p->manifest.text || !selected_mod_path(p, root, path))
    return;
  value = SendMessageW(p->enabled, BM_GETCHECK, 0, 0) == BST_CHECKED;
  ok = SudekiMpModManifestEncode(&p->manifest, &bytes, &size) &&
       SudekiMpModManifestLoad(bytes, size, &next);
  free(bytes);
  if (ok)
    ok = SudekiMpModManifestSetEnabled(&next, value) &&
         save_manifest_at(path, &next);
  SudekiMpModManifestFree(&next);
  if (!ok) {
    SendMessageW(
        p->enabled, BM_SETCHECK,
        p->mod_list[p->selected_mod].enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    error_box(p, L"Unable to save this mod's enabled setting.");
    return;
  }
  StringCchCopyW(preferred, MAX_PATH, p->mod_list[p->selected_mod].folder);
  refresh_mods(p, preferred);
  status(p, L"Mod setting saved. It takes effect on the next game launch.");
}
/* Same look as the launcher's own buttons: rounded, outlined, cyan when pressed. */
static void draw_button(Panel *p, const DRAWITEMSTRUCT *draw) {
  RECT r = draw->rcItem;
  WCHAR text[128];
  int disabled = (draw->itemState & ODS_DISABLED) != 0;
  int pressed = (draw->itemState & ODS_SELECTED) != 0;
  HBRUSH fill = CreateSolidBrush(disabled  ? RGB(43, 52, 62)
                                 : pressed ? RGB(40, 100, 168)
                                           : BUTTON_BG);
  HPEN pen = CreatePen(PS_SOLID, 1, pressed ? CYAN : OUTLINE);
  HGDIOBJ old_brush, old_pen, old_font;
  FillRect(draw->hDC, &r, p->background);
  old_brush = SelectObject(draw->hDC, fill);
  old_pen = SelectObject(draw->hDC, pen);
  RoundRect(draw->hDC, r.left, r.top, r.right, r.bottom, 8, 8);
  SelectObject(draw->hDC, old_brush);
  SelectObject(draw->hDC, old_pen);
  DeleteObject(fill);
  DeleteObject(pen);
  SetBkMode(draw->hDC, TRANSPARENT);
  SetTextColor(draw->hDC, disabled ? RGB(125, 140, 155) : FG);
  GetWindowTextW(draw->hwndItem, text, 128);
  old_font = SelectObject(draw->hDC, body(p));
  DrawTextW(draw->hDC, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  if (draw->itemState & ODS_FOCUS) {
    InflateRect(&r, -4, -4);
    DrawFocusRect(draw->hDC, &r);
  }
  SelectObject(draw->hDC, old_font);
}
/* Character/category lists: dark rows, the selection marked by a cyan bar. */
static void draw_list_item(Panel *p, const DRAWITEMSTRUCT *draw) {
  RECT r = draw->rcItem, bar;
  WCHAR text[128];
  int selected = (draw->itemState & ODS_SELECTED) != 0;
  HBRUSH fill;
  HGDIOBJ old_font;
  if ((int)draw->itemID < 0)
    return;
  fill = CreateSolidBrush(selected ? SELECTED_BG : INPUT_BG);
  FillRect(draw->hDC, &r, fill);
  DeleteObject(fill);
  if (selected) {
    bar = r;
    bar.right = bar.left + 4;
    fill = CreateSolidBrush(CYAN);
    FillRect(draw->hDC, &bar, fill);
    DeleteObject(fill);
  }
  text[0] = 0;
  if (SendMessageW(draw->hwndItem, LB_GETTEXTLEN, draw->itemID, 0) < 128)
    SendMessageW(draw->hwndItem, LB_GETTEXT, draw->itemID, (LPARAM)text);
  r.left += 14;
  SetBkMode(draw->hDC, TRANSPARENT);
  SetTextColor(draw->hDC, selected ? CYAN : FG);
  old_font = SelectObject(draw->hDC, body(p));
  DrawTextW(draw->hDC, text, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  SelectObject(draw->hDC, old_font);
}
static HWND control(Panel *p, const WCHAR *class_name, const WCHAR *text,
                    DWORD style, int id) {
  HWND window = CreateWindowExW(
      (!wcscmp(class_name, L"EDIT") || !wcscmp(class_name, L"LISTBOX"))
          ? WS_EX_CLIENTEDGE
          : 0,
      class_name, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1, p->window,
      (HMENU)(INT_PTR)id, p->instance, NULL);
  SendMessageW(window, WM_SETFONT, (WPARAM)p->font, TRUE);
  return window;
}
static HWND button(Panel *p, const WCHAR *text, int id) {
  return control(p, L"BUTTON", text, WS_TABSTOP | BS_OWNERDRAW, id);
}
/* Sized for the launcher's full-width Mods page (about 864 x 488), and still
 * usable larger. Left: character/category; centre: thumbnails; right: detail. */
static void layout(Panel *p, int width, int height) {
  int margin = 12, left = 150, right = 268, top = 72,
      bottom = height - (p->status_sink ? 10 : 34),
      centre_x = margin + left + 12, centre_w = width - left - right - 48,
      detail_x = width - right - margin, preview_h, list_h;
  if (centre_w < 100)
    centre_w = 100;
  preview_h = (bottom - top - 62 - 22 - 34 - 2 - 38 - 36) / 2;
  if (preview_h < 60)
    preview_h = 60;
  list_h = (bottom - top - 2 * 26 - 10) / 2;
  MoveWindow(p->selector, margin, 10, 236, 300, TRUE);
  MoveWindow(p->new_mod, margin + 244, 8, 96, 30, TRUE);
  MoveWindow(p->enabled, margin + 352, 12, 130, 24, TRUE);
  MoveWindow(p->folder, width - margin - 238, 8, 112, 30, TRUE);
  MoveWindow(p->rescan, width - margin - 118, 8, 118, 30, TRUE);
  MoveWindow(p->order_label, margin, 44, width - 2 * margin, 20, TRUE);
  MoveWindow(p->character_label, margin, top, left, 22, TRUE);
  MoveWindow(p->character, margin, top + 24, left, list_h, TRUE);
  MoveWindow(p->category_label, margin, top + 34 + list_h, left, 22, TRUE);
  MoveWindow(p->category, margin, top + 58 + list_h, left, bottom - top - 58 - list_h, TRUE);
  MoveWindow(p->search, centre_x, top, centre_w, 28, TRUE);
  MoveWindow(p->grid, centre_x, top + 36, centre_w, bottom - top - 36, TRUE);
  /* Three metadata lines, two equal previews, a two-line hint, then checks. */
  MoveWindow(p->information, detail_x, top - 10, right, 68, TRUE);
  MoveWindow(p->original_label, detail_x, top + 62, right, 20, TRUE);
  MoveWindow(p->original_preview, detail_x, top + 84, right, preview_h, TRUE);
  MoveWindow(p->replacement_label, detail_x, top + 92 + preview_h, 120, 20, TRUE);
  MoveWindow(p->browse, detail_x + right - 96, top + 88 + preview_h, 96, 28, TRUE);
  MoveWindow(p->replacement_preview, detail_x, top + 118 + preview_h, right,
             preview_h, TRUE);
  MoveWindow(p->drop, detail_x, top + 120 + preview_h * 2, right, 38, TRUE);
  MoveWindow(p->check, detail_x, top + 158 + preview_h * 2, right,
             bottom - 36 - (top + 158 + preview_h * 2), TRUE);
  MoveWindow(p->apply, detail_x, bottom - 30, 84, 30, TRUE);
  MoveWindow(p->revert, detail_x + 90, bottom - 30, 80, 30, TRUE);
  MoveWindow(p->export_original, detail_x + 176, bottom - 30, right - 176, 30,
             TRUE);
  MoveWindow(p->status, margin, height - 26, width - 24, 22, TRUE);
  ShowWindow(p->status, p->status_sink ? SW_HIDE : SW_SHOW);
}
static void create_controls(Panel *p) {
  size_t i;
  p->original.font = p->font;
  p->replacement.font = p->font;
  p->selector = control(p, L"COMBOBOX", L"",
                        WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, ID_MOD);
  p->new_mod = button(p, L"New mod…", ID_NEW);
  p->enabled = control(p, L"BUTTON", L"Load this mod",
                       WS_TABSTOP | BS_AUTOCHECKBOX, ID_ENABLED);
  p->folder = button(p, L"Open folder", ID_FOLDER);
  p->rescan = button(p, L"Rescan game", ID_RESCAN);
  p->order_label = control(
      p, L"STATIC",
      L"Mods load in folder-name order; a later mod wins when two change the same thing.",
      SS_ENDELLIPSIS, ID_NOTE);
  p->character_label = control(p, L"STATIC", L"Character", 0, ID_HEADING);
  p->category_label = control(p, L"STATIC", L"Category", 0, ID_HEADING);
  p->character = control(p, L"LISTBOX", L"",
                         WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                             LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                         ID_CHARACTER);
  p->category = control(p, L"LISTBOX", L"",
                        WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                        ID_CATEGORY);
  for (i = 0; i < sizeof(characters) / sizeof(characters[0]); ++i)
    SendMessageW(p->character, LB_ADDSTRING, 0, (LPARAM)characters[i]);
  for (i = 0; i < sizeof(categories) / sizeof(categories[0]); ++i)
    SendMessageW(p->category, LB_ADDSTRING, 0, (LPARAM)categories[i]);
  SendMessageW(p->character, LB_SETCURSEL, p->character_filter, 0);
  SendMessageW(p->category, LB_SETCURSEL, p->category_filter, 0);
  p->search = control(p, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_SEARCH);
  SendMessageW(p->search, EM_SETCUEBANNER, TRUE,
               (LPARAM)L"Search names or texture keys…");
  p->grid = control(p, WC_LISTVIEWW, L"",
                    WS_TABSTOP | WS_VSCROLL | LVS_ICON | LVS_SINGLESEL |
                        LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
                    ID_GRID);
  ListView_SetBkColor(p->grid, INPUT_BG);
  ListView_SetTextBkColor(p->grid, INPUT_BG);
  ListView_SetTextColor(p->grid, FG);
  ListView_SetIconSpacing(p->grid, 122, 130);
  p->images = ImageList_Create(THUMB_SIDE, THUMB_SIDE, ILC_COLOR32, 32, 32);
  ListView_SetImageList(p->grid, p->images, LVSIL_NORMAL);
  p->information = control(p, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 0);
  p->original_label = control(p, L"STATIC", L"Original", 0, ID_HEADING);
  p->replacement_label = control(p, L"STATIC", L"Replacement", 0, ID_HEADING);
  p->original_preview =
      CreateWindowExW(0, PREVIEW_CLASS, L"", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                      p->window, NULL, p->instance, &p->original);
  p->replacement_preview =
      CreateWindowExW(0, PREVIEW_CLASS, L"", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                      p->window, NULL, p->instance, &p->replacement);
  StringCchCopyW(p->original.empty, 128, L"Select a resource");
  StringCchCopyW(p->replacement.empty, 128, L"No replacement");
  p->drop = control(p, L"STATIC",
                    L"Drop an image on the box above, or Browse…",
                    SS_LEFT, ID_NOTE);
  p->browse = button(p, L"Browse…", ID_BROWSE);
  p->check = control(p, L"STATIC", L"", SS_LEFT, ID_NOTE);
  p->apply = button(p, L"Apply", ID_APPLY);
  p->revert = button(p, L"Revert", ID_REVERT);
  p->export_original = button(p, L"Export PNG", ID_EXPORT);
  p->status = control(
      p, L"STATIC", L"Select your Sudeki game folder on Play, then open Mods.",
      SS_LEFT, 0);
  DragAcceptFiles(p->window, TRUE);
  DragAcceptFiles(p->replacement_preview, TRUE);
  SetWindowSubclass(p->drop, drop_zone_proc, 1, 0);
  DragAcceptFiles(p->drop, TRUE);
  set_action_state(p);
  SetTimer(p->window, 1, 100, NULL);
}
static void join_worker(HANDLE worker) {
  /* A cancellation request reaches core loops frequently. Keep painting
   * during teardown while positively joining before freeing its context. */
  if (!worker)
    return;
  while (WaitForSingleObject(worker, 0) != WAIT_OBJECT_0) {
    DWORD result = MsgWaitForMultipleObjects(1, &worker, FALSE, 30, QS_PAINT);
    if (result == WAIT_OBJECT_0 + 1) {
      MSG message;
      while (PeekMessageW(&message, NULL, WM_PAINT, WM_PAINT, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
    }
  }
  CloseHandle(worker);
}
static void destroy_panel(Panel *p) {
  p->destroying = 1;
  KillTimer(p->window, 1);
  if (p->scan)
    SetEvent(p->scan->cancel);
  cancel_detail(p);
  join_worker(p->scan_thread);
  join_worker(p->detail_thread);
  if (p->scan) {
    release_scan_job(p, p->scan);
    p->scan = NULL;
  }
  if (p->detail) {
    CloseHandle(p->detail->cancel);
    SudekiMpModImageFree(&p->detail->original);
    SudekiMpModImageFree(&p->detail->imported);
    free(p->detail);
  }
  free_catalog(p->catalog);
  SudekiMpModImageFree(&p->original_image);
  SudekiMpModManifestFree(&p->manifest);
  SudekiMpModGroupsFree(&p->grouping);
  if (p->original.bitmap)
    DeleteObject(p->original.bitmap);
  if (p->replacement.bitmap)
    DeleteObject(p->replacement.bitmap);
  if (p->images)
    ImageList_Destroy(p->images);
  DeleteObject(p->background);
  DeleteObject(p->input_background);
  DeleteObject(p->font);
  free(p);
}
static LRESULT CALLBACK panel_proc(HWND window, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
  Panel *p = (Panel *)GetWindowLongPtrW(window, GWLP_USERDATA);
  if (message == WM_NCCREATE) {
    p = (Panel *)((CREATESTRUCTW *)lparam)->lpCreateParams;
    p->window = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)p);
    return TRUE;
  }
  if (!p)
    return DefWindowProcW(window, message, wparam, lparam);
  if (p->destroying)
    return DefWindowProcW(window, message, wparam, lparam);
  switch (message) {
  case WM_CREATE:
    create_controls(p);
    return 0;
  case WM_SIZE:
    layout(p, LOWORD(lparam), HIWORD(lparam));
    return 0;
  case WM_TIMER:
    poll_jobs(p);
    return 0;
  case WM_MEASUREITEM:
    ((MEASUREITEMSTRUCT *)lparam)->itemHeight = 26;
    return TRUE;
  case WM_DRAWITEM:
    if (((const DRAWITEMSTRUCT *)lparam)->CtlType == ODT_LISTBOX)
      draw_list_item(p, (const DRAWITEMSTRUCT *)lparam);
    else
      draw_button(p, (const DRAWITEMSTRUCT *)lparam);
    return TRUE;
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLORBTN: {
    int id = GetDlgCtrlID((HWND)lparam);
    SetTextColor((HDC)wparam, id == ID_HEADING ? CYAN : id == ID_NOTE ? MUTED : FG);
    SetBkColor((HDC)wparam, BG);
    return (LRESULT)p->background;
  }
  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORLISTBOX:
    SetTextColor((HDC)wparam, FG);
    SetBkColor((HDC)wparam, INPUT_BG);
    return (LRESULT)p->input_background;
  case WM_ERASEBKGND: {
    RECT r;
    GetClientRect(window, &r);
    FillRect((HDC)wparam, &r, p->background);
    return TRUE;
  }
  case WM_NOTIFY: {
    NMHDR *header = (NMHDR *)lparam;
    if (header->hwndFrom == p->grid && header->code == NM_CUSTOMDRAW)
      return grid_custom_draw(p, (NMLVCUSTOMDRAW *)lparam);
    if (header->hwndFrom == p->grid && header->code == LVN_ITEMCHANGED &&
        !p->refreshing) {
      NMLISTVIEW *change = (NMLISTVIEW *)lparam;
      if ((change->uChanged & LVIF_STATE) &&
          (change->uNewState & LVIS_SELECTED)) {
        LVITEMW item;
        memset(&item, 0, sizeof(item));
        item.mask = LVIF_PARAM;
        item.iItem = change->iItem;
        if (ListView_GetItem(p->grid, &item))
          select_resource(p, (int)item.lParam);
      }
    }
    return 0;
  }
  case WM_DROPFILES: {
    WCHAR path[PATH_CAP];
    HDROP drop = (HDROP)wparam;
    UINT count = DragQueryFileW(drop, 0xffffffffu, NULL, 0);
    if (count == 1 && DragQueryFileW(drop, 0, NULL, 0) < PATH_CAP &&
        DragQueryFileW(drop, 0, path, PATH_CAP))
      queue_candidate(p, path);
    else
      status(p, L"Drop one replacement image onto the selected resource.");
    DragFinish(drop);
    return 0;
  }
  case WM_COMMAND:
    switch (LOWORD(wparam)) {
    case ID_MOD:
      if (HIWORD(wparam) == CBN_SELCHANGE) {
        int selected = (int)SendMessageW(p->selector, CB_GETCURSEL, 0, 0);
        if ((size_t)selected == p->mod_count)
          new_mod_prompt(p);
        else {
          p->selected_mod = selected;
          load_selected_mod(p);
        }
      }
      break;
    case ID_NEW:
      new_mod_prompt(p);
      break;
    case ID_ENABLED:
      save_mod_enabled(p);
      break;
    case ID_FOLDER: {
      /* The selected mod's folder, else the mods folder itself (created). */
      WCHAR root[PATH_CAP];
      if (!p->mods[0]) {
        status(p, L"Choose the Sudeki folder on the Play tab first.");
        break;
      }
      if (!selected_mod_path(p, root, NULL)) {
        if (!make_directory(p->mods)) {
          error_box(p, L"Unable to create the mods folder.");
          break;
        }
        StringCchCopyW(root, PATH_CAP, p->mods);
      }
      if ((INT_PTR)ShellExecuteW(window, L"explore", root, NULL, NULL, SW_SHOWNORMAL) <= 32 &&
          (INT_PTR)ShellExecuteW(window, L"open", root, NULL, NULL, SW_SHOWNORMAL) <= 32)
        error_box(p, L"Windows could not open the mod folder.");
      break;
    }
    case ID_RESCAN:
      if (p->scan_thread) {
        SetEvent(p->scan->cancel);
        p->rescanning = 0;
        status(p, L"Cancelling scan…");
      } else {
        refresh_mods(p, NULL);
        scan_start(p);
      }
      break;
    case ID_CHARACTER:
      if (HIWORD(wparam) == LBN_SELCHANGE) {
        p->character_filter =
            (int)SendMessageW(p->character, LB_GETCURSEL, 0, 0);
        if (p->scan)
          InterlockedExchange(&p->scan->priority_character, p->character_filter);
        refresh_grid(p);
      }
      break;
    case ID_CATEGORY:
      if (HIWORD(wparam) == LBN_SELCHANGE) {
        p->category_filter = (int)SendMessageW(p->category, LB_GETCURSEL, 0, 0);
        if (p->scan)
          InterlockedExchange(&p->scan->priority_category, p->category_filter);
        refresh_grid(p);
      }
      break;
    case ID_SEARCH:
      if (HIWORD(wparam) == EN_CHANGE)
        refresh_grid(p);
      break;
    case ID_BROWSE:
      browse_replacement(p);
      break;
    case ID_APPLY:
      apply_resource(p);
      break;
    case ID_REVERT:
      revert_resource(p);
      break;
    case ID_EXPORT:
      export_original(p);
      break;
    }
    return 0;
  case WM_DESTROY:
    destroy_panel(p);
    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}
HWND SudekiMpModsPanelCreate(HWND parent, HINSTANCE instance) {
  WNDCLASSW wc;
  INITCOMMONCONTROLSEX common;
  Panel *p;
  HWND window;
  WCHAR executable[PATH_CAP], *separator, path[PATH_CAP];
  uint8_t *bytes = NULL;
  size_t size = 0;
  DWORD length;
  memset(&common, 0, sizeof(common));
  common.dwSize = sizeof(common);
  common.dwICC = ICC_LISTVIEW_CLASSES;
  InitCommonControlsEx(&common);
  memset(&wc, 0, sizeof(wc));
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
  wc.lpfnWndProc = panel_proc;
  wc.lpszClassName = PANEL_CLASS;
  if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return NULL;
  wc.lpfnWndProc = preview_proc;
  wc.lpszClassName = PREVIEW_CLASS;
  if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return NULL;
  wc.lpfnWndProc = name_proc;
  wc.lpszClassName = NAME_CLASS;
  if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return NULL;
  p = (Panel *)calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  p->instance = instance;
  p->selected_mod = -1;
  p->selected_resource = -1;
  p->character_filter = 0;
  p->category_filter = CAT_BODY;
  p->background = CreateSolidBrush(BG);
  p->input_background = CreateSolidBrush(INPUT_BG);
  p->font =
      CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
  length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, PATH_CAP);
  if (length && length < PATH_CAP &&
      join(p->cache, PATH_CAP, path, L"SudekiMP\\thumbs") &&
      !make_directory(p->cache))
    p->cache[0] = 0;
  length = GetModuleFileNameW(NULL, executable, PATH_CAP);
  separator = length && length < PATH_CAP ? wcsrchr(executable, L'\\') : NULL;
  if (separator) {
    *separator = 0;
    if (join(p->groups, PATH_CAP, executable, L"mod-groups.ini") &&
        read_range(p->groups, 0, (size_t)-1, &bytes, &size))
      SudekiMpModGroupsLoad(&p->grouping, bytes, size);
    free(bytes);
  }
  window = CreateWindowExW(WS_EX_CONTROLPARENT, PANEL_CLASS, L"Mods",
                           WS_CHILD | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, NULL,
                           instance, p);
  if (!window) {
    DeleteObject(p->background);
    DeleteObject(p->input_background);
    DeleteObject(p->font);
    SudekiMpModGroupsFree(&p->grouping);
    free(p);
  }
  return window;
}
void SudekiMpModsPanelSetPaths(HWND panel, const WCHAR *game_directory,
                               const WCHAR *ini_path) {
  Panel *p = (Panel *)GetWindowLongPtrW(panel, GWLP_USERDATA);
  WCHAR folder[PATH_CAP], enabled[32];
  int changed;
  if (!p || p->destroying)
    return;
  changed = _wcsicmp(p->game, game_directory ? game_directory : L"") ||
            _wcsicmp(p->ini, ini_path ? ini_path : L"");
  if (!changed && (p->catalog || p->scan_thread)) {
    /* Same game: re-read only the mod folders (cheap), never the archives,
     * so mods copied in from Explorer appear without a rescan. */
    refresh_mods(p, NULL);
    return;
  }
  if (changed) {
    select_resource(p, -1);
    stop_scan(p); /* it may still be reading the published catalogue */
    free_catalog(p->catalog);
    p->catalog = NULL;
    p->refreshing = 1;
    ListView_DeleteAllItems(p->grid);
    p->refreshing = 0;
    ImageList_RemoveAll(p->images);
  }
  if (FAILED(StringCchCopyW(p->game, PATH_CAP,
                            game_directory ? game_directory : L"")) ||
      FAILED(StringCchCopyW(p->ini, PATH_CAP, ini_path ? ini_path : L""))) {
    status(p, L"The selected game or configuration path is too long.");
    return;
  }
  EnableWindow(p->new_mod, p->game[0] && is_directory(p->game));
  if (!p->game[0] || !is_directory(p->game)) {
    p->mods[0] = 0;
    p->mod_count = 0;
    p->selected_mod = -1;
    SendMessageW(p->selector, CB_RESETCONTENT, 0, 0);
    SudekiMpModManifestFree(&p->manifest);
    set_action_state(p);
    if (p->scan)
      SetEvent(p->scan->cancel);
    p->rescanning = 0;
    status(p, L"Select your Sudeki game folder on Play, then open Mods.");
    return;
  }
  GetPrivateProfileStringW(L"Mods", L"Folder", L"mods", folder, PATH_CAP,
                           p->ini);
  if ((folder[0] && folder[1] == L':') ||
      (folder[0] == L'\\' && folder[1] == L'\\'))
    StringCchCopyW(p->mods, PATH_CAP, folder);
  else
    join(p->mods, PATH_CAP, p->game, folder);
  GetPrivateProfileStringW(L"Mods", L"Enable", L"true", enabled, 32, p->ini);
  p->loading_off = !_wcsicmp(enabled, L"false") || !wcscmp(enabled, L"0") ||
                   !_wcsicmp(enabled, L"no") || !_wcsicmp(enabled, L"off");
  refresh_mods(p, NULL);
  scan_start(p);
}
void SudekiMpModsPanelDestroy(HWND panel) {
  if (panel && IsWindow(panel))
    DestroyWindow(panel);
}
void SudekiMpModsPanelSetStyle(HWND panel, HFONT body_font, HFONT heading_font) {
  Panel *p = (Panel *)GetWindowLongPtrW(panel, GWLP_USERDATA);
  HWND child;
  if (!p)
    return;
  p->body_font = body_font;
  p->heading_font = heading_font;
  for (child = GetWindow(panel, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    SendMessageW(child, WM_SETFONT,
                 (WPARAM)(GetDlgCtrlID(child) == ID_HEADING && heading_font ? heading_font
                                                                           : body(p)),
                 TRUE);
  p->original.font = p->replacement.font = body(p);
  InvalidateRect(panel, NULL, TRUE);
}
void SudekiMpModsPanelSetStatusLabel(HWND panel, HWND label) {
  Panel *p = (Panel *)GetWindowLongPtrW(panel, GWLP_USERDATA);
  RECT r;
  if (!p)
    return;
  p->status_sink = label;
  GetClientRect(panel, &r);
  layout(p, r.right, r.bottom);
}
