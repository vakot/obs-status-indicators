#include "overlay-settings-dialog.hpp"

#include <obs-module.h>

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QVBoxLayout>

#include <utility>

namespace {
constexpr int kOriginTopLeft = 0;
constexpr int kOriginTopRight = 1;
constexpr int kOriginBottomLeft = 2;
constexpr int kOriginBottomRight = 3;
constexpr int kOrientationVertical = 0;
constexpr int kOrientationHorizontal = 1;

OverlayOrigin origin_from_index(int index)
{
	switch (index) {
	case kOriginTopRight:
		return OverlayOrigin::TopRight;
	case kOriginBottomLeft:
		return OverlayOrigin::BottomLeft;
	case kOriginBottomRight:
		return OverlayOrigin::BottomRight;
	default:
		return OverlayOrigin::TopLeft;
	}
}

int origin_to_index(OverlayOrigin origin)
{
	switch (origin) {
	case OverlayOrigin::TopRight:
		return kOriginTopRight;
	case OverlayOrigin::BottomLeft:
		return kOriginBottomLeft;
	case OverlayOrigin::BottomRight:
		return kOriginBottomRight;
	default:
		return kOriginTopLeft;
	}
}
}

OverlaySettingsDialog::OverlaySettingsDialog(QWidget *parent, const OverlaySettings &settings,
	SettingsAppliedCallback settings_applied)
	: QDialog(parent), applied_settings_(normalize_overlay_settings(settings)),
	  draft_settings_(applied_settings_),
	  settings_applied_(std::move(settings_applied))
{
	setWindowTitle(obs_module_text("SettingsDialogTitle"));
	setModal(false);

	auto *form = new QFormLayout();
	origin_combo_ = new QComboBox(this);
	origin_combo_->addItem(obs_module_text("TopLeft"));
	origin_combo_->addItem(obs_module_text("TopRight"));
	origin_combo_->addItem(obs_module_text("BottomLeft"));
	origin_combo_->addItem(obs_module_text("BottomRight"));
	form->addRow(obs_module_text("Origin"), origin_combo_);

	orientation_combo_ = new QComboBox(this);
	orientation_combo_->addItem(obs_module_text("Vertical"));
	orientation_combo_->addItem(obs_module_text("Horizontal"));
	form->addRow(obs_module_text("Orientation"), orientation_combo_);

	offset_spin_ = new QSpinBox(this);
	offset_spin_->setRange(0, 4096);
	offset_spin_->setSuffix(" px");
	form->addRow(obs_module_text("Offset"), offset_spin_);

	gap_spin_ = new QSpinBox(this);
	gap_spin_->setRange(0, 512);
	gap_spin_->setSuffix(" px");
	form->addRow(obs_module_text("Gap"), gap_spin_);

	background_color_button_ = new QPushButton(this);
	connect(background_color_button_, &QPushButton::clicked, this, [this] {
		choose_color(background_color_button_, draft_settings_.background_color);
	});
	form->addRow(obs_module_text("BackgroundColor"), background_color_button_);

	icon_color_button_ = new QPushButton(this);
	connect(icon_color_button_, &QPushButton::clicked, this, [this] {
		choose_color(icon_color_button_, draft_settings_.icon_color);
	});
	form->addRow(obs_module_text("IconColor"), icon_color_button_);

	button_box_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
									QDialogButtonBox::Apply,
									Qt::Horizontal, this);
	connect(button_box_, &QDialogButtonBox::accepted, this, [this] {
		apply_settings();
		accept();
	});
	connect(button_box_, &QDialogButtonBox::rejected, this, [this] {
		populate_controls(applied_settings_);
		reject();
	});
	connect(button_box_->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
		[this] { apply_settings(); });

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(button_box_);
	populate_controls(applied_settings_);
	resize(360, sizeHint().height());
}

void OverlaySettingsDialog::showEvent(QShowEvent *event)
{
	populate_controls(applied_settings_);
	QDialog::showEvent(event);
}

void OverlaySettingsDialog::apply_settings()
{
	draft_settings_ = settings_from_controls();
	applied_settings_ = draft_settings_;
	applied_settings_ = normalize_overlay_settings(applied_settings_);
	draft_settings_ = applied_settings_;
	if (settings_applied_)
		settings_applied_(applied_settings_);
}

void OverlaySettingsDialog::populate_controls(const OverlaySettings &settings)
{
	const OverlaySettings normalized = normalize_overlay_settings(settings);
	draft_settings_ = normalized;
	origin_combo_->setCurrentIndex(origin_to_index(normalized.origin));
	orientation_combo_->setCurrentIndex(normalized.orientation == OverlayOrientation::Horizontal
																	 ? kOrientationHorizontal
																	 : kOrientationVertical);
	offset_spin_->setValue(normalized.offset);
	gap_spin_->setValue(normalized.gap);
	update_color_button(background_color_button_, normalized.background_color);
	update_color_button(icon_color_button_, normalized.icon_color);
}

OverlaySettings OverlaySettingsDialog::settings_from_controls() const
{
	OverlaySettings settings = draft_settings_;
	settings.origin = origin_from_index(origin_combo_->currentIndex());
	settings.orientation = orientation_combo_->currentIndex() == kOrientationHorizontal
																	 ? OverlayOrientation::Horizontal
																	 : OverlayOrientation::Vertical;
	settings.offset = offset_spin_->value();
	settings.gap = gap_spin_->value();
	return settings;
}

void OverlaySettingsDialog::choose_color(QPushButton *button, std::uint32_t &color)
{
	const QColor selected = QColorDialog::getColor(QColor::fromRgba(color), this,
		obs_module_text("SelectColor"), QColorDialog::ShowAlphaChannel);
	if (!selected.isValid())
		return;
	color = selected.rgba();
	update_color_button(button, color);
}

void OverlaySettingsDialog::update_color_button(QPushButton *button, std::uint32_t color)
{
	const QColor selected = QColor::fromRgba(color);
	button->setText(selected.name(QColor::HexArgb));
	button->setStyleSheet(QString("background-color: %1; color: %2")
									 .arg(selected.name(QColor::HexArgb),
										 selected.lightness() > 128 ? "black" : "white"));
}
