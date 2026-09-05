#pragma once

#include "overlay-settings.hpp"

#include <QDialog>

#include <functional>

class QComboBox;
class QDialogButtonBox;
class QPushButton;
class QSpinBox;

class OverlaySettingsDialog final : public QDialog {
public:
	using SettingsAppliedCallback = std::function<void(const OverlaySettings &)>;

	OverlaySettingsDialog(QWidget *parent, const OverlaySettings &settings,
		SettingsAppliedCallback settings_applied);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void apply_settings();
	void populate_controls(const OverlaySettings &settings);
	OverlaySettings settings_from_controls() const;
	void choose_color(QPushButton *button, std::uint32_t &color);
	void update_color_button(QPushButton *button, std::uint32_t color);

	QComboBox *origin_combo_ = nullptr;
	QComboBox *orientation_combo_ = nullptr;
	QSpinBox *offset_spin_ = nullptr;
	QSpinBox *gap_spin_ = nullptr;
	QSpinBox *indicator_size_spin_ = nullptr;
	QSpinBox *opacity_spin_ = nullptr;
	QPushButton *background_color_button_ = nullptr;
	QPushButton *icon_color_button_ = nullptr;
	QDialogButtonBox *button_box_ = nullptr;
	OverlaySettings applied_settings_;
	OverlaySettings draft_settings_;
	SettingsAppliedCallback settings_applied_;
};
