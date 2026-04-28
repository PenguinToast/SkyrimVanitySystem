#include "ui/ConditionParamOptionCache.h"

#include "ArmorUtils.h"
#include "IncrementalLoader.h"
#include "conditions/ParamEnumOptions.h"
#include "ui/Localization.h"

#include <RE/Skyrim.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {
using ParamType = RE::SCRIPT_PARAM_TYPE;

struct CacheEntryLoadState {
  std::vector<std::string> stagedOptions;
  std::vector<RE::TESForm *> forms;
  std::unordered_set<RE::FormID> seenForms;
  sosr::IncrementalLoader loader;
};

struct CacheEntry {
  std::vector<std::string> options;
  std::unique_ptr<CacheEntryLoadState> loadState;
  bool loaded{false};
  bool unsupported{false};
};

bool IsTokenCompatible(std::string_view a_text) {
  if (a_text.empty()) {
    return false;
  }

  return std::ranges::all_of(a_text, [](const unsigned char a_char) {
    return std::isalnum(a_char) != 0 || a_char == '_';
  });
}

void SortUniqueStrings(std::vector<std::string> &a_values) {
  std::ranges::sort(a_values);
  a_values.erase(std::unique(a_values.begin(), a_values.end()), a_values.end());
}

std::string JoinStrings(const std::vector<std::string> &a_values) {
  std::string result;
  for (const auto &value : a_values) {
    if (!result.empty()) {
      result.append(", ");
    }
    result.append(value);
  }
  return result;
}

std::string GetEditorIdToken(const RE::TESForm *a_form) {
  if (!a_form || a_form->IsDeleted() || a_form->IsIgnored()) {
    return {};
  }

  auto editorID = sosr::armor::GetEditorID(a_form);
  if (!IsTokenCompatible(editorID)) {
    return {};
  }
  return editorID;
}

template <class T> std::vector<RE::TESForm *> CollectForms() {
  std::vector<RE::TESForm *> forms;

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return forms;
  }

  for (auto *form : dataHandler->GetFormArray<T>()) {
    forms.push_back(form);
  }
  return forms;
}

template <class T> bool IsAssignableForm(RE::TESForm *a_form) {
  return a_form && a_form->As<T>();
}

template <class... T> std::vector<RE::TESForm *> CollectAssignableForms() {
  std::vector<RE::TESForm *> forms;
  std::unordered_set<RE::FormID> seenForms;

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return forms;
  }

  for (const auto &formArray : dataHandler->formArrays) {
    for (auto *form : formArray) {
      if (!form || !(IsAssignableForm<T>(form) || ...)) {
        continue;
      }
      const auto formID = form->GetFormID();
      if (formID != 0 && seenForms.insert(formID).second) {
        forms.push_back(form);
      }
    }
  }
  return forms;
}

void AppendFormToken(CacheEntryLoadState &a_state, RE::TESForm *a_form) {
  if (!a_form) {
    return;
  }

  const auto formID = a_form->GetFormID();
  if (formID == 0 || !a_state.seenForms.insert(formID).second) {
    return;
  }

  if (const auto token = GetEditorIdToken(a_form); !token.empty()) {
    a_state.stagedOptions.push_back(token);
  }
}

void AppendCellRefTokens(CacheEntryLoadState &a_state,
                         RE::TESObjectCELL *a_cell) {
  if (!a_cell) {
    return;
  }

  const auto &runtimeData = a_cell->GetRuntimeData();
  for (auto *ref : runtimeData.objectList) {
    AppendFormToken(a_state, ref);
  }

  a_cell->ForEachReference([&](RE::TESObjectREFR *a_ref) {
    AppendFormToken(a_state, a_ref);
    return RE::BSContainer::ForEachResult::kContinue;
  });
}

std::vector<std::string> BuildImmediateOptions(const ParamType a_type) {
  if (sosr::conditions::HasParamEnumOptions(a_type)) {
    return sosr::conditions::BuildParamEnumOptionLabels(a_type);
  }
  if (sosr::conditions::HasParamTextOptions(a_type)) {
    return sosr::conditions::BuildParamTextOptionLabels(a_type);
  }

  switch (a_type) {
  case ParamType::kAxis:
    return {"X", "Y", "Z"};
  default:
    return {};
  }
}

bool UsesGenericFormSource(const ParamType a_type) {
  switch (a_type) {
  case ParamType::kKnowableForm:
  case ParamType::kForm:
    return true;
  default:
    return false;
  }
}

