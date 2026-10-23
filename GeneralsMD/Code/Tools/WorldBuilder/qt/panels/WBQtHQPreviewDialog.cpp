// WBQtHQPreviewDialog.cpp -- shows the HQ map preview, live from above, with its controls and the
// water, macro texture and sky values before the tga is written. See WBQtHQPreviewBridge.h.
#include "WBQtHQPreviewBridge.h"
#include "WBQtWaterTuningBridge.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <math.h>
#include <string.h>
#include <vector>

// Modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

namespace
{

const int kDisplaySize = 512;
const int kLiveIntervalMs = 33;

// A native child window Direct3D presents the live view into. Qt never paints it.
class NativeView : public QWidget
{
public:
	explicit NativeView(QWidget *parent)
		: QWidget(parent)
	{
		setAttribute(Qt::WA_NativeWindow);
		setAttribute(Qt::WA_PaintOnScreen);
		setAttribute(Qt::WA_NoSystemBackground);
		setAttribute(Qt::WA_OpaquePaintEvent);
	}

	QPaintEngine *paintEngine() const override
	{
		return NULL;
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
	}
};

// One water, macro texture or sky key, with the value it had when the dialog opened.
struct TunedKey
{
	int index;
	WBQtWaterTuningDesc desc;
	float original[3];
	float value[3];
	QDoubleSpinBox *spin;
	QSlider *slider;
	QCheckBox *check;
	QPushButton *swatch;
};

class WBQtHQPreviewDialog : public QDialog
{
public:
	WBQtHQPreviewDialog(const QString &tgaPath, QWidget *parent)
		: QDialog(parent)
	{
		setWindowTitle(tr("HQ Map Preview"));

		m_image = new QLabel(this);
		m_image->setFixedSize(kDisplaySize, kDisplaySize);
		m_image->setAlignment(Qt::AlignCenter);
		m_image->setFrameShape(QFrame::Box);
		m_image->setStyleSheet("background-color: black;");
		m_native = new NativeView(this);
		m_native->setFixedSize(kDisplaySize, kDisplaySize);
		m_view = new QStackedWidget(this);
		m_view->setFixedSize(kDisplaySize, kDisplaySize);
		m_view->addWidget(m_native);
		m_view->addWidget(m_image);
		m_status = new QLabel(this);
		m_status->setWordWrap(true);
		m_status->setMaximumWidth(kDisplaySize);

		m_renderBtn = new QPushButton(tr("Render"), this);
		m_renderBtn->setToolTip(tr("Freezes the view with a full-quality render and the shading, as Save writes it."));
		connect(m_renderBtn, &QPushButton::clicked, this, [this]()
		{
			render();
		});
		m_liveBtn = new QPushButton(tr("Live"), this);
		m_liveBtn->setToolTip(tr("Goes back to the live view."));
		connect(m_liveBtn, &QPushButton::clicked, this, [this]()
		{
			goLive();
		});
		QHBoxLayout *viewButtons = new QHBoxLayout();
		viewButtons->addWidget(m_renderBtn);
		viewButtons->addWidget(m_liveBtn);
		viewButtons->addStretch(1);

		QVBoxLayout *viewColumn = new QVBoxLayout();
		viewColumn->addWidget(m_view);
		viewColumn->addLayout(viewButtons);
		viewColumn->addWidget(m_status);
		viewColumn->addStretch(1);

		m_tabs = new QTabWidget(this);
		m_tabs->addTab(buildPreviewTab(), tr("Preview"));
		m_tabs->addTab(buildKeyTab(tr("Water"), NULL, true), tr("Water"));
		m_tabs->addTab(buildKeyTab(tr("Macro texture"), "Ground", false), tr("Macro texture"));
		m_tabs->addTab(buildKeyTab(tr("Sky"), "Sky", false), tr("Sky"));

		QLabel *pathLabel = new QLabel(tgaPath, this);
		pathLabel->setWordWrap(true);
		pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

		QVBoxLayout *controls = new QVBoxLayout();
		controls->addWidget(m_tabs, 1);
		controls->addWidget(pathLabel);

		QHBoxLayout *body = new QHBoxLayout();
		body->addLayout(viewColumn, 0);
		body->addLayout(controls, 1);

		QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
		connect(buttons, &QDialogButtonBox::accepted, this, [this]()
		{
			if (needsRender() && !render())
			{
				return;
			}
			const WBQtHQPreviewParams params = shading();
			const WBQtHQCaptureParams capture = captureSettings();
			if (WBQtHQPreview_Save(&params, &capture) != 0)
			{
				accept();
			}
			else
			{
				QMessageBox::warning(this, windowTitle(), tr("Couldn't write the tga (is the map folder writable?)."));
			}
		});
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

		QVBoxLayout *root = new QVBoxLayout(this);
		root->addLayout(body);
		root->addWidget(buttons);

		WBQtHQPreviewParams params;
		WBQtHQCaptureParams capture;
		WBQtHQPreview_GetLast(&params, &capture);
		loadCapture(capture);
		loadShading(params);

		m_timer = new QTimer(this);
		connect(m_timer, &QTimer::timeout, this, [this]()
		{
			liveFrame();
		});
		goLive();
	}

