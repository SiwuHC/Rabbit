#include <QMessageBox>

#include "Components/ArrayPortMappingSettingsDialog.h"
#include "Components/ComponentSettingsDialog.h"
#include "Ports/PinInfo.h"
#include "Ports/PortsFileReader.h"

using namespace rabbit_App::component;

ArrayPortMappingSettingsFeatureWidget::ArrayPortMappingSettingsFeatureWidget(
    AbstractComponent *component, QWidget *parent)
    : SettingsFeatureWidget<ArrayPortMappingSettingsFeatureWidget>(parent),
      component_(component),
      bit_width_(0) {

  auto main_layout = new QVBoxLayout(this);

  // Array mapping group
  auto array_group = new QGroupBox(tr("Array Port Mapping"), this);
  auto array_layout = new QVBoxLayout(array_group);

  // Input array selection
  auto input_layout = new QHBoxLayout();
  input_layout->addWidget(new QLabel(tr("Input Array Prefix:"), this));
  input_array_combobox_ = new QComboBox(this);
  input_array_combobox_->setEditable(true);
  input_layout->addWidget(input_array_combobox_);
  array_layout->addLayout(input_layout);

  // Output array selection
  auto output_layout = new QHBoxLayout();
  output_layout->addWidget(new QLabel(tr("Output Array Prefix:"), this));
  output_array_combobox_ = new QComboBox(this);
  output_array_combobox_->setEditable(true);
  output_layout->addWidget(output_array_combobox_);
  array_layout->addLayout(output_layout);

  // Bit width info
  bit_width_label_ = new QLabel(tr("Bit Width: --"), this);
  array_layout->addWidget(bit_width_label_);

  // Auto map button
  auto_map_btn_ = new QPushButton(tr("Auto Map Ports"), this);
  array_layout->addWidget(auto_map_btn_);

  main_layout->addWidget(array_group);
  setLayout(main_layout);

  // Connect signals
  connect(auto_map_btn_, &QPushButton::clicked, this,
          &ArrayPortMappingSettingsFeatureWidget::onAutoMapClicked);
  connect(input_array_combobox_, &QComboBox::currentTextChanged, this,
          &ArrayPortMappingSettingsFeatureWidget::onArrayNameChanged);
  connect(output_array_combobox_, &QComboBox::currentTextChanged, this,
          &ArrayPortMappingSettingsFeatureWidget::onArrayNameChanged);

  // Initialize combo boxes with available ports from constraint file
  populatePortComboBoxes();
}

void ArrayPortMappingSettingsFeatureWidget::populatePortComboBoxes() {
  // Get constraint file path
  QString constraint_path = component_->constraintFilePath();
  if (constraint_path.isEmpty()) {
    return;
  }

  // Create ports reader to get available ports
  rabbit_App::ports::PortsFileReader reader;
  reader.readFromFile(constraint_path);

  auto &inputs = reader.inputs();
  auto &outputs = reader.outputs();

  // Extract unique array prefixes from input ports
  QSet<QString> input_prefixes;
  for (auto &port : inputs) {
    // Check if port name matches pattern like "data[0]", "data[1]", etc.
    QRegularExpression regex(R"(^(\w+)\[)");
    QRegularExpressionMatch match = regex.match(port.name);
    if (match.hasMatch()) {
      input_prefixes.insert(match.captured(1));
    }
  }

  // Extract unique array prefixes from output ports
  QSet<QString> output_prefixes;
  for (auto &port : outputs) {
    QRegularExpression regex(R"(^(\w+)\[)");
    QRegularExpressionMatch match = regex.match(port.name);
    if (match.hasMatch()) {
      output_prefixes.insert(match.captured(1));
    }
  }

  // Populate combo boxes
  for (auto &prefix : input_prefixes) {
    input_array_combobox_->addItem(prefix);
  }
  for (auto &prefix : output_prefixes) {
    output_array_combobox_->addItem(prefix);
  }
}

