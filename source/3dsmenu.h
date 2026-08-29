#ifndef _3DSMENU_H_
#define _3DSMENU_H_

#include <functional>
#include <string>
#include <vector>

#include "3dsui.h"
#include "3dsthemes.h"
#include "3dssettings.h"


#define MENU_PREFIX_FILE "  "
#define MENU_PREFIX_CHILD_DIRECTORY "  \x01 "
#define MENU_PREFIX_PARENT_DIRECTORY ""

#define MENU_HEIGHT             (14)

enum { TAB_EMULATOR, TAB_SETTINGS, TAB_DEPTH3D, TAB_CONTROLS, TAB_CHEATS, TAB_DIRTY_COUNT };

static_assert(TAB_DIRTY_COUNT == MENU_TAB_DIRTY_COUNT, "settings3DS.menuTabDirty is sized for a different number of tabs");

typedef struct 
{
    const char* label;
    const char* icon;
    uint32 color;
} MenuButton;

// currently used for save states
typedef enum
{
    RADIO_INACTIVE = 0,
    RADIO_INACTIVE_CHECKED = 1,
    RADIO_ACTIVE = 2,
	RADIO_ACTIVE_CHECKED = 3,
} RadioState;

enum class FileMenuOption {
    None,
    SetDefaultDir,
    ResetDefaultDir,
    RandomGame,
    RescanDir,
    DeleteGame
};

enum class MenuItemType {
    Disabled,
    Header1,
    Header2,
    Textarea, // for now this shouldn't be used when other menu items are following (menuStartY value has to be adjusted afterwards)
    Action,
    Checkbox,
    Radio,
    Gauge,
    Picker
};

class SMenuItem {
public:
    MenuItemType Type;

    std::string Text;

    std::string Description;

    int     Value;              
                                // Type = Gauge:
                                //   Value = Gauge Value
                                // Type = Checkbox:
                                //   0, unchecked
                                //   1, checked
                                // Type = Radio: (see enum RadioState)
                                //   0, unchecked and inactive
                                //   1, checked and inactive
                                //   2, unchecked and active
                                //   3, checked and active
                                // Type = Picker:
                                //   Selected ID of Picker

    // workaround: we also use GaugeMinValue to determine if a picker should show its selected option in the menu or not.
    int     GaugeMinValue;
    int     GaugeMaxValue;

    // Set on a row whose setting the game is not currently using. It is drawn
    // in the disabled colour but stays selectable, because what a mode
    // composites can change while the game runs.
    bool    Dimmed = false;

    // The depth slot this row edits, for the previews in the 3D depth tab.
    // -1 on every other row.
    int     PreviewSlot = -1;

    // All these fields are used if this is a picker.
    // (ID = 100000)
    //
    std::string PickerDescription;
    std::vector<SMenuItem> PickerItems;
    int     PickerDialogType;

protected:
    std::function<void(int)> ValueChangedCallback;

public:
    SMenuItem(
        std::function<void(int)> callback,
        MenuItemType type, const std::string& text, const std::string& description, int value = 0,
        int min = 0, int max = 0,
        const std::string& pickerDesc = std::string(), const std::vector<SMenuItem>& pickerItems = std::vector<SMenuItem>(), int pickerDialogType = 0
    ) : Type(type), Text(text), Description(description), Value(value),
        GaugeMinValue(min), GaugeMaxValue(max),
        PickerDescription(pickerDesc), PickerItems(pickerItems), PickerDialogType(pickerDialogType),
        ValueChangedCallback(callback) {}

    void SetValue(int value) {
        this->Value = value;
        if (this->ValueChangedCallback) {
            this->ValueChangedCallback(value);
        }
    }

    bool IsHighlightable() const {
        return !( Type == MenuItemType::Disabled || Type == MenuItemType::Header1 || Type == MenuItemType::Header2 || Type == MenuItemType::Textarea );
    }
};

class SMenuTab {
public:
    std::vector<SMenuItem> MenuItems;

    // Height of one row. A tab that shows more than text in a row -- the 3D
    // depth tab and its slot previews -- asks for taller ones, and fewer of
    // them fit on the screen as a result.
    int     RowHeight = FONT_HEIGHT;

    // How many rows fit on the screen from the current scroll position. Rows
    // are not all the same height -- only the ones carrying a picture take the
    // tab's height -- so this counts them rather than dividing.
    //
    // It counts rows, not items: past the end of the list a row is the ordinary
    // height. Stopping at the last item would answer "how many are left", and
    // callers that subtract a row for a subtitle would then drop the last one.
    int     VisibleItems() const {
        const int available = MENU_HEIGHT * FONT_HEIGHT;
        const int total = static_cast<int>(MenuItems.size());
        int used = 0;
        int count = 0;

        for (int i = FirstItemIndex < 0 ? 0 : FirstItemIndex; ; i++) {
            int height = i < total && MenuItems[i].PreviewSlot >= 0 ? RowHeight : FONT_HEIGHT;

            if (used + height > available)
                break;

            used += height;
            count++;
        }

        return count;
    }
    std::string SubTitle;
    std::string Title;
    std::string DialogText;
    int         FirstItemIndex;
    int         SelectedItemIndex;

