#include "ui/conditions/ValueEditors.h"

#include "StringUtils.h"
#include "conditions/ParamEnumOptions.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/Localization.h"
#include "ui/components/EditableCombo.h"
#include "ui/conditions/FunctionRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <sstream>

namespace {
using ValueEditorKind = sosr::ui::condition_editor::ValueEditorKind;

std::string FormatNumberStringImpl(const double a_value) {
  std::ostringstream stream;
  stream.setf(std::ios::fixed, std::ios::floatfield);
  stream << std::setprecision(3) << a_value;
  auto text = stream.str();
  const auto dotIndex = text.find('.');
  if (dotIndex != std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  return text.empty() ? "0" : text;
}

ValueEditorKind
GetEditorKindForParamTypeImpl(const RE::SCRIPT_PARAM_TYPE a_type) {
  if (sosr::conditions::HasParamEnumOptions(a_type) ||
      sosr::conditions::HasParamTextOptions(a_type)) {
    return ValueEditorKind::CachedOption;
  }

  switch (a_type) {
  case RE::SCRIPT_PARAM_TYPE::kChar:
  case RE::SCRIPT_PARAM_TYPE::kInt:
  case RE::SCRIPT_PARAM_TYPE::kStage:
  case RE::SCRIPT_PARAM_TYPE::kRelationshipRank:
  case RE::SCRIPT_PARAM_TYPE::kCrimeType:
  case RE::SCRIPT_PARAM_TYPE::kAlignment:
  case RE::SCRIPT_PARAM_TYPE::kEquipType:
  case RE::SCRIPT_PARAM_TYPE::kSkillAction:
    return ValueEditorKind::Integer;
  case RE::SCRIPT_PARAM_TYPE::kFloat:
    return ValueEditorKind::Number;
  default:
    return sosr::ui::conditions::ConditionParamOptionCache::Supports(a_type)
               ? ValueEditorKind::CachedOption
               : ValueEditorKind::Unsupported;
  }
}

bool DrawNumericClauseValueEditorImpl(const char *a_id, std::string &a_value,
                                      const ValueEditorKind a_kind,
                                      const float a_width) {
  if (a_kind == ValueEditorKind::Unsupported ||
      a_kind == ValueEditorKind::CachedOption) {
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(a_width);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%s",
                  sosr::ui::Localization::GetSingleton()->GetCStr(
                      "conditions.unsupported"));
    ImGui::InputText(a_id, buffer, sizeof(buffer),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    return false;
  }

  ImGui::SetNextItemWidth(a_width);

  if (a_kind == ValueEditorKind::Integer) {
    int numericValue = 0;
    if (!a_value.empty()) {
      try {
        numericValue = std::stoi(a_value);
      } catch (const std::exception &) {
      }
    }

    if (ImGui::InputInt(a_id, &numericValue, 0, 0)) {
      a_value = std::to_string(numericValue);
      return true;
    }
    return false;
  }

  if (a_kind == ValueEditorKind::Number) {
    double numericValue = 0.0;
    if (!a_value.empty()) {
      try {
        numericValue = std::stod(a_value);
      } catch (const std::exception &) {
      }
    }

    if (ImGui::InputDouble(a_id, &numericValue, 0.0, 0.0, "%.3f")) {
      a_value = FormatNumberStringImpl(numericValue);
      return true;
    }
    return false;
  }

  return false;
}
} // namespace

namespace sosr::ui::condition_editor {
ValueEditorKind GetEditorKindForParamType(const RE::SCRIPT_PARAM_TYPE a_type) {
  return GetEditorKindForParamTypeImpl(a_type);
}

RE::SCRIPT_PARAM_TYPE
ResolveEditorParamType(std::string_view a_functionName,
                       const std::uint16_t a_paramIndex,
                       const RE::SCRIPT_PARAM_TYPE a_type) {
  // SVS conditions are evaluated against actors, so constrain GetIsID to actor
  // bases even if the runtime command table exposes the parameter more broadly.
  if (a_paramIndex == 0 &&
      CompareTextInsensitive(a_functionName, "GetIsID") == 0) {
    return RE::SCRIPT_PARAM_TYPE::kActorBase;
  }

  return a_type;
}

std::string FormatNumberString(const double a_value) {
  return FormatNumberStringImpl(a_value);
}

bool DrawNumericClauseValueEditor(const char *a_id, std::string &a_value,
                                  const ValueEditorKind a_kind,
                                  const float a_width) {
  return DrawNumericClauseValueEditorImpl(a_id, a_value, a_kind, a_width);
}

bool DrawConditionParamEditor(const char *a_id, std::string &a_value,
                              const RE::SCRIPT_PARAM_TYPE a_type,
                              const float a_width) {
  const auto editorKind = GetEditorKindForParamTypeImpl(a_type);
  if (editorKind == ValueEditorKind::Integer ||
      editorKind == ValueEditorKind::Number) {
    return DrawNumericClauseValueEditorImpl(a_id, a_value, editorKind, a_width);
  }

  if (editorKind == ValueEditorKind::CachedOption) {
    auto &optionCache = sosr::ui::conditions::ConditionParamOptionCache::Get();
    const auto state = optionCache.Ensure(a_type);
    if (state ==
        sosr::ui::conditions::ConditionParamOptionCache::State::Ready) {
      const auto *options = optionCache.GetOptions(a_type);
      if (options) {
        return sosr::ui::components::DrawSearchableStringDropdown(
            a_id,
            sosr::ui::Localization::GetSingleton()->GetCStr(
                "conditions.select_value"),
            a_value, std::span<const std::string>(*options), a_width);
      }
    } else if (state == sosr::ui::conditions::ConditionParamOptionCache::State::
                            Loading) {
      const auto progress =
          std::clamp(optionCache.GetProgress(a_type), 0.0f, 1.0f);
      const auto status = optionCache.GetStatus(a_type);
      ImGui::ProgressBar(progress, ImVec2(a_width, 0.0f),
                         status.empty() ? nullptr : status.data());
      return false;
    }
  }

  ImGui::BeginDisabled();
  ImGui::SetNextItemWidth(a_width);
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%s",
                sosr::ui::Localization::GetSingleton()->GetCStr(
                    "conditions.unsupported"));
  ImGui::InputText(a_id, buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
  ImGui::EndDisabled();
  return false;
}
} // namespace sosr::ui::condition_editor
