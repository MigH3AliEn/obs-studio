#include "context-bar-controls.hpp"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"

#include <QStandardItemModel>
#include <QColorDialog>
#include <QFontDialog>

#include "ui_browser-source-toolbar.h"
#include "ui_device-select-toolbar.h"
#include "ui_game-capture-toolbar.h"
#include "ui_image-source-toolbar.h"
#include "ui_color-source-toolbar.h"
#include "ui_text-source-toolbar.h"
#include "ui_slideshow-toolbar.h"

#ifdef _WIN32
#define get_os_module(win, mac, linux) obs_get_module(win)
#define get_os_text(mod, win, mac, linux) obs_module_get_locale_text(mod, win)
#elif __APPLE__
#define get_os_module(win, mac, linux) obs_get_module(mac)
#define get_os_text(mod, win, mac, linux) obs_module_get_locale_text(mod, mac)
#else
#define get_os_module(win, mac, linux) obs_get_module(linux)
#define get_os_text(mod, win, mac, linux) obs_module_get_locale_text(mod, linux)
#endif

/* ========================================================================= */

SourceToolbar::SourceToolbar(QWidget *parent, OBSSource source)
	: QWidget(parent),
	  weakSource(OBSGetWeakRef(source)),
	  props(obs_source_properties(source), obs_properties_destroy)
{
	obs_data_t *settings = obs_source_get_settings(source);
	obs_properties_apply_settings(props.get(), settings);
	obs_data_release(settings);
}

/* ========================================================================= */

BrowserToolbar::BrowserToolbar(QWidget *parent, OBSSource source)
	: SourceToolbar(parent, source), ui(new Ui_BrowserSourceToolbar)
{
	ui->setupUi(this);
}

BrowserToolbar::~BrowserToolbar()
{
	delete ui;
}

void BrowserToolbar::on_refresh_clicked()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	obs_property_t *p = obs_properties_get(props.get(), "refreshnocache");
	obs_property_button_clicked(p, source.Get());
}

/* ========================================================================= */

ComboSelectToolbar::ComboSelectToolbar(QWidget *parent, OBSSource source)
	: SourceToolbar(parent, source), ui(new Ui_DeviceSelectToolbar)
{
	ui->setupUi(this);
}

ComboSelectToolbar::~ComboSelectToolbar()
{
	delete ui;
}

static int FillPropertyCombo(QComboBox *c, obs_property_t *p,
			     const std::string &cur_id, bool is_int = false)
{
	size_t count = obs_property_list_item_count(p);
	int cur_idx = -1;

	for (size_t i = 0; i < count; i++) {
		const char *name = obs_property_list_item_name(p, i);
		std::string id;

		if (is_int) {
			id = std::to_string(obs_property_list_item_int(p, i));
		} else {
			const char *val = obs_property_list_item_string(p, i);
			id = val ? val : "";
		}

		if (cur_id == id)
			cur_idx = (int)i;

		c->addItem(name, id.c_str());
	}

	return cur_idx;
}

static void SetComboItemDisabled(QComboBox *c, int idx)
{
	QStandardItemModel *model =
		dynamic_cast<QStandardItemModel *>(c->model());
	QStandardItem *item = model->item(idx);
	item->setFlags(Qt::NoItemFlags);
}

void ComboSelectToolbar::Init()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	std::string cur_id;

	obs_data_t *settings = obs_source_get_settings(source);
	if (is_int) {
		cur_id = std::to_string(obs_data_get_int(settings, prop_name));
	} else {
		cur_id = obs_data_get_string(settings, prop_name);
	}
	obs_data_release(settings);

	ui->device->blockSignals(true);

	obs_property_t *p = obs_properties_get(props.get(), prop_name);
	int cur_idx = FillPropertyCombo(ui->device, p, cur_id, is_int);

	if (cur_idx == -1 || obs_property_list_item_disabled(p, cur_idx)) {
		if (cur_idx == -1) {
			ui->device->insertItem(
				0,
				QTStr("Basic.Settings.Audio.UnknownAudioDevice"));
			cur_idx = 0;
		}

		SetComboItemDisabled(ui->device, cur_idx);
	} else {
		ui->device->setCurrentIndex(cur_idx);
	}

	ui->device->blockSignals(false);
}

