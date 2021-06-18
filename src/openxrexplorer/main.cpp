#include "imgui/imgui_skg.h"
#include "xrruntime.h"
#include "openxr_info.h"
#include "app_cli.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*** Global Variables ********************/

const char*   app_name        = "OpenXR Explorer";
xr_settings_t app_xr_settings = {};

runtime_t *runtimes      = nullptr;
int32_t    runtime_count = 0;

/*** Signatures **************************/

void app_window_openxr_functionality();
void app_window_runtime();
void app_window_view();
void app_element_table(const display_table_t *table);

void app_set_runtime   (int32_t runtime_index);
void app_open_link     (const char *link);
void app_open_spec     (const char *spec_item_name);

/*** Code ********************************/

bool app_args(int32_t arg_count, const char **args) {
	if (arg_count > 1) {
		app_cli(arg_count, args);
		return false;
	}

	return true;
}

///////////////////////////////////////////

bool app_init() {
	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;
	style->FrameRounding = 1;

	ImVec4 major    = ImVec4(0.471f, 0.169f, 0.565f, 1.00f);
	ImVec4 mid      = ImVec4(major.x*0.75f, major.y*0.75f, major.z*0.75f, 1);
	ImVec4 minor    = ImVec4(major.x*0.5f, major.y*0.5f, major.z*0.5f, 1);
	ImVec4 barely   = ImVec4(major.x*0.3f, major.y*0.3f, major.z*0.3f, 1);
	ImVec4 hover    = ImVec4(major.x*0.85f, major.y*0.85f, major.z*0.85f, 1);
	ImVec4 action   = major;

	colors[ImGuiCol_DockingPreview] = major;
	colors[ImGuiCol_Header] = colors[ImGuiCol_FrameBg] = colors[ImGuiCol_TabUnfocused] = minor;
	colors[ImGuiCol_Button]        = colors[ImGuiCol_ResizeGrip]        = colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_Tab] = mid;
	colors[ImGuiCol_ButtonActive ] = colors[ImGuiCol_ResizeGripActive]  = colors[ImGuiCol_HeaderActive]       = colors[ImGuiCol_TabActive]  = colors[ImGuiCol_FrameBgActive]  = action;
	colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_HeaderHovered]      = colors[ImGuiCol_TabHovered] = colors[ImGuiCol_FrameBgHovered] = hover;
	colors[ImGuiCol_TableRowBgAlt] = colors[ImGuiCol_TitleBgActive] = barely;
 
	load_runtimes("xr_runtimes.txt", &runtimes, &runtime_count);

	app_xr_settings.allow_session = true;
	app_xr_settings.form          = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	openxr_info_reload(app_xr_settings);
	
	return true;
}

///////////////////////////////////////////

void app_shutdown() {
	openxr_info_release();
}

///////////////////////////////////////////

void app_step(ImVec2 canvas_size) {
	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
		ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
		ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode); // Add empty node
		ImGui::DockBuilderSetNodeSize(dockspace_id, canvas_size);

		ImGuiID dock_id_left;
		ImGuiID dock_id_mid;
		ImGuiID dock_id_right;
		ImGuiID dock_id_left_bot;
		ImGui::DockBuilderSplitNode(dockspace_id,  ImGuiDir_Left, 0.33f, &dock_id_left,     &dock_id_right);
		ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Left, 0.5f,  &dock_id_mid,      &dock_id_right);
		ImGui::DockBuilderSplitNode(dock_id_left,  ImGuiDir_Down, 0.33f, &dock_id_left_bot, &dock_id_left);

		ImGui::DockBuilderDockWindow("Runtime Information", dock_id_left);
		ImGui::DockBuilderDockWindow("Misc Enumerations",   dock_id_left_bot);
		ImGui::DockBuilderDockWindow("Extensions & Layers", dock_id_mid);
		ImGui::DockBuilderDockWindow("View Configuration",  dock_id_right);
		ImGui::DockBuilderFinish(dockspace_id);
	}

	app_window_openxr_functionality();
	app_window_runtime();
	app_window_view();
	//ImGui::ShowDemoWindow();

	ImGui::Begin("Misc Enumerations");

	for (size_t i = 0; i < xr_tables.count; i++) {
		if (xr_tables[i].tag == display_tag_misc)
			app_element_table(&xr_tables[i]);
	}

	ImGui::End();
}

