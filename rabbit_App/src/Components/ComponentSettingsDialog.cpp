#include <QFile>
#include <QHeaderView>
#include <QPushButton>

#include "Components/AbstractComponent.h"
#include "Components/ComponentSettingsDialog.h"
#include "Ports/PinInfo.h"
#include "Ports/Port.h"
#include "Ports/PortsFileReader.h"
#include "qboxlayout.h"
#include "qcombobox.h"
#include "qgroupbox.h"
#include "qlabel.h"
#include "qmessagebox.h"
#include "qstandarditemmodel.h"

using namespace rabbit_App::component;

const QMap<QString, QColor> ComponentSettingsDialog::all_supported_color = {
    {tr("Red"), Qt::red},     {tr("Green"), Qt::green},
    {tr("Blue"), Qt::blue},   {tr("Yellow"), Qt::yellow},
    {tr("Black"), Qt::black}, {tr("White"), Qt::white}};

ComponentSettingsDialog::ComponentSettingsDialog(AbstractComponent *component,
                                                 QWidget *parent)
    : QDialog(parent), component_(component) {
  initPortsReader();
  initUi();
  initConnections();
}

ComponentSettingsDialog::~ComponentSettingsDialog() {}

void ComponentSettingsDialog::appendSettingWidget(QWidget *widget) {
  basic_settings_layout_->addWidget(widget);
}

void ComponentSettingsDialog::appendSettingLayout(QLayout *layout) {
  basic_settings_layout_->addLayout(layout);
}

void ComponentSettingsDialog::initPortsReader() {
  ports_file_reader_ = new rabbit_App::ports::PortsFileReader(this);
  component_->askForConstraintFilePath();
  // check if the file exists
  QFile file(component_->constraintFilePath());
  if (!file.exists()) {
    QMessageBox::warning(this, tr("Warning"), tr("Constraint file not found!"));
    return;
  }
  ports_file_reader_->readFromFile(component_->constraintFilePath());
}

void ComponentSettingsDialog::initUi() {
  auto type = component_->componentType();
  setWindowTitle(type + tr(" Settings"));
  setMinimumWidth(kWindowMinWidth);

  QLabel *basic_settings_label = new QLabel("Basic Settings", this);

  basic_settings_group_ = new QGroupBox(this);
  basic_settings_layout_ = new QVBoxLayout(basic_settings_group_);

  QLabel *component_name_label =
      new QLabel("Component Name: ", basic_settings_group_);
  component_name_edit_ =
      new QLineEdit(component_->componentName(), basic_settings_group_);
  auto name_layout = new QHBoxLayout();
  name_layout->addWidget(component_name_label);
  name_layout->addWidget(component_name_edit_);
  basic_settings_layout_->addLayout(name_layout);

  QLabel *port_settings_label = new QLabel("Port Settings", this);

  initTable();

  ok_button_ = new QPushButton(tr("OK"), this);
  cancel_button_ = new QPushButton(tr("Cancel"), this);

  auto main_layout = new QVBoxLayout(this);
  auto button_layout = new QHBoxLayout();
  button_layout->addWidget(ok_button_);
  button_layout->addWidget(cancel_button_);
  main_layout->addWidget(basic_settings_label);
  main_layout->addWidget(basic_settings_group_);
  main_layout->addWidget(port_settings_label);
  main_layout->addWidget(table_view_);
  main_layout->addLayout(button_layout);
  main_layout->setContentsMargins(10, 10, 10, 10);
}

void ComponentSettingsDialog::initConnections() {
  connect(ok_button_, &QPushButton::clicked, this,
          &ComponentSettingsDialog::accept);
  connect(cancel_button_, &QPushButton::clicked, this,
          &ComponentSettingsDialog::reject);
}

void ComponentSettingsDialog::initTable() {

  table_view_ = new QTableView(this);
  table_view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  model_ = new QStandardItemModel(this);

  model_->setColumnCount(4);
  const QVector<QString> header_name = {tr("Component Port Name"),
                                        tr("Port Type"), tr("Pin Name"),
                                        tr("HDL Port Name")};
  for (int i = 0; i < header_name.size(); ++i) {
    model_->setHeaderData(i, Qt::Horizontal, header_name[i]);
  }
  table_view_->verticalHeader()->hide();

  table_view_->setModel(model_);

  auto *header = table_view_->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::ResizeToContents);
  header->setMinimumWidth(80);
  header->setStretchLastSection(true);

  auto raw_component = component_->rawComponent();
  addRow(raw_component->inputPorts(), ports::PortType::Input);
  addRow(raw_component->outputPorts(), ports::PortType::Output);
}

