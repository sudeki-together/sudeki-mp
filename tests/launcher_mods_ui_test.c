/* Runs only against tests/make_modding_ui_fixture.py's synthetic install.
 * Including the panel exposes worker completion for deterministic acceptance. */
#include "../src/launcher/mods_panel.c"
#include <assert.h>
#include <stdio.h>
#include <dlgs.h>

static DWORD max_dispatch_ms;
static void pump(Panel *p) {
    DWORD deadline = GetTickCount() + 30000;
    do {
        DWORD began = GetTickCount();
        MSG message;
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        poll_jobs(p);
        if (GetTickCount() - began > max_dispatch_ms) max_dispatch_ms = GetTickCount() - began;
        if (!p->scan_thread && !p->detail_thread && !p->rescanning && !p->detail_pending) return;
        Sleep(10);
    } while ((LONG)(deadline - GetTickCount()) > 0);
    assert(!"Panel worker timed out");
}
static void identical(const WCHAR *a, const WCHAR *b) {
    uint8_t *x = NULL, *y = NULL;
    size_t xs = 0, ys = 0;
    assert(read_range(a, 0, (size_t)-1, &x, &xs));
    assert(read_range(b, 0, (size_t)-1, &y, &ys));
    assert(xs == ys && !memcmp(x, y, xs));
    free(x); free(y);
}
static DWORD WINAPI dismiss_expected_error(void *unused) {
    DWORD deadline = GetTickCount() + 10000;
    (void)unused;
    do {
        HWND dialog = FindWindowW(L"#32770", L"SudekiMP Mods");
        if (dialog) { SendMessageW(dialog, WM_COMMAND, IDOK, 0); return 0; }
        Sleep(10);
    } while ((LONG)(deadline - GetTickCount()) > 0);
    assert(!"Expected save error was not shown");
    return 1;
}
static BOOL CALLBACK filename_edit(HWND window, LPARAM context) {
    WCHAR class_name[32];
    GetClassNameW(window, class_name, 32);
    if (!wcscmp(class_name, L"Edit")) {
        *(HWND *)context = window;
        return FALSE;
    }
    return TRUE;
}
static DWORD WINAPI choose_browse_file(void *context) {
    const WCHAR *path = context;
    DWORD deadline = GetTickCount() + 10000;
    do {
        HWND dialog = FindWindowW(L"#32770", L"Choose a replacement texture");
        if (dialog) {
            HWND edit = GetDlgItem(dialog, edt1);
            if (!edit) EnumChildWindows(dialog, filename_edit, (LPARAM)&edit);
            if (edit) {
                SetWindowTextW(edit, path);
                SendMessageW(dialog, WM_COMMAND, IDOK, 0);
                return 0;
            }
        }
        Sleep(10);
    } while ((LONG)(deadline - GetTickCount()) > 0);
    assert(!"Browse dialog did not expose its filename input");
    return 1;
}
typedef struct ZipBuffer { uint8_t *bytes; size_t size, capacity; } ZipBuffer;
static int zip_buffer_sink(void *context, const void *bytes, size_t size) {
    ZipBuffer *b = (ZipBuffer *)context;
    if (b->size + size > b->capacity) {
        size_t capacity = (b->size + size) * 2u + 64u;
        uint8_t *grown = (uint8_t *)realloc(b->bytes, capacity);
        if (!grown) return 0;
        b->bytes = grown; b->capacity = capacity;
    }
    memcpy(b->bytes + b->size, bytes, size); b->size += size; return 1;
}
int wmain(int argc, WCHAR **argv) {
    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_LISTVIEW_CLASSES };
    HINSTANCE instance = GetModuleHandleW(NULL);
    HWND parent, panel;
    Panel *p;
    WCHAR ini[PATH_CAP], candidate[PATH_CAP], exported[PATH_CAP], root[PATH_CAP], manifest[PATH_CAP], copied[PATH_CAP], temporary[PATH_CAP];
    uint8_t *bytes = NULL; size_t size = 0;
    SudekiMpModImage exported_image = {0};
    SudekiMpModManifest before = {0};
    char relative[240], old_relative[240], value[128];
    uint32_t key;
    size_t i;
    int texture = -1, model = -1;
    assert(argc == 2 || argc == 3);
    InitCommonControlsEx(&controls);
    assert(SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)));
    parent = CreateWindowW(L"STATIC", L"Synthetic Mods Test", WS_OVERLAPPEDWINDOW,
                           0, 0, 1120, 790, NULL, NULL, instance, NULL);
    assert(parent);
    panel = SudekiMpModsPanelCreate(parent, instance); assert(panel);
    MoveWindow(panel, 0, 0, 1100, 720, TRUE);
    p = (Panel *)GetWindowLongPtrW(panel, GWLP_USERDATA); assert(p);
    if (argc == 3) StringCchCopyW(ini, PATH_CAP, argv[2]);
    else join(ini, PATH_CAP, argv[1], L"SudekiMP.ini");
    SudekiMpModsPanelSetPaths(panel, argv[1], ini); pump(p);
    if (argc == 3) {
        int found = -1;
        assert(p->catalog && p->catalog->count);
        for (i = 0; i < p->catalog->count; ++i)
            if (!_stricmp(p->catalog->resources[i].entry.name, "CL002_Tal_New_FaceLR.SQX")) found = (int)i;
        assert(found >= 0);
        select_resource(p, found); pump(p);
        assert(p->original_image.rgba);
        printf("Read-only install scan: %u resources; Tal face %ux%u %s key0x%08X; preview decoded\n",
               (unsigned)p->catalog->count, p->original_image.info.width, p->original_image.info.height,
               SudekiMpModImageFormatName(p->original_image.info.d3d_format),
               p->catalog->resources[found].entry.texture_key);
        printf("Maximum main-thread dispatch batch: %lu ms\n", (unsigned long)max_dispatch_ms);
        SudekiMpModsPanelDestroy(panel); DestroyWindow(parent); CoUninitialize();
        return 0;
    }
    assert(p->catalog && p->catalog->count == 3);
    /* Thumbnails reach the catalogue (and the image list) for every texture. */
    for (i = 0; i < p->catalog->count; ++i)
        if (p->catalog->resources[i].entry.kind == SUDEKIMP_MOD_RESOURCE_TEXTURE)
            assert(p->catalog->resources[i].icon >= 0);
    assert(ImageList_GetImageCount(p->images) >= 2);
    for (i = 0; i < p->catalog->count; ++i) {
        Resource *r = &p->catalog->resources[i];
        if (strstr(r->entry.name, "Face")) texture = (int)i;
        if (r->entry.kind == SUDEKIMP_MOD_RESOURCE_MODEL) model = (int)i;
    }
    assert(texture >= 0 && model >= 0);
    /* The model's texture table links it to the face; its tile borrows that thumbnail. */
    assert(p->catalog->resources[model].preview == texture);
    assert(resource_icon(p->catalog, &p->catalog->resources[model]) == p->catalog->resources[texture].icon);
    key = p->catalog->resources[texture].entry.texture_key;
    assert(create_mod(p, L"My Mods")); refresh_mods(p, L"My Mods");
    assert(p->mod_count == 3 && p->selected_mod == 1);
    assert(SudekiMpModManifestGetValue(&p->manifest, "Mod", "Name", value, sizeof(value)) && !strcmp(value, "My Mods"));
    p->character_filter = p->catalog->resources[texture].character;
    p->category_filter = p->catalog->resources[texture].category;
    refresh_grid(p); assert(ListView_GetItemCount(p->grid) == 1);
    SetWindowTextW(p->search, L"nonexistent-search"); refresh_grid(p);
    assert(ListView_GetItemCount(p->grid) == 0);
    SetWindowTextW(p->search, L""); refresh_grid(p);
    select_resource(p, texture); pump(p);
    assert(p->original_image.rgba && p->original_image.info.width == 2 && p->original_image.info.height == 2);
    join(exported, PATH_CAP, argv[1], L"exported.png");
    assert(export_to_path(p, exported));
    assert(read_range(exported, 0, (size_t)-1, &bytes, &size));
    assert(SUCCEEDED(wic_decode(bytes, size, &exported_image)));
    assert(exported_image.info.width == 2 && exported_image.info.height == 2 && !memcmp(exported_image.rgba, p->original_image.rgba, 16));
    free(bytes); bytes = NULL; SudekiMpModImageFree(&exported_image);
    join(candidate, PATH_CAP, argv[1], L"replacement.png");
    {
        size_t chars = wcslen(candidate) + 2;
        HGLOBAL memory = GlobalAlloc(GHND, sizeof(DROPFILES) + chars * sizeof(WCHAR));
        DROPFILES *drop = (DROPFILES *)GlobalLock(memory);
        assert(drop); drop->pFiles = sizeof(DROPFILES); drop->fWide = TRUE;
        wcscpy((WCHAR *)((BYTE *)drop + sizeof(DROPFILES)), candidate);
        GlobalUnlock(memory);
        SendMessageW(p->drop, WM_DROPFILES, (WPARAM)memory, 0);
    }
    pump(p); assert(p->candidate_valid && IsWindowEnabled(p->apply));
    {
        HANDLE choose = CreateThread(NULL, 0, choose_browse_file, candidate, 0, NULL); assert(choose);
        browse_replacement(p);
        assert(WaitForSingleObject(choose, 15000) == WAIT_OBJECT_0); CloseHandle(choose);
        pump(p); assert(p->candidate_valid);
    }
    GetWindowTextW(p->check, root, PATH_CAP); assert(wcsstr(root, L"Size differs"));
    apply_resource(p); pump(p);
    assert(SudekiMpModManifestGetTexture(&p->manifest, key, relative, sizeof(relative)));
    assert(selected_mod_path(p, root, manifest));
    { WCHAR wide[240]; utf8_to_wide(relative, wide, 240); join(copied, PATH_CAP, root, wide); }
    identical(candidate, copied);
    strcpy(old_relative, relative);
    assert(load_manifest_at(manifest, &before));
    /* Force atomic manifest failure while preserving an existing replacement. */
    StringCchPrintfW(temporary, PATH_CAP, L"%ls.launcher-%lu.tmp", manifest, (unsigned long)GetCurrentProcessId());
    { HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL); assert(file != INVALID_HANDLE_VALUE); CloseHandle(file); }
    queue_candidate(p, candidate); pump(p);
    {
        HANDLE dismiss = CreateThread(NULL, 0, dismiss_expected_error, NULL, 0, NULL); assert(dismiss);
        apply_resource(p); assert(WaitForSingleObject(dismiss, 15000) == WAIT_OBJECT_0); CloseHandle(dismiss);
    }
    assert(SudekiMpModManifestGetTexture(&p->manifest, key, relative, sizeof(relative)) && !strcmp(relative, old_relative));
    { SudekiMpModManifest disk = {0}; assert(load_manifest_at(manifest, &disk)); assert(disk.length == before.length && !memcmp(disk.text, before.text, disk.length)); SudekiMpModManifestFree(&disk); }
    identical(candidate, copied); DeleteFileW(temporary); SudekiMpModManifestFree(&before);
    join(candidate, PATH_CAP, argv[1], L"bad.png"); queue_candidate(p, candidate); pump(p);
    assert(!p->candidate_valid && !IsWindowEnabled(p->apply));
    { const uint8_t bad_jpeg[] = {255,216,255,255,255,224}; uint32_t w = 0, h = 0; assert(!header_dimensions(bad_jpeg, sizeof(bad_jpeg), L".jpg", &w, &h)); }
    for (i = 0; i < 2; ++i) {
        join(candidate, PATH_CAP, argv[1], i ? L"replacement.jpg" : L"replacement.bmp");
        queue_candidate(p, candidate); pump(p); assert(p->candidate_valid);
    }
    revert_resource(p); pump(p); assert(!SudekiMpModManifestGetTexture(&p->manifest, key, relative, sizeof(relative)));
    SendMessageW(p->enabled, BM_CLICK, 0, 0);
    assert(SudekiMpModManifestGetValue(&p->manifest, "Mod", "Enabled", value, sizeof(value)) && !strcmp(value, "false"));
    select_resource(p, model); pump(p); assert(!IsWindowEnabled(p->apply));
    { WCHAR hint[512]; GetWindowTextW(p->drop, hint, 512); assert(wcsstr(hint, L"CL002_Tal_Test_FaceLR") && wcsstr(hint, L"Tal_Test_spec")); }
    assert(p->original.bitmap); /* the face texture previews the model */
    /* Archive data remains byte-identical; cancel/close also drains workers. */
    join(root, PATH_CAP, argv[1], L"Synthetic.baf"); join(copied, PATH_CAP, argv[1], L"original-archive.bin"); identical(root, copied);
    assert(create_mod(p, L"Größe")); refresh_mods(p, L"Größe");
    assert(selected_mod_path(p, root, manifest));
    assert(read_range(manifest, 0, (size_t)-1, &bytes, &size));
    assert(size >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe); free(bytes); bytes = NULL;
    GetPrivateProfileStringW(L"Mod", L"Name", L"", root, PATH_CAP, manifest); assert(!wcscmp(root, L"Größe"));
    /* Unsupported manifests are refused; external disabling stays visible. */
    assert(WritePrivateProfileStringW(L"Mod", L"Format", L"SudekiMP.Mod/2", manifest));
    refresh_mods(p, L"Größe"); assert(!p->manifest.text);
    assert(WritePrivateProfileStringW(L"Mod", L"Format", L"SudekiMP.Mod/1", manifest));
    assert(WritePrivateProfileStringW(L"Mod", L"Enabled", L"off", manifest));
    refresh_mods(p, L"Größe"); assert(p->manifest.text && SendMessageW(p->enabled, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    /* Import: TexMod .tpf, zipped mod folder, unsafe zip; load order; export. */
    {
        WCHAR package[PATH_CAP], mods[PATH_CAP], probe[PATH_CAP];
        SudekiMpModManifest imported = {0};
        size_t k; int at = -1;
        join(mods, PATH_CAP, argv[1], L"mods");
        join(package, PATH_CAP, argv[1], L"package.tpf"); import_path(p, package);
        join(probe, PATH_CAP, mods, L"package\\textures\\0x12345678.png");
        assert(GetFileAttributesW(probe) != INVALID_FILE_ATTRIBUTES);
        join(probe, PATH_CAP, mods, L"package\\mod.ini");
        assert(load_manifest_at(probe, &imported));
        assert(SudekiMpModManifestGetTexture(&imported, 0x12345678u, relative, sizeof(relative)) &&
               !strcmp(relative, "textures/0x12345678.png"));
        assert(SudekiMpModManifestGetValue(&imported, "Mod", "Author", value, sizeof(value)) && !strcmp(value, "Tester"));
        SudekiMpModManifestFree(&imported);
        join(package, PATH_CAP, argv[1], L"pack.zip"); import_path(p, package);
        join(probe, PATH_CAP, mods, L"Packed\\textures\\x.png");
        assert(GetFileAttributesW(probe) != INVALID_FILE_ATTRIBUTES);
        quiet_errors = 1;
        join(package, PATH_CAP, argv[1], L"evil.zip"); import_path(p, package);
        quiet_errors = 0;
        join(probe, PATH_CAP, mods, L"Evil");
        assert(GetFileAttributesW(probe) == INVALID_FILE_ATTRIBUTES); /* nothing written */
        join(probe, PATH_CAP, mods, L"escape.txt");
        assert(GetFileAttributesW(probe) == INVALID_FILE_ATTRIBUTES);
        join(probe, PATH_CAP, argv[1], L"escape.txt");
        assert(GetFileAttributesW(probe) == INVALID_FILE_ATTRIBUTES);
        /* Load order: move "package" one earlier; load-order.txt drives the list. */
        refresh_mods(p, L"package");
        for (k = 0; k < p->mod_count; ++k) if (!_wcsicmp(p->mod_list[k].folder, L"package")) at = (int)k;
        assert(at > 0 && p->selected_mod == at);
        move_selected_mod(p, -1);
        assert(!_wcsicmp(p->mod_list[at - 1].folder, L"package") && p->selected_mod == at - 1);
        join(probe, PATH_CAP, mods, L"load-order.txt");
        assert(GetFileAttributesW(probe) != INVALID_FILE_ATTRIBUTES);
        refresh_mods(p, NULL);
        assert(!_wcsicmp(p->mod_list[at - 1].folder, L"package"));
        /* Export tree -> zip -> read back. */
        {
            ZipBuffer out = {0};
            SudekiMpModZipWriter w;
            SudekiMpModZip zip;
            char error[128];
            join(probe, PATH_CAP, mods, L"package");
            SudekiMpModZipWriterBegin(&w, zip_buffer_sink, &out);
            assert(export_tree(&w, probe, "package", 0));
            assert(SudekiMpModZipWriterFinish(&w, "test"));
            assert(SudekiMpModZipRead(out.bytes, out.size, NULL, 0, 0, &zip, error, sizeof(error)));
            assert(SudekiMpModZipFind(&zip, "package/mod.ini") >= 0 &&
                   SudekiMpModZipFind(&zip, "package/textures/0x12345678.png") >= 0);
            SudekiMpModZipFree(&zip);
            free(out.bytes);
        }
    }
    /* No global toggle in the workshop: [Mods] Enable is only reported. */
    assert(WritePrivateProfileStringW(L"Mods", L"Enable", L"false", ini));
    SudekiMpModsPanelSetPaths(panel, L"", ini); SudekiMpModsPanelSetPaths(panel, argv[1], ini); pump(p);
    GetWindowTextW(p->order_label, root, PATH_CAP); assert(wcsstr(root, L"switched off"));
    scan_start(p); SudekiMpModsPanelDestroy(panel); DestroyWindow(parent); CoUninitialize();
    puts("LauncherModsUiTest: passed (scan/filter/WIC/Browse/drop/apply rollback/revert/Unicode/enable/model gate/import tpf+zip/unsafe zip/load order/export/teardown)");
    return 0;
}