    void SetTitle(const std::string& title) {
        // Left trim the dialog title
        size_t offs = title.find_first_not_of(' ');
        Title.assign(offs != title.npos ? title.substr(offs) : title);
    }

    void MakeSureSelectionIsOnScreen(int maxItems, int spacing) {
        int offs = spacing;
        // the visible item count must fit at least two spacings and one item in the middle for sensible scrolling logic
        if (offs * 2 + 1 >= maxItems) {
            offs = ( maxItems - 1 ) / 2;
        }
        if (SelectedItemIndex < FirstItemIndex + offs) {
            FirstItemIndex = SelectedItemIndex < offs ? 0 : ( SelectedItemIndex - offs );
        } else if (SelectedItemIndex >= FirstItemIndex + maxItems - offs) {
            int top = SelectedItemIndex - maxItems + 1;
            int itemsBelow = static_cast<int>(MenuItems.size()) - SelectedItemIndex - 1;
            FirstItemIndex = itemsBelow < offs ? ( top + itemsBelow ) : ( top + offs );
        }
        
        // FirstItemIndex should not be negative, otherwise it causes a missing item list on scroll
        // (happens e.g. when Load game tab has 12 menu items)
        if (FirstItemIndex < 0) {
            FirstItemIndex = 0;
        }
    }
};

// The file tab is always the last tab. Its index is dynamic.
// 1 with no ROM loaded (Emulator, Load Game), 4 in-game (Emulator, Settings, Controls, Cheats, Load Game).
inline bool menu3dsIsFileTab(int tabIndex, const std::vector<SMenuTab>& menuTabs) {
    return tabIndex == (static_cast<int>(menuTabs.size()) - 1);
}

void menu3dsAddTab(std::vector<SMenuTab>& menuTabs, const char *title, const std::vector<SMenuItem>& menuItems);

void menu3dsDrawEverything(SMenuTab& dialogTab, bool& isDialog, int& currentMenuTab, std::vector<SMenuTab>& menuTabs, int menuFrame = 0, int menuItemsFrame = 0, int dialogFrame = 0, bool animationFinished = true);
void menu3dsDrawEverything(int& currentMenuTab, std::vector<SMenuTab>& menuTabs);

// Previews of what each depth slot holds in the paused frame, shown beside the
// sliders in the 3D depth tab.
void menu3dsInvalidateSlotPreviews();
void menu3dsResetDepth3DFamily();
bool menu3dsDepth3DPreviewsApply();
void menu3dsDrawSlotPreview(int slot, int x, int y);
void menu3dsSwapBuffersAndWaitForVBlank();

int menu3dsMenuSelectItem(SMenuTab& dialogTab, bool& isDialog, int& currentMenuTab, std::vector<SMenuTab>& menuTabs);
void menu3dsHideMenu(SMenuTab& dialogTab, bool& isDialog, int& currentMenuTab, std::vector<SMenuTab>& menuTabs);

int menu3dsShowDialog(SMenuTab& dialogTab, bool& isDialog, int& currentMenuTab, std::vector<SMenuTab>& menuTabs, const std::string& title, const std::string& dialogText, int dialogBackColor, const std::vector<SMenuItem>& menuItems, int selectedID = -1, bool fadeIn = true, int textLines = -1);
void menu3dsShowRomLoadingDialog(SMenuTab& dialogTab, bool& isDialog, int& currentMenuTab, std::vector<SMenuTab>& menuTabs, const std::string& title, const std::string& text, int dialogColor, const char* romName = nullptr);
void menu3dsHideDialog(SMenuTab& dialogTab, bool& isDialog, int& currentMenuTab, std::vector<SMenuTab>& menuTabs, bool fadeOut = true);

int menu3dsGetLastSelectedTabIndex();
void menu3dsSetLastSelectedTabIndex(int index);
void menu3dsSelectRandomGameIndex(SMenuTab& currentTab, int min, int max, int lastSelected);

void menu3dsSetScreenDirty(bool gameScreen = true, bool secondScreen = false);

void menu3dsMarkTabDirty(int tab);
bool menu3dsHasDirtyTabs();

std::string menu3dsGetRomInfo();
void menu3dsSetHotkeysData(const char* hotkeysData[HOTKEYS_COUNT][3]);

void menu3dsSetCheatsCount(SMenuItem& item, int active, int total);

void menu3dsShowSplashMessage(const char *message);

#endif
