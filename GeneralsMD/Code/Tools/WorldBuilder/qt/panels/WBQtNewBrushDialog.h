// WBQtNewBrushDialog.h -- defines a WorldBuilder-only terrain brush from the Terrain Material
// panel. A brush paints an existing terrain under a new name, or registers a new texture; it is
// saved to WorldBuilderQt\TerrainBrushes.ini through WBQtTerrainMaterial_CreateBrush.
#ifndef WB_QT_NEW_BRUSH_DIALOG_H
#define WB_QT_NEW_BRUSH_DIALOG_H

#include <QDialog>
#include <QStringList>

namespace Ui { class WBQtNewBrushDialog; }	// generated from WBQtNewBrushDialog.ui

class WBQtNewBrushDialog : public QDialog
{
	Q_OBJECT
public:
	WBQtNewBrushDialog(const QStringList &categories, const QString &category, const QString &terrain, QWidget *parent);
	virtual ~WBQtNewBrushDialog();

	// The texture class the new brush paints, valid after the dialog is accepted.
	int createdTexClass() const { return m_texClass; }
	QString createdName() const { return m_name; }

public slots:
	virtual void accept();

private slots:
	void onModeChanged();
	void onBrowse();

private:
	Ui::WBQtNewBrushDialog *m_ui;	// owns the static widget tree (WBQtNewBrushDialog.ui)
	int m_texClass;
	QString m_name;
};

#endif // WB_QT_NEW_BRUSH_DIALOG_H