void ArrayPortMappingSettingsFeatureWidget::onArrayNameChanged(const QString &text) {
  Q_UNUSED(text);
  // Update bit width based on available ports
  QString constraint_path = component_->constraintFilePath();
  if (constraint_path.isEmpty()) {
    return;
  }

  rabbit_App::ports::PortsFileReader reader;
  reader.readFromFile(constraint_path);

  QString input_prefix = input_array_combobox_->currentText();
  QString output_prefix = output_array_combobox_->currentText();

  int input_count = 0;
  int output_count = 0;

  // Count matching input ports
  for (auto &port : reader.inputs()) {
    if (port.name.startsWith(input_prefix + "[") || port.name == input_prefix) {
      input_count++;
    }
  }

  // Count matching output ports
  for (auto &port : reader.outputs()) {
    if (port.name.startsWith(output_prefix + "[") || port.name == output_prefix) {
      output_count++;
    }
  }

  int max_count = qMax(input_count, output_count);
  if (max_count > 0) {
    bit_width_ = max_count;
    bit_width_label_->setText(QString("Bit Width: %1").arg(bit_width_));
  } else {
    bit_width_ = 0;
    bit_width_label_->setText("Bit Width: --");
  }
}

void ArrayPortMappingSettingsFeatureWidget::onAutoMapClicked() {
  if (!parent_dialog_) {
    QMessageBox::warning(this, tr("Warning"), tr("Parent dialog not set!"));
    return;
  }

  QString constraint_path = component_->constraintFilePath();
  if (constraint_path.isEmpty()) {
    QMessageBox::warning(this, tr("Warning"), tr("Constraint file not found!"));
    return;
  }

  rabbit_App::ports::PortsFileReader reader;
  reader.readFromFile(constraint_path);

  QString input_prefix = input_array_combobox_->currentText();
  QString output_prefix = output_array_combobox_->currentText();

  auto raw_component = component_->rawComponent();
  auto &inputs_vec = raw_component->inputPorts();
  auto &outputs_vec = raw_component->outputPorts();

  int mapped_count = 0;

  // Map input ports
  for (auto &port : inputs_vec) {
    // Extract index from port name (e.g., "DIN5" -> 5)
    QRegularExpression regex(R"(DIN(\d+))");
    QRegularExpressionMatch match = regex.match(port.name);
    if (match.hasMatch()) {
      int index = match.captured(1).toInt();
      QString array_port_name = input_prefix.isEmpty()
                                    ? QString()
                                    : QString("%1[%2]").arg(input_prefix).arg(index);

      if (!array_port_name.isEmpty()) {
        // Find matching port in constraint file
        for (auto &hdl_port : reader.inputs()) {
          if (hdl_port.name == array_port_name) {
            // Update the table's combo box
            parent_dialog_->setHdlPortName(port.name, hdl_port.name);
            mapped_count++;
            break;
          }
        }
      }
    }
  }

  // Map output ports
  for (auto &port : outputs_vec) {
    // Skip the first port (CAPTURE signal)
    if (port.name == "CAPTURE") {
      continue;
    }

    // Extract index from port name (e.g., "DOUT5" -> 5)
    QRegularExpression regex(R"(DOUT(\d+))");
    QRegularExpressionMatch match = regex.match(port.name);
    if (match.hasMatch()) {
      int index = match.captured(1).toInt();
      QString array_port_name = output_prefix.isEmpty()
                                    ? QString()
                                    : QString("%1[%2]").arg(output_prefix).arg(index);

      if (!array_port_name.isEmpty()) {
        // Find matching port in constraint file
        for (auto &hdl_port : reader.outputs()) {
          if (hdl_port.name == array_port_name) {
            // Update the table's combo box
            parent_dialog_->setHdlPortName(port.name, hdl_port.name);
            mapped_count++;
            break;
          }
        }
      }
    }
  }

  if (mapped_count > 0) {
    QMessageBox::information(this, tr("Success"),
                             QString("Mapped %1 ports successfully!").arg(mapped_count));
  } else {
    QMessageBox::warning(this, tr("Warning"),
                        tr("No ports were mapped. Check the array prefix names."));
  }
}

void ArrayPortMappingSettingsFeatureWidget::accept(AbstractComponent *component) {
  // Mapping is already done in onAutoMapClicked
  // This function is called when the dialog is accepted
  Q_UNUSED(component);
}

QString ArrayPortMappingSettingsFeatureWidget::getArrayElementName(
    const QString &array_name, int index) {
  return QString("%1[%2]").arg(array_name).arg(index);
}
