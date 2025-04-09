#include "my_widget.hpp"
#include "ui_sim_widget.h"  // auto-generated by uic
#include <QMessageBox>
#include "service.h"
#include <tf2/LinearMath/Quaternion.h>
#include <QDebug>
#include <rclcpp_action/create_client.hpp>
#include <simulation_interfaces/action/simulate_steps.hpp>
#include "stringToKeys.h"


QString VectorToQstring (const tf2::Vector3& v )
{
    QString qString = QString::asprintf("%f %f %f", v.x(), v.y(), v.z());
    return qString;
}

tf2::Vector3 QStringToVector (const QString& line )
{
    tf2::Vector3 v;
    QStringList list = line.split(" ");
    if (list.size() == 3) {
        v.setX(list[0].toDouble());
        v.setY(list[1].toDouble());
        v.setZ(list[2].toDouble());
    } else {
        qWarning() << "Invalid input format. Expected 3 values.";
        v = tf2::Vector3(0, 0, 0); // Default to zero vector
    }
    return v;
}


MyWidget::MyWidget(QWidget *parent)
        : QWidget(parent),
          ui_(new Ui::simWidgetUi) {
    ui_->setupUi(this);
    node_ = rclcpp::Node::make_shared("qt_gui_node");

    // propagate reset scope combo
    for (const auto& [name, _] : ScopeNameToId)
    {
        ui_->resetModeCombo->addItem(QString::fromStdString(name));
    }
    // propagate state combo
    for (const auto& [name, _] : SimStateNameToId)
    {
        ui_->simStateToSetComboBox->addItem(QString::fromStdString(name));
    }

    connect(ui_->PushButtonRefresh, &QPushButton::clicked, this, &MyWidget::GetSpawnables);
    connect(ui_->SpawnButton, &QPushButton::clicked, this, &MyWidget::SpawnButton);
    connect(ui_->getAllEntitiesButton, &QPushButton::clicked, this, &MyWidget::GetAllEntities);
    connect(ui_->getEntityStateButton, &QPushButton::clicked, this, &MyWidget::GetEntityState);
    connect(ui_->setEntityStateButton, &QPushButton::clicked, this, &MyWidget::SetEntityState);
    connect(ui_->despawnButton, &QPushButton::clicked, this, &MyWidget::DespawnButton);
    connect(ui_->despawnAll, &QPushButton::clicked, this, &MyWidget::DespawnnAll );
    connect(ui_->GetSimCapabilites, &QPushButton::clicked, this, &MyWidget::GetSimFeatures);
    connect(ui_->resetSimButton, &QPushButton::clicked, this, &MyWidget::ResetSimulation);
    connect(ui_->stepSimButtonAction, &QPushButton::clicked, this, &MyWidget::StepSimulation);
    connect(ui_->getSimStateBtn, &QPushButton::clicked, this, &MyWidget::GetSimulationState);
    connect(ui_->setSimStateButton, &QPushButton::clicked, this, &MyWidget::SetSimulationState);
    connect(ui_->stepSimServiceButton, &QPushButton::clicked, this, &MyWidget::StepSimulationService);
    connect(ui_->ComboEntities, &QComboBox::currentTextChanged, this, &MyWidget::GetEntityState);
}

MyWidget::~MyWidget() {
    delete ui_;
}


void MyWidget::GetSimulationState()
{
    Service<simulation_interfaces::srv::GetSimulationState> service("/get_simulation_state", node_);
    auto response = service.call_service_sync();
    if (response)
    {
        int stateId = response->state.state;
        QString stateName;
        auto it = SimStateIdToName.find(stateId);
        if (it == SimStateIdToName.end())
        {
            stateName = QString::asprintf("Unknow state %d", stateId);
        }
        else
        {
            stateName = QString::fromStdString(it->second);
        }
        ui_->simStateLabel->setText(stateName);
    }

}

void MyWidget::SetSimulationState()
{
    simulation_interfaces::srv::SetSimulationState::Request request;
    auto selectedMode = ui_->simStateToSetComboBox->currentText();
    auto it = SimStateNameToId.find(selectedMode.toStdString());
    Q_ASSERT(it != SimStateNameToId.end());
    Service<simulation_interfaces::srv::SetSimulationState> service("/set_simulation_state", node_);
    request.state.state = it->second;
    auto response = service.call_service_sync(request);
    if (response)
    {
        GetSimulationState();
    }
}