void ComponentSettingsDialog::addRow(const QVector<ports::Port> &ports,
                                     ports::PortType type) {

  auto &choosable_ports = (type == ports::PortType::Input)
                              ? ports_file_reader_->inputs()
                              : ports_file_reader_->outputs();

  for (auto &port : ports) {
    int row_index = model_->rowCount();

    QList<QStandardItem *> row_items;
    row_items.append(createItem(port.name));
    row_items.append(createItem(ports::portTypeToString(port.type)));
    model_->appendRow(row_items);

    auto combobox = creatCombobox(choosable_ports);
    QString combobox_text;
    if (!port.pin_name.isEmpty() &&
        ports::declIndexMap(port.pin_name).second != -1) {
      combobox_text = findHdlPortName(choosable_ports, port.pin_name);
    } else {
      combobox_text = "None";
    }
    combobox->setCurrentText(combobox_text);
    original_ports_names_.push_back(combobox_text);
    auto pin_name_label = new QLabel(port.pin_name, this);
    pin_name_label->setStyleSheet("color:grey");
    pin_name_label->setAlignment(Qt::AlignCenter);
    table_view_->setIndexWidget(model_->index(row_index, 2), pin_name_label);
    table_view_->setIndexWidget(model_->index(row_index, 3), combobox);
    connect(combobox, &QComboBox::currentTextChanged, this,
            [=](const QString &text) {
              pin_name_label->setText(
                  ports_file_reader_->findPort(text, type).pin_name);
            });
  }
}

QComboBox *
ComponentSettingsDialog::creatCombobox(const QVector<ports::Port> &map) {
  auto combobox = new QComboBox(table_view_);

  for (auto &port : map) {
    combobox->addItem(port.name);
  }
  combobox->addItem(tr("None"));
  return combobox;
}

QStandardItem *ComponentSettingsDialog::createItem(const QString &text) {
  auto item = new QStandardItem(text);
  item->setTextAlignment(Qt::AlignCenter);
  item->setEditable(false);
  return item;
}

const QString
ComponentSettingsDialog::findHdlPortName(const QVector<ports::Port> &map,
                                         const QString &pin_name) {
  for (auto &port : map) {
    if (port.pin_name == pin_name) {
      return port.name;
    }
  }
  throw(std::runtime_error(
      "ComponentSettingsDialog::findHdlPortName: pin name not found"));
}

void ComponentSettingsDialog::setHdlPortName(const QString &component_port_name,
                                             const QString &hdl_port_name) {
  // Find the row with the matching component port name
  for (int i = 0; i < model_->rowCount(); i++) {
    auto pname = model_->item(i, 0)->text();
    if (pname == component_port_name) {
      auto type = model_->item(i, 1)->text();
      auto port_type = ports::stringToPortType(type);

      // Get the combo box and set the value
      auto combo = qobject_cast<QComboBox *>(table_view_->indexWidget(model_->index(i, 3)));
      if (combo) {
        combo->setCurrentText(hdl_port_name);
      }

      // Update the pin name label
      auto &ports_vec = (port_type == ports::PortType::Input)
                            ? ports_file_reader_->inputs()
                            : ports_file_reader_->outputs();
      QString pin_name;
      for (auto &port : ports_vec) {
        if (port.name == hdl_port_name) {
          pin_name = port.pin_name;
          break;
        }
      }
      auto label = qobject_cast<QLabel *>(table_view_->indexWidget(model_->index(i, 2)));
      if (label) {
        label->setText(pin_name);
      }

      break;
    }
  }
}