std::vector<RE::TESForm *> CollectFormsForType(const ParamType a_type) {
  switch (a_type) {
  case ParamType::kMagicItem:
    return CollectAssignableForms<RE::MagicItem>();
  case ParamType::kObject:
  case ParamType::kInventoryObject:
    return CollectAssignableForms<RE::TESBoundObject>();
  case ParamType::kFurnitureOrFormList:
    return CollectAssignableForms<RE::TESFurniture, RE::BGSListForm>();
  case ParamType::kOwner:
    return CollectAssignableForms<RE::TESNPC, RE::TESFaction>();
  case ParamType::kInvObjectOrFormList:
  case ParamType::kObjectOrFormList:
    return CollectAssignableForms<RE::TESBoundObject, RE::BGSListForm>();
  case ParamType::kWorldOrList:
    return CollectAssignableForms<RE::TESWorldSpace, RE::BGSListForm>();
  case ParamType::kRegion:
    return CollectForms<RE::TESRegion>();
  case ParamType::kFormList:
    return CollectForms<RE::BGSListForm>();
  case ParamType::kSpellItem:
    return CollectForms<RE::SpellItem>();
  case ParamType::kPackage:
    return CollectForms<RE::TESPackage>();
  case ParamType::kMagicEffect:
    return CollectForms<RE::EffectSetting>();
  case ParamType::kBGSScene:
    return CollectForms<RE::BGSScene>();
  case ParamType::kAssociationType:
    return CollectForms<RE::BGSAssociationType>();
  case ParamType::kNote:
    return CollectForms<RE::BGSNote>();
  case ParamType::kEncounterZone:
    return CollectForms<RE::BGSEncounterZone>();
  case ParamType::kIdleForm:
    return CollectForms<RE::TESIdleForm>();
  case ParamType::kActor:
  case ParamType::kActorBase:
  case ParamType::kNPC:
    return CollectForms<RE::TESNPC>();
  case ParamType::kActorValue:
    return CollectForms<RE::ActorValueInfo>();
  case ParamType::kRace:
    return CollectForms<RE::TESRace>();
  case ParamType::kClass:
    return CollectForms<RE::TESClass>();
  case ParamType::kFaction:
    return CollectForms<RE::TESFaction>();
  case ParamType::kGlobal:
    return CollectForms<RE::TESGlobal>();
  case ParamType::kQuest:
    return CollectForms<RE::TESQuest>();
  case ParamType::kKeyword:
    return CollectForms<RE::BGSKeyword>();
  case ParamType::kRefType:
    return CollectForms<RE::BGSLocationRefType>();
  case ParamType::kPerk:
    return CollectForms<RE::BGSPerk>();
  case ParamType::kVoiceType:
    return CollectForms<RE::BGSVoiceType>();
  case ParamType::kCell:
    return CollectForms<RE::TESObjectCELL>();
  case ParamType::kLocation:
    return CollectForms<RE::BGSLocation>();
  case ParamType::kWeather:
    return CollectForms<RE::TESWeather>();
  case ParamType::kShout:
    return CollectForms<RE::TESShout>();
  case ParamType::kWordOfPower:
    return CollectForms<RE::TESWordOfPower>();
  default:
    return {};
  }
}

