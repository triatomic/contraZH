// WBQtWaterTuningPanel.cpp -- see WBQtWaterTuningPanel.h.
#include "WBQtWaterTuningPanel.h"
#include "ui_WBQtWaterTuningPanel.h"
#include "WBQtWaterTuningBridge.h"
#include "WBQtMapIniEditorBridge.h"
#include "WBQtScrubSpinBox.h"
#include "WBQtWindowPos.h"

#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QPixmap>
#include <QSlider>
#include <QStyle>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

WBQtWaterTuningPanel *WBQtWaterTuningPanel::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	QWidget *s_owner = NULL;	// owner for the floating panel (created on first open)

	const int kSaveDelayMs = 400;
	const int kSaveRetryMs = 1500;

	// What a ticked ShaderWaterDeepColor starts from when Water.ini leaves it unset.
	const float kDeepColorStart[3] = { 40.0f, 85.0f, 95.0f };

	enum
	{
		COL_LABEL = 0,
		COL_SLIDER,
		COL_SPIN,
		COL_RESET
	};

	QString settingLabel(const QString &key)
	{
		static const char *const names[][2] =
		{
			{ "IsWater", "Is water" },
			{ "ShaderWaterWaveScale", "Ripple scale" },
			{ "ShaderWaterWaveStrength", "Ripple strength" },
			{ "ShaderWaterSpecular", "Sun glint" },
			{ "ShaderWaterDeepColor", "Deep colour" },
			{ "ShaderWaterAutoMeasure", "Calm enclosed water automatically" },
			{ "ShaderWaterZoomCompensation", "Keep ripple size when zooming" },
			{ "TransparentWaterDepth", "Shore depth" },
			{ "TransparentWaterMinOpacity", "Minimum opacity" },
			{ "ColorLut", "Colour table" },
			{ "ColorLutStrength", "Strength" },
			{ "DistanceFog", "Enable distance fog" },
			{ "DistanceFogColor", "Custom colour" }
		};
		for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); ++i)
		{
			if (key == QLatin1String(names[i][0]))
			{
				return QString::fromLatin1(names[i][1]);
			}
		}
		QString label = key;
		label.remove(QRegularExpression("^(ShaderWater|TerrainGlint|GroundNoise|TerrainHeightBlend|SkyCloud|DistanceFog)"));
		label.replace(QRegularExpression("([a-z0-9])([A-Z])"), "\\1 \\2");
		label = label.toLower();
		if (!label.isEmpty())
		{
			label[0] = label.at(0).toUpper();
		}
		return label;
	}

	QString waterGroup(const QString &key, bool advanced)
	{
		if (advanced)
		{
			return "Advanced";
		}
		if (key == "IsWater" || key == "AdditiveBlending")
		{
			return QString();
		}
		if (key == "ShaderWaterOpacity" || key == "ShaderWaterClarity" || key == "ShaderWaterDeepColor")
		{
			return "Appearance";
		}
		if (key == "ShaderWaterWaveScale" || key == "ShaderWaterWaveStrength" || key == "ShaderWaterSwellHeight")
		{
			return "Waves";
		}
		if (key == "ShaderWaterAutoMeasure" || key == "ShaderWaterEnclosedCalm" || key == "ShaderWaterZoomCompensation")
		{
			return "Behaviour";
		}
		return "Light & foam";
	}

	int decimalsOf(double value)
	{
		const QString text = QString::number(value, 'g', 6);
		if (text.contains('e'))
		{
			return 6;
		}
		const int dot = text.indexOf('.');
		return (dot < 0) ? 0 : text.size() - dot - 1;
	}

	//----------------------------------------------------------------------------------------
	// A WaterTransparency or GameData block of a map.ini, as text.
	//----------------------------------------------------------------------------------------

	struct Block
	{
		bool found;
		int bodyStart;	// first character after the header line
		int bodyEnd;	// first character of the End line
	};

	Block findBlock(const QString &text, const QString &name)
	{
		const QRegularExpression header("^[ \\t]*" + QRegularExpression::escape(name) + "\\b[^\\n]*\\n",
			QRegularExpression::MultilineOption);
		static const QRegularExpression word("^[ \\t]*(\\w+)[ \\t]*(?:;[^\\n]*)?\\r?$",
			QRegularExpression::MultilineOption);

		Block block = { false, 0, 0 };
		const QRegularExpressionMatch head = header.match(text);
		if (!head.hasMatch())
		{
			return block;
		}

		// A line of one word other than End opens a nested block, which has an End of its own.
		int depth = 0;
		QRegularExpressionMatchIterator lines = word.globalMatch(text, head.capturedEnd());
		while (lines.hasNext())
		{
			const QRegularExpressionMatch line = lines.next();
			if (line.captured(1) != "End")
			{
				++depth;
				continue;
			}
			if (depth == 0)
			{
				block.found = true;
				block.bodyStart = head.capturedEnd();
				block.bodyEnd = line.capturedStart();
				return block;
			}
			--depth;
		}
		return block;
	}

	// Captures: 1 the "Key = " lead, 2 the value, 3 the blanks before a comment or the line end.
	QRegularExpression keyPattern(const QString &key)
	{
		return QRegularExpression("^([ \\t]*" + QRegularExpression::escape(key)
			+ "[ \\t]*=[ \\t]*)([^;\\r\\n]*?)([ \\t]*)(?=;|\\r?$)",
			QRegularExpression::MultilineOption);
	}

	// The key's value text, or a null string when the block does not set it.
	QString readValue(const QString &body, const QString &key)
	{
		const QRegularExpressionMatch m = keyPattern(key).match(body);
		if (!m.hasMatch())
		{
			return QString();
		}
		const QString value = m.captured(2);
		return value.isNull() ? QString("") : value;
	}

	void writeValue(QString &body, const QString &key, const QString &value, const QString &nl)
	{
		const QRegularExpressionMatch m = keyPattern(key).match(body);
		if (m.hasMatch())
		{
			// Padding keeps a trailing comment in its column.
			QString pad;
			const int after = m.capturedEnd(3);
			if (after < body.size() && body.at(after) == ';')
			{
				const int width = m.capturedLength(2) + m.capturedLength(3) - value.size();
				pad = QString(qMax(1, width), ' ');
			}
			body.replace(m.capturedStart(2), after - m.capturedStart(2), value + pad);
			return;
		}

		// A new key takes the indent of the block's last line.
		static const QRegularExpression indented("^([ \\t]+)\\S[^\\n]*\\n?\\z",
			QRegularExpression::MultilineOption);
		const QRegularExpressionMatch last = indented.match(body);
		const QString indent = last.hasMatch() ? last.captured(1) : QString("  ");
		if (!body.isEmpty() && !body.endsWith('\n'))
		{
			body += nl;
		}
		body += indent + key + " = " + value + nl;
	}

	void removeValue(QString &body, const QString &key)
	{
		const QRegularExpression line("^[ \\t]*" + QRegularExpression::escape(key)
			+ "[ \\t]*=[^\\n]*\\n?", QRegularExpression::MultilineOption);
		const QRegularExpressionMatch m = line.match(body);
		if (m.hasMatch())
		{
			body.remove(m.capturedStart(), m.capturedLength());
		}
	}

	// Writes one block's pending keys into the file text. False when the block is absent and no key needs it.
	bool editBlock(QString &text, const QString &name, const QMap<QString, QString> &pending, const QString &nl)
	{
		bool anyWrite = false;
		QMap<QString, QString>::const_iterator it;
		for (it = pending.constBegin(); it != pending.constEnd(); ++it)
		{
			if (!it.value().isNull())
			{
				anyWrite = true;
			}
		}

		Block block = findBlock(text, name);
		if (!block.found)
		{
			if (!anyWrite)
			{
				return false;
			}
			if (!text.isEmpty())
			{
				text += text.endsWith('\n') ? nl : nl + nl;
			}
			text += name + nl;
			block.bodyStart = text.size();
			block.bodyEnd = text.size();
			text += "End" + nl;
		}

		QString body = text.mid(block.bodyStart, block.bodyEnd - block.bodyStart);
		for (it = pending.constBegin(); it != pending.constEnd(); ++it)
		{
			if (it.value().isNull())
			{
				removeValue(body, it.key());
			}
			else
			{
				writeValue(body, it.key(), it.value(), nl);
			}
		}
		text.replace(block.bodyStart, block.bodyEnd - block.bodyStart, body);
		return true;
	}

	bool parseValue(int kind, const QString &raw, float value[3])
	{
		const QString text = raw.trimmed();
		switch (kind)
		{
			case WBQT_WATER_FLOAT:
			{
				bool ok = false;
				const float parsed = text.section(QRegularExpression("\\s"), 0, 0).toFloat(&ok);
				if (ok)
				{
					value[0] = parsed;
				}
				return ok;
			}
			case WBQT_WATER_BOOL:
			{
				if (text.compare("yes", Qt::CaseInsensitive) == 0)
				{
					value[0] = 1.0f;
					return true;
				}
				if (text.compare("no", Qt::CaseInsensitive) == 0)
				{
					value[0] = 0.0f;
					return true;
				}
				return false;
			}
			case WBQT_WATER_COLOR:
			{
				static const QRegularExpression rgb("R:\\s*(\\d+)\\s+G:\\s*(\\d+)\\s+B:\\s*(\\d+)");
				const QRegularExpressionMatch m = rgb.match(text);
				if (!m.hasMatch())
				{
					return false;
				}
				for (int c = 0; c < 3; ++c)
				{
					value[c] = m.captured(c + 1).toFloat();
				}
				return true;
			}
		}
		return false;
	}

	// A text value is its first word, which is all a file name or None is.
	QString firstWord(const QString &raw)
	{
		return raw.trimmed().section(QRegularExpression("\\s"), 0, 0);
	}

	QStringList tableNames()
	{
		char buf[4096];
		WBQtWaterTuning_ListTables(buf, sizeof(buf));
		return QString::fromLocal8Bit(buf).split('\n', QString::SkipEmptyParts);
	}

	QToolButton *makeButton(QWidget *parent, const QString &text, int row)
	{
		QToolButton *button = new QToolButton(parent);
		button->setText(text);
		button->setFixedWidth(24);
		button->setFocusPolicy(Qt::NoFocus);
		button->setProperty("row", row);
		return button;
	}
}

