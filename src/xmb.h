#pragma once
#include "ui.h"
#include <string>
#include <vector>

namespace xmb {

extern int current_category_idx;
extern int current_item_idx;
extern float category_scroll_x, target_category_scroll_x;
extern float item_scroll_y, target_item_scroll_y;
extern ui::Marquee item_marquee;
extern float list_slide_x;
extern float list_slide_alpha;
extern std::vector<int> nav_stack;
extern std::string view_type;
extern void* view_data;

struct ContextMenuItem { std::string id; std::string label; };
struct ContextMenu {
    bool active = false;
    float alpha = 0;
    int selected_idx = 1;
    std::string title;
    std::vector<ContextMenuItem> items;
    std::string target_path;
};
extern ContextMenu context_menu;

extern bool help_overlay_active;
extern float help_overlay_alpha;

bool in_submenu();
void go_back();
void refresh_items();
void refresh_browser(const std::string& slide_dir = "");
bool navigate(const std::string& dir);
void update(float dt);
void keypressed(const std::string& key);

} // namespace xmb
