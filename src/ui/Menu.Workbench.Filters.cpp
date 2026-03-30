#include "Menu.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "StringUtils.h"
#include "workbench/InitialFilterSelection.h"

#include <algorithm>
#include <optional>
#include <unordered_set>

namespace sosr {
void Menu::BuildWorkbenchFilterOptions(
    std::vector<WorkbenchFilterOption> &a_options) {
  a_options.clear();

  a_options.push_back({.label = "Show All",
                       .isSection = false,
                       .kind = WorkbenchFilterKind::All,
                       .actorFormID = 0,
                       .conditionId = {}});

  std::vector<WorkbenchFilterOption> actorOptions;
  std::unordered_set<RE::FormID> seenActorFormIDs;
  const auto *player = RE::PlayerCharacter::GetSingleton();
  const auto playerFormID = player ? player->GetFormID() : 0;

  const auto buildActorLabel = [&](const RE::FormID a_actorFormID) {
    if (playerFormID != 0 && a_actorFormID == playerFormID) {
      return std::string("Player [") + armor::FormatFormID(a_actorFormID) + "]";
    }

    if (auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_actorFormID);
        actor != nullptr) {
      std::string label;
      if (const auto *displayName = actor->GetDisplayFullName();
          displayName != nullptr && displayName[0] != '\0') {
        label = displayName;
      } else if (const auto *name = actor->GetName();
                 name != nullptr && name[0] != '\0') {
        label = name;
      } else if (const auto *actorBase = actor->GetActorBase()) {
        label = armor::GetDisplayName(actorBase);
      }

      if (!label.empty()) {
        return label + " [" + armor::FormatFormID(a_actorFormID) + "]";
      }
    }

    return std::string("Actor [") + armor::FormatFormID(a_actorFormID) + "]";
  };

  if (playerFormID != 0 && seenActorFormIDs.insert(playerFormID).second) {
    actorOptions.push_back({.label = "Actor: " + buildActorLabel(playerFormID),
                            .isSection = false,
                            .kind = WorkbenchFilterKind::ActorRef,
                            .actorFormID = playerFormID,
                            .conditionId = {}});
  }

  for (auto &condition : ConditionDefinitions()) {
    const auto materialized =
        conditions::MaterializeConditionById(condition.id, ConditionDefinitions());
    if (!materialized.has_value()) {
      continue;
    }

    for (const auto actorFormID : materialized->refreshTargets.actorFormIDs) {
      if (!seenActorFormIDs.insert(actorFormID).second) {
        continue;
      }

      actorOptions.push_back({.label = "Actor: " + buildActorLabel(actorFormID),
                              .isSection = false,
                              .kind = WorkbenchFilterKind::ActorRef,
                              .actorFormID = actorFormID,
                              .conditionId = {}});
    }
  }

  if (!actorOptions.empty()) {
    a_options.push_back({.label = "Actor Ref Filter", .isSection = true});

    std::stable_sort(
        actorOptions.begin(), actorOptions.end(),
        [&](const WorkbenchFilterOption &a_left,
            const WorkbenchFilterOption &a_right) {
          if (playerFormID != 0 && a_left.actorFormID == playerFormID) {
            return true;
          }
          if (playerFormID != 0 && a_right.actorFormID == playerFormID) {
            return false;
          }
          return strings::CompareTextInsensitive(a_left.label, a_right.label) <
                 0;
        });

    for (auto &option : actorOptions) {
      a_options.push_back(std::move(option));
    }
  }

  if (!ConditionDefinitions().empty()) {
    a_options.push_back({.label = "Condition Filter", .isSection = true});
    for (const auto &condition : ConditionDefinitions()) {
      a_options.push_back({.label = "Condition: " + condition.name,
                           .isSection = false,
                           .kind = WorkbenchFilterKind::Condition,
                           .actorFormID = 0,
                           .conditionId = condition.id});
    }
  }
}

void Menu::ValidateWorkbenchFilterSelection() {
  std::vector<WorkbenchFilterOption> options;
  BuildWorkbenchFilterOptions(options);

  const auto matchesCurrentFilter = [&](const WorkbenchFilterOption &a_option) {
    if (a_option.isSection) {
      return false;
    }
    if (a_option.kind != workbenchFilter_.kind) {
      return false;
    }

    switch (a_option.kind) {
    case WorkbenchFilterKind::All:
      return true;
    case WorkbenchFilterKind::ActorRef:
      return a_option.actorFormID == workbenchFilter_.actorFormID;
    case WorkbenchFilterKind::Condition:
      return a_option.conditionId == workbenchFilter_.conditionId;
    }

    return false;
  };

  if (std::ranges::find_if(options, matchesCurrentFilter) == options.end()) {
    workbenchFilter_ = {};
  }
}