WBQtWaterTuningPanel::Row::Row()
	: index(0),
	  kind(WBQT_WATER_FLOAT),
	  lo(0.0f),
	  hi(1.0f),
	  step(1.0f),
	  advanced(false),
	  inFile(false),
	  label(NULL),
	  check(NULL),
	  combo(NULL),
	  spin(NULL),
	  slider(NULL),
	  colour(NULL),
	  reset(NULL)
{
	for (int c = 0; c < 3; ++c)
	{
		value[c] = 0.0f;
	}
}

WBQtWaterTuningPanel::WBQtWaterTuningPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtWaterTuningPanel),
	  m_saveTimer(new QTimer(this)),
	  m_updating(false),
	  m_advancedGroup(NULL)
{
	// The shell and its two tabs live in WBQtWaterTuningPanel.ui; the rows come from the bridge's key table.
	m_ui->setupUi(this);
	WBQtWindowPos_Track(this, "WaterTuning");

	buildRows();
	onAdvancedToggled(m_ui->advancedCheck->isChecked());

	m_saveTimer->setSingleShot(true);
	connect(m_saveTimer, SIGNAL(timeout()), this, SLOT(onSaveTimer()));
	connect(m_ui->advancedCheck, SIGNAL(toggled(bool)), this, SLOT(onAdvancedToggled(bool)));

	s_instance = this;
}