void ComboSelectToolbar::UpdateActivateButtonName()
{
	obs_property_t *p = obs_properties_get(props.get(), "activate");
	ui->activateButton->setText(obs_property_description(p));
}

void ComboSelectToolbar::on_device_currentIndexChanged(int idx)
{
	OBSSource source = GetSource();
	if (idx == -1 || !source) {
		return;
	}

	QString id = ui->device->itemData(idx).toString();

	obs_data_t *settings = obs_data_create();
	if (is_int) {
		obs_data_set_int(settings, prop_name, id.toInt());
	} else {
		obs_data_set_string(settings, prop_name, QT_TO_UTF8(id));
	}
	obs_source_update(source, settings);
	obs_data_release(settings);
}

void ComboSelectToolbar::on_activateButton_clicked()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	obs_property_t *p = obs_properties_get(props.get(), "activate");
	if (!p) {
		return;
	}

	obs_property_button_clicked(p, source.Get());
	UpdateActivateButtonName();
}

AudioCaptureToolbar::AudioCaptureToolbar(QWidget *parent, OBSSource source)
	: ComboSelectToolbar(parent, source)
{
}

void AudioCaptureToolbar::Init()
{
	delete ui->activateButton;
	ui->activateButton = nullptr;

	obs_module_t *mod =
		get_os_module("win-wasapi", "mac-capture", "linux-pulseaudio");
	const char *device_str =
		get_os_text(mod, "Device", "CoreAudio.Device", "Device");
	ui->deviceLabel->setText(device_str);

	prop_name = "device_id";

	ComboSelectToolbar::Init();
}

WindowCaptureToolbar::WindowCaptureToolbar(QWidget *parent, OBSSource source)
	: ComboSelectToolbar(parent, source)
{
}

void WindowCaptureToolbar::Init()
{
	delete ui->activateButton;
	ui->activateButton = nullptr;

	obs_module_t *mod =
		get_os_module("win-capture", "mac-capture", "linux-capture");
	const char *device_str = get_os_text(mod, "WindowCapture.Window",
					     "WindowUtils.Window", "Window");
	ui->deviceLabel->setText(device_str);

#if !defined(_WIN32) && !defined(__APPLE__) //linux
	prop_name = "capture_window";
#else
	prop_name = "window";
#endif

#ifdef __APPLE__
	is_int = true;
#endif

	ComboSelectToolbar::Init();
}

DisplayCaptureToolbar::DisplayCaptureToolbar(QWidget *parent, OBSSource source)
	: ComboSelectToolbar(parent, source)
{
}

void DisplayCaptureToolbar::Init()
{
	delete ui->activateButton;
	ui->activateButton = nullptr;

	obs_module_t *mod =
		get_os_module("win-capture", "mac-capture", "linux-capture");
	const char *device_str =
		get_os_text(mod, "Monitor", "DisplayCapture.Display", "Screen");
	ui->deviceLabel->setText(device_str);
	is_int = true;

#ifdef _WIN32
	prop_name = "monitor";
#elif __APPLE__
	prop_name = "display";
#else
	prop_name = "screen";
#endif

	ComboSelectToolbar::Init();
}

DeviceCaptureToolbar::DeviceCaptureToolbar(QWidget *parent, OBSSource source)
	: ComboSelectToolbar(parent, source)
{
}

