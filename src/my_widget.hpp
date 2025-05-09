#pragma once

#include <QWidget>
#include <rclcpp/rclcpp.hpp>

namespace Ui {
class simWidgetUi;
}

class MyWidget : public QWidget
{
  Q_OBJECT

public:
  explicit MyWidget(QWidget *parent = nullptr);
  ~MyWidget();


private:
  void GetSpawnables();
  void SpawnButton();
  void GetAllEntities();
  void GetEntityState(bool silent = false);
  void SetEntityState();
  void DespawnButton();
  void GetSimFeatures();
  void ResetSimulation();
  void StepSimulation();
  void GetSimulationState();
  void SetSimulationState();
  void StepSimulationService();


  void ActionThreadWorker(int steps);
  std::thread actionThread_;

  Ui::simWidgetUi *ui_;
  rclcpp::Node::SharedPtr node_;
};

