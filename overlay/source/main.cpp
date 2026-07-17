#define TESLA_INIT_IMPL

#include <exception_wrap.hpp>
#include <tesla.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PHANTOM_CONFIG_PATH "/config/phantomctrl/config.ini"

struct PhantomState {
    bool enabled = true;
    bool virt_attached = false;
    u64 virt_handle = 0;
    u32 wireless_count = 0;
};

static PhantomState g_state;

static bool parse_bool(const char *v) {
    return v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y';
}

static bool loadPhantomState() {
    FILE *f = fopen(PHANTOM_CONFIG_PATH, "r");
    if (!f) return false;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if (strncmp(key, "enabled", 7) == 0)
            g_state.enabled = parse_bool(val);
        else if (strncmp(key, "virt_attached", 13) == 0)
            g_state.virt_attached = parse_bool(val);
        else if (strncmp(key, "virt_handle", 11) == 0) {
            char *end;
            g_state.virt_handle = strtoull(val, &end, 16);
        }
        else if (strncmp(key, "wireless_count", 14) == 0) {
            char *end;
            g_state.wireless_count = strtoul(val, &end, 10);
        }
    }
    fclose(f);
    return true;
}

static void savePhantomState() {
    FILE *f = fopen(PHANTOM_CONFIG_PATH, "w");
    if (!f) return;

    fprintf(f, "enabled=%d\n", g_state.enabled ? 1 : 0);
    fclose(f);
}

class PhantomCtrlMenu : public tsl::Gui {
private:
    tsl::elm::ListItem* m_toggleItem;
    tsl::elm::ListItem* m_statusItem;
    tsl::elm::ListItem* m_wirelessItem;

public:
    PhantomCtrlMenu() : m_toggleItem(nullptr), m_statusItem(nullptr), m_wirelessItem(nullptr) {}

    virtual tsl::elm::Element* createUI() override {
        auto *list = new tsl::elm::List();

        list->addItem(new tsl::elm::CategoryHeader("PhantomCtrl"));

        m_toggleItem = new tsl::elm::ListItem("Enabled");
        m_toggleItem->setValue(g_state.enabled ? "ON" : "OFF");
        m_toggleItem->setClickListener([this](u64 keys) {
            if (keys & KEY_A) {
                g_state.enabled = !g_state.enabled;
                savePhantomState();
                m_toggleItem->setValue(g_state.enabled ? "ON" : "OFF");
                return true;
            }
            return false;
        });
        list->addItem(m_toggleItem);

        m_statusItem = new tsl::elm::ListItem("Status");
        std::string status = (g_state.virt_attached) 
            ? ("Handle: 0x" + std::to_string(g_state.virt_handle)) 
            : "Detached";
        m_statusItem->setValue(status);
        list->addItem(m_statusItem);

        m_wirelessItem = new tsl::elm::ListItem("Wireless Pads");
        m_wirelessItem->setValue(std::to_string(g_state.wireless_count));
        list->addItem(m_wirelessItem);

        auto *rootFrame = new tsl::elm::HeaderOverlayFrame(97);
        rootFrame->setContent(list);

        return rootFrame;
    }

    virtual void update() override {
        bool changed = loadPhantomState();
        if (changed) {
            if (m_toggleItem)
                m_toggleItem->setValue(g_state.enabled ? "ON" : "OFF");
            if (m_statusItem) {
                std::string status = (g_state.virt_attached) 
                    ? ("Handle: 0x" + std::to_string(g_state.virt_handle)) 
                    : "Detached";
                m_statusItem->setValue(status);
            }
            if (m_wirelessItem)
                m_wirelessItem->setValue(std::to_string(g_state.wireless_count));
        }
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        if (keysDown & KEY_B) {
            tsl::goBack();
            return true;
        }
        return false;
    }
};

class PhantomCtrlOverlay : public tsl::Overlay {
public:
    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<PhantomCtrlMenu>();
    }

    virtual void onShow() override {}
    virtual void onHide() override {}
};

int main(int argc, char **argv) {
    return tsl::loop<PhantomCtrlOverlay>(argc, argv);
}