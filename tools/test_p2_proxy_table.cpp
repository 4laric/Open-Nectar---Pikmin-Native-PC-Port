// Standalone probe for the data-driven campaign proxy table (#871).
#include "pc_p2_proxy_table.h"

#include <cstdio>
#include <sstream>
#include <string>

namespace {

int failures = 0;

bool check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
    return ok;
}

bool allowAll(unsigned) { return true; }
bool reject17(unsigned id) { return id != 17; }

void testValid() {
    std::istringstream in(
        "P2_PROXY_CAMPAIGN_1 3\n"
        "2 Kochappy 3\n"
        "17 Bulborb 4\n"
        "10 Hana 0\n");
    const p2proxy::Table table = p2proxy::parse(in, allowAll, 36);
    check(table.valid, "valid table accepted");
    check(table.rows.size() == 3, "valid table has 3 rows");
    check(table.error.empty(), "valid table has empty error");
    const p2proxy::Row* bySource = p2proxy::bySource(table, 2);
    check(bySource != nullptr && bySource->species == "Kochappy" && bySource->host == 3,
          "bySource 2");
    const p2proxy::Row* bySpecies = p2proxy::bySpecies(table, std::string("Hana"));
    check(bySpecies != nullptr && bySpecies->source == 10 && bySpecies->host == 0,
          "bySpecies Hana");
    check(p2proxy::bySource(table, 99) == nullptr, "bySource unknown null");
    check(p2proxy::bySpecies(table, std::string("Missing")) == nullptr, "bySpecies unknown null");
    const p2proxy::Row* middle = p2proxy::bySource(table, 17);
    check(middle != nullptr && middle->species == "Bulborb" && middle->host == 4,
          "bySource 17");
}

void expectInvalid(const char* what, const std::string& text,
                   bool (*bindable)(unsigned) = allowAll) {
    std::istringstream in(text);
    const p2proxy::Table table = p2proxy::parse(in, bindable, 36);
    char message[256];
    std::snprintf(message, sizeof(message), "%s invalid", what);
    check(!table.valid, message);
    std::snprintf(message, sizeof(message), "%s empty rows", what);
    check(table.rows.empty(), message);
    std::snprintf(message, sizeof(message), "%s has error", what);
    check(!table.error.empty(), message);
}

void testViolations() {
    expectInvalid("bad header", "P2_BAD 1\n2 Foo 3\n");
    expectInvalid("count zero", "P2_PROXY_CAMPAIGN_1 0\n");
    expectInvalid("count too big", "P2_PROXY_CAMPAIGN_1 65\n");
    expectInvalid("fewer rows",
                  "P2_PROXY_CAMPAIGN_1 2\n"
                  "2 Foo 3\n");
    expectInvalid("more rows",
                  "P2_PROXY_CAMPAIGN_1 1\n"
                  "2 Foo 3\n"
                  "10 Bar 4\n");
    expectInvalid("trailing data",
                  "P2_PROXY_CAMPAIGN_1 1\n"
                  "2 Foo 3\n"
                  "extra\n");
    expectInvalid("host negative",
                  "P2_PROXY_CAMPAIGN_1 1\n"
                  "2 Foo -1\n");
    expectInvalid("host too big",
                  "P2_PROXY_CAMPAIGN_1 1\n"
                  "2 Foo 36\n");
    for (int host : {1, 5, 7, 10, 12, 13, 14, 21, 22, 23, 26, 27, 28, 29, 34, 35}) {
        const std::string text = "P2_PROXY_CAMPAIGN_1 1\n2 Foo " + std::to_string(host) + "\n";
        expectInvalid("unsafe host", text);
    }
    for (int host : {0, 2, 3, 4, 6, 8, 9, 11, 15, 16, 17, 18, 19, 20, 24, 25, 30, 31, 32, 33})
        check(p2proxy::safeHost(host), "safe host accepted");
    expectInvalid("species leading digit",
                  "P2_PROXY_CAMPAIGN_1 1\n"
                  "2 1Bad 3\n");
    expectInvalid("species hyphen",
                  "P2_PROXY_CAMPAIGN_1 1\n"
                  "2 Bad-Name 3\n");
    expectInvalid("species too long",
                  "P2_PROXY_CAMPAIGN_1 1\n"
                  "2 Abcdefghijklmnopqrstuvwxyz1234567 3\n");
    expectInvalid("species leading underscore",
                  "P2_PROXY_CAMPAIGN_1 1\n"
                  "2 _Foo 3\n");
    expectInvalid("duplicate source",
                  "P2_PROXY_CAMPAIGN_1 2\n"
                  "2 Foo 3\n"
                  "2 Bar 4\n");
    expectInvalid("duplicate species",
                  "P2_PROXY_CAMPAIGN_1 2\n"
                  "2 Foo 3\n"
                  "10 Foo 4\n");
    expectInvalid("rejected bindable", "P2_PROXY_CAMPAIGN_1 1\n17 Foo 3\n", reject17);
}

void testBindableCallback() {
    std::istringstream okIn(
        "P2_PROXY_CAMPAIGN_1 1\n"
        "2 Foo 3\n");
    check(p2proxy::parse(okIn, reject17, 36).valid, "bindable accepts 2");
    std::istringstream badIn(
        "P2_PROXY_CAMPAIGN_1 1\n"
        "17 Foo 3\n");
    const p2proxy::Table table = p2proxy::parse(badIn, reject17, 36);
    check(!table.valid && table.rows.empty(), "bindable rejects 17");
}

}  // namespace

int main() {
    testValid();
    testViolations();
    testBindableCallback();
    if (failures == 0) {
        std::printf("PASS p2_proxy_table\n");
        return 0;
    }
    std::fprintf(stderr, "FAIL p2_proxy_table: %d failures\n", failures);
    return 1;
}