	// Every way out ends the live view and puts the water, macro texture and sky values back.
	void done(int result) override
	{
		m_timer->stop();
		WBQtHQPreview_LiveEnd();
		for (size_t i = 0; i < m_keys.size(); ++i)
		{
			const TunedKey &k = m_keys[i];
			if (k.value[0] != k.original[0] || k.value[1] != k.original[1] || k.value[2] != k.original[2])
			{
				WBQtWaterTuning_SetLive(k.index, k.original);
			}
		}
		QDialog::done(result);
	}

private:
	QWidget *buildPreviewTab()
	{
		QWidget *page = new QWidget(this);
		QVBoxLayout *layout = new QVBoxLayout(page);
		layout->addWidget(buildShadingBox());
		layout->addWidget(buildRenderBox());

		QPushButton *resetBtn = new QPushButton(tr("Defaults"), page);
		resetBtn->setToolTip(tr("Puts the shading and render settings back to their defaults."));
		connect(resetBtn, &QPushButton::clicked, this, [this]()
		{
			WBQtHQPreviewParams params;
			WBQtHQCaptureParams capture;
			WBQtHQPreview_GetDefaults(&params, &capture);
			loadCapture(capture);
			loadShading(params);
			captureChanged();
		});
		layout->addWidget(resetBtn, 0, Qt::AlignLeft);
		layout->addStretch(1);

		QScrollArea *scroll = new QScrollArea(this);
		scroll->setWidget(page);
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		return scroll;
	}

	QGroupBox *buildShadingBox()
	{
		QGroupBox *box = new QGroupBox(tr("Shading (rendered frames)"), this);
		QGridLayout *grid = new QGridLayout(box);
		m_relief = addSlider(grid, 0, tr("Relief"), 0, 200, 1000, true,
			tr("Strength of the hillshade that lights slopes from the north-west. Type past the slider for more."));
		m_elevation = addSlider(grid, 1, tr("Elevation"), 0, 100, 500, true,
			tr("How much brighter high ground is than low ground. Type past the slider for more."));
		m_falloff = addSlider(grid, 2, tr("Water depth"), 5, 200, 5000, false,
			tr("Depth in world units at which water reaches most of its deep colour. Type past the slider for more."));
		m_shallowBtn = addColorButton(grid, 3, tr("Shallow water"), m_shallow);
		m_deepBtn = addColorButton(grid, 4, tr("Deep water"), m_deep);
		m_depthTint = new QCheckBox(tr("Depth tint"), box);
		m_depthTint->setToolTip(tr("Colours water from shallow to deep by its depth, over whatever the render drew."));
		grid->addWidget(m_depthTint, 5, 0, 1, 3);
		connect(m_depthTint, &QCheckBox::toggled, this, [this]() { refresh(); });
		return box;
	}