WBQtWaterTuningPanel::~WBQtWaterTuningPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtWaterTuningPanel::addSlider(Row &row, int rowIndex, QGridLayout *grid, int gridRow, const QString &tip)
{
	QWidget *host = grid->parentWidget();
	row.slider = new QSlider(Qt::Horizontal, host);
	row.slider->setObjectName(row.key + "Slider");
	row.slider->setAccessibleName(settingLabel(row.key));
	row.slider->setRange(0, qMax(1, qRound((row.hi - row.lo) / row.step)));
	row.slider->setProperty("row", rowIndex);
	row.slider->setToolTip(tip);
	row.slider->setMinimumWidth(80);

	row.spin = new WBQtScrubSpinBox(host);
	row.spin->setObjectName(row.key + "Value");
	row.spin->setAccessibleName(settingLabel(row.key));
	row.spin->setKeyboardTracking(false);
	row.spin->setAlignment(Qt::AlignRight);
	row.spin->setFixedWidth(86);
	row.spin->setToolTip(tip);
	row.spin->setProperty("row", rowIndex);
	// Typed and loaded values can exceed the slider's normal tuning range.
	row.spin->setRange((row.lo < 0.0f) ? row.lo * 10.0 - 10.0 : 0.0, row.hi * 10.0 + 10.0);
	row.spin->setSingleStep(row.step);
	row.spin->setDecimals(decimalsOf(row.step));

	grid->addWidget(row.slider, gridRow, COL_SLIDER);
	grid->addWidget(row.spin, gridRow, COL_SPIN);
	connect(row.slider, SIGNAL(valueChanged(int)), this, SLOT(onSliderChanged(int)));
	connect(row.spin, SIGNAL(valueChanged(double)), this, SLOT(onSpinChanged(double)));
	row.widgets << row.slider << row.spin;
}

