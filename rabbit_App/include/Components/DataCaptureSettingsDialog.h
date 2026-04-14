#pragma once
#ifndef DATA_CAPTURE_SETTINGS_DIALOG_H
#define DATA_CAPTURE_SETTINGS_DIALOG_H

#include "Components/ArrayPortMappingSettingsDialog.h"
#include "Components/ComponentSettingsDialog.h"

namespace rabbit_App::component {

/// @brief Settings dialog for data capture/input components with array port mapping
/// This dialog combines the basic component settings with array port mapping
/// functionality for components like DataCapture16, DecimalInput32, etc.
class DataCaptureSettingsDialog : public ComponentSettingsDialog {
public:
  DataCaptureSettingsDialog(AbstractComponent *component, QWidget *parent = nullptr)
      : ComponentSettingsDialog(component, parent) {
    // Create and add the array port mapping widget manually
    array_mapping_widget_ = new ArrayPortMappingSettingsFeatureWidget(component, this);
    array_mapping_widget_->setParentDialog(this);
    appendSettingWidget(array_mapping_widget_);
  }

private:
  ArrayPortMappingSettingsFeatureWidget *array_mapping_widget_;
};

} // namespace rabbit_App::component

#endif // DATA_CAPTURE_SETTINGS_DIALOG_H
