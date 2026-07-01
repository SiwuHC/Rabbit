#pragma once
#ifndef NEWKEYBOARD_COMPONENT_H
#define NEWKEYBOARD_COMPONENT_H

#include <QLabel>
#include <QRadioButton>
#include <queue>
#include <array>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"
#include "Components/ComponentSettingsDialog.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(NewKeyboard)


/// @brief NewKeyboardRawComponent class
/// This class implements the NewKeyboard component.
class NewKeyboardRawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  NewKeyboardRawComponent(QWidget *parent = nullptr);
  virtual ~NewKeyboardRawComponent();

  void reset() override;

  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void initPorts() override;
  //special functions 
  
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;

  
  void initConnections();

  const QPixmap *keyboard_picture_;
  QLabel *keyboard_picture_label_;

  static const QPixmap &keyboardPictureStatic() {
    static const QPixmap kb_picture_ =
        QPixmap(":/icons/icons/icons8-nkeyboard-94.png");
    return kb_picture_;
  };


  static const QPixmap &keyboardPictureOnFocus() {
    static const QPixmap kb_picture_ =
        QPixmap(":/icons/icons/icons8-nkeyboard-onfocus-94.png");
    return kb_picture_;
  };

  static const QPixmap &keyboardPictureInUse() {
    static const QPixmap kb_inuse_picture_ =
        QPixmap(":/icons/icons/icons8-nkeyboard-inuse-94.png");
    return kb_inuse_picture_;
  };


  auto keyMapIndex(const int &keyType) -> int;

  bool checkSpecialKey2(int keyValue);
  void pclkEnqueue(int cycle, bool data);
  void pdataEnqueue(int cycle, std::array<bool,9> data);
  void pclkWriteDataGenerate(int keyValue);
  void pdataWriteDataGenerate(int keyValue);


signals:
  void keyPressed(int keyType, int state);
  void keyReleased(int keyType, int state);


protected slots:
  void keyProcess(int keyType, int state);


private:
  int key_pressed_num_ = 0;
  bool item_foucused_ = false;
  mutable std::queue<bool> pclk_to_write_;
  mutable std::queue<std::array<bool,9>> pdata_to_write_;

};


} 

#endif // NewKEYBOARD_COMPONENT_H