void ComponentSettingsDialog::accept() {
  auto raw_component = component_->rawComponent();
  auto &inputs_vec = raw_component->inputPorts();
  auto &outputs_vec = raw_component->outputPorts();
  for (int i = 0; i < model_->rowCount(); i++) {
    auto component_pname = model_->item(i, 0)->text();
    auto port_type = model_->item(i, 1)->text();
    auto pin_name =
        qobject_cast<QLabel *>(table_view_->indexWidget(model_->index(i, 2)))
            ->text();
    auto hdl_pname =
        qobject_cast<QComboBox *>(table_view_->indexWidget(model_->index(i, 3)))
            ->currentText();
    if (hdl_pname != original_ports_names_[i]) {
      is_modifieds_ = true;
    }
    bool is_none = (hdl_pname == tr("None"));
    switch (ports::stringToPortType(port_type)) {
    case ports::PortType::Input: {
      auto &port = inputs_vec[i];
      port.pin_name = is_none ? "" : pin_name;
      port.pin_index = ports::inputDeclIndexMap(pin_name);
      break;
    }
    case ports::PortType::Output: {
      auto &port_2 = outputs_vec[i];
      port_2.pin_name = is_none ? "" : pin_name;
      port_2.pin_index = ports::outputDeclIndexMap(pin_name);
      break;
    }
    default:
      throw(std::runtime_error(
          "ComponentSettingsDialog::accept: port type error"));
    }
  }
  if (component_->componentName() != component_name_edit_->text()) {
    component_->setComponentName(component_name_edit_->text());
    is_modifieds_ = true;
  }
  if (is_modifieds_) {
    component_->settingsChanged();
  }
  QDialog::accept();
}

ActiveModeSettingsFeatureWidget::ActiveModeSettingsFeatureWidget(
    AbstractComponent *component, QWidget *parent)
    : SettingsFeatureWidget<ActiveModeSettingsFeatureWidget>(parent) {
  QHBoxLayout *main_layout = new QHBoxLayout(this);
  QGroupBox *active_mode_group_box = new QGroupBox(tr("Active Mode"), this);
  QHBoxLayout *active_mode_layout = new QHBoxLayout(active_mode_group_box);
  active_high_radio_button_ =
      new QRadioButton(tr("High Active"), active_mode_group_box);
  active_high_radio_button_->setChecked(
      !component->rawComponent()->isLowActive());
  QRadioButton *active_low_radio_button_ =
      new QRadioButton(tr("Low Active"), active_mode_group_box);
  active_low_radio_button_->setChecked(
      component->rawComponent()->isLowActive());
  active_mode_layout->addWidget(active_high_radio_button_);
  active_mode_layout->addWidget(active_low_radio_button_);
  main_layout->addWidget(active_mode_group_box);
  setLayout(main_layout);
}

void ActiveModeSettingsFeatureWidget::accept(AbstractComponent *component) {
  if (active_high_radio_button_->isChecked() ==
      component->rawComponent()->isLowActive()) {
    component->rawComponent()->setLowActive(
        !active_high_radio_button_->isChecked());
  }
}

VisionPersistenceSettingsFeatureWidget::VisionPersistenceSettingsFeatureWidget(
    AbstractComponent *component, QWidget *parent)
    : SettingsFeatureWidget<VisionPersistenceSettingsFeatureWidget>(parent) {
  vision_persistence_edit_ = new QLineEdit(this);
  vision_persistence_edit_->setValidator(
      new QRegularExpressionValidator(QRegularExpression("[0-9]{1,4}")));
  vision_persistence_edit_->setText(
      QString::number(component->rawComponent()->visionPersistence()));
  QHBoxLayout *vision_persistence_layout = new QHBoxLayout();
  vision_persistence_layout->addWidget(
      new QLabel(tr("Vision Persistence: "), this));
  vision_persistence_layout->addWidget(vision_persistence_edit_);
  vision_persistence_layout->addWidget(new QLabel("ms", this));
  setLayout(vision_persistence_layout);
}

void VisionPersistenceSettingsFeatureWidget::accept(
    AbstractComponent *component) {
  auto text = vision_persistence_edit_->text();
  auto vision_persistence = vision_persistence_edit_->text().toUInt();
  if (static_cast<int>(vision_persistence) !=
      component->rawComponent()->visionPersistence()) {
    component->rawComponent()->setVisionPersistence(vision_persistence);
  }
}