void DeviceCaptureToolbar::Init()
{
#ifndef _WIN32
	delete ui->activateButton;
	ui->activateButton = nullptr;
#endif

	obs_module_t *mod =
		get_os_module("win-dshow", "mac-avcapture", "linux-v4l2");
	const char *device_str = obs_module_get_locale_text(mod, "Device");
	ui->deviceLabel->setText(device_str);

#ifdef _WIN32
	prop_name = "video_device_id";
#elif __APPLE__
	prop_name = "device";
#else
	prop_name = "device_id";
#endif

#ifdef _WIN32
	UpdateActivateButtonName();
#endif

	ComboSelectToolbar::Init();
}

/* ========================================================================= */

GameCaptureToolbar::GameCaptureToolbar(QWidget *parent, OBSSource source)
	: SourceToolbar(parent, source), ui(new Ui_GameCaptureToolbar)
{
	obs_property_t *p;
	int cur_idx;

	ui->setupUi(this);

	obs_module_t *mod = obs_get_module("win-capture");
	ui->modeLabel->setText(obs_module_get_locale_text(mod, "Mode"));
	ui->windowLabel->setText(
		obs_module_get_locale_text(mod, "WindowCapture.Window"));

	obs_data_t *settings = obs_source_get_settings(source);
	std::string cur_mode = obs_data_get_string(settings, "capture_mode");
	std::string cur_window = obs_data_get_string(settings, "window");
	obs_data_release(settings);

	ui->mode->blockSignals(true);
	p = obs_properties_get(props.get(), "capture_mode");
	cur_idx = FillPropertyCombo(ui->mode, p, cur_mode);
	ui->mode->setCurrentIndex(cur_idx);
	ui->mode->blockSignals(false);

	ui->window->blockSignals(true);
	p = obs_properties_get(props.get(), "window");
	cur_idx = FillPropertyCombo(ui->window, p, cur_window);
	ui->window->setCurrentIndex(cur_idx);
	ui->window->blockSignals(false);

	if (cur_idx != -1 && obs_property_list_item_disabled(p, cur_idx)) {
		SetComboItemDisabled(ui->window, cur_idx);
	}

	UpdateWindowVisibility();
}

GameCaptureToolbar::~GameCaptureToolbar()
{
	delete ui;
}

void GameCaptureToolbar::UpdateWindowVisibility()
{
	QString mode = ui->mode->currentData().toString();
	bool is_window = (mode == "window");
	ui->windowLabel->setVisible(is_window);
	ui->window->setVisible(is_window);
	ui->empty->setVisible(!is_window);
}

void GameCaptureToolbar::on_mode_currentIndexChanged(int idx)
{
	OBSSource source = GetSource();
	if (idx == -1 || !source) {
		return;
	}

	QString id = ui->mode->itemData(idx).toString();

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "capture_mode", QT_TO_UTF8(id));
	obs_source_update(source, settings);
	obs_data_release(settings);

	UpdateWindowVisibility();
}

void GameCaptureToolbar::on_window_currentIndexChanged(int idx)
{
	OBSSource source = GetSource();
	if (idx == -1 || !source) {
		return;
	}

	QString id = ui->window->itemData(idx).toString();

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "window", QT_TO_UTF8(id));
	obs_source_update(source, settings);
	obs_data_release(settings);
}

/* ========================================================================= */

ImageSourceToolbar::ImageSourceToolbar(QWidget *parent, OBSSource source)
	: SourceToolbar(parent, source), ui(new Ui_ImageSourceToolbar)
{
	ui->setupUi(this);

	obs_module_t *mod = obs_get_module("image-source");
	ui->pathLabel->setText(obs_module_get_locale_text(mod, "File"));

	obs_data_t *settings = obs_source_get_settings(source);
	std::string file = obs_data_get_string(settings, "file");
	obs_data_release(settings);

	ui->path->setText(file.c_str());
}

ImageSourceToolbar::~ImageSourceToolbar()
{
	delete ui;
}