std::string GetStatusLabel(const ParamType a_type) {
  const auto *localization = sosr::ui::Localization::GetSingleton();
  switch (a_type) {
  case ParamType::kMagicItem:
  case ParamType::kObject:
  case ParamType::kInventoryObject:
  case ParamType::kFurnitureOrFormList:
  case ParamType::kOwner:
  case ParamType::kKnowableForm:
  case ParamType::kForm:
  case ParamType::kInvObjectOrFormList:
  case ParamType::kObjectOrFormList:
  case ParamType::kWorldOrList:
    return std::string(localization->Get("conditions.loading.forms"));
  case ParamType::kRegion:
    return std::string(localization->Get("conditions.loading.regions"));
  case ParamType::kFormList:
    return std::string(localization->Get("conditions.loading.form_lists"));
  case ParamType::kSpellItem:
    return std::string(localization->Get("conditions.loading.spells"));
  case ParamType::kPackage:
    return std::string(localization->Get("conditions.loading.packages"));
  case ParamType::kMagicEffect:
    return std::string(localization->Get("conditions.loading.magic_effects"));
  case ParamType::kBGSScene:
    return std::string(localization->Get("conditions.loading.scenes"));
  case ParamType::kAssociationType:
    return std::string(
        localization->Get("conditions.loading.association_types"));
  case ParamType::kNote:
    return std::string(localization->Get("conditions.loading.notes"));
  case ParamType::kEncounterZone:
    return std::string(localization->Get("conditions.loading.encounter_zones"));
  case ParamType::kIdleForm:
    return std::string(localization->Get("conditions.loading.idles"));
  case ParamType::kObjectRef:
    return std::string(localization->Get("conditions.loading.references"));
  case ParamType::kActor:
  case ParamType::kActorBase:
  case ParamType::kNPC:
    return std::string(localization->Get("conditions.loading.actors"));
  case ParamType::kRace:
    return std::string(localization->Get("conditions.loading.races"));
  case ParamType::kActorValue:
    return std::string(localization->Get("conditions.loading.actor_values"));
  case ParamType::kClass:
    return std::string(localization->Get("conditions.loading.classes"));
  case ParamType::kFaction:
    return std::string(localization->Get("conditions.loading.factions"));
  case ParamType::kGlobal:
    return std::string(localization->Get("conditions.loading.globals"));
  case ParamType::kQuest:
    return std::string(localization->Get("conditions.loading.quests"));
  case ParamType::kKeyword:
    return std::string(localization->Get("conditions.loading.keywords"));
  case ParamType::kRefType:
    return std::string(localization->Get("conditions.loading.ref_types"));
  case ParamType::kPerk:
    return std::string(localization->Get("conditions.loading.perks"));
  case ParamType::kVoiceType:
    return std::string(localization->Get("conditions.loading.voice_types"));
  case ParamType::kCell:
    return std::string(localization->Get("conditions.loading.cells"));
  case ParamType::kLocation:
    return std::string(localization->Get("conditions.loading.locations"));
  case ParamType::kWeather:
    return std::string(localization->Get("conditions.loading.weather"));
  case ParamType::kShout:
    return std::string(localization->Get("conditions.loading.shouts"));
  case ParamType::kWordOfPower:
    return std::string(localization->Get("conditions.loading.words_of_power"));
  default:
    return std::string(localization->Get("conditions.loading.options"));
  }
}

std::string_view GetParamTypeLabel(const ParamType a_type) {
  switch (a_type) {
  case ParamType::kMagicItem:
    return "MagicItem";
  case ParamType::kObject:
    return "Object";
  case ParamType::kInventoryObject:
    return "InventoryObject";
  case ParamType::kFurnitureOrFormList:
    return "FurnitureOrFormList";
  case ParamType::kOwner:
    return "Owner";
  case ParamType::kKnowableForm:
    return "KnowableForm";
  case ParamType::kForm:
    return "Form";
  case ParamType::kInvObjectOrFormList:
    return "InvObjectOrFormList";
  case ParamType::kObjectOrFormList:
    return "ObjectOrFormList";
  case ParamType::kWorldOrList:
    return "WorldOrList";
  case ParamType::kFormList:
    return "FormList";
  case ParamType::kSpellItem:
    return "SpellItem";
  case ParamType::kObjectRef:
    return "ObjectRef";
  case ParamType::kActor:
    return "Actor";
  case ParamType::kActorBase:
    return "ActorBase";
  case ParamType::kNPC:
    return "NPC";
  case ParamType::kActorValue:
    return "ActorValue";
  case ParamType::kRace:
    return "Race";
  case ParamType::kClass:
    return "Class";
  case ParamType::kFaction:
    return "Faction";
  case ParamType::kGlobal:
    return "Global";
  case ParamType::kQuest:
    return "Quest";
  case ParamType::kKeyword:
    return "Keyword";
  case ParamType::kPerk:
    return "Perk";
  case ParamType::kVoiceType:
    return "VoiceType";
  case ParamType::kCell:
    return "Cell";
  case ParamType::kLocation:
    return "Location";
  case ParamType::kWeather:
    return "Weather";
  case ParamType::kShout:
    return "Shout";
  case ParamType::kWordOfPower:
    return "WordOfPower";
  case ParamType::kRegion:
    return "Region";
  case ParamType::kPackage:
    return "Package";
  case ParamType::kMagicEffect:
    return "MagicEffect";
  case ParamType::kCrimeType:
    return "CrimeType";
  case ParamType::kFormType:
    return "FormType";
  case ParamType::kBGSScene:
    return "Scene";
  case ParamType::kAssociationType:
    return "AssociationType";
  case ParamType::kNote:
    return "Note";
  case ParamType::kEncounterZone:
    return "EncounterZone";
  case ParamType::kIdleForm:
    return "IdleForm";
  case ParamType::kAlignment:
    return "Alignment";
  case ParamType::kEquipType:
    return "EquipType";
  case ParamType::kCritStage:
    return "CritStage";
  case ParamType::kRefType:
    return "RefType";
  case ParamType::kWardState:
    return "WardState";
  case ParamType::kFurnitureAnimType:
    return "FurnitureAnimType";
  case ParamType::kFurnitureEntryType:
    return "FurnitureEntryType";
  case ParamType::kSkillAction:
    return "SkillAction";
  case ParamType::kAxis:
    return "Axis";
  case ParamType::kSex:
    return "Sex";
  case ParamType::kCastingSource:
    return "CastingSource";
  default:
    return "Unknown";
  }
}

