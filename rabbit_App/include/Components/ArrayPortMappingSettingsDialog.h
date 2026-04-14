#pragma once
#ifndef ARRAY_PORT_MAPPING_SETTINGS_DIALOG_H
#define ARRAY_PORT_MAPPING_SETTINGS_DIALOG_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "Components/AbstractComponent.h"
#include "Components/ComponentSettingsDialog.h"
#include "Ports/PinInfo.h"
#include "Ports/Port.h"
#include "Ports/PortsFileReader.h"

namespace rabbit_App::component {

/// @brief Settings feature widget for array port mapping
/// This widget allows users to specify an array name prefix (e.g., "data")
/// and automatically maps all ports like data[0], data[1], ... to component ports.
class ArrayPortMappingSettingsFeatureWidget
    : public SettingsFeatureWidget<ArrayPortMappingSettingsFeatureWidget> {
  Q_OBJECT

public:
  ArrayPortMappingSettingsFeatureWidget(AbstractComponent *component,
                                        QWidget *parent = nullptr);
  void accept(AbstractComponent *component);

  /// @brief Set the parent settings dialog for updating table combo boxes
  void setParentDialog(ComponentSettingsDialog *dialog) { parent_dialog_ = dialog; }

private slots:
  void onAutoMapClicked();
  void onArrayNameChanged(const QString &text);

private:
  void populatePortComboBoxes();
  QString getArrayElementName(const QString &array_name, int index);

private:
  AbstractComponent *component_;
  ComponentSettingsDialog *parent_dialog_ = nullptr;
  QLineEdit *array_name_edit_;
  QPushButton *auto_map_btn_;
  QLabel *bit_width_label_;
  QComboBox *input_array_combobox_;
  QComboBox *output_array_combobox_;
  QVector<QComboBox *> input_port_comboboxes_;
  QVector<QComboBox *> output_port_comboboxes_;
  int bit_width_;
};

} // namespace rabbit_App::component

#endif // ARRAY_PORT_MAPPING_SETTINGS_DIALOG_H