void MyWidget::ActionThreadWorker(int steps) {

    using SimulateSteps=simulation_interfaces::action::SimulateSteps;
    auto client = rclcpp_action::create_client<SimulateSteps>(node_, "/simulate_steps");

    auto send_goal_options = rclcpp_action::Client<SimulateSteps>::SendGoalOptions();
    auto goal = std::make_shared<SimulateSteps::Goal>();
    goal->steps = steps;

    send_goal_options.feedback_callback =
        [this](rclcpp_action::ClientGoalHandle<SimulateSteps>::SharedPtr goal_handle,
               const std::shared_ptr<const SimulateSteps::Feedback> feedback) {
            // Handle feedback here
            float progress = static_cast<float>(feedback->completed_steps) / feedback->remaining_steps;
            ui_->simProgressBar->setValue(static_cast<int>(progress * 100));
        };
    auto goal_handle_future = client->async_send_goal(*goal, send_goal_options);
    if (rclcpp::spin_until_future_complete(node_, goal_handle_future) != rclcpp::FutureReturnCode::SUCCESS) {
        RCLCPP_ERROR(node_->get_logger(), "Failed to call service");
        return;
    }
    auto goal_handle = goal_handle_future.get();
    auto result_future = client->async_get_result(goal_handle);
    if (rclcpp::spin_until_future_complete(node_, result_future) != rclcpp::FutureReturnCode::SUCCESS) {
        RCLCPP_ERROR(node_->get_logger(), "Failed to call service");
        return;
    }
    auto result = result_future.get();
    if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
        RCLCPP_INFO(node_->get_logger(), "Simulation completed successfully");
    } else {
        RCLCPP_ERROR(node_->get_logger(), "Simulation failed");
    }


}
void MyWidget::StepSimulation() {

    int steps = ui_->stepsSpinBox->value();

    if (actionThread_.joinable())
    {
        actionThread_.join();
    }
    actionThread_ = std::thread(&MyWidget::ActionThreadWorker, this, steps);
}

void MyWidget::ResetSimulation() {
    simulation_interfaces::srv::ResetSimulation::Request request;
    Service<simulation_interfaces::srv::ResetSimulation> service("/reset_simulation", node_);
    //get seletected mode
    auto selectedMode = ui_->resetModeCombo->currentText();
    auto it = ScopeNameToId.find(selectedMode.toStdString());
    Q_ASSERT(it != ScopeNameToId.end());
    request.scope = it->second;
    auto response = service.call_service_sync(request);

}

void MyWidget::GetSimFeatures()
{
    Service<simulation_interfaces::srv::GetSimulatorFeatures> service("/get_simulation_features", node_);
    auto response = service.call_service_sync();
    ui_->listCapabilities->clear();
    std::set<int> features;
    for (auto& feature : response->features.features)
    {
        features.emplace(feature);
        QString capabilityName;
        auto it = FeatureToName.find(feature);
        if (it == FeatureToName.end())
        {
            capabilityName = QString::asprintf("Unknow feature %d", feature);
            ui_->listCapabilities->addItem(capabilityName);
        }
    }
    for (auto &[featrueId, featureName] : FeatureToName)
    {
        const QString labelSupported = u8"✔️";
        const QString labelNotSupported = u8"❌";
        bool isSupported = features.find(featrueId) != features.end();
        QString label = QString(featureName.c_str()) + " : " + (isSupported ? labelSupported : labelNotSupported);
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setTextAlignment(Qt::AlignLeft);
        item->setText(label);
        item->setToolTip(QString::fromStdString(FeatureDescription.at(featrueId)));
        ui_->listCapabilities->addItem(item);
    }

}

void MyWidget::StepSimulationService() {
    simulation_interfaces::srv::StepSimulation::Request request;
    request.steps = ui_->stepsSpinBox->value();
    Service<simulation_interfaces::srv::StepSimulation> service("/step_simulation", node_);
    auto response = service.call_service_sync(request);
    if (response) {
        // Handle the response if needed
    }
}
void MyWidget::DespawnButton() {
    simulation_interfaces::srv::DeleteEntity::Request request;
    request.entity = ui_->ComboEntities->currentText().toStdString();
    Service<simulation_interfaces::srv::DeleteEntity> service("/delete_entity", node_);
    service.call_service_sync(request);
}

void MyWidget::DespawnnAll() {
    for (int i =0; i < ui_->ComboEntities->count(); i++)
    {
        simulation_interfaces::srv::DeleteEntity::Request request;
        request.entity = ui_->ComboEntities->itemText(i).toStdString();
        Service<simulation_interfaces::srv::DeleteEntity> service("/delete_entity", node_);
        service.call_service_sync(request);
    }

}

void MyWidget::GetAllEntities() {
    Service<simulation_interfaces::srv::GetEntities> service("/get_entities", node_);
    auto response = service.call_service_sync();
    if (response)
    {
        ui_->ComboEntities->clear();
        for (const auto &entity: response->entities) {
            ui_->ComboEntities->addItem(QString(entity.c_str()));
        }
    }
}

