// WBQtWaterTuningPanel.h -- the modeless Water tuning window (File > Map.ini > Water tuning).
//
// One row per key of the bridge's table. The WaterTransparency keys sit on the Water tab and the
// GameData keys on a tab per group. A change shows in the 3D view at once and is saved to
// the key's block of the map's map.ini. Only keys the map overrides are written; the rest keep
// following Water.ini and GameData.ini.
#ifndef WB_QT_WATER_TUNING_PANEL_H
#define WB_QT_WATER_TUNING_PANEL_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QGroupBox;
class QHideEvent;
class QLabel;
class QTimer;
class QSlider;
class QToolButton;
class WBQtScrubSpinBox;

namespace Ui { class WBQtWaterTuningPanel; }	// generated from WBQtWaterTuningPanel.ui

class WBQtWaterTuningPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtWaterTuningPanel(QWidget *owner);
	virtual ~WBQtWaterTuningPanel();

	// Point the window at a map.ini and fill the rows from it.
	void openOn(const QString &iniPath);

	// Read map.ini again, then push every row to the 3D view.
	void reseed();

	// Write the pending changes. False when map.ini could not be written yet.
	bool flush();

	// Another map is loading: save, forget the path and hide.
	void mapChanged();

	static WBQtWaterTuningPanel *instance() { return s_instance; }

protected:
	virtual void hideEvent(QHideEvent *event);

private slots:
	void onSpinChanged(double value);
	void onSliderChanged(int value);
	void onColourClicked();
	void onCheckToggled(bool on);
	void onTextChosen();
	void onResetClicked();
	void onAdvancedToggled(bool on);
	void onSaveTimer();

private:
	struct Row
	{
		Row();

		int index;				// the key's index in the bridge table
		int kind;				// WBQT_WATER_*
		QString key;
		QString block;			// map.ini block the key is saved in
		float lo;				// the slider spans lo to hi
		float hi;
		float step;
		bool advanced;
		bool inFile;			// map.ini sets this key
		float value[3];
		QString text;			// text rows
		QLabel *label;			// float and text rows
		QCheckBox *check;		// bool rows, and the colour's "set in map.ini" box
		QComboBox *combo;		// text rows, editable, listing the files the bridge found
		WBQtScrubSpinBox *spin;
		QSlider *slider;
		QToolButton *colour;
		QToolButton *reset;
		QVector<QWidget *> widgets;
	};

	void buildRows();
	QGridLayout *addTab(const QString &title);
	void addSlider(Row &row, int rowIndex, QGridLayout *grid, int gridRow, const QString &tip);
	void updateWaterAvailability();
	void showRow(int r);
	void loadBase(Row &row);		// the Water.ini or GameData.ini value
	void pushLive(const Row &row);	// into the 3D view
	void markChanged(int r);
	void resetRow(int r);
	QString formatValue(const Row &row) const;
	void setStatus(const QString &text);

	Ui::WBQtWaterTuningPanel *m_ui;	// owns the static widget tree (WBQtWaterTuningPanel.ui)

	QVector<Row> m_rows;
	QMap<QString, QString> m_pending;	// key -> text to write, a null string removes the key
	QString m_path;
	QTimer *m_saveTimer;
	bool m_updating;
	QGroupBox *m_advancedGroup;

	static WBQtWaterTuningPanel *s_instance;
};

#endif // WB_QT_WATER_TUNING_PANEL_H
