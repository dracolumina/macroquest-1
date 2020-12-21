/*
 * MacroQuest2: The extension platform for EverQuest
 * Copyright (C) 2002-2019 MacroQuest Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "pch.h"

#include "common/StringUtils.h"
#include "MQ2DeveloperTools.h"
#include "MQ2ImGuiTools.h"
#include "MQ2KeyBinds.h"

#include "imgui/ImGuiTreePanelWindow.h"
#include "imgui/ImGuiTextEditor.h"
#include <imgui/imgui_internal.h>

#include <fmt/format.h>

using namespace std::chrono_literals;

namespace mq {

static bool gbShowSettingsWindow = false;
imgui::ImGuiTreePanelWindow* gSettingsWindow = nullptr;

static void InitializeMQ2ImGuiTools();
static void ShutdownMQ2ImGuiTools();
static void PulseMQ2ImGuiTools();
static void UpdateOverlayUI();

static DWORD WriteChatColorImGuiAPI(const char* line, DWORD color, DWORD filter);

static MQModule gImGuiModule = {
	"ImGuiAPI",                   // Name
	false,                        // CanUnload
	InitializeMQ2ImGuiTools,      // Initialize
	ShutdownMQ2ImGuiTools,        // Shutdown
	PulseMQ2ImGuiTools,           // Pulse
	nullptr,                      // SetGameState
	UpdateOverlayUI,              // UpdateImGui
	nullptr,                      // Zoned
	WriteChatColorImGuiAPI,       // WriteChatColor
};
MQModule* GetImGuiToolsModule() { return &gImGuiModule; }

//============================================================================

void AddSettingsPanel(const char* name, fPanelDrawFunction drawFunction)
{
	if (gSettingsWindow)
		gSettingsWindow->AddPanel(name, drawFunction);
}

void RemoveSettingsPanel(const char* name)
{
	if (gSettingsWindow)
		gSettingsWindow->RemovePanel(name);
}

void ImGuiTools_DrawWindowsMenu()
{
	ImGui::MenuItem("Settings", nullptr, &gbShowSettingsWindow);
}

//----------------------------------------------------------------------------

void ToggleImGuiOverlay()
{
	SetOverlayEnabled(!IsOverlayEnabled());
}

static void DoToggleImGuiOverlay(const char* name, bool down)
{
	if (down)
	{
		ToggleImGuiOverlay();
	}
}

void UpdateOverlayUI()
{
	if (gbShowSettingsWindow)
	{
		if (gSettingsWindow)
			gSettingsWindow->Draw(&gbShowSettingsWindow);
	}

	UpdateImGuiConsole();
}

//============================================================================
// KeyBinds Settings
//============================================================================

static ImVec4 GetKeyBindColor(bool empty, bool hovered)
{
	if (hovered)
		return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
	if (empty)
		return ImVec4(1.0f, 1.0f, 1.0f, 0.5f);

	return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

static bool sEditKeyBind = false;
static std::string sEditKeyBindName;
static bool sEditKeyBindAlt = false;
static bool sEditKeyBindEQ = false;
static char sEditKeyCombo[64];

void DoEditKeyCombo(const std::string& name, const std::string& keyCombo, bool alt, bool eq)
{
	sEditKeyBindName = name;
	sEditKeyBindAlt = alt;
	sEditKeyBindEQ = eq;
	strcpy_s(sEditKeyCombo, keyCombo.c_str());

	ImGui::OpenPopup("Edit Key Binding");
}

int DoEditKeyComboTextEditCallback(ImGuiInputTextCallbackData* data)
{
	return 1;
}

void DoEditKeyComboPopup()
{
	bool unused_open = true;
	if (ImGui::BeginPopupModal("Edit Key Binding", &unused_open))
	{
		ImGui::Text("Editing %s key binding for \"%s\"",
			sEditKeyBindAlt ? "alt" : "normal", sEditKeyBindName.c_str());

		ImGui::InputText("", sEditKeyCombo, sizeof(sEditKeyCombo), 0, DoEditKeyComboTextEditCallback);

		eqlib::KeyCombo keyCombo;
		bool valid = ParseKeyCombo(sEditKeyCombo, keyCombo);

		if (!valid)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		if (ImGui::Button("Save", ImVec2(80, 0)))
		{
			if (sEditKeyBindEQ)
				SetEQKeyBind(sEditKeyBindName.c_str(), sEditKeyBindAlt, keyCombo);
			else
				SetMQ2KeyBind(sEditKeyBindName.c_str(), sEditKeyBindAlt, keyCombo);

			ImGui::CloseCurrentPopup();
		}

		if (!valid)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void DoKeybindSettings()
{
	ImGui::Text("Clicking a binding will allow you change it.");

	char label[64];
	sprintf_s(label, "MacroQuest Key Bindings (%d)", GetKeyBindsCount());

	std::string clickedName;
	bool clickedAlt = false;
	bool clickedEQ = false;
	std::string clickedCombo;
	bool clicked = false;

	if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		static int sHoveredIndex = -1;
		static bool sHoveredAlt = false;
		bool isAnyHovered = false;

		if (ImGui::BeginTable("##MQKeybindTable", 3, ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Bind", ImGuiTableColumnFlags_WidthFixed, .20f);
			ImGui::TableSetupColumn("Alt", ImGuiTableColumnFlags_WidthFixed, .20f);
			ImGui::TableHeadersRow();

			bool hovered = false;

			EnumerateKeyBinds(
				[&](const mq::MQKeyBind& keyBind)
			{
				char keyComboDesc[64];

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text(keyBind.Name.c_str());

				ImGui::TableNextColumn();

				DescribeKeyCombo(keyBind.Normal, keyComboDesc, sizeof(keyComboDesc));
				hovered = (sHoveredIndex == keyBind.Id && sHoveredAlt == false);
				ImGui::TextColored(GetKeyBindColor(ci_equals("clear", keyComboDesc), hovered), "%s", keyComboDesc);
				if (ImGui::IsItemHovered()) { sHoveredAlt = false; sHoveredIndex = keyBind.Id; isAnyHovered = true; }
				if (ImGui::IsItemClicked()) { clickedName = keyBind.Name; clickedAlt = false; clickedEQ = false; clickedCombo = keyComboDesc; clicked = true; }

				ImGui::TableNextColumn();
				DescribeKeyCombo(keyBind.Alt, keyComboDesc, sizeof(keyComboDesc));
				hovered = (sHoveredIndex == keyBind.Id && sHoveredAlt == true);
				ImGui::TextColored(GetKeyBindColor(ci_equals("clear", keyComboDesc), hovered), "%s", keyComboDesc);
				if (ImGui::IsItemHovered()) { sHoveredAlt = true; sHoveredIndex = keyBind.Id; isAnyHovered = true; }
				if (ImGui::IsItemClicked()) { clickedName = keyBind.Name; clickedAlt = true; clickedEQ = false; clickedCombo = keyComboDesc; clicked = true; }
			});
		}

		if (!isAnyHovered)
		{
			sHoveredIndex = -1;
			sHoveredAlt = false;
		}

		ImGui::EndTable();
	}

	// Count the keybinds.
	int eqCount = 0;
	for (auto& szEQMappableCommand : szEQMappableCommands)
	{
		if (szEQMappableCommand == nullptr)
			continue;

		eqCount++;
	}

	sprintf_s(label, "EverQuest Key Bindings (%d)", eqCount);

	if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		static int sHoveredIndex = -1;
		static bool sHoveredAlt = false;
		bool isAnyHovered = false;

		if (ImGui::BeginTable("##EQKeybindTable", 3, ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, .60f);
			ImGui::TableSetupColumn("Bind", ImGuiTableColumnFlags_WidthFixed, .20f);
			ImGui::TableSetupColumn("Alt", ImGuiTableColumnFlags_WidthFixed, .20f);
			ImGui::TableHeadersRow();

			bool hovered = false;

			for (int i = 0; i < nEQMappableCommands; ++i)
			{
				if (szEQMappableCommands[i] == nullptr)
					continue;

				char keyComboDesc[64];

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text(szEQMappableCommands[i]);

				ImGui::TableNextColumn();

				DescribeKeyCombo(pKeypressHandler->NormalKey[i], keyComboDesc, sizeof(keyComboDesc));
				hovered = (sHoveredIndex == i && sHoveredAlt == false);
				ImGui::TextColored(GetKeyBindColor(ci_equals("clear", keyComboDesc), hovered), "%s", keyComboDesc);
				if (ImGui::IsItemHovered()) { sHoveredAlt = false; sHoveredIndex = i; isAnyHovered = true; }
				if (ImGui::IsItemClicked()) { clickedName = szEQMappableCommands[i]; clickedAlt = false; clickedEQ = true; clickedCombo = keyComboDesc; clicked = true; }

				ImGui::TableNextColumn();
				DescribeKeyCombo(pKeypressHandler->AltKey[i], keyComboDesc, sizeof(keyComboDesc));
				hovered = (sHoveredIndex == i && sHoveredAlt == true);
				ImGui::TextColored(GetKeyBindColor(ci_equals("clear", keyComboDesc), hovered), "%s", keyComboDesc);
				if (ImGui::IsItemHovered()) { sHoveredAlt = true; sHoveredIndex = i; isAnyHovered = true; }
				if (ImGui::IsItemClicked()) { clickedName = szEQMappableCommands[i]; clickedAlt = true; clickedEQ = true; clickedCombo = keyComboDesc; clicked = true; }
			}
		}

		if (!isAnyHovered)
		{
			sHoveredIndex = -1;
			sHoveredAlt = false;
		}

		ImGui::EndTable();
	}

	if (clicked)
	{
		DoEditKeyCombo(clickedName, clickedCombo, clickedAlt, clickedEQ);
	}

	DoEditKeyComboPopup();
}

//============================================================================

static void InitializeMQ2ImGuiTools()
{
	// Add keybind to toggle imgui
	AddMQ2KeyBind("TOGGLE_IMGUI_OVERLAY", DoToggleImGuiOverlay);

	AddCascadeMenuItem("Settings", []() { gbShowSettingsWindow = true; }, 2);

	gSettingsWindow = new imgui::ImGuiTreePanelWindow("MacroQuest Settings");

	AddSettingsPanel("Key Bindings", DoKeybindSettings);

	InitializeImGuiConsole();
}

static void ShutdownMQ2ImGuiTools()
{
	ShutdownImGuiConsole();

	delete gSettingsWindow;
	gSettingsWindow = nullptr;

	RemoveMQ2KeyBind("TOGGLE_IMGUI_OVERLAY");
}

static void PulseMQ2ImGuiTools()
{
}

static DWORD WriteChatColorImGuiAPI(const char* line, DWORD color, DWORD filter)
{
	return ImGuiConsoleAddText(line, color, filter);
}

} // namespace mq