// A scrolling grid of rows on its own tab, like the Water tab in WBQtWaterTuningPanel.ui.
QGridLayout *WBQtWaterTuningPanel::addTab(const QString &title)
{
	QWidget *page = new QWidget(m_ui->tabs);
	QVBoxLayout *layout = new QVBoxLayout(page);
	QScrollArea *scroll = new QScrollArea(page);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidgetResizable(true);
	QWidget *host = new QWidget(scroll);
	QGridLayout *grid = new QGridLayout(host);
	grid->setColumnMinimumWidth(COL_LABEL, 155);
	grid->setColumnStretch(COL_SLIDER, 1);
	grid->setVerticalSpacing(4);
	grid->setAlignment(Qt::AlignTop);
	scroll->setWidget(host);
	layout->addWidget(scroll);
	m_ui->tabs->addTab(page, title);
	return grid;
}

void WBQtWaterTuningPanel::buildRows()
{
	QMap<QString, QGridLayout *> grids;
	QMap<QGridLayout *, int> gridRows;
	int waterRow = 0;
	m_ui->rowsGrid->setColumnMinimumWidth(COL_LABEL, 155);
	m_ui->rowsGrid->setColumnStretch(COL_SLIDER, 1);
	m_ui->rowsGrid->setAlignment(Qt::AlignTop);
	m_ui->rowsGrid->setVerticalSpacing(6);

	const int count = WBQtWaterTuning_Count();
	m_rows.reserve(count);
	for (int i = 0; i < count; ++i)
	{
		WBQtWaterTuningDesc desc;
		if (!WBQtWaterTuning_GetDesc(i, &desc))
		{
			continue;
		}
		const int r = m_rows.size();
		const QString tip = QString::fromLatin1(desc.key) + "\n" + QString::fromLatin1(desc.help);
		Row row;
		row.index = i;
		row.kind = desc.kind;
		row.key = QString::fromLatin1(desc.key);
		row.block = QString::fromLatin1(desc.block);
		row.lo = desc.lo;
		row.hi = desc.hi;
		row.step = desc.step;
		row.advanced = desc.advanced != 0;

		const bool water = (row.block == "WaterTransparency");
		const QString group = water ? waterGroup(row.key, row.advanced) : QString::fromLatin1(desc.group);
		QGridLayout *grid = m_ui->rowsGrid;
		int gridRow;
		if (water && group.isEmpty())
		{
			gridRow = waterRow++;
		}
		else
		{
			const QString groupKey = row.block + "/" + group;
			if (!grids.contains(groupKey))
			{
				if (water)
				{
					QString title = group;
					title.replace("&", "&&");
					QGroupBox *box = new QGroupBox(title, m_ui->rowsHost);
					grid = new QGridLayout(box);
					grid->setColumnMinimumWidth(COL_LABEL, 155);
					grid->setColumnStretch(COL_SLIDER, 1);
					grid->setVerticalSpacing(4);
					grid->setContentsMargins(8, 4, 8, 6);
					m_ui->rowsGrid->addWidget(box, waterRow++, 0, 1, COL_RESET + 1);
					if (row.advanced)
					{
						m_advancedGroup = box;
					}
				}
				else
				{
					grid = addTab(group);
				}
				grids[groupKey] = grid;
			}
			grid = grids.value(groupKey);
			gridRow = gridRows[grid]++;
		}
		QWidget *host = grid->parentWidget();
		row.reset = makeButton(host, QString(), r);
		row.reset->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
		row.reset->setObjectName(row.key + "Reset");
		row.reset->setAccessibleName(tr("Reset %1").arg(settingLabel(row.key)));
		row.reset->setToolTip(tr("Remove the key from map.ini, so the map follows %1")
			.arg(water ? "Water.ini" : "GameData.ini"));
		connect(row.reset, SIGNAL(clicked()), this, SLOT(onResetClicked()));
		grid->addWidget(row.reset, gridRow, COL_RESET);
		row.widgets << row.reset;

		if (desc.kind == WBQT_WATER_FLOAT || desc.kind == WBQT_WATER_TEXT || desc.kind == WBQT_WATER_CHOICE)
		{
			row.label = new QLabel(settingLabel(row.key), host);
			row.label->setToolTip(tip);
			grid->addWidget(row.label, gridRow, COL_LABEL);
			row.widgets << row.label;
			if (desc.kind == WBQT_WATER_FLOAT)
			{
				addSlider(row, r, grid, gridRow, tip);
				row.label->setBuddy(row.spin);
			}
			else if (desc.kind == WBQT_WATER_CHOICE)
			{
				row.combo = new QComboBox(host);
				row.combo->setObjectName(row.key + "Combo");
				row.combo->setToolTip(tip);
				row.combo->setProperty("row", r);
				for (int choice = 0; desc.choices != NULL && desc.choices[choice] != NULL; ++choice)
				{
					row.combo->addItem(QString::fromLatin1(desc.choices[choice]));
				}
				grid->addWidget(row.combo, gridRow, COL_SLIDER, 1, COL_SPIN - COL_SLIDER + 1);
				connect(row.combo, SIGNAL(activated(int)), this, SLOT(onChoiceChosen(int)));
				row.label->setBuddy(row.combo);
				row.widgets << row.combo;
			}
			else
			{
				row.combo = new QComboBox(host);
				row.combo->setEditable(true);
				row.combo->setInsertPolicy(QComboBox::NoInsert);
				row.combo->setToolTip(tip);
				row.combo->setProperty("row", r);
				row.combo->lineEdit()->setProperty("row", r);
				row.combo->addItem("None");
				row.combo->addItems(tableNames());
				grid->addWidget(row.combo, gridRow, COL_SLIDER, 1, COL_SPIN - COL_SLIDER + 1);
				connect(row.combo, SIGNAL(activated(int)), this, SLOT(onTextChosen()));
				connect(row.combo->lineEdit(), SIGNAL(editingFinished()), this, SLOT(onTextChosen()));
				row.label->setBuddy(row.combo);
				row.widgets << row.combo;
			}
		}
		else
		{
			row.check = new QCheckBox(settingLabel(row.key), host);
			row.check->setObjectName(row.key + "Check");
			row.check->setToolTip(tip);
			row.check->setProperty("row", r);
			connect(row.check, SIGNAL(toggled(bool)), this, SLOT(onCheckToggled(bool)));
			grid->addWidget(row.check, gridRow, COL_LABEL, 1, desc.kind == WBQT_WATER_COLOR ? 1 : COL_RESET);
			row.widgets << row.check;
			if (desc.kind == WBQT_WATER_COLOR)
			{
				row.colour = makeButton(host, QString(), r);
				row.colour->setFixedWidth(86);
				row.colour->setIconSize(QSize(60, 16));
				row.colour->setObjectName(row.key + "Colour");
				row.colour->setAccessibleName(tr("Choose %1").arg(settingLabel(row.key)));
				row.colour->setToolTip(tip);
				connect(row.colour, SIGNAL(clicked()), this, SLOT(onColourClicked()));
				grid->addWidget(row.colour, gridRow, COL_SLIDER, 1, COL_SPIN - COL_SLIDER + 1, Qt::AlignRight);
				row.widgets << row.colour;
			}
		}
		m_rows.append(row);
	}
}