	QGroupBox *buildRenderBox()
	{
		QGroupBox *box = new QGroupBox(tr("Render"), this);
		QGridLayout *grid = new QGridLayout(box);
		int row = 0;

		m_objects = new QCheckBox(tr("Objects"), box);
		m_trees = new QCheckBox(tr("Trees"), box);
		m_roads = new QCheckBox(tr("Roads and bridges"), box);
		m_colorGrade = new QCheckBox(tr("Colour grade"), box);
		m_colorGrade->setToolTip(tr("Applies the map.ini colour grade, as in the game. Water is tinted after it."));
		grid->addWidget(m_objects, row, 0);
		grid->addWidget(m_trees, row, 1);
		row++;
		grid->addWidget(m_roads, row, 0);
		grid->addWidget(m_colorGrade, row, 1);
		row++;

		m_renderedWater = new QCheckBox(tr("Rendered water"), box);
		m_renderedWater->setToolTip(tr("Draws the flat water surface with the map's water texture. Bridges cover it."));
		m_shaderWater = new QCheckBox(tr("Shader water"), box);
		m_shaderWater->setToolTip(tr("Draws the shader water, as in the game, in place of the flat surface. Seen from straight above it shows little reflection."));
		grid->addWidget(m_renderedWater, row, 0);
		grid->addWidget(m_shaderWater, row, 1);
		row++;

		m_clouds = new QCheckBox(tr("Clouds"), box);
		m_clouds->setToolTip(tr("Draws the cloud shadows moving over the ground."));
		m_macroTexture = new QCheckBox(tr("Macro texture"), box);
		m_macroTexture->setToolTip(tr("Draws the map's macro texture, the large light and dark patches over the ground."));
		grid->addWidget(m_clouds, row, 0);
		grid->addWidget(m_macroTexture, row, 1);
		row++;

		m_stochastic = new QCheckBox(tr("Stochastic filtering"), box);
		m_stochastic->setToolTip(tr("Shifts and turns the ground textures in hex cells over the whole map, as the Stochastic Terrain brush does, to break up their repeat. The map's own paint is left as it was."));
		m_shadows = new QCheckBox(tr("Shadows"), box);
		m_shadows->setToolTip(tr("Draws the shadows of objects, trees and buildings."));
		grid->addWidget(m_stochastic, row, 0);
		grid->addWidget(m_shadows, row, 1);
		row++;

		m_timeOfDay = new QComboBox(box);
		m_timeOfDay->addItem(tr("Current"));
		m_timeOfDay->addItem(tr("Morning"));
		m_timeOfDay->addItem(tr("Afternoon"));
		m_timeOfDay->addItem(tr("Evening"));
		m_timeOfDay->addItem(tr("Night"));
		m_timeOfDay->setToolTip(tr("The lighting to render with. Current keeps the editor's time of day."));
		grid->addWidget(new QLabel(tr("Time of day"), box), row, 0);
		grid->addWidget(m_timeOfDay, row, 1);
		row++;

		m_area = new QComboBox(box);
		m_area->addItem(tr("Whole map"));
		m_area->addItem(tr("Playable area"));
		m_area->addItem(tr("Custom"));
		grid->addWidget(new QLabel(tr("Area"), box), row, 0);
		grid->addWidget(m_area, row, 1);
		row++;

		int mapWidth = 0;
		int mapHeight = 0;
		int playableWidth = 0;
		int playableHeight = 0;
		WBQtHQPreview_GetMapCells(&mapWidth, &mapHeight, &playableWidth, &playableHeight);
		QHBoxLayout *custom = new QHBoxLayout();
		m_x0 = addCellSpin(custom, tr("X"), mapWidth);
		m_y0 = addCellSpin(custom, tr("Y"), mapHeight);
		m_x1 = addCellSpin(custom, tr("to X"), mapWidth);
		m_y1 = addCellSpin(custom, tr("Y"), mapHeight);
		grid->addLayout(custom, row, 0, 1, 2);
		row++;

		m_areaWarning = new QLabel(tr("The lobby places start positions across the whole map, so they will not line up with this preview."), box);
		m_areaWarning->setWordWrap(true);
		m_areaWarning->setStyleSheet("color: #b06000;");
		grid->addWidget(m_areaWarning, row, 0, 1, 2);
		row++;

		m_size = new QComboBox(box);
		m_size->addItem("128", 128);
		m_size->addItem("256", 256);
		m_size->addItem("512", 512);
		m_size->setToolTip(tr("Pixels per side of the tga."));
		grid->addWidget(new QLabel(tr("Output size"), box), row, 0);
		grid->addWidget(m_size, row, 1);
		row++;

		m_supersample = new QComboBox(box);
		m_supersample->addItem("2x", 2);
		m_supersample->addItem("4x", 4);
		m_supersample->addItem("8x", 8);
		m_supersample->setToolTip(tr("Renders this many pixels per output pixel and averages them. Higher smooths edges but renders slower."));
		grid->addWidget(new QLabel(tr("Supersampling"), box), row, 0);
		grid->addWidget(m_supersample, row, 1);
		row++;

		m_renderInfo = new QLabel(box);
		m_renderInfo->setWordWrap(true);
		grid->addWidget(m_renderInfo, row, 0, 1, 2);

		QCheckBox *checks[] = { m_objects, m_trees, m_roads, m_colorGrade, m_renderedWater, m_shaderWater, m_clouds, m_macroTexture, m_stochastic, m_shadows };
		for (int i = 0; i < 10; i++)
		{
			connect(checks[i], &QCheckBox::toggled, this, [this]() { captureChanged(); });
		}
		QComboBox *combos[] = { m_timeOfDay, m_area, m_size, m_supersample };
		for (int i = 0; i < 4; i++)
		{
			connect(combos[i], QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { captureChanged(); });
		}
		QSpinBox *spins[] = { m_x0, m_y0, m_x1, m_y1 };
		for (int i = 0; i < 4; i++)
		{
			connect(spins[i], QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { captureChanged(); });
		}
		return box;
	}