bool SupportsCachedOptions(const ParamType a_type) {
  if (!BuildImmediateOptions(a_type).empty()) {
    return true;
  }

  switch (a_type) {
  case ParamType::kMagicItem:
  case ParamType::kObject:
  case ParamType::kInventoryObject:
  case ParamType::kFurnitureOrFormList:
  case ParamType::kOwner:
  case ParamType::kKnowableForm:
  case ParamType::kForm:
  case ParamType::kInvObjectOrFormList:
  case ParamType::kObjectOrFormList:
  case ParamType::kWorldOrList:
  case ParamType::kFormList:
  case ParamType::kSpellItem:
  case ParamType::kRegion:
  case ParamType::kPackage:
  case ParamType::kMagicEffect:
  case ParamType::kBGSScene:
  case ParamType::kAssociationType:
  case ParamType::kNote:
  case ParamType::kEncounterZone:
  case ParamType::kIdleForm:
  case ParamType::kObjectRef:
  case ParamType::kActor:
  case ParamType::kActorBase:
  case ParamType::kNPC:
  case ParamType::kActorValue:
  case ParamType::kRace:
  case ParamType::kClass:
  case ParamType::kFaction:
  case ParamType::kGlobal:
  case ParamType::kQuest:
  case ParamType::kKeyword:
  case ParamType::kRefType:
  case ParamType::kPerk:
  case ParamType::kVoiceType:
  case ParamType::kCell:
  case ParamType::kLocation:
  case ParamType::kWeather:
  case ParamType::kShout:
  case ParamType::kWordOfPower:
    return true;
  default:
    return false;
  }
}

class ConditionParamOptionCacheImpl {
public:
  static auto Get() -> ConditionParamOptionCacheImpl & {
    static ConditionParamOptionCacheImpl singleton;
    return singleton;
  }

  void Continue(const double a_maxMillisecondsPerTick) {
    std::vector<std::pair<int, std::reference_wrapper<CacheEntry>>>
        activeEntries;
    activeEntries.reserve(entries_.size());

    for (auto &[type, entry] : entries_) {
      if (entry.loadState && entry.loadState->loader.IsRunning()) {
        activeEntries.emplace_back(type, entry);
      }
    }

    if (activeEntries.empty()) {
      return;
    }

    const auto budgetPerEntry =
        a_maxMillisecondsPerTick / static_cast<double>(activeEntries.size());
    for (auto &[type, entryRef] : activeEntries) {
      auto &entry = entryRef.get();
      if (!entry.loadState) {
        continue;
      }

      if (!entry.loadState->loader.Continue(budgetPerEntry)) {
        entry.options = std::move(entry.loadState->stagedOptions);
        SortUniqueStrings(entry.options);
        entry.loaded = true;
        const auto typeLabel =
            std::string(GetParamTypeLabel(static_cast<ParamType>(type)));
        logger::info(
            "Condition param cache ready: {} (type {}) with {} option(s)",
            typeLabel, type, entry.options.size());
        if (entry.options.size() <= 8) {
          logger::info("Condition param cache values [{}]: {}", typeLabel,
                       JoinStrings(entry.options));
        }
        entry.loadState.reset();
      }
    }
  }

  void Reset() { entries_.clear(); }