QString WBQtWaterTuningPanel::formatValue(const Row &row) const
{
	switch (row.kind)
	{
		case WBQT_WATER_BOOL:
			return (row.value[0] >= 0.5f) ? "Yes" : "No";
		case WBQT_WATER_COLOR:
			return QString("R:%1 G:%2 B:%3")
				.arg(qBound(0, qRound(row.value[0]), 255))
				.arg(qBound(0, qRound(row.value[1]), 255))
				.arg(qBound(0, qRound(row.value[2]), 255));
		case WBQT_WATER_CHOICE:
			return row.combo->itemText(qRound(row.value[0]));
		case WBQT_WATER_TEXT:
			return row.text.isEmpty() ? QString("None") : row.text;
		default:
			return QString::number(row.value[0], 'g', 6);
	}
}

void WBQtWaterTuningPanel::setStatus(const QString &text)
{
	m_ui->statusLabel->setText(text);
}

// Put a row's value and its "set in map.ini" state on its widgets.
void WBQtWaterTuningPanel::showRow(int r)
{
	const bool wasUpdating = m_updating;
	m_updating = true;

	Row &row = m_rows[r];
	QWidget *title = (row.label != NULL) ? static_cast<QWidget *>(row.label) : row.check;
	QFont font = title->font();
	font.setBold(row.inFile);
	title->setFont(font);
	if (row.spin != NULL)
	{
		row.spin->setFont(font);
	}
	if (row.combo != NULL)
	{
		row.combo->setFont(font);
	}
	row.reset->setEnabled(row.inFile);

	switch (row.kind)
	{
		case WBQT_WATER_FLOAT:
		{
			// A loaded value keeps the decimals it was written with.
			row.spin->setDecimals(qMax(decimalsOf(row.step), decimalsOf(row.value[0])));
			row.spin->setValue(row.value[0]);
			row.slider->setValue(qRound((row.value[0] - row.lo) / row.step));
			break;
		}
		case WBQT_WATER_BOOL:
		{
			row.check->setChecked(row.value[0] >= 0.5f);
			break;
		}
		case WBQT_WATER_COLOR:
		{
			row.check->setChecked(row.inFile);
			QPixmap swatch(60, 16);
			swatch.fill(QColor(qBound(0, qRound((row.value[0] < 0.0f) ? kDeepColorStart[0] : row.value[0]), 255),
				qBound(0, qRound((row.value[1] < 0.0f) ? kDeepColorStart[1] : row.value[1]), 255),
				qBound(0, qRound((row.value[2] < 0.0f) ? kDeepColorStart[2] : row.value[2]), 255)));
			row.colour->setIcon(QIcon(swatch));
			break;
		}
		case WBQT_WATER_CHOICE:
		{
			row.combo->setCurrentIndex(qRound(row.value[0]));
			break;
		}
		case WBQT_WATER_TEXT:
		{
			row.combo->setEditText(formatValue(row));
			break;
		}
	}

	m_updating = wasUpdating;
}

