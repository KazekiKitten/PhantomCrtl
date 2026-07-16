#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

// PhantomCtrl overlay <-> sysmodule channel. Must match the sysmodule's
// phantom.h (PHANTOM_CONFIG_PATH). Kept as literals so the overlay has no
// build dependency on the sysmodule tree.
static constexpr const char *CONFIG_DIR  = "/config/phantomctrl";
static constexpr const char *CONFIG_PATH = "/config/phantomctrl/config.ini";

struct PhantomView {
    bool enabled       = true;
    bool suppress      = false;
    bool virt_attached = false;
    u64  virt_handle   = 0;
    u32  wireless_count = 0;
};

static bool parseBool(const char *v) {
    return v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y';
}

// Read the full state the sysmodule publishes.
static PhantomView loadState() {
    PhantomView pv;
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f)
        return pv;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if      (!strncmp(key, "enabled", 7))        pv.enabled = parseBool(val);
        else if (!strncmp(key, "suppress", 8))       pv.suppress = parseBool(val);
        else if (!strncmp(key, "virt_attached", 13)) pv.virt_attached = parseBool(val);
        else if (!strncmp(key, "virt_handle", 11))   pv.virt_handle = strtoull(val, nullptr, 16);
        else if (!strncmp(key, "wireless_count", 14)) pv.wireless_count = (u32)strtoul(val, nullptr, 10);
    }
    fclose(f);
    return pv;
}

// Write only the two writable knobs. The sysmodule owns status fields and
// re-publishes them on its next sync tick.
static void saveKnobs(bool enabled, bool suppress) {
    mkdir(CONFIG_DIR, 0777);
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f)
        return;
    fprintf(f, "enabled=%d\n",  enabled ? 1 : 0);
    fprintf(f, "suppress=%d\n", suppress ? 1 : 0);
    fclose(f);
}

class PhantomGui : public tsl::Gui {
public:
    PhantomGui() { m_state = loadState(); }

    virtual tsl::elm::Element* createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("PhantomCtrl", "v0.1.0");
        auto *list  = new tsl::elm::List();

        list->addItem(new tsl::elm::CategoryHeader("Control"));

        // Master enable toggle.
        auto *enableToggle = new tsl::elm::ToggleListItem("PhantomCtrl", m_state.enabled);
        enableToggle->setStateChangedListener([this](bool on) {
            m_state.enabled = on;
            saveKnobs(m_state.enabled, m_state.suppress);
        });
        list->addItem(enableToggle);

        // Handheld suppression toggle (the experimental lever).
        auto *suppressToggle = new tsl::elm::ToggleListItem("Suppress Handheld", m_state.suppress);
        suppressToggle->setStateChangedListener([this](bool on) {
            m_state.suppress = on;
            saveKnobs(m_state.enabled, m_state.suppress);
        });
        list->addItem(suppressToggle);

        list->addItem(new tsl::elm::CategoryHeader("Status"));

        m_statusVirt = new tsl::elm::ListItem("Virtual pad");
        list->addItem(m_statusVirt);

        m_statusHandle = new tsl::elm::ListItem("Virtual ID");
        list->addItem(m_statusHandle);

        m_statusWireless = new tsl::elm::ListItem("Wireless pads");
        list->addItem(m_statusWireless);

        refreshStatus();

        frame->setContent(list);
        return frame;
    }

    // Poll the sysmodule's published status each frame so the display is live.
    virtual void update() override {
        PhantomView fresh = loadState();
        // Keep the user's toggle intent; only status fields update here.
        fresh.enabled  = m_state.enabled;
        fresh.suppress = m_state.suppress;
        m_state = fresh;
        refreshStatus();
    }

private:
    void refreshStatus() {
        if (!m_statusVirt)
            return;
        m_statusVirt->setValue(m_state.virt_attached ? "attached" : "detached");

        char idbuf[24];
        if (m_state.virt_attached)
            snprintf(idbuf, sizeof(idbuf), "%016lx", m_state.virt_handle);
        else
            snprintf(idbuf, sizeof(idbuf), "-");
        m_statusHandle->setValue(idbuf);

        char wbuf[16];
        snprintf(wbuf, sizeof(wbuf), "%u", m_state.wireless_count);
        m_statusWireless->setValue(wbuf);
    }

    PhantomView m_state;
    tsl::elm::ListItem *m_statusVirt     = nullptr;
    tsl::elm::ListItem *m_statusHandle   = nullptr;
    tsl::elm::ListItem *m_statusWireless = nullptr;
};

class PhantomOverlay : public tsl::Overlay {
public:
    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<PhantomGui>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<PhantomOverlay>(argc, argv);
}
