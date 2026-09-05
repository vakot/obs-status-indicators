#include "overlay-settings-dialog.hpp"

#include <obs-module.h>

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
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

	auto *layout = new QVBoxLayout(this);
	auto *layout_section = new QGroupBox(obs_module_text("Layout"), this);
	auto *layout_grid = new QGridLayout(layout_section);
	origin_combo_ = new QComboBox(this);
	origin_combo_->addItem(obs_module_text("TopLeft"));
	origin_combo_->addItem(obs_module_text("TopRight"));
	origin_combo_->addItem(obs_module_text("BottomLeft"));
	origin_combo_->addItem(obs_module_text("BottomRight"));
	layout_grid->addWidget(new QLabel(obs_module_text("Origin"), layout_section), 0, 0);
	layout_grid->addWidget(origin_combo_, 0, 1);

	orientation_combo_ = new QComboBox(this);
	orientation_combo_->addItem(obs_module_text("Vertical"));
	orientation_combo_->addItem(obs_module_text("Horizontal"));
	layout_grid->addWidget(new QLabel(obs_module_text("Orientation"), layout_section), 0, 2);
	layout_grid->addWidget(orientation_combo_, 0, 3);
	layout_grid->setColumnStretch(1, 1);
	layout_grid->setColumnStretch(3, 1);
	layout->addWidget(layout_section);

	auto *spacing_section = new QGroupBox(obs_module_text("Spacing"), this);
	auto *spacing_grid = new QGridLayout(spacing_section);
	indicator_size_spin_ = new QSpinBox(this);
	indicator_size_spin_->setRange(kMinIndicatorSize, kMaxIndicatorSize);
	indicator_size_spin_->setSuffix(" px");
	spacing_grid->addWidget(new QLabel(obs_module_text("IndicatorSize"), spacing_section), 0, 0);
	spacing_grid->addWidget(indicator_size_spin_, 0, 1);

	gap_spin_ = new QSpinBox(this);
	gap_spin_->setRange(0, 512);
	gap_spin_->setSuffix(" px");
	spacing_grid->addWidget(new QLabel(obs_module_text("Gap"), spacing_section), 0, 2);
	spacing_grid->addWidget(gap_spin_, 0, 3);

	offset_spin_ = new QSpinBox(this);
	offset_spin_->setRange(0, 4096);
	offset_spin_->setSuffix(" px");
	spacing_grid->addWidget(new QLabel(obs_module_text("Offset"), spacing_section), 1, 0);
	spacing_grid->addWidget(offset_spin_, 1, 1);
	spacing_grid->setColumnStretch(1, 1);
	spacing_grid->setColumnStretch(3, 1);
	layout->addWidget(spacing_section);

	auto *appearance_section = new QGroupBox(obs_module_text("Appearance"), this);
	auto *appearance_grid = new QGridLayout(appearance_section);
	background_color_button_ = new QPushButton(this);
	connect(background_color_button_, &QPushButton::clicked, this, [this] {
		choose_color(background_color_button_, draft_settings_.background_color);
	});
	appearance_grid->addWidget(new QLabel(obs_module_text("BackgroundColor"), appearance_section), 0, 0);
	appearance_grid->addWidget(background_color_button_, 0, 1);

	icon_color_button_ = new QPushButton(this);
	connect(icon_color_button_, &QPushButton::clicked, this, [this] {
		choose_color(icon_color_button_, draft_settings_.icon_color);
	});
	appearance_grid->addWidget(new QLabel(obs_module_text("IconColor"), appearance_section), 0, 2);
	appearance_grid->addWidget(icon_color_button_, 0, 3);
	opacity_spin_ = new QSpinBox(this);
	opacity_spin_->setRange(1, 100);
	opacity_spin_->setSuffix(" %");
	appearance_grid->addWidget(new QLabel(obs_module_text("Opacity"), appearance_section), 1, 0);
	appearance_grid->addWidget(opacity_spin_, 1, 1);
	appearance_grid->setColumnStretch(1, 1);
	appearance_grid->setColumnStretch(3, 1);
	layout->addWidget(appearance_section);

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
	indicator_size_spin_->setValue(normalized.indicator_size);
	opacity_spin_->setValue(normalized.opacity);
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
	settings.indicator_size = indicator_size_spin_->value();
	settings.opacity = opacity_spin_->value();
	return settings;
}

void OverlaySettingsDialog::choose_color(QPushButton *button, std::uint32_t &color)
{
	const QColor selected = QColorDialog::getColor(QColor::fromRgb(color), this,
		obs_module_text("SelectColor"));
	if (!selected.isValid())
		return;
	color = selected.rgb();
	update_color_button(button, color);
}

void OverlaySettingsDialog::update_color_button(QPushButton *button, std::uint32_t color)
{
	const QColor selected = QColor::fromRgb(color);
	button->setText(selected.name(QColor::HexRgb));
	button->setStyleSheet(QString("background-color: %1; color: %2")
									 .arg(selected.name(QColor::HexRgb),
										 selected.lightness() > 128 ? "black" : "white"));
}