void MyWidget::GetEntityState() {
    simulation_interfaces::srv::GetEntityState::Request request;
    request.entity = ui_->ComboEntities->currentText().toStdString();

    Service<simulation_interfaces::srv::GetEntityState> service("/get_entity_state", node_);
    auto response = service.call_service_sync(request);
    if (response) {
        ui_->StatePosX->setValue(response->state.pose.position.x);
        ui_->StatePosY->setValue(response->state.pose.position.y);
        ui_->StatePosZ->setValue(response->state.pose.position.z);

        tf2::Quaternion q = tf2::Quaternion(
                response->state.pose.orientation.x,
                response->state.pose.orientation.y,
                response->state.pose.orientation.z,
                response->state.pose.orientation.w);

        const auto axis = q.getAxis();
        const auto angle = q.getAngle();
        ui_->RotVector->setText(VectorToQstring(axis));
        ui_->RotAngle->setValue(angle);

        ui_->StateVelX->setValue(response->state.twist.linear.x);
        ui_->StateVelY->setValue(response->state.twist.linear.y);
        ui_->StateVelZ->setValue(response->state.twist.linear.z);
        ui_->StateVelRotX->setValue(response->state.twist.angular.x);
        ui_->StateVelRotY->setValue(response->state.twist.angular.y);
        ui_->StateVelRotZ->setValue(response->state.twist.angular.z);
    }


}

void MyWidget::SetEntityState() {
    simulation_interfaces::srv::SetEntityState::Request request;
    request.entity = ui_->ComboEntities->currentText().toStdString();
    request.state.pose.position.x = ui_->StatePosX->value();
    request.state.pose.position.y = ui_->StatePosY->value();
    request.state.pose.position.z = ui_->StatePosZ->value();


    const QString vectorStr = ui_->RotVector->text();
    const double angle = ui_->RotAngle->value();
    const auto vector = QStringToVector(vectorStr);
    tf2::Quaternion q (vector,angle);

    request.state.pose.orientation.x = q.x();
    request.state.pose.orientation.y = q.y();
    request.state.pose.orientation.z = q.z();
    request.state.pose.orientation.w = q.w();


    request.state.twist.linear.x = ui_->StateVelX->value();
    request.state.twist.linear.y = ui_->StateVelY->value();
    request.state.twist.linear.z = ui_->StateVelZ->value();
    request.state.twist.angular.x = ui_->StateVelRotX->value();
    request.state.twist.angular.y = ui_->StateVelRotY->value();
    request.state.twist.angular.z = ui_->StateVelRotZ->value();

    Service<simulation_interfaces::srv::SetEntityState> service("/set_entity_state", node_);
    auto response = service.call_service_sync(request);
}

void MyWidget::SpawnButton() {
    simulation_interfaces::srv::SpawnEntity::Request request;
    request.name = ui_->lineEditName->text().toStdString();
    request.uri = ui_->ComboSpawables->currentText().toStdString();
    request.entity_namespace = ui_->lineEditNamespace->text().toStdString();
    request.allow_renaming = ui_->checkBoxAllowRename->isChecked();

    request.initial_pose.pose.position.x = ui_->doubleSpinBoxX->value();
    request.initial_pose.pose.position.y = ui_->doubleSpinBoxY->value();
    request.initial_pose.pose.position.z = ui_->doubleSpinBoxZ->value();
    // Create a client for the SpawnEntity service
    Service<simulation_interfaces::srv::SpawnEntity> service("/spawn_entity", node_);
    auto response = service.call_service_sync(request);
    if (response && response->result.result == simulation_interfaces::msg::Result::RESULT_OK)
    {
        QString message = QString::asprintf("Spawned as %s", response->entity_name.c_str());
        QMessageBox::information(this, "Success", message);
    }

}

void MyWidget::GetSpawnables() {
    // Create a client for the GetSpawnables service
    auto client = node_->create_client<simulation_interfaces::srv::GetSpawnables>("/get_spawnables");
    if (!client->wait_for_service(std::chrono::seconds(10))) {
        RCLCPP_ERROR(node_->get_logger(), "Service not available after waiting");
        return;
    }
    auto request = std::make_shared<simulation_interfaces::srv::GetSpawnables::Request>();

    // Call the service
    auto future = client->async_send_request(request);
    if (rclcpp::spin_until_future_complete(node_, future) !=
        rclcpp::FutureReturnCode::SUCCESS) {
        RCLCPP_ERROR(node_->get_logger(), "Failed to call service");
        return;
    }
    auto response = future.get();
    if (response->result.result != simulation_interfaces::msg::Result::RESULT_OK) {
        RCLCPP_ERROR(node_->get_logger(), "Service call failed: %s", response->result.error_message.c_str());
        return;
    }
    // Process the response
    ui_->ComboSpawables->clear();
    for (const auto &spawnable: response->spawnables) {
        ui_->ComboSpawables->addItem(QString::fromStdString(spawnable.uri));
    }
}





