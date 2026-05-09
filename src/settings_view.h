#pragma once
#include <string>
#include <vector>

namespace settings_view {

extern bool active;
extern float alpha;
extern float scroll_y, target_scroll_y;
extern int selected_option_idx;

extern bool picker_active;
extern float picker_alpha;
struct PickerItem { std::string name; std::string path; };
extern std::vector<PickerItem> picker_items;
extern int picker_selected_idx;
extern std::string picker_current_path;
extern int picker_setting_idx;
extern float picker_scroll_y, picker_target_scroll_y;

void update(float dt, int setting_idx);
void draw_popup(int setting_idx);
void draw_folder_picker();

void open_folder_picker(const std::string& initial_path, int setting_idx);
void close_folder_picker();
void ensure_picker_visible();

bool keypressed(const std::string& key);

} // namespace settings_view