  auto Ensure(const ParamType a_type)
      -> sosr::ui::conditions::ConditionParamOptionCache::State {
    auto &entry = entries_[static_cast<int>(a_type)];
    if (entry.unsupported) {
      return sosr::ui::conditions::ConditionParamOptionCache::State::
          Unsupported;
    }
    if (entry.loaded) {
      return sosr::ui::conditions::ConditionParamOptionCache::State::Ready;
    }
    if (entry.loadState) {
      return sosr::ui::conditions::ConditionParamOptionCache::State::Loading;
    }

    if (!SupportsCachedOptions(a_type)) {
      entry.unsupported = true;
      return sosr::ui::conditions::ConditionParamOptionCache::State::
          Unsupported;
    }

    entry.options = BuildImmediateOptions(a_type);
    if (!entry.options.empty()) {
      entry.loaded = true;
      return sosr::ui::conditions::ConditionParamOptionCache::State::Ready;
    }

    auto loadState = std::make_unique<CacheEntryLoadState>();
    loadState->stagedOptions = std::move(entry.options);
    auto *statePtr = loadState.get();

    if (UsesGenericFormSource(a_type)) {
      auto *dataHandler = RE::TESDataHandler::GetSingleton();
      const auto arrayCount =
          dataHandler
              ? static_cast<std::size_t>(std::size(dataHandler->formArrays))
              : std::size_t{0};
      std::size_t totalFormCount = 0;
      if (dataHandler) {
        for (const auto &formArray : dataHandler->formArrays) {
          totalFormCount += static_cast<std::size_t>(formArray.size());
        }
      }

      logger::info(
          "Condition param cache loading: {} (type {}) from {} form(s) "
          "across {} form array(s)",
          std::string(GetParamTypeLabel(a_type)), static_cast<int>(a_type),
          totalFormCount, arrayCount);

      std::vector<sosr::IncrementalLoader::Phase> phases;
      phases.reserve(arrayCount + 1);
      if (dataHandler) {
        for (std::size_t arrayIndex = 0; arrayIndex < arrayCount;
             ++arrayIndex) {
          phases.push_back(
              {GetStatusLabel(a_type),
               static_cast<std::size_t>(
                   dataHandler->formArrays[arrayIndex].size()),
               [statePtr, dataHandler, arrayIndex](const std::size_t a_index) {
                 const auto formIndex = static_cast<
                     decltype(dataHandler->formArrays[arrayIndex].size())>(
                     a_index);
                 AppendFormToken(
                     *statePtr, dataHandler->formArrays[arrayIndex][formIndex]);
               }});
        }
      }
      phases.push_back(
          {"Finalizing options...", 1, [statePtr](const std::size_t) {
             SortUniqueStrings(statePtr->stagedOptions);
           }});
      loadState->loader.Start(std::move(phases));
    } else if (a_type == ParamType::kObjectRef) {
      auto *dataHandler = RE::TESDataHandler::GetSingleton();
      const auto interiorCount =
          dataHandler
              ? static_cast<std::size_t>(dataHandler->interiorCells.size())
              : std::size_t{0};
      const auto worldspaceCount =
          dataHandler
              ? static_cast<std::size_t>(
                    dataHandler->GetFormArray<RE::TESWorldSpace>().size())
              : std::size_t{0};
      const auto cellCount =
          dataHandler
              ? static_cast<std::size_t>(
                    dataHandler->GetFormArray<RE::TESObjectCELL>().size())
              : std::size_t{0};

      statePtr->stagedOptions.push_back("Player");
      logger::info("Condition param cache loading: {} (type {}) from {} "
                   "interior cell(s), "
                   "{} worldspace(s), {} cell form(s)",
                   std::string(GetParamTypeLabel(a_type)),
                   static_cast<int>(a_type), interiorCount, worldspaceCount,
                   cellCount);

      loadState->loader.Start({
          {std::string(sosr::ui::Localization::GetSingleton()->Get(
               "conditions.loading.interior_references")),
           interiorCount,
           [statePtr, dataHandler](const std::size_t a_index) {
             if (!dataHandler) {
               return;
             }
             const auto cellIndex =
                 static_cast<decltype(dataHandler->interiorCells.size())>(
                     a_index);
             AppendCellRefTokens(*statePtr,
                                 dataHandler->interiorCells[cellIndex]);
           }},
          {std::string(sosr::ui::Localization::GetSingleton()->Get(
               "conditions.loading.worldspace_references")),
           worldspaceCount,
           [statePtr, dataHandler](const std::size_t a_index) {
             if (!dataHandler) {
               return;
             }
             const auto worldspaceIndex =
                 static_cast<decltype(dataHandler
                                          ->GetFormArray<RE::TESWorldSpace>()
                                          .size())>(a_index);
             auto *worldspace =
                 dataHandler
                     ->GetFormArray<RE::TESWorldSpace>()[worldspaceIndex];
             if (!worldspace) {
               return;
             }
             AppendCellRefTokens(*statePtr, worldspace->persistentCell);
             for (const auto &ref : worldspace->mobilePersistentRefs) {
               AppendFormToken(*statePtr, ref.get());
             }
           }},
          {std::string(sosr::ui::Localization::GetSingleton()->Get(
               "conditions.loading.cell_references")),
           cellCount,
           [statePtr, dataHandler](const std::size_t a_index) {
             if (!dataHandler) {
               return;
             }
             const auto cellIndex =
                 static_cast<decltype(dataHandler
                                          ->GetFormArray<RE::TESObjectCELL>()
                                          .size())>(a_index);
             AppendCellRefTokens(
                 *statePtr,
                 dataHandler->GetFormArray<RE::TESObjectCELL>()[cellIndex]);
           }},
          {"Finalizing options...", 1,
           [statePtr](const std::size_t) {
             SortUniqueStrings(statePtr->stagedOptions);
           }},
      });
    } else {
      loadState->forms = CollectFormsForType(a_type);
      logger::info(
          "Condition param cache loading: {} (type {}) from {} form(s)",
          std::string(GetParamTypeLabel(a_type)), static_cast<int>(a_type),
          loadState->forms.size());
      loadState->loader.Start({
          {GetStatusLabel(a_type), loadState->forms.size(),
           [statePtr](const std::size_t a_index) {
             if (const auto token = GetEditorIdToken(statePtr->forms[a_index]);
                 !token.empty()) {
               statePtr->stagedOptions.push_back(token);
             }
           }},
          {"Finalizing options...", 1,
           [statePtr](const std::size_t) {
             SortUniqueStrings(statePtr->stagedOptions);
           }},
      });
    }
    entry.loadState = std::move(loadState);
    return sosr::ui::conditions::ConditionParamOptionCache::State::Loading;
  }

