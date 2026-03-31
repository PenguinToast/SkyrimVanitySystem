#include "workbench/InitialFilterSelection.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "StringUtils.h"
#include "conditions/Creation.h"
#include "conditions/Defaults.h"
#include "conditions/Status.h"
#include "conditions/Validation.h"

#include <algorithm>
#include <optional>

namespace {
using FilterKind = sosr::ui::workbench::FilterKind;
using FilterState = sosr::ui::workbench::FilterState;

FilterState BuildActorFilterState(const RE::FormID a_actorFormID) {
  return {.kind = FilterKind::ActorRef,
          .actorFormID = a_actorFormID,
          .conditionId = {}};
}

bool IsReservedOrConflictingConditionName(
    std::string_view a_candidate,
    const std::vector<sosr::conditions::Definition> &a_conditions) {
  if (auto *command = RE::SCRIPT_FUNCTION::LocateScriptCommand(
          sosr::strings::TrimText(a_candidate).c_str());
      command != nullptr && command->conditionFunction) {
    return true;
  }

  return sosr::conditions::FindDefinitionByName(a_conditions, a_candidate) !=
         nullptr;
}

std::string BuildActorConditionBaseName(RE::Actor *a_actor) {
  if (!a_actor) {
    return "Target";
  }

  if (const auto *displayName = a_actor->GetDisplayFullName();
      displayName != nullptr && displayName[0] != '\0') {
    return displayName;
  }
  if (const auto *name = a_actor->GetName();
      name != nullptr && name[0] != '\0') {
    return name;
  }
  if (const auto *actorBase = a_actor->GetActorBase()) {
    const auto baseName = sosr::armor::GetDisplayName(actorBase);
    if (!baseName.empty()) {
      return baseName;
    }
  }

  return "Target";
}

std::string BuildUniqueActorConditionName(
    std::string_view a_baseName,
    const std::vector<sosr::conditions::Definition> &a_conditions) {
  auto baseName = sosr::strings::TrimText(a_baseName);
  if (baseName.empty()) {
    baseName = "Target";
  }

  if (!IsReservedOrConflictingConditionName(baseName, a_conditions)) {
    return baseName;
  }

  for (int index = 2;; ++index) {
    const auto candidate = baseName + " " + std::to_string(index);
    if (!IsReservedOrConflictingConditionName(candidate, a_conditions)) {
      return candidate;
    }
  }
}

std::string BuildActorReferenceToken(RE::Actor *a_actor) {
  if (!a_actor) {
    return {};
  }

  if (const auto editorID = sosr::armor::GetEditorID(a_actor);
      !editorID.empty()) {
    return editorID;
  }

  if (const auto identifier = sosr::armor::GetFormIdentifier(a_actor);
      !identifier.empty()) {
    return identifier;
  }

  return sosr::armor::FormatFormID(a_actor->GetFormID());
}

RE::Actor *ResolveCrosshairActor() {
  const auto *crosshairPickData = RE::CrosshairPickData::GetSingleton();
  if (!crosshairPickData) {
    return nullptr;
  }

  RE::ObjectRefHandle handle;
#if defined(EXCLUSIVE_SKYRIM_FLAT)
  handle = crosshairPickData->targetActor;
#else
  handle = crosshairPickData->targetActor[RE::VR_DEVICE::kHeadset];
#endif

  auto ref = handle.get();
  if (!ref) {
    return nullptr;
  }

  return ref->As<RE::Actor>();
}

RE::Actor *ResolveInitialWorkbenchFilterActor() {
  if (auto *actor = ResolveCrosshairActor()) {
    return actor;
  }

  return RE::PlayerCharacter::GetSingleton();
}
} // namespace

namespace sosr::workbench {
std::optional<std::string> FindFirstConditionForActorFilter(
    const RE::FormID a_actorFormID,
    std::vector<conditions::Definition> &a_conditions) {
  for (const auto &condition : a_conditions) {
    if (!sosr::conditions::IsWorkbenchSelectable(condition)) {
      continue;
    }
    const auto materialized =
        conditions::MaterializeConditionById(condition.id, a_conditions);
    if (!materialized.has_value()) {
      continue;
    }

    if (std::ranges::find(materialized->refreshTargets.actorFormIDs,
                          a_actorFormID) !=
        materialized->refreshTargets.actorFormIDs.end()) {
      return condition.id;
    }
  }

  return std::nullopt;
}

InitialFilterSelection
BuildInitialFilterSelection(std::vector<conditions::Definition> &a_conditions,
                            const int a_nextConditionId) {
  if (auto *actor = ResolveInitialWorkbenchFilterActor(); actor != nullptr) {
    const auto actorFormID = actor->GetFormID();
    const auto *player = RE::PlayerCharacter::GetSingleton();
    if (player && actorFormID == player->GetFormID()) {
      return {.filter = BuildActorFilterState(actorFormID),
              .createdCondition = {}};
    }

    if (FindFirstConditionForActorFilter(actorFormID, a_conditions)
            .has_value()) {
      return {.filter = BuildActorFilterState(actorFormID),
              .createdCondition = {}};
    }

    std::vector<conditions::Color> existingColors;
    existingColors.reserve(a_conditions.size());
    for (const auto &condition : a_conditions) {
      if (const auto *catalog = condition.GetCatalog(); catalog != nullptr) {
        existingColors.push_back(catalog->color);
      }
    }

    const auto baseName = BuildActorConditionBaseName(actor);
    auto condition = conditions::BuildNewConditionTemplate(
        BuildUniqueActorConditionName(baseName, a_conditions),
        conditions::PickDistinctConditionColor(existingColors));
    condition.id = conditions::BuildConditionId(a_nextConditionId);
    condition.description = "Applies to " + baseName;
    condition.clauses.clear();

    conditions::Clause clause;
    clause.functionName = "GetIsReference";
    clause.arguments[0] = BuildActorReferenceToken(actor);
    clause.comparator = conditions::Comparator::Equal;
    clause.comparand = "1";
    clause.connectiveToNext = conditions::Connective::And;
    condition.clauses.push_back(std::move(clause));

    return {.filter = BuildActorFilterState(actorFormID),
            .createdCondition = std::move(condition)};
  }

  return {.filter = {}, .createdCondition = {}};
}
} // namespace sosr::workbench
