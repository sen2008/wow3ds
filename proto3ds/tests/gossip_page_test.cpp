// PC check of the gossip window's pages: a menu with more rows than fit above Goodbye (custom servers' long menus)
// keeps every option reachable - options and quests on later pages, a page control, page 1 again for a new menu,
// and a tap sends the option's own index.
//   run_gossip.sh
#include <cstdio>
#include <string>
#include <vector>

#include "game_ui.hpp"
#include "wire.hpp"

void uiWriteFileAsync(const char* path, const std::string& data) {
    if (FILE* f = fopen(path, "wb")) { fwrite(data.data(), 1, data.size(), f); fclose(f); }
}

using namespace w3n;
static int fails = 0, checks = 0;
#define CHECK(c, ...) do { ++checks; if (!(c)) { ++fails; std::printf("FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Sent { uint16_t op; Bytes data; };

struct Rig {
    GameSession s;
    GameUi ui;
    PadConfig cfg;
    ActionLayout bars;
    std::vector<Sent> sent;
    float pos[3] = {0, 0, 0};
    GameUi::Input in{};
    uint32_t now = 1000;
    Rig() {
        s.selfName = "Testone";
        s.tap = [this](uint32_t, int d, uint16_t op, const Bytes& b) { if (d == 1) sent.push_back({op, b}); };
        in = GameUi::Input{0, false, 0, 0, pos, 0, now, "Testone", nullptr, &cfg, &bars};
    }
    // SMSG_GOSSIP_MESSAGE (0x17D) with 'n' options and 'q' quests, then the NPC's text (SMSG_NPC_TEXT_UPDATE 0x180)
    void menu(uint64_t npc, uint32_t textId, int n, int q, const std::string& text) {
        Writer w;
        w.u64(npc); w.u32(textId); w.u32(n);
        for (int i = 0; i < n; ++i) { w.u32(100 + i); w.u8(0); w.u8(0); w.cstr("Option " + std::to_string(i)); }
        w.u32(q);
        for (int i = 0; i < q; ++i) { w.u32(500 + i); w.u32(2); w.u32(5); w.cstr("Quest " + std::to_string(i)); }
        s.handle(0x17D, w.b, now);
        if (textId) {
            Writer t;
            t.u32(textId);
            for (int i = 0; i < 8; ++i) {
                t.f32(i == 0 ? 1.0f : 0.0f); t.cstr(i == 0 ? text : ""); t.cstr("");
                t.u32(0); for (int k = 0; k < 6; ++k) t.u32(0);
            }
            s.handle(0x180, t.b, now);
        }
        sent.clear();
        frame();
    }
    GameUi::Output frame(const GameUi::Input* t = nullptr) {
        now += 300;
        GameUi::Input i = t ? *t : in;
        i.nowMs = now;
        GameUi::Output o = ui.update(s, i, true);
        ui.draw(s, i, 30.0f, 85);
        return o;
    }
    int count(GameUi::Act a) const { int c = 0; for (const GameUi::Button& b : ui.buttons()) c += b.act == a; return c; }
    bool has(GameUi::Act a, int arg) const { for (const GameUi::Button& b : ui.buttons()) if (b.act == a && b.arg == arg) return true; return false; }
    bool tap(GameUi::Act act, int arg = -1) {
        frame();
        for (const GameUi::Button& b : ui.buttons()) {
            if (b.act != act || (arg >= 0 && b.arg != arg)) continue;
            GameUi::Input t = in;
            t.down = KEY_TOUCH; t.touched = true; t.touchHeld = true; t.touchX = b.cx(); t.touchY = b.cy();
            frame(&t);
            t.down = 0; t.touched = false; t.touchHeld = false; t.touchUp = true;
            frame(&t);
            frame();
            return true;
        }
        return false;
    }
    uint32_t lastSelected() const {   // the option index of the last CMSG_GOSSIP_SELECT_OPTION (0x17C)
        for (size_t i = sent.size(); i-- > 0;)
            if (sent[i].op == 0x17C && sent[i].data.size() >= 12)
                return sent[i].data[8] | sent[i].data[9] << 8 | sent[i].data[10] << 16 | (uint32_t)sent[i].data[11] << 24;
        return ~0u;
    }
};