  auto GetOptions(const ParamType a_type) const
      -> const std::vector<std::string> * {
    const auto it = entries_.find(static_cast<int>(a_type));
    if (it == entries_.end() || !it->second.loaded) {
      return nullptr;
    }
    return std::addressof(it->second.options);
  }

  auto GetProgress(const ParamType a_type) const -> float {
    const auto it = entries_.find(static_cast<int>(a_type));
    if (it == entries_.end()) {
      return 0.0f;
    }
    if (it->second.loadState) {
      return it->second.loadState->loader.GetProgress();
    }
    return it->second.loaded ? 1.0f : 0.0f;
  }

  auto GetStatus(const ParamType a_type) const -> std::string_view {
    const auto it = entries_.find(static_cast<int>(a_type));
    if (it == entries_.end()) {
      return {};
    }
    if (it->second.loadState) {
      return it->second.loadState->loader.GetStatus();
    }
    return {};
  }

private:
  std::unordered_map<int, CacheEntry> entries_;
};
} // namespace

namespace sosr::ui::conditions {
ConditionParamOptionCache &ConditionParamOptionCache::Get() {
  static ConditionParamOptionCache singleton;
  return singleton;
}

bool ConditionParamOptionCache::Supports(const ParamType a_type) {
  return SupportsCachedOptions(a_type);
}

void ConditionParamOptionCache::Continue(
    const double a_maxMillisecondsPerTick) {
  ConditionParamOptionCacheImpl::Get().Continue(a_maxMillisecondsPerTick);
}

void ConditionParamOptionCache::Reset() {
  ConditionParamOptionCacheImpl::Get().Reset();
}

ConditionParamOptionCache::State
ConditionParamOptionCache::Ensure(const ParamType a_type) {
  return ConditionParamOptionCacheImpl::Get().Ensure(a_type);
}

const std::vector<std::string> *
ConditionParamOptionCache::GetOptions(const ParamType a_type) const {
  return ConditionParamOptionCacheImpl::Get().GetOptions(a_type);
}

float ConditionParamOptionCache::GetProgress(const ParamType a_type) const {
  return ConditionParamOptionCacheImpl::Get().GetProgress(a_type);
}

std::string_view
ConditionParamOptionCache::GetStatus(const ParamType a_type) const {
  return ConditionParamOptionCacheImpl::Get().GetStatus(a_type);
}
} // namespace sosr::ui::conditions