void WBQtWaterTuningPanel::loadBase(Row &row)
{
	if (row.kind == WBQT_WATER_TEXT)
	{
		char buf[256];
		WBQtWaterTuning_GetBaseText(row.index, buf, sizeof(buf));
		row.text = QString::fromLocal8Bit(buf);
		return;
	}
	WBQtWaterTuning_GetBase(row.index, row.value);
}

void WBQtWaterTuningPanel::pushLive(const Row &row)
{
	if (row.kind == WBQT_WATER_TEXT)
	{
		WBQtWaterTuning_SetLiveText(row.index, formatValue(row).toLocal8Bit().constData());
		return;
	}
	WBQtWaterTuning_SetLive(row.index, row.value);
}

// The user changed a row: show it in the 3D view and queue it for map.ini.
void WBQtWaterTuningPanel::markChanged(int r)
{
	Row &row = m_rows[r];
	row.inFile = true;
	showRow(r);
	pushLive(row);
	updateWaterAvailability();
	m_pending[row.key] = formatValue(row);
	m_saveTimer->start(kSaveDelayMs);
}

// Take the key out of map.ini and return the row to the Water.ini value.
void WBQtWaterTuningPanel::resetRow(int r)
{
	Row &row = m_rows[r];
	loadBase(row);
	row.inFile = false;
	showRow(r);
	pushLive(row);
	updateWaterAvailability();
	m_pending[row.key] = QString();
	m_saveTimer->start(kSaveDelayMs);
}

void WBQtWaterTuningPanel::openOn(const QString &iniPath)
{
	flush();
	if (iniPath != m_path)
	{
		m_pending.clear();
	}
	m_path = iniPath;
	reseed();
	setStatus(QDir::toNativeSeparators(m_path));
}

void WBQtWaterTuningPanel::reseed()
{
	QString text;
	QFile file(m_path);
	if (!m_path.isEmpty() && file.open(QIODevice::ReadOnly))
	{
		text = QString::fromLatin1(file.readAll());
		file.close();
	}

	QMap<QString, QString> bodies;
	for (int r = 0; r < m_rows.size(); ++r)
	{
		const QString &name = m_rows[r].block;
		if (!bodies.contains(name))
		{
			const Block block = findBlock(text, name);
			bodies[name] = block.found ? text.mid(block.bodyStart, block.bodyEnd - block.bodyStart) : QString();
		}
	}

	// Edits a failed save left pending win over the file, and stay queued.
	for (int r = 0; r < m_rows.size(); ++r)
	{
		Row &row = m_rows[r];
		loadBase(row);
		const QString raw = m_pending.contains(row.key) ? m_pending.value(row.key) : readValue(bodies.value(row.block), row.key);
		if (row.kind == WBQT_WATER_TEXT)
		{
			row.inFile = !raw.isNull() && !firstWord(raw).isEmpty();
			if (row.inFile)
			{
				row.text = firstWord(raw);
			}
		}
		else if (row.kind == WBQT_WATER_CHOICE)
		{
			const int choice = row.combo->findText(firstWord(raw), Qt::MatchFixedString);
			row.inFile = !raw.isNull() && choice >= 0;
			if (row.inFile)
			{
				row.value[0] = (float)choice;
			}
		}
		else
		{
			row.inFile = !raw.isNull() && parseValue(row.kind, raw, row.value);
		}
		showRow(r);
		pushLive(row);
	}
	updateWaterAvailability();
	if (!m_pending.isEmpty())
	{
		m_saveTimer->start(kSaveRetryMs);
	}
}