void ImageSourceToolbar::on_browse_clicked()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	obs_property_t *p = obs_properties_get(props.get(), "file");
	const char *desc = obs_property_description(p);
	const char *filter = obs_property_path_filter(p);
	const char *default_path = obs_property_path_default_path(p);

	QString path = OpenFile(this, desc, default_path, filter);
	if (path.isEmpty()) {
		return;
	}

	ui->path->setText(path);

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "file", QT_TO_UTF8(path));
	obs_source_update(source, settings);
	obs_data_release(settings);
}

/* ========================================================================= */

SlideshowToolbar::SlideshowToolbar(QWidget *parent, OBSSource source)
	: SourceToolbar(parent, source), ui(new Ui_SlideshowToolbar)
{
	ui->setupUi(this);
	UpdateState();
}

SlideshowToolbar::~SlideshowToolbar()
{
	delete ui;
}

void SlideshowToolbar::UpdateState()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	proc_handler_t *ph = obs_source_get_proc_handler(source);
	calldata_t cd = {};

	proc_handler_call(ph, "get_state", &cd);
	const char *state = calldata_string(&cd, "return");

	if (strcmp(state, "playing") == 0) {
		ui->playPauseButton->setProperty("themeID", "pauseIcon");
		ui->playPauseButton->style()->unpolish(ui->playPauseButton);
		ui->playPauseButton->style()->polish(ui->playPauseButton);
		ui->playPauseButton->setToolTip(
			QTStr("ContextBar.MediaControls.PauseMedia"));
	} else if (strcmp(state, "paused") == 0 ||
		   strcmp(state, "stopped") == 0) {
		ui->playPauseButton->setProperty("themeID", "playIcon");
		ui->playPauseButton->style()->unpolish(ui->playPauseButton);
		ui->playPauseButton->style()->polish(ui->playPauseButton);
		ui->playPauseButton->setToolTip(
			QTStr("ContextBar.MediaControls.PlayMedia"));
	}
}

void SlideshowToolbar::on_playPauseButton_clicked()
{
	OBSSource source = GetSource();
	if (source) {
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		proc_handler_call(ph, "playpause", nullptr);
		UpdateState();
	}
}

void SlideshowToolbar::on_previousButton_clicked()
{
	OBSSource source = GetSource();
	if (source) {
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		proc_handler_call(ph, "previous_slide", nullptr);
	}
}

void SlideshowToolbar::on_stopButton_clicked()
{
	OBSSource source = GetSource();
	if (source) {
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		proc_handler_call(ph, "stop", nullptr);
		UpdateState();
	}
}

void SlideshowToolbar::on_nextButton_clicked()
{
	OBSSource source = GetSource();
	if (source) {
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		proc_handler_call(ph, "next_slide", nullptr);
	}
}

/* ========================================================================= */

static inline QColor color_from_int(long long val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff,
		      (val >> 24) & 0xff);
}

static inline long long color_to_int(QColor color)
{
	auto shift = [&](unsigned val, int shift) {
		return ((val & 0xff) << shift);
	};

	return shift(color.red(), 0) | shift(color.green(), 8) |
	       shift(color.blue(), 16) | shift(color.alpha(), 24);
}

ColorSourceToolbar::ColorSourceToolbar(QWidget *parent, OBSSource source)
	: SourceToolbar(parent, source), ui(new Ui_ColorSourceToolbar)
{
	ui->setupUi(this);

	obs_data_t *settings = obs_source_get_settings(source);
	unsigned int val = (unsigned int)obs_data_get_int(settings, "color");
	obs_data_release(settings);

	color = color_from_int(val);
	UpdateColor();
}

ColorSourceToolbar::~ColorSourceToolbar()
{
	delete ui;
}