ColorSettingsFeatureWidget::ColorSettingsFeatureWidget(
    AbstractComponent *component, QWidget *parent)
    : SettingsFeatureWidget<ColorSettingsFeatureWidget>(parent) {
  auto main_layout = new QHBoxLayout(this);
  QGroupBox *colors_group_box = new QGroupBox(tr("Colors"), this);
  QGridLayout *colors_layout = new QGridLayout(colors_group_box);
  auto &supported_colors = component->rawComponent()->supportedColors();
  auto &colors_map = component->rawComponent()->componentColors();
  for (auto itor = colors_map.begin(); itor != colors_map.end(); ++itor) {
    auto color_usage = itor.key();
    auto color = itor.value();
    QLabel *color_label = new QLabel(color_usage, this);
    QComboBox *color_combo_box = new QComboBox(this);
    for (auto supported_color : supported_colors) {
      color_combo_box->addItem(
          component->rawComponent()->supportedColors().key(supported_color));
    }
    color_combo_box->setCurrentText(
        component->rawComponent()->supportedColors().key(color));
    color_map_[color_usage] = color_combo_box;
    colors_layout->addWidget(color_label, colors_layout->rowCount(), 0);
    colors_layout->addWidget(color_combo_box, colors_layout->rowCount() - 1, 1);
  }
  main_layout->addWidget(colors_group_box);
  setLayout(main_layout);
}

void ColorSettingsFeatureWidget::accept(AbstractComponent *component) {
  auto &colors_map = component->rawComponent()->componentColors();
  for (auto itor = colors_map.begin(); itor != colors_map.end(); ++itor) {
    auto color_usage = itor.key();
    auto color_combo_box = color_map_.value(color_usage);
    if (colors_map[color_usage] !=
        ComponentSettingsDialog::all_supported_color.value(
            color_combo_box->currentText())) {
      colors_map[color_usage] =
          ComponentSettingsDialog::all_supported_color.value(
              color_combo_box->currentText());
    }
  }
}

ArrayPortMappingSettingsFeatureWidget::ArrayPortMappingSettingsFeatureWidget(
    AbstractComponent *component, QWidget *parent)
    : SettingsFeatureWidget<ArrayPortMappingSettingsFeatureWidget>(parent),
      component_(component),
      bit_width_(0) {

  auto main_layout = new QVBoxLayout(this);

  // Type selection group (Input or Output)
  auto type_group = new QGroupBox(tr("Port Type"), this);
  auto type_layout = new QHBoxLayout(type_group);
  input_radio_ = new QRadioButton(tr("Input"), this);
  output_radio_ = new QRadioButton(tr("Output"), this);
  input_radio_->setChecked(true);
  type_layout->addWidget(input_radio_);
  type_layout->addWidget(output_radio_);
  type_layout->addStretch();
  main_layout->addWidget(type_group);

  // HDL Array Prefix selection
  auto hdl_layout = new QHBoxLayout();
  hdl_layout->addWidget(new QLabel(tr("HDL Array Prefix:"), this));
  hdl_array_prefix_combobox_ = new QComboBox(this);
  hdl_array_prefix_combobox_->setEditable(true);
  hdl_layout->addWidget(hdl_array_prefix_combobox_);
  main_layout->addLayout(hdl_layout);

  // Pin Prefix selection
  auto pin_layout = new QHBoxLayout();
  pin_layout->addWidget(new QLabel(tr("Pin Prefix:"), this));
  pin_prefix_edit_ = new QLineEdit(this);
  pin_layout->addWidget(pin_prefix_edit_);
  main_layout->addLayout(pin_layout);

  // Range selection
  auto range_layout = new QHBoxLayout();
  range_layout->addWidget(new QLabel(tr("Range (e.g. 0-31):"), this));
  range_edit_ = new QLineEdit(this);
  range_edit_->setPlaceholderText("0-31");
  range_layout->addWidget(range_edit_);
  main_layout->addLayout(range_layout);

  // Bit width info
  bit_width_label_ = new QLabel(tr("Bit Width: --"), this);
  main_layout->addWidget(bit_width_label_);

  // Auto map button
  auto_map_btn_ = new QPushButton(tr("Auto Map Ports"), this);
  main_layout->addWidget(auto_map_btn_);

  main_layout->addStretch();
  setLayout(main_layout);

  // Connect signals
  connect(auto_map_btn_, &QPushButton::clicked, this,
          &ArrayPortMappingSettingsFeatureWidget::onAutoMapClicked);
  connect(input_radio_, &QRadioButton::toggled, this,
          &ArrayPortMappingSettingsFeatureWidget::onTypeChanged);
  connect(hdl_array_prefix_combobox_, &QComboBox::currentTextChanged, this,
          &ArrayPortMappingSettingsFeatureWidget::onArrayPrefixChanged);
  connect(pin_prefix_edit_, &QLineEdit::textChanged, this,
          &ArrayPortMappingSettingsFeatureWidget::onPinPrefixChanged);
  connect(range_edit_, &QLineEdit::textChanged, this,
          &ArrayPortMappingSettingsFeatureWidget::onPinPrefixChanged);

  // Initialize combo box with available ports from constraint file
  populateArrayPrefixComboBox();
}