///////////////////////////////////////////

void app_window_runtime() {
	static int32_t current_runtime = -1;

	ImGui::Begin("Runtime Information");

	ImGui::Text("OpenXR Loader v%d.%d.%d", 
		(int32_t)XR_VERSION_MAJOR(XR_CURRENT_API_VERSION),
		(int32_t)XR_VERSION_MINOR(XR_CURRENT_API_VERSION),
		(int32_t)XR_VERSION_PATCH(XR_CURRENT_API_VERSION));

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Runtime picker
	if (ImGui::BeginCombo("##Change Runtime", current_runtime == -1 ? "Change Runtime" : runtimes[current_runtime].name)) {
		for (int n = 0; n < runtime_count; n++) {
			bool is_selected = (current_runtime == n);
			if (ImGui::Selectable(runtimes[n].name, is_selected) && current_runtime != n) {
				current_runtime = n;
				app_set_runtime(current_runtime);
			}
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload runtime data")) {
		openxr_info_reload(app_xr_settings);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Runtime list
	ImGui::Text("Runtime not here?");
	ImGui::SameLine();
	if (ImGui::Button("Edit runtime list"))
		app_open_link("xr_runtimes.txt");
	ImGui::SameLine();
	if (ImGui::Button("Reload list")) {
		free(runtimes);
		runtime_count = 0;
		load_runtimes("xr_runtimes.txt", &runtimes, &runtime_count);
		current_runtime = -1;
	}

	ImGui::Spacing();
	ImGui::Separator();

	if (xr_instance_err || xr_system_err || xr_session_err) {
		if (xr_instance_err) ImGui::Text("xrCreateInstance error: %s", xr_instance_err);
		if (xr_system_err  ) ImGui::Text("xrGetSystem error: %s", xr_system_err);
		if (xr_session_err ) ImGui::Text("xrCreateSession error: %s", xr_session_err);
		ImGui::Spacing();
		ImGui::Separator();
	}

	for (size_t i = 0; i < xr_tables.count; i++) {
		if (xr_tables[i].tag == display_tag_properties)
			app_element_table(&xr_tables[i]);
	}

	ImGui::End();
}

///////////////////////////////////////////

void app_window_openxr_functionality() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Extensions & Layers");

	for (size_t i = 0; i < xr_tables.count; i++) {
		if (xr_tables[i].tag == display_tag_features)
			app_element_table(&xr_tables[i]);
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

///////////////////////////////////////////

void app_window_view() {
	ImGui::Begin("View Configuration");

	const char *config_string = "N/A";
	if (app_xr_settings.view_config == (XrViewConfigurationType)0)
		app_xr_settings.view_config = xr_view.current_config;
	for (size_t i = 0; i < xr_view.available_configs.count; i++) {
		if (xr_view.available_configs[i] == app_xr_settings.view_config) {
			config_string = xr_view.available_config_names[i];
			break;
		}
	}

	if (ImGui::BeginCombo("Switch view config", config_string)) {
		for (int n = 0; n < xr_view.available_configs.count; n++) {
			bool is_selected = (app_xr_settings.view_config == xr_view.available_configs[n]);
			if (ImGui::Selectable(xr_view.available_config_names[n], is_selected)) {
				app_xr_settings.view_config = xr_view.available_configs[n];
				openxr_info_reload(app_xr_settings);
			}
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();
	ImGui::Separator();

	for (size_t i = 0; i < xr_tables.count; i++) {
		if (xr_tables[i].tag == display_tag_view)
			app_element_table(&xr_tables[i]);
	}
	ImGui::End();
}

///////////////////////////////////////////

void app_element_table(const display_table_t *table) {
	if (ImGui::TreeNodeEx(table->show_type ? table->name_type : table->name_func, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowItemOverlap)) {
		if (table->spec) {
			ImGui::SameLine(ImGui::GetContentRegionMax().x - (ImGui::CalcTextSize("Open Spec").x + GImGui->Style.FramePadding.x * 3));
			if (ImGui::Button("Open Spec")) {
				app_open_spec(table->spec);
			}
		}

		ImGui::Unindent(0);

		ImGuiTableFlags_ flags = ImGuiTableFlags_BordersV;
		if (table->column_count > 2) flags = (ImGuiTableFlags_)(flags | ImGuiTableFlags_RowBg);
		if (table->error) {
			if (ImGui::BeginTable(table->name_type, 1, flags)) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%s", table->error);

				ImGui::EndTable();
			}
		} else if (ImGui::BeginTable(table->name_type, table->column_count, flags)) {
			if (table->header_row) {
				for (size_t c = 0; c < table->column_count; c++)
					ImGui::TableSetupColumn(table->cols[c][0].text, table->cols[c][0].spec ? ImGuiTableColumnFlags_WidthFixed : 0, table->cols[c][0].spec ? ImGui::CalcTextSize(table->cols[c][0].spec).x+GImGui->Style.FramePadding.x*2 : 0);
				ImGui::TableHeadersRow();
			}

			for (size_t i = table->header_row?1:0; i < table->cols[0].count; i++) {
				ImGui::TableNextRow();
				for (size_t c = 0; c < table->column_count; c++) {
					ImGui::TableNextColumn(); 
					if (table->cols[c][i].spec) {
						ImGui::PushID(i);
						if (ImGui::Button("Spec"))
							app_open_spec(table->cols[c][i].spec);
						ImGui::PopID();
					} else {
						ImGui::Text("%s", table->cols[c][i].text);
					}
				}
			}
			ImGui::EndTable();
		}

		ImGui::Indent(0);

		ImGui::TreePop();
	} else if (table->spec) {
		ImGui::SameLine(ImGui::GetContentRegionMax().x - (ImGui::CalcTextSize("Open Spec").x + GImGui->Style.FramePadding.x * 3));
		if (ImGui::Button("Open Spec")) {
			app_open_spec(table->spec);
		}
	}
}

///////////////////////////////////////////

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
void app_set_runtime(int32_t runtime_index) {
	char command[1024];
	snprintf(command, sizeof(command), " -%s", runtimes[runtime_index].name);

	SHELLEXECUTEINFO info = {0};
	info.cbSize       = sizeof(SHELLEXECUTEINFO);
	info.fMask        = SEE_MASK_NOCLOSEPROCESS;
	info.hwnd         = nullptr;
	info.lpVerb       = "runas"; // ask for admin rights
	info.lpFile       = "xrsetruntime.exe";
	info.lpParameters = command;
	info.lpDirectory  = nullptr;
	info.nShow        = SW_SHOW;
	info.hInstApp     = nullptr; 
	if (!ShellExecuteEx(&info)) {
		printf("Failed to execute set-runtime command!\n");
	} else {
		WaitForSingleObject(info.hProcess, INFINITE);
		CloseHandle(info.hProcess);

		openxr_info_reload(app_xr_settings);
	}
}

#elif defined(__linux__)

void app_set_runtime(int32_t runtime_index) {
	char command[1024];
	snprintf(command, sizeof(command), "xrsetruntime -%s", runtimes[runtime_index].name);

	system(command);
}

#endif

///////////////////////////////////////////

void app_open_link(const char *link) {
#if defined(_WIN32)
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "start %s", link);
#elif defined(__linux__)
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "xdg-open %s", link);
#endif
	system(buffer);
}

///////////////////////////////////////////

void app_open_spec(const char *spec_item_name) {
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#%s", spec_item_name);
	app_open_link(buffer);
}