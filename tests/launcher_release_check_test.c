/* Release-check parsing for the launcher's update prompt (no network). */
#include "release_check.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PAGE "https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/releases/tag/v0.7.0"
#define ZIP "https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/releases/download/v0.7.0/sudekimp-windows-launcher-0.7.0.zip"

static int parse(const char *json, SudekiMpRelease *r) { return SudekiMpReleaseParse(json, strlen(json), r); }

int main(void) {
    SudekiMpRelease r;
    /* Shape of Gitea's /releases/latest: nested objects carry their own
     * "html_url"/"name" keys, which must not shadow the release's. */
    const char *good =
        "{\"id\":58,\"author\":{\"id\":1,\"name\":\"wander\",\"html_url\":\"https://git.unfilteredrealm.com/wander\"},"
        "\"tag_name\":\"v0.7.0\",\"target_commitish\":\"abc\",\"name\":\"SudekiMP launcher 0.7.0 \\\"quoted\\\"\","
        "\"body\":\"Notes with } and ] and {\\\"tag_name\\\":\\\"v9.9.9\\\"} inside\",\"html_url\":\"" PAGE "\","
        "\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-10-09T10:00:00Z\","
        "\"assets\":[{\"id\":1,\"name\":\"SHA256SUMS\",\"browser_download_url\":\"https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/releases/download/v0.7.0/SHA256SUMS\"},"
        "{\"id\":2,\"name\":\"sudekimp-windows-launcher-0.7.0.zip\",\"size\":7381722,\"browser_download_url\":\"" ZIP "\"}]}";
    assert(parse(good, &r));
    assert(!strcmp(r.tag, "v0.7.0") && !strcmp(r.version, "0.7.0"));
    assert(!strcmp(r.title, "SudekiMP launcher 0.7.0 \"quoted\""));
    assert(!strcmp(r.page_url, PAGE) && !strcmp(r.windows_zip_url, ZIP));

    /* Drafts, pre-releases, missing flags, foreign pages and bad tags are refused. */
    const char *draft = "{\"tag_name\":\"v0.7.0\",\"html_url\":\"" PAGE "\",\"draft\":true,\"prerelease\":false}";
    const char *pre = "{\"tag_name\":\"v0.7.0\",\"html_url\":\"" PAGE "\",\"draft\":false,\"prerelease\":true}";
    const char *noflags = "{\"tag_name\":\"v0.7.0\",\"html_url\":\"" PAGE "\"}";
    const char *foreign = "{\"tag_name\":\"v0.7.0\",\"html_url\":\"https://example.com/releases/tag/v0.7.0\",\"draft\":false,\"prerelease\":false}";
    const char *badtag = "{\"tag_name\":\"latest\",\"html_url\":\"" PAGE "\",\"draft\":false,\"prerelease\":false}";
    const char *html = "<html><body>Anubis check</body></html>";
    const char *truncated = "{\"tag_name\":\"v0.7.0\",\"html_url\":\"" PAGE "\",\"draft\":false,\"prerelease\":fal";
    assert(!parse(draft, &r) && !parse(pre, &r) && !parse(noflags, &r) && !parse(foreign, &r));
    assert(!parse(badtag, &r) && !parse(html, &r) && !parse(truncated, &r) && !parse("", &r));

    /* Untitled release falls back to its tag; no Windows asset is fine. */
    const char *untitled = "{\"tag_name\":\"0.8.1\",\"name\":\"\",\"html_url\":\"" PAGE "\",\"draft\":false,\"prerelease\":false,\"assets\":[]}";
    assert(parse(untitled, &r) && !strcmp(r.title, "0.8.1") && !strcmp(r.version, "0.8.1") && !r.windows_zip_url[0]);

    /* Numeric, not lexical, version order. */
    assert(SudekiMpVersionCompare("0.6.0", "0.7.0") < 0);
    assert(SudekiMpVersionCompare("0.10.0", "0.9.1") > 0);
    assert(SudekiMpVersionCompare("v0.6.0", "0.6.0") == 0);
    assert(SudekiMpVersionCompare("0.6", "0.6.1") < 0);
    assert(SudekiMpVersionCompare("1.0.0", "0.99.99") > 0);
    assert(SudekiMpVersionCompare("0.6.0", "garbage") == 0); /* malformed: never prompt */
    puts("LauncherReleaseCheckTest: passed (latest-release JSON, nested keys, refusals, version order)");
    return 0;
}