void ArrayPortMappingSettingsFeatureWidget::populateArrayPrefixComboBox() {
  QString constraint_path = component_->constraintFilePath();
  if (constraint_path.isEmpty()) {
    return;
  }

  rabbit_App::ports::PortsFileReader reader;
  reader.readFromFile(constraint_path);

  auto &ports = input_radio_->isChecked() ? reader.inputs() : reader.outputs();

  // Extract unique array prefixes from ports
  QSet<QString> prefixes;
  for (auto &port : ports) {
    // Check if port name matches pattern like "data[0]", "data[1]", etc.
    QRegularExpression regex(R"(^(\w+)\[)");
    QRegularExpressionMatch match = regex.match(port.name);
    if (match.hasMatch()) {
      prefixes.insert(match.captured(1));
    }
  }

  // Populate combo box
  hdl_array_prefix_combobox_->clear();
  for (auto &prefix : prefixes) {
    hdl_array_prefix_combobox_->addItem(prefix);
  }
}

void ArrayPortMappingSettingsFeatureWidget::onTypeChanged(bool checked) {
  Q_UNUSED(checked);
  populateArrayPrefixComboBox();
  updateBitWidth();
}

void ArrayPortMappingSettingsFeatureWidget::onArrayPrefixChanged(const QString &text) {
  Q_UNUSED(text);
  updateBitWidth();
}

void ArrayPortMappingSettingsFeatureWidget::onPinPrefixChanged(const QString &text) {
  Q_UNUSED(text);
  updateBitWidth();
}

void ArrayPortMappingSettingsFeatureWidget::updateBitWidth() {
  QString constraint_path = component_->constraintFilePath();
  if (constraint_path.isEmpty()) {
    bit_width_ = 0;
    bit_width_label_->setText("Bit Width: --");
    return;
  }

  rabbit_App::ports::PortsFileReader reader;
  reader.readFromFile(constraint_path);

  QString hdl_prefix = hdl_array_prefix_combobox_->currentText();
  QString pin_prefix = pin_prefix_edit_->text();

  auto &hdl_ports = input_radio_->isChecked() ? reader.inputs() : reader.outputs();
  auto &pin_ports = input_radio_->isChecked()
                        ? component_->rawComponent()->inputPorts()
                        : component_->rawComponent()->outputPorts();

  int hdl_count = 0;
  int pin_count = 0;

  // Count HDL ports matching the prefix
  for (auto &port : hdl_ports) {
    if (port.name.startsWith(hdl_prefix + "[") || port.name == hdl_prefix) {
      hdl_count++;
    }
  }

  // Count pin ports matching the prefix (extract prefix from pin name)
  for (auto &port : pin_ports) {
    if (extractPrefix(port.name) == pin_prefix) {
      pin_count++;
    }
  }

  int count = qMin(hdl_count, pin_count);
  if (count > 0) {
    bit_width_ = count;
    bit_width_label_->setText(QString("Bit Width: %1").arg(bit_width_));
  } else {
    bit_width_ = 0;
    bit_width_label_->setText("Bit Width: --");
  }
}

QList<QPair<int, int>> ArrayPortMappingSettingsFeatureWidget::parseRange(const QString &range_str) {
  QList<QPair<int, int>> result;
  if (range_str.isEmpty()) {
    return result;
  }

  QStringList parts = range_str.split(",", Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    QString trimmed = part.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }

    // Check for range like "0-31" or "31-0"
    if (trimmed.contains("-")) {
      QStringList range_parts = trimmed.split("-", Qt::SkipEmptyParts);
      if (range_parts.size() == 2) {
        bool ok1, ok2;
        int start = range_parts[0].trimmed().toInt(&ok1);
        int end = range_parts[1].trimmed().toInt(&ok2);
        if (ok1 && ok2) {
          result.append({start, end});
        }
      }
    } else {
      // Single number like "5"
      bool ok;
      int val = trimmed.toInt(&ok);
      if (ok) {
        result.append({val, val});
      }
    }
  }
  return result;
}