void ColorSourceToolbar::UpdateColor()
{
	color.setAlpha(255);

	QPalette palette = QPalette(color);
	ui->color->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	ui->color->setText(color.name(QColor::HexRgb));
	ui->color->setPalette(palette);
	ui->color->setStyleSheet(
		QString("background-color :%1; color: %2;")
			.arg(palette.color(QPalette::Window)
				     .name(QColor::HexRgb))
			.arg(palette.color(QPalette::WindowText)
				     .name(QColor::HexRgb)));
	ui->color->setAutoFillBackground(true);
	ui->color->setAlignment(Qt::AlignCenter);
}

void ColorSourceToolbar::on_choose_clicked()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	obs_property_t *p = obs_properties_get(props.get(), "color");
	const char *desc = obs_property_description(p);

	QColorDialog::ColorDialogOptions options;

#ifndef _WIN32
	options |= QColorDialog::DontUseNativeDialog;
#endif

	color = QColorDialog::getColor(color, this, desc, options);
	UpdateColor();

	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "color", color_to_int(color));
	obs_source_update(source, settings);
	obs_data_release(settings);
}

/* ========================================================================= */

extern void MakeQFont(obs_data_t *font_obj, QFont &font, bool limit = false);

TextSourceToolbar::TextSourceToolbar(QWidget *parent, OBSSource source)
	: SourceToolbar(parent, source), ui(new Ui_TextSourceToolbar)
{
	ui->setupUi(this);

	obs_data_t *settings = obs_source_get_settings(source);

	obs_data_t *font_obj = obs_data_get_obj(settings, "font");
	MakeQFont(font_obj, font);
	obs_data_release(font_obj);

	unsigned int val = (unsigned int)obs_data_get_int(settings, "color");
	color = color_from_int(val);

	const char *text = obs_data_get_string(settings, "text");

	bool single_line = !text || (strchr(text, '\n') == nullptr);
	ui->emptySpace->setVisible(!single_line);
	ui->text->setVisible(single_line);
	if (single_line)
		ui->text->setText(text);

	obs_data_release(settings);
}

TextSourceToolbar::~TextSourceToolbar()
{
	delete ui;
}

void TextSourceToolbar::on_selectFont_clicked()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	QFontDialog::FontDialogOptions options;
	uint32_t flags;
	bool success;

#ifndef _WIN32
	options = QFontDialog::DontUseNativeDialog;
#endif

	font = QFontDialog::getFont(&success, font, this, "Pick a Font",
				    options);
	if (!success) {
		return;
	}

	obs_data_t *font_obj = obs_data_create();

	obs_data_set_string(font_obj, "face", QT_TO_UTF8(font.family()));
	obs_data_set_string(font_obj, "style", QT_TO_UTF8(font.styleName()));
	obs_data_set_int(font_obj, "size", font.pointSize());
	flags = font.bold() ? OBS_FONT_BOLD : 0;
	flags |= font.italic() ? OBS_FONT_ITALIC : 0;
	flags |= font.underline() ? OBS_FONT_UNDERLINE : 0;
	flags |= font.strikeOut() ? OBS_FONT_STRIKEOUT : 0;
	obs_data_set_int(font_obj, "flags", flags);

	obs_data_t *settings = obs_data_create();

	obs_data_set_obj(settings, "font", font_obj);
	obs_data_release(font_obj);

	obs_source_update(source, settings);
	obs_data_release(settings);
}

void TextSourceToolbar::on_selectColor_clicked()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	obs_property_t *p = obs_properties_get(props.get(), "color");
	const char *desc = obs_property_description(p);

	QColorDialog::ColorDialogOptions options;

#ifndef _WIN32
	options |= QColorDialog::DontUseNativeDialog;
#endif

	QColor newColor = QColorDialog::getColor(color, this, desc, options);
	if (!newColor.isValid()) {
		return;
	}

	color = newColor;

	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "color", color_to_int(color));
	obs_source_update(source, settings);
	obs_data_release(settings);
}

void TextSourceToolbar::on_text_textChanged()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "text",
			    QT_TO_UTF8(ui->text->text()));
	obs_source_update(source, settings);
	obs_data_release(settings);
}