bool WBQtWaterTuningPanel::flush()
{
	m_saveTimer->stop();
	if (m_pending.isEmpty() || m_path.isEmpty())
	{
		m_pending.clear();
		return true;
	}

	const QByteArray localPath = m_path.toLocal8Bit();
	if (WBQtMapIniEditor_HasUnsavedChanges(localPath.constData()))
	{
		setStatus(tr("Not saved: map.ini has unsaved edits in the map.ini editor."));
		return false;
	}

	QString text;
	QFile file(m_path);
	if (file.exists())
	{
		if (!file.open(QIODevice::ReadOnly))
		{
			setStatus(tr("Not saved: could not read %1.").arg(QDir::toNativeSeparators(m_path)));
			return false;
		}
		text = QString::fromLatin1(file.readAll());
		file.close();
	}
	const QString nl = (text.isEmpty() || text.contains("\r\n")) ? "\r\n" : "\n";

	QMap<QString, QMap<QString, QString> > byBlock;
	for (int r = 0; r < m_rows.size(); ++r)
	{
		const Row &row = m_rows[r];
		if (m_pending.contains(row.key))
		{
			byBlock[row.block][row.key] = m_pending.value(row.key);
		}
	}

	bool edited = false;
	QMap<QString, QMap<QString, QString> >::const_iterator it;
	for (it = byBlock.constBegin(); it != byBlock.constEnd(); ++it)
	{
		if (editBlock(text, it.key(), it.value(), nl))
		{
			edited = true;
		}
	}
	if (!edited)
	{
		m_pending.clear();
		return true;
	}

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		setStatus(tr("Not saved: could not write %1.").arg(QDir::toNativeSeparators(m_path)));
		return false;
	}
	file.write(text.toLatin1());
	file.close();

	m_pending.clear();
	WBQtWaterTuning_NoteSaved();
	WBQtMapIniEditor_ReloadIfOpen(localPath.constData());
	setStatus(tr("Saved to %1.").arg(QDir::toNativeSeparators(m_path)));
	return true;
}

void WBQtWaterTuningPanel::mapChanged()
{
	flush();
	m_pending.clear();
	m_path.clear();
	hide();
}

void WBQtWaterTuningPanel::hideEvent(QHideEvent *event)
{
	flush();
	QWidget::hideEvent(event);
}

void WBQtWaterTuningPanel::onSpinChanged(double value)
{
	if (m_updating)
	{
		return;
	}
	const int r = sender()->property("row").toInt();
	m_rows[r].value[0] = (float)value;
	markChanged(r);
}

void WBQtWaterTuningPanel::onSliderChanged(int value)
{
	if (m_updating)
	{
		return;
	}
	Row &row = m_rows[sender()->property("row").toInt()];
	row.spin->setValue(qBound((double)row.lo, row.lo + value * (double)row.step, (double)row.hi));
}

void WBQtWaterTuningPanel::onColourClicked()
{
	const int r = sender()->property("row").toInt();
	Row &row = m_rows[r];
	const QColor initial(qBound(0, qRound((row.value[0] < 0.0f) ? kDeepColorStart[0] : row.value[0]), 255),
		qBound(0, qRound((row.value[1] < 0.0f) ? kDeepColorStart[1] : row.value[1]), 255),
		qBound(0, qRound((row.value[2] < 0.0f) ? kDeepColorStart[2] : row.value[2]), 255));
	const QColor colour = QColorDialog::getColor(initial, this, settingLabel(row.key));
	if (!colour.isValid())
	{
		return;
	}
	row.value[0] = (float)colour.red();
	row.value[1] = (float)colour.green();
	row.value[2] = (float)colour.blue();
	markChanged(r);
}

