#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::conditions {
inline constexpr std::string_view kDefaultConditionId = "condition-1";

enum class DefinitionKind : std::uint8_t { Catalog, Library };

enum class Comparator : std::uint8_t {
  Equal,
  NotEqual,
  Greater,
  GreaterOrEqual,
  Less,
  LessOrEqual
};

enum class Connective : std::uint8_t { And, Or };

struct Clause {
  std::string functionName;
  std::string customConditionId;
  std::array<std::string, 2> arguments{};
  Comparator comparator{Comparator::Equal};
  std::string comparand{"1"};
  Connective connectiveToNext{Connective::And};
};

struct Color {
  float x{0.55f};
  float y{0.55f};
  float z{0.55f};
  float w{1.0f};
};

struct CatalogProperties {
  Color color{};
  bool enabled{true};
};

struct LibraryProperties {
  std::string storagePath;
};

struct Definition {
  std::string id;
  std::string name;
  std::string description;
  DefinitionKind kind{DefinitionKind::Catalog};
  std::optional<CatalogProperties> catalog;
  std::optional<LibraryProperties> library;
  std::vector<Clause> clauses;

  [[nodiscard]] bool IsCatalog() const {
    return kind == DefinitionKind::Catalog;
  }

  [[nodiscard]] bool IsLibrary() const {
    return kind == DefinitionKind::Library;
  }

  [[nodiscard]] const CatalogProperties *GetCatalog() const {
    return catalog ? &*catalog : nullptr;
  }

  [[nodiscard]] CatalogProperties *GetCatalog() {
    return catalog ? &*catalog : nullptr;
  }

  [[nodiscard]] const LibraryProperties *GetLibrary() const {
    return library ? &*library : nullptr;
  }

  [[nodiscard]] LibraryProperties *GetLibrary() {
    return library ? &*library : nullptr;
  }

  CatalogProperties &EnsureCatalog() {
    kind = DefinitionKind::Catalog;
    library.reset();
    if (!catalog) {
      catalog = CatalogProperties{};
    }
    return *catalog;
  }

  LibraryProperties &EnsureLibrary() {
    kind = DefinitionKind::Library;
    catalog.reset();
    if (!library) {
      library = LibraryProperties{};
    }
    return *library;
  }
};
} // namespace sosr::conditions