QString ArrayPortMappingSettingsFeatureWidget::extractPrefix(const QString &pin_name) {
  // Extract prefix: "DIN5" -> "DIN", "RW" -> "RW"
  QRegularExpression regex(R"(^([A-Za-z_]+))");
  QRegularExpressionMatch match = regex.match(pin_name);
  if (match.hasMatch()) {
    return match.captured(1);
  }
  return pin_name;
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

  QString hdl_prefix = hdl_array_prefix_combobox_->currentText();
  QString pin_prefix = pin_prefix_edit_->text();
  QString range_str = range_edit_->text();

  if (hdl_prefix.isEmpty()) {
    QMessageBox::warning(this, tr("Warning"), tr("Please specify HDL Array Prefix."));
    return;
  }
  if (pin_prefix.isEmpty()) {
    QMessageBox::warning(this, tr("Warning"), tr("Please specify Pin Prefix."));
    return;
  }

  auto &hdl_ports = input_radio_->isChecked() ? reader.inputs() : reader.outputs();
  auto &pin_ports = input_radio_->isChecked()
                        ? component_->rawComponent()->inputPorts()
                        : component_->rawComponent()->outputPorts();

  // Build a map of pin port name -> pin port for quick lookup
  QMap<QString, ports::Port> pin_port_map;
  for (auto &port : pin_ports) {
    pin_port_map[port.name] = port;
  }

  int mapped_count = 0;

  // Parse range or use default
  QList<QPair<int, int>> ranges = parseRange(range_str);
  if (ranges.isEmpty()) {
    // Default: find all indices from both HDL and pin ports
    QList<int> indices;
    for (auto &port : hdl_ports) {
      if (port.name.startsWith(hdl_prefix + "[")) {
        QRegularExpression regex(R"(\d+)");
        QRegularExpressionMatch match = regex.match(port.name);
        if (match.hasMatch()) {
          indices.append(match.captured(0).toInt());
        }
      }
    }
    if (!indices.isEmpty()) {
      ranges.append({indices.front(), indices.back()});
    }
  }

  // For each range, map the ports
  for (auto &range : ranges) {
    int start = range.first;
    int end = range.second;
    int step = (start <= end) ? 1 : -1;
    int range_size = qAbs(end - start) + 1;

    // Pin index goes from 0 to range_size-1
    for (int pin_idx = 0; pin_idx < range_size; pin_idx++) {
      // HDL index follows the range direction
      int hdl_idx = start + pin_idx * step;

      // Find the HDL port
      QString hdl_port_name = QString("%1[%2]").arg(hdl_prefix).arg(hdl_idx);
      bool hdl_port_exists = false;
      for (auto &port : hdl_ports) {
        if (port.name == hdl_port_name) {
          hdl_port_exists = true;
          break;
        }
      }
      if (!hdl_port_exists) {
        continue;
      }

      // Find the pin port with matching prefix and index
      QString pin_port_name_exact = QString("%1%2").arg(pin_prefix).arg(pin_idx);
      // Also check for ports without number suffix (like "CAPTURE" mapping to index 0)
      QString pin_port_name_no_suffix = pin_prefix;

      QString matched_pin_name;
      if (pin_port_map.contains(pin_port_name_exact)) {
        matched_pin_name = pin_port_name_exact;
      } else if (pin_idx == 0 && pin_port_map.contains(pin_port_name_no_suffix)) {
        matched_pin_name = pin_port_name_no_suffix;
      }

      if (!matched_pin_name.isEmpty()) {
        parent_dialog_->setHdlPortName(matched_pin_name, hdl_port_name);
        mapped_count++;
      }
    }
  }

  if (mapped_count > 0) {
    QMessageBox::information(this, tr("Success"),
                             QString("Mapped %1 ports successfully!").arg(mapped_count));
  } else {
    QMessageBox::warning(this, tr("Warning"),
                        tr("No ports were mapped. Check the prefix names."));
  }
}

void ArrayPortMappingSettingsFeatureWidget::accept(AbstractComponent *component) {
  Q_UNUSED(component);
}