void WBQtWaterTuningPanel::updateWaterAvailability()
{
	bool enabled = true;
	bool additive = false;
	for (int r = 0; r < m_rows.size(); ++r)
	{
		if (m_rows[r].key == "IsWater")
		{
			enabled = m_rows[r].value[0] >= 0.5f;
		}
		if (m_rows[r].key == "AdditiveBlending")
		{
			additive = m_rows[r].value[0] >= 0.5f;
		}
	}
	enabled = enabled && !additive;
	m_ui->waterNotice->setVisible(!enabled);
	m_ui->waterNotice->setText(additive ? tr("Additive blending uses legacy water. Shader controls are disabled.")
		: tr("Enable Is water to use shader water."));
	for (int r = 0; r < m_rows.size(); ++r)
	{
		Row &row = m_rows[r];
		if (row.block != "WaterTransparency" || !row.key.startsWith("ShaderWater"))
		{
			continue;
		}
		for (int w = 0; w < row.widgets.size(); ++w)
		{
			row.widgets[w]->setEnabled(enabled);
		}
		row.reset->setEnabled(enabled && row.inFile);
	}
}

void WBQtWaterTuningPanel::onCheckToggled(bool on)
{
	if (m_updating)
	{
		return;
	}
	const int r = sender()->property("row").toInt();
	Row &row = m_rows[r];
	if (row.kind == WBQT_WATER_BOOL)
	{
		row.value[0] = on ? 1.0f : 0.0f;
		markChanged(r);
		return;
	}

	// The colour's box says whether map.ini sets the key at all.
	if (!on)
	{
		resetRow(r);
		return;
	}
	for (int c = 0; c < 3; ++c)
	{
		if (row.value[c] < 0.0f)
		{
			row.value[c] = kDeepColorStart[c];
		}
	}
	markChanged(r);
}

void WBQtWaterTuningPanel::onChoiceChosen(int choice)
{
	if (m_updating || choice < 0)
	{
		return;
	}
	const int r = sender()->property("row").toInt();
	m_rows[r].value[0] = (float)choice;
	markChanged(r);
}

void WBQtWaterTuningPanel::onTextChosen()
{
	if (m_updating)
	{
		return;
	}
	const int r = sender()->property("row").toInt();
	Row &row = m_rows[r];
	const QString text = firstWord(row.combo->currentText());
	if (text.isEmpty() || (row.inFile && text == row.text))
	{
		return;
	}
	row.text = text;
	markChanged(r);
}

void WBQtWaterTuningPanel::onResetClicked()
{
	resetRow(sender()->property("row").toInt());
}

void WBQtWaterTuningPanel::onAdvancedToggled(bool on)
{
	if (m_advancedGroup != NULL)
	{
		m_advancedGroup->setVisible(on);
	}
	for (int r = 0; r < m_rows.size(); ++r)
	{
		const Row &row = m_rows[r];
		if (!row.advanced)
		{
			continue;
		}
		for (int w = 0; w < row.widgets.size(); ++w)
		{
			row.widgets[w]->setVisible(on);
		}
	}
}

void WBQtWaterTuningPanel::onSaveTimer()
{
	if (!flush() && isVisible())
	{
		m_saveTimer->start(kSaveRetryMs);
	}
}

// --- Open / map change / refresh hooks (the Qt side of WBQtWaterTuningBridge.h) ---------------

extern "C" void WBQtWaterTuning_Open(void *frameHwnd, const char *iniPath)
{
	if (frameHwnd == NULL || iniPath == NULL)
	{
		return;
	}
	if (s_owner == NULL)
	{
		s_owner = WBQt_CreateOwnerBridgeWidget(frameHwnd);
	}
	WBQtWaterTuningPanel *panel = WBQtWaterTuningPanel::instance();
	if (panel == NULL)
	{
		panel = new WBQtWaterTuningPanel(s_owner);
	}
	panel->openOn(QString::fromLocal8Bit(iniPath));
	// Show WITHOUT activating (== the MFC SW_SHOWNA) so the viewport keeps focus.
	panel->setAttribute(Qt::WA_ShowWithoutActivating);
	panel->show();
	panel->raise();
}

extern "C" void WBQtWaterTuning_MapChanged(void)
{
	WBQtWaterTuningPanel *panel = WBQtWaterTuningPanel::instance();
	if (panel != NULL)
	{
		panel->mapChanged();
	}
}

extern "C" void WBQtWaterTuning_PushRefresh(void)
{
	WBQtWaterTuningPanel *panel = WBQtWaterTuningPanel::instance();
	if (panel != NULL && panel->isVisible())
	{
		panel->flush();
		panel->reseed();
	}
}