	// A tab of tuning keys: the water keys when group is NULL, else the GameData keys of that group.
	QWidget *buildKeyTab(const QString &title, const char *group, bool water)
	{
		QWidget *page = new QWidget(this);
		QVBoxLayout *layout = new QVBoxLayout(page);
		QFormLayout *form = new QFormLayout();
		layout->addLayout(form);

		const size_t first = m_keys.size();
		const int count = WBQtWaterTuning_Count();
		for (int i = 0; i < count; ++i)
		{
			WBQtWaterTuningDesc desc;
			if (WBQtWaterTuning_GetDesc(i, &desc) == 0 || desc.kind == WBQT_WATER_TEXT)
			{
				continue;
			}
			const bool isWater = desc.group == NULL;
			if (water != isWater || (!water && (desc.group == NULL || strcmp(desc.group, group) != 0)))
			{
				continue;
			}
			TunedKey k;
			k.index = i;
			k.desc = desc;
			WBQtWaterTuning_GetLive(i, k.original);
			memcpy(k.value, k.original, sizeof(k.value));
			k.spin = NULL;
			k.slider = NULL;
			k.check = NULL;
			k.swatch = NULL;
			m_keys.push_back(k);
		}
		for (size_t n = first; n < m_keys.size(); ++n)
		{
			form->addRow(QString::fromLatin1(m_keys[n].desc.key), buildKeyEditor(n, page));
		}

		QPushButton *revertBtn = new QPushButton(tr("Revert %1").arg(title), page);
		revertBtn->setToolTip(tr("Puts this tab's values back to what they were when the dialog opened."));
		const size_t last = m_keys.size();
		connect(revertBtn, &QPushButton::clicked, this, [this, first, last]()
		{
			for (size_t n = first; n < last; ++n)
			{
				setKey(n, m_keys[n].original);
				loadKeyEditor(n);
			}
		});
		layout->addWidget(revertBtn, 0, Qt::AlignLeft);
		layout->addStretch(1);

		QScrollArea *scroll = new QScrollArea(this);
		scroll->setWidget(page);
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		return scroll;
	}

