#include "integrations/DynamicArmorVariantsExtendedClient.h"

#include "ui/Localization.h"

namespace sosr::integrations {
namespace {
constexpr REL::Version kMinimumDavVersion{1, 3, 0, 0};
}

void DynamicArmorVariantsExtendedClient::Initialize(
    const SKSE::LoadInterface *a_loadInterface) {
  loadInterface_ = a_loadInterface;
  RefreshDetectedVersion();
}

void DynamicArmorVariantsExtendedClient::RefreshDetectedVersion() {
  detectedVersion_.reset();
  if (!loadInterface_) {
    return;
  }

  if (const auto *pluginInfo =
          loadInterface_->GetPluginInfo("DynamicArmorVariants")) {
    detectedVersion_ = REL::Version::unpack(pluginInfo->version);
  }
}

void DynamicArmorVariantsExtendedClient::Refresh() {
  RefreshDetectedVersion();
  interface_ = DynamicArmorVariantsExtendedAPI::
      GetDynamicArmorVariantsExtendedInterface001();
  attemptedFetch_ = true;

  if (!interface_) {
    if (!availabilityWarningLogged_) {
      logger::warn("Dynamic Armor Variants Extended API not available");
      availabilityWarningLogged_ = true;
    }
    return;
  }

  availabilityWarningLogged_ = false;
  logger::info("Dynamic Armor Variants Extended API handle acquired");
}

auto DynamicArmorVariantsExtendedClient::Get()
    -> DynamicArmorVariantsExtendedAPI::
        IDynamicArmorVariantsExtendedInterface001 * {
  if (!interface_) {
    Refresh();
  }

  return interface_;
}

auto DynamicArmorVariantsExtendedClient::GetReady()
    -> DynamicArmorVariantsExtendedAPI::
        IDynamicArmorVariantsExtendedInterface001 * {
  auto *davInterface = Get();
  if (!davInterface || !HasMinimumVersion() || !davInterface->IsReady()) {
    return nullptr;
  }

  return davInterface;
}

auto DynamicArmorVariantsExtendedClient::IsAvailable() -> bool {
  return Get() != nullptr;
}

auto DynamicArmorVariantsExtendedClient::HasMinimumVersion() -> bool {
  if (!detectedVersion_.has_value()) {
    RefreshDetectedVersion();
  }

  return detectedVersion_.has_value() &&
         *detectedVersion_ >= kMinimumDavVersion;
}

auto DynamicArmorVariantsExtendedClient::GetDetectedVersion()
    -> std::optional<REL::Version> {
  if (!detectedVersion_.has_value()) {
    RefreshDetectedVersion();
  }

  return detectedVersion_;
}

auto DynamicArmorVariantsExtendedClient::GetStatus() -> Status {
  auto *davInterface = Get();
  if (!davInterface) {
    return Status::Unavailable;
  }

  if (!HasMinimumVersion()) {
    return Status::VersionTooOld;
  }

  if (!davInterface->IsReady()) {
    return Status::NotReady;
  }

  return Status::Ready;
}

auto DynamicArmorVariantsExtendedClient::GetAvailabilityMessage()
    -> std::optional<std::string> {
  auto *localization = ui::Localization::GetSingleton();
  switch (GetStatus()) {
  case Status::Unavailable:
    return std::string(localization->Get("dav.status.unavailable"));
  case Status::VersionTooOld: {
    std::string message = std::string(localization->Get("dav.status.too_old"));
    if (const auto detectedVersion = GetDetectedVersion();
        detectedVersion.has_value()) {
      message.append(" ");
      message.append(localization->Get("dav.status.detected_version"));
      message.append(" ");
      message.append(std::to_string(*detectedVersion));
      message.push_back('.');
    }
    return message;
  }
  case Status::NotReady:
    return std::string(localization->Get("dav.status.not_ready"));
  case Status::Ready:
    return std::nullopt;
  }

  return std::nullopt;
}
} // namespace sosr::integrations