bool Menu::MatchesWorkbenchFilter(const workbench::VariantWorkbenchRow &a_row) {
  switch (workbenchFilter_.kind) {
  case WorkbenchFilterKind::All:
    return true;
  case WorkbenchFilterKind::Condition:
    return a_row.conditionId.has_value() &&
           *a_row.conditionId == workbenchFilter_.conditionId;
  case WorkbenchFilterKind::ActorRef:
    if (!a_row.conditionId.has_value()) {
      return false;
    }
    if (const auto materialized = conditions::MaterializeConditionById(
            *a_row.conditionId, ConditionDefinitions());
        materialized.has_value()) {
      return std::ranges::find(materialized->refreshTargets.actorFormIDs,
                               workbenchFilter_.actorFormID) !=
             materialized->refreshTargets.actorFormIDs.end();
    }
    return false;
  }

  return true;
}

std::vector<int> Menu::BuildVisibleWorkbenchRowIndices() {
  ValidateWorkbenchFilterSelection();

  std::vector<int> visibleRowIndices;
  const auto &rows = workbench_.GetRows();
  visibleRowIndices.reserve(rows.size());
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    if (MatchesWorkbenchFilter(rows[static_cast<std::size_t>(rowIndex)])) {
      visibleRowIndices.push_back(rowIndex);
    }
  }

  return visibleRowIndices;
}

std::optional<std::string>
Menu::ResolveFirstConditionForActorFilter(const RE::FormID a_actorFormID) {
  return workbench::FindFirstConditionForActorFilter(a_actorFormID,
                                                     ConditionDefinitions());
}

void Menu::ApplyInitialWorkbenchFilterSelection() {
  EnsureDefaultConditions();

  auto selection = workbench::BuildInitialFilterSelection(ConditionDefinitions(),
                                                          NextConditionId());
  if (selection.createdCondition.has_value()) {
    ConditionDefinitions().push_back(std::move(*selection.createdCondition));
    ++NextConditionId();
    conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
    conditions::InvalidateConditionMaterializationCaches(ConditionDefinitions());
  }

  workbenchFilter_ = std::move(selection.filter);
}

std::optional<std::string> Menu::ResolveNewWorkbenchRowConditionId() {
  ValidateWorkbenchFilterSelection();

  if (workbenchFilter_.kind == WorkbenchFilterKind::Condition &&
      !workbenchFilter_.conditionId.empty()) {
    return workbenchFilter_.conditionId;
  }
  if (workbenchFilter_.kind == WorkbenchFilterKind::ActorRef &&
      workbenchFilter_.actorFormID != 0) {
    if (const auto conditionId =
            ResolveFirstConditionForActorFilter(workbenchFilter_.actorFormID);
        conditionId.has_value()) {
      return conditionId;
    }
  }

  return std::string(ui::conditions::kDefaultConditionId);
}

RE::Actor *Menu::ResolveWorkbenchPreviewActor() {
  ValidateWorkbenchFilterSelection();

  if (workbenchFilter_.kind == WorkbenchFilterKind::ActorRef &&
      workbenchFilter_.actorFormID != 0) {
    if (auto *actor =
            RE::TESForm::LookupByID<RE::Actor>(workbenchFilter_.actorFormID);
        actor != nullptr) {
      return actor;
    }
  }

  return RE::PlayerCharacter::GetSingleton();
}

void Menu::SyncWorkbenchRowsForCurrentFilter() {
  ValidateWorkbenchFilterSelection();

  if (workbenchFilter_.kind == WorkbenchFilterKind::ActorRef &&
      workbenchFilter_.actorFormID != 0) {
    if (auto *actor =
            RE::TESForm::LookupByID<RE::Actor>(workbenchFilter_.actorFormID);
        actor != nullptr) {
      workbench_.SyncRowsFromActor(actor, ResolveFirstConditionForActorFilter(
                                              workbenchFilter_.actorFormID));
      return;
    }
  }

  workbench_.SyncRowsFromPlayer(
      std::string(ui::conditions::kDefaultConditionId));
}
} // namespace sosr