	QWidget *buildKeyEditor(size_t n, QWidget *parent)
	{
		TunedKey &k = m_keys[n];
		const QString help = QString::fromLatin1(k.desc.help ? k.desc.help : "");
		QWidget *editor = new QWidget(parent);
		QHBoxLayout *row = new QHBoxLayout(editor);
		row->setContentsMargins(0, 0, 0, 0);
		editor->setToolTip(help);

		if (k.desc.kind == WBQT_WATER_BOOL)
		{
			k.check = new QCheckBox(editor);
			k.check->setToolTip(help);
			row->addWidget(k.check);
			row->addStretch(1);
			connect(k.check, &QCheckBox::toggled, this, [this, n](bool on)
			{
				float v[3] = { on ? 1.0f : 0.0f, 0.0f, 0.0f };
				setKey(n, v);
			});
		}
		else if (k.desc.kind == WBQT_WATER_COLOR)
		{
			k.swatch = new QPushButton(editor);
			k.swatch->setFixedSize(60, 22);
			k.swatch->setToolTip(help);
			row->addWidget(k.swatch);
			row->addStretch(1);
			connect(k.swatch, &QPushButton::clicked, this, [this, n]()
			{
				TunedKey &key = m_keys[n];
				const bool unset = key.value[0] < 0.0f;
				const QColor start = unset ? QColor(128, 128, 128) : QColor((int)key.value[0], (int)key.value[1], (int)key.value[2]);
				const QColor picked = QColorDialog::getColor(start, this, QString::fromLatin1(key.desc.key));
				if (picked.isValid())
				{
					float v[3] = { (float)picked.red(), (float)picked.green(), (float)picked.blue() };
					setKey(n, v);
					loadKeyEditor(n);
				}
			});
		}
		else
		{
			const float step = (k.desc.step > 0.0f) ? k.desc.step : 0.01f;
			const int decimals = (step >= 1.0f) ? 0 : qBound(1, (int)ceil(-log10(step) - 1e-4), 4);
			k.slider = new QSlider(Qt::Horizontal, editor);
			k.slider->setRange(0, 1000);
			k.slider->setMinimumWidth(120);
			k.slider->setToolTip(help);
			k.spin = new QDoubleSpinBox(editor);
			k.spin->setDecimals(decimals);
			k.spin->setSingleStep(step);
			k.spin->setRange(k.desc.lo, k.desc.hi);
			k.spin->setMinimumWidth(80);
			k.spin->setToolTip(help);
			row->addWidget(k.slider, 1);
			row->addWidget(k.spin);
			QSlider *slider = k.slider;
			QDoubleSpinBox *spin = k.spin;
			const float lo = k.desc.lo;
			const float hi = k.desc.hi;
			connect(slider, &QSlider::valueChanged, this, [spin, lo, hi](int v)
			{
				spin->setValue(lo + (hi - lo) * v / 1000.0f);
			});
			connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, n, slider, lo, hi](double v)
			{
				const QSignalBlocker block(slider);
				slider->setValue((hi > lo) ? (int)((v - lo) / (hi - lo) * 1000.0 + 0.5) : 0);
				float value[3] = { (float)v, 0.0f, 0.0f };
				setKey(n, value);
			});
		}
		loadKeyEditor(n);
		return editor;
	}

	void loadKeyEditor(size_t n)
	{
		TunedKey &k = m_keys[n];
		m_loadingKey = true;
		if (k.check != NULL)
		{
			k.check->setChecked(k.value[0] >= 0.5f);
		}
		if (k.spin != NULL)
		{
			k.spin->setValue(k.value[0]);
		}
		if (k.swatch != NULL)
		{
			if (k.value[0] < 0.0f)
			{
				k.swatch->setText(tr("unset"));
				k.swatch->setStyleSheet(QString());
			}
			else
			{
				k.swatch->setText(QString());
				paintSwatch(k.swatch, QColor((int)k.value[0], (int)k.value[1], (int)k.value[2]));
			}
		}
		m_loadingKey = false;
	}

	void setKey(size_t n, const float v[3])
	{
		if (m_loadingKey)
		{
			return;
		}
		TunedKey &k = m_keys[n];
		memcpy(k.value, v, sizeof(k.value));
		WBQtWaterTuning_SetLive(k.index, k.value);
		m_keysChangedSinceRender = true;
		if (m_frozen)
		{
			goLive();
		}
	}

	QSpinBox *addSlider(QGridLayout *grid, int row, const QString &label, int lo, int hi, int typedMax, bool percent, const QString &help)
	{
		QLabel *name = new QLabel(label, this);
		QSlider *slider = new QSlider(Qt::Horizontal, this);
		QSpinBox *value = new QSpinBox(this);
		slider->setRange(lo, hi);
		value->setRange(lo, typedMax);
		if (percent)
		{
			value->setSuffix("%");
		}
		slider->setMinimumWidth(160);
		value->setMinimumWidth(70);
		name->setToolTip(help);
		slider->setToolTip(help);
		value->setToolTip(help);
		grid->addWidget(name, row, 0);
		grid->addWidget(slider, row, 1);
		grid->addWidget(value, row, 2);
		connect(slider, &QSlider::valueChanged, this, [value](int v)
		{
			value->setValue(v);
		});
		connect(value, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, slider](int v)
		{
			const QSignalBlocker block(slider);
			slider->setValue(v);
			refresh();
		});
		return value;
	}

	QPushButton *addColorButton(QGridLayout *grid, int row, const QString &label, QColor &color)
	{
		QPushButton *button = new QPushButton(this);
		button->setFixedSize(60, 22);
		grid->addWidget(new QLabel(label, this), row, 0);
		grid->addWidget(button, row, 1, Qt::AlignLeft);
		connect(button, &QPushButton::clicked, this, [this, button, label, &color]()
		{
			const QColor picked = QColorDialog::getColor(color, this, label);
			if (picked.isValid())
			{
				color = picked;
				paintSwatch(button, color);
				refresh();
			}
		});
		return button;
	}

	QSpinBox *addCellSpin(QHBoxLayout *row, const QString &label, int maximum)
	{
		QSpinBox *spin = new QSpinBox(this);
		spin->setRange(0, maximum);
		spin->setToolTip(tr("Cells from the map's south-west corner, without the border."));
		row->addWidget(new QLabel(label, this));
		row->addWidget(spin);
		return spin;
	}

	static void paintSwatch(QPushButton *button, const QColor &color)
	{
		button->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(color.name()));
	}

	static void selectData(QComboBox *combo, int value)
	{
		const int index = combo->findData(value);
		if (index >= 0)
		{
			combo->setCurrentIndex(index);
		}
	}

	void loadShading(const WBQtHQPreviewParams &params)
	{
		m_loading = true;
		m_relief->setValue((int)(params.relief*100.0f + 0.5f));
		m_elevation->setValue((int)(params.elevation*100.0f + 0.5f));
		m_falloff->setValue((int)(params.waterFalloff + 0.5f));
		m_depthTint->setChecked(params.depthTint != 0);
		m_shallow = QColor(params.shallow[0], params.shallow[1], params.shallow[2]);
		m_deep = QColor(params.deep[0], params.deep[1], params.deep[2]);
		paintSwatch(m_shallowBtn, m_shallow);
		paintSwatch(m_deepBtn, m_deep);
		m_loading = false;
		refresh();
	}

	void loadCapture(const WBQtHQCaptureParams &capture)
	{
		m_loading = true;
		m_objects->setChecked(capture.objects != 0);
		m_trees->setChecked(capture.trees != 0);
		m_roads->setChecked(capture.roads != 0);
		m_colorGrade->setChecked(capture.colorGrade != 0);
		m_renderedWater->setChecked(capture.renderedWater != 0);
		m_shaderWater->setChecked(capture.shaderWater != 0);
		m_clouds->setChecked(capture.clouds != 0);
		m_macroTexture->setChecked(capture.macroTexture != 0);
		m_stochastic->setChecked(capture.stochastic != 0);
		m_shadows->setChecked(capture.shadows != 0);
		m_timeOfDay->setCurrentIndex(qBound(0, capture.timeOfDay, m_timeOfDay->count() - 1));
		m_area->setCurrentIndex(qBound(0, capture.area, m_area->count() - 1));
		m_x0->setValue(capture.customX0);
		m_y0->setValue(capture.customY0);
		m_x1->setValue(capture.customX1);
		m_y1->setValue(capture.customY1);
		selectData(m_size, capture.size);
		selectData(m_supersample, capture.supersample);
		m_loading = false;
		updateRenderState();
	}

	WBQtHQPreviewParams shading() const
	{
		WBQtHQPreviewParams params;
		params.relief = m_relief->value() / 100.0f;
		params.elevation = m_elevation->value() / 100.0f;
		params.waterFalloff = (float)m_falloff->value();
		params.depthTint = m_depthTint->isChecked() ? 1 : 0;
		params.shallow[0] = m_shallow.red();
		params.shallow[1] = m_shallow.green();
		params.shallow[2] = m_shallow.blue();
		params.deep[0] = m_deep.red();
		params.deep[1] = m_deep.green();
		params.deep[2] = m_deep.blue();
		return params;
	}

	WBQtHQCaptureParams captureSettings() const
	{
		WBQtHQCaptureParams capture;
		capture.objects = m_objects->isChecked() ? 1 : 0;
		capture.trees = m_trees->isChecked() ? 1 : 0;
		capture.roads = m_roads->isChecked() ? 1 : 0;
		capture.colorGrade = m_colorGrade->isChecked() ? 1 : 0;
		capture.renderedWater = m_renderedWater->isChecked() ? 1 : 0;
		capture.shaderWater = m_shaderWater->isChecked() ? 1 : 0;
		capture.clouds = m_clouds->isChecked() ? 1 : 0;
		capture.macroTexture = m_macroTexture->isChecked() ? 1 : 0;
		capture.stochastic = m_stochastic->isChecked() ? 1 : 0;
		capture.shadows = m_shadows->isChecked() ? 1 : 0;
		capture.timeOfDay = m_timeOfDay->currentIndex();
		capture.area = m_area->currentIndex();
		capture.customX0 = m_x0->value();
		capture.customY0 = m_y0->value();
		capture.customX1 = m_x1->value();
		capture.customY1 = m_y1->value();
		capture.size = m_size->currentData().toInt();
		capture.supersample = m_supersample->currentData().toInt();
		return capture;
	}

	static bool sameCapture(const WBQtHQCaptureParams &a, const WBQtHQCaptureParams &b)
	{
		const bool sameCustom = a.area != WBQT_HQ_AREA_CUSTOM || (a.customX0 == b.customX0 && a.customY0 == b.customY0
			&& a.customX1 == b.customX1 && a.customY1 == b.customY1);
		return a.objects == b.objects && a.trees == b.trees && a.roads == b.roads && a.colorGrade == b.colorGrade
			&& a.renderedWater == b.renderedWater && a.shaderWater == b.shaderWater
			&& a.clouds == b.clouds && a.macroTexture == b.macroTexture && a.stochastic == b.stochastic
			&& a.shadows == b.shadows && a.timeOfDay == b.timeOfDay && a.area == b.area && sameCustom && a.size == b.size && a.supersample == b.supersample;
	}

	// A render setting changed, so the live view sets the scene up again with it.
	void captureChanged()
	{
		if (m_loading)
		{
			return;
		}
		updateRenderState();
		m_liveStarted = false;
		if (m_frozen)
		{
			goLive();
		}
	}

	void updateRenderState()
	{
		if (m_loading)
		{
			return;
		}
		const WBQtHQCaptureParams capture = captureSettings();
		const bool custom = capture.area == WBQT_HQ_AREA_CUSTOM;
		m_x0->setEnabled(custom);
		m_y0->setEnabled(custom);
		m_x1->setEnabled(custom);
		m_y1->setEnabled(custom);
		m_areaWarning->setVisible(capture.area != WBQT_HQ_AREA_MAP);
		m_renderedWater->setEnabled(!m_shaderWater->isChecked());

		const int maxCapture = WBQtHQPreview_MaxCapture();
		const int effective = qMax(1, qMin(capture.supersample, maxCapture / qMax(1, capture.size)));
		QString info = tr("Render draws %1 x %1 pixels.").arg(capture.size*effective);
		if (effective < capture.supersample)
		{
			info += " " + tr("Supersampling is reduced to %1x to stay within %2 pixels.").arg(effective).arg(maxCapture);
		}
		m_renderInfo->setText(info);
	}

	bool needsRender() const
	{
		return !m_frozen || m_keysChangedSinceRender || !sameCapture(captureSettings(), m_rendered);
	}

	void goLive()
	{
		m_frozen = false;
		m_liveStarted = false;
		m_liveBtn->setEnabled(false);
		m_view->setCurrentWidget(m_useNative ? (QWidget *)m_native : (QWidget *)m_image);
		m_status->setText(tr("Live view, without supersampling or shading. Render freezes a full-quality frame."));
		m_timer->start(kLiveIntervalMs);
		liveFrame();
	}

	void liveFrame()
	{
		if (m_frozen)
		{
			return;
		}
		if (!m_liveStarted)
		{
			const WBQtHQCaptureParams capture = captureSettings();
			if (WBQtHQPreview_LiveBegin(&capture) == 0)
			{
				showError(tr("Couldn't start the live view"));
				return;
			}
			m_liveStarted = true;
		}
		// Direct3D draws into the native window. Reading the frame back is the slower way, kept for when that fails.
		if (m_useNative)
		{
			const int scaled = (int)(kDisplaySize * m_native->devicePixelRatioF() + 0.5);
			if (WBQtHQPreview_LivePresent((void *)m_native->winId(), scaled) != 0)
			{
				return;
			}
			m_useNative = false;
			m_view->setCurrentWidget(m_image);
		}
		m_live.resize(kDisplaySize*kDisplaySize*4);
		if (WBQtHQPreview_LiveFrame(&m_live[0], kDisplaySize) == 0)
		{
			// A full render ends the session, so set it up once more before giving up.
			const WBQtHQCaptureParams capture = captureSettings();
			if (WBQtHQPreview_LiveBegin(&capture) == 0 || WBQtHQPreview_LiveFrame(&m_live[0], kDisplaySize) == 0)
			{
				showError(tr("Couldn't draw the live view"));
				return;
			}
		}
		const WBQtHQCaptureParams capture = captureSettings();
		int width = 0;
		int height = 0;
		WBQtHQPreview_FitSize(&capture, kDisplaySize, &width, &height);
		QImage image(&m_live[0], kDisplaySize, kDisplaySize, kDisplaySize*4, QImage::Format_RGB32);
		m_image->setPixmap(QPixmap::fromImage(image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
	}

	void showError(const QString &what)
	{
		m_timer->stop();
		char reason[256];
		WBQtHQPreview_GetError(reason, sizeof(reason));
		m_status->setText(QString("%1: %2.").arg(what).arg(QString::fromLocal8Bit(reason)));
	}

	bool render()
	{
		m_timer->stop();
		WBQtHQPreview_LiveEnd();
		m_liveStarted = false;

		const WBQtHQCaptureParams capture = captureSettings();
		QApplication::setOverrideCursor(Qt::WaitCursor);
		const int ok = WBQtHQPreview_Render(&capture);
		QApplication::restoreOverrideCursor();
		if (ok == 0)
		{
			char reason[256];
			WBQtHQPreview_GetError(reason, sizeof(reason));
			QMessageBox::warning(this, windowTitle(), tr("Couldn't render the map: %1.").arg(QString::fromLocal8Bit(reason)));
			goLive();
			return false;
		}
		m_rendered = capture;
		m_frozen = true;
		m_keysChangedSinceRender = false;
		m_liveBtn->setEnabled(true);
		m_view->setCurrentWidget(m_image);
		m_status->setText(tr("Rendered at full quality with the shading. Save writes this frame; Live goes back to the live view."));
		refresh();
		return true;
	}

	void refresh()
	{
		if (m_loading || !m_frozen)
		{
			return;
		}
		const WBQtHQPreviewParams params = shading();
		const int size = WBQtHQPreview_Size();
		m_pixels.resize(size*size*4);
		WBQtHQPreview_Compose(&params, &m_pixels[0]);
		int width = 0;
		int height = 0;
		WBQtHQPreview_FitSize(&m_rendered, kDisplaySize, &width, &height);
		QImage image(&m_pixels[0], size, size, size*4, QImage::Format_RGB32);
		m_image->setPixmap(QPixmap::fromImage(image.scaled(width, height,
			Qt::IgnoreAspectRatio, size < kDisplaySize ? Qt::FastTransformation : Qt::SmoothTransformation)));
	}

	std::vector<unsigned char> m_pixels;
	std::vector<unsigned char> m_live;
	std::vector<TunedKey> m_keys;
	QLabel *m_image;
	NativeView *m_native;
	QStackedWidget *m_view;
	bool m_useNative = true;
	QLabel *m_status;
	QPushButton *m_renderBtn;
	QPushButton *m_liveBtn;
	QTabWidget *m_tabs;
	QTimer *m_timer;

	QSpinBox *m_relief;
	QSpinBox *m_elevation;
	QSpinBox *m_falloff;
	QPushButton *m_shallowBtn;
	QPushButton *m_deepBtn;
	QCheckBox *m_depthTint;
	QColor m_shallow;
	QColor m_deep;

	QCheckBox *m_objects;
	QCheckBox *m_trees;
	QCheckBox *m_roads;
	QCheckBox *m_colorGrade;
	QCheckBox *m_renderedWater;
	QCheckBox *m_shaderWater;
	QCheckBox *m_clouds;
	QCheckBox *m_shadows;
	QCheckBox *m_macroTexture;
	QCheckBox *m_stochastic;
	QComboBox *m_timeOfDay;
	QComboBox *m_area;
	QSpinBox *m_x0;
	QSpinBox *m_y0;
	QSpinBox *m_x1;
	QSpinBox *m_y1;
	QLabel *m_areaWarning;
	QComboBox *m_size;
	QComboBox *m_supersample;
	QLabel *m_renderInfo;

	WBQtHQCaptureParams m_rendered;
	bool m_frozen = false;
	bool m_liveStarted = false;
	bool m_keysChangedSinceRender = false;
	bool m_loading = true;
	bool m_loadingKey = false;
};

}

extern "C" int WBQtHQPreview_Show(const char *tgaPath)
{
	WBQtHQPreviewDialog dlg(QString::fromLocal8Bit(tgaPath ? tgaPath : ""), WBQt_DialogParent());
	dlg.setWindowModality(Qt::ApplicationModal);
	return dlg.exec() == QDialog::Accepted ? 1 : 0;
}