int main() {
    const std::string longText = "Review this hire $B$B Check the character, role, equipment tier, slots and price below. Gold is "
                                 "spent only when you choose Confirm hire.";
    {   // a short menu: as before, no page control
        Rig r;
        r.menu(0xF1300000000000AAull, 0, 3, 0, "");
        CHECK(r.s.gossip.open, "menu open");
        CHECK(r.count(GameUi::Act::GossipOption) == 3, "3 options shown (%d)", r.count(GameUi::Act::GossipOption));
        CHECK(r.count(GameUi::Act::PageNext) == 0 && r.count(GameUi::Act::PagePrev) == 0, "no pages for a menu that fits");
    }
    {   // the hire page: long text, 6 options, 2 quests
        Rig r;
        r.menu(0xF1300000000000BBull, 7001, 6, 2, longText);
        CHECK(!r.s.gossip.text.empty(), "NPC text arrived");
        const int perPage = r.count(GameUi::Act::GossipOption);
        CHECK(perPage >= 1 && perPage < 6, "first page shows part of the options (%d)", perPage);
        CHECK(r.has(GameUi::Act::PageNext, 0), "page control shown");
        // walk every page: each option and quest exactly once, in order
        std::vector<int> opts, quests;
        for (int page = 0; page < 10; ++page) {
            for (const GameUi::Button& b : r.ui.buttons()) {
                if (b.act == GameUi::Act::GossipOption) opts.push_back(b.arg);
                if (b.act == GameUi::Act::GossipQuest) quests.push_back(b.arg);
            }
            const size_t before = opts.size() + quests.size();
            r.tap(GameUi::Act::PageNext);
            size_t shown = 0;
            for (const GameUi::Button& b : r.ui.buttons()) shown += b.act == GameUi::Act::GossipOption || b.act == GameUi::Act::GossipQuest;
            if (opts.size() + quests.size() >= 8 || (shown && before == 8)) break;
        }
        bool inOrder = opts.size() == 6 && quests.size() == 2;
        for (size_t i = 0; inOrder && i < opts.size(); ++i) inOrder = opts[i] == (int)i;
        for (size_t i = 0; inOrder && i < quests.size(); ++i) inOrder = quests[i] == (int)i;
        CHECK(inOrder, "every option and quest reachable once, in order (%zu options, %zu quests)", opts.size(), quests.size());
        // the last page: ▶ does not go past it
        r.tap(GameUi::Act::PageNext);
        CHECK(r.has(GameUi::Act::GossipQuest, 1), "the last page stays (quest 1 still shown)");
        // tap the last option on its page: the server's index for it goes out
        for (int i = 0; i < 5 && !r.has(GameUi::Act::GossipOption, 5); ++i) r.tap(GameUi::Act::PagePrev);
        CHECK(r.tap(GameUi::Act::GossipOption, 5), "option 5 tappable");
        CHECK(r.lastSelected() == 105, "option 5 sends its index 105 (%u)", r.lastSelected());
        // regions stay above Goodbye and on screen
        bool inside = true;
        for (const GameUi::Button& b : r.ui.buttons())
            if (b.act == GameUi::Act::GossipOption || b.act == GameUi::Act::GossipQuest) inside = inside && b.y + b.h <= 196 - 28;   // GameUi::kTabY - 28: the top of Goodbye
        CHECK(inside, "rows end above the Goodbye button");
        // a new menu (the server's answer to an option) starts on page 1
        r.tap(GameUi::Act::PageNext);
        r.menu(0xF1300000000000BBull, 7002, 6, 0, longText);
        CHECK(r.has(GameUi::Act::GossipOption, 0), "a new menu starts on page 1");
        // the same menu sent again (same text id) also starts on page 1
        r.tap(GameUi::Act::PageNext);
        r.menu(0xF1300000000000BBull, 7002, 6, 0, longText);
        CHECK(r.has(GameUi::Act::GossipOption, 0), "the same menu sent again starts on page 1");
        // a menu that shrinks while on a later page: the page is clamped, rows still shown
        r.tap(GameUi::Act::PageNext);
        r.s.gossip.options.resize(2);
        r.frame();
        CHECK(r.count(GameUi::Act::GossipOption) == 2, "a shrunk menu clamps the page (%d shown)", r.count(GameUi::Act::GossipOption));
    }
    {   // no text: 7 options, more fit per page than under the long text
        Rig r;
        r.menu(0xF1300000000000CCull, 0, 7, 0, "");
        const int noText = r.count(GameUi::Act::GossipOption);
        Rig t;
        t.menu(0xF1300000000000CCull, 7003, 7, 0, longText);
        CHECK(noText > t.count(GameUi::Act::GossipOption), "more rows fit without text (%d vs %d)", noText, t.count(GameUi::Act::GossipOption));
    }
    std::printf("GOSSIP PAGES: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